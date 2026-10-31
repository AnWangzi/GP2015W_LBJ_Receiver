#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Interactive Windows UART updater. Uses the existing LBJ IAP protocol."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import traceback

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools/serial_deps'))
from serial_update import BAUD, MAGIC, Updater, unpack


class TracedPort:
    def __init__(self, port):
        self.port = port
        self.rx_bytes = self.tx_bytes = 0
        self.read_calls = 0
        self.rx_preview = bytearray()
        self.commands = {}

    def write(self, data):
        if data.startswith(MAGIC) and len(data) >= 13:
            cmd = str(data[4])
            self.commands[cmd] = self.commands.get(cmd, 0) + 1
        count = self.port.write(data)
        self.tx_bytes += count
        return count

    def read(self, count):
        self.read_calls += 1
        data = self.port.read(count)
        self.rx_bytes += len(data)
        self.rx_preview.extend(data[:max(0, 64-len(self.rx_preview))])
        return data

    def flush(self):
        self.port.flush()

    def summary(self):
        return dict(tx_bytes=self.tx_bytes, rx_bytes=self.rx_bytes, read_calls=self.read_calls,
                    rx_preview_hex=bytes(self.rx_preview).hex(),
                    commands=self.commands)


def transfer(port, image, probe_only, seconds, emit=print, progress=lambda n, t: None):
    """Only valid, matching PING replies allow any Flash command to be sent."""
    updater = Updater(port, timeout=min(0.4, seconds), retries=1)
    deadline = time.monotonic() + seconds
    next_notice = 0
    while time.monotonic() < deadline:
        try:
            updater.probe()
            break
        except TimeoutError:
            if time.monotonic() >= next_notice:
                emit('等待设备握手，剩余约 %d 秒；可按 Ctrl+C 取消。' %
                     max(0, int(deadline-time.monotonic())))
                if isinstance(port, TracedPort):
                    emit('TX:%d 字节  RX:%d 字节  已读取:%d 次' %
                         (port.tx_bytes, port.rx_bytes, port.read_calls))
                next_notice = time.monotonic()+2
    else:
        raise TimeoutError('没有收到有效 IAP 握手；没有发送擦写命令。')
    emit('握手成功：LBJ H176，38400 8N1。')
    if probe_only:
        emit('仅握手测试完成，没有发送擦写命令。')
        return
    updater.timeout, updater.retries = 10, 3
    emit('开始传输，请保持连接和设备供电。')
    updater.flash(image, progress)
    emit('CRC 校验和安装确认通过，已发送启动命令。')
    emit('可以松开触点；设备可能回到待机黑屏，请长按绿键约 1.5 秒开机检查。')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='e.g. COM7; omitted for interactive selection')
    parser.add_argument('--file', type=Path, default=ROOT/'keilproj/iap/release/lbj-update.lbju')
    parser.add_argument('--probe-only', action='store_true', help='PING only, no Flash commands')
    parser.add_argument('--list', action='store_true', help='List ports and exit')
    parser.add_argument('--check', action='store_true', help='Validate package/dependency offline; never opens a port')
    parser.add_argument('--wait', type=float, default=60, help='Handshake window in seconds, default 60')
    args = parser.parse_args(argv)
    if not 0 < args.wait <= 300:
        parser.error('--wait must be >0 and <=300')
    log_dir = ROOT/'keilproj/diagnostics/uart-updates'
    log_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).strftime('%Y%m%d-%H%M%S-%f')
    log_path = log_dir/(stamp+'.log')
    traced = None
    with log_path.open('w', encoding='utf-8') as log:
        def emit(message):
            print(message, flush=True)
            log.write(message+'\n')
            log.flush()
        try:
            import serial
            from serial.tools import list_ports
            emit('日志：'+str(log_path))
            if not args.list:
                blob = args.file.read_bytes()
                image = unpack(blob)
                emit('固件：%s\n应用：%d 字节\n包 SHA256：%s' %
                     (args.file.resolve(), len(image), hashlib.sha256(blob).hexdigest()))
                if args.check:
                    emit('离线检查通过，未打开任何串口。')
                    return 0
            ports = list(list_ports.comports())
            for index, item in enumerate(ports, 1):
                emit('%d. %s  %s' % (index, item.device, item.description))
            if args.list:
                return 0
            if not ports:
                raise RuntimeError('未找到串口，请先插入 USB 转串口模块。')
            if not args.port:
                candidates = [p for p in ports if p.vid is not None]
                default = candidates[0].device if len(candidates) == 1 else ''
                choice = input('输入 COM 号或列表序号%s：' %
                               ('（回车选 '+default+'）' if default else '')).strip()
                if choice.isdigit():
                    index = int(choice)-1
                    if not 0 <= index < len(ports):
                        raise ValueError('无效串口序号。')
                    args.port = ports[index].device
                else:
                    args.port = (choice or default).upper()
                mode = input('1=烧录新版，2=只测握手（回车选1）：').strip() or '1'
                if mode not in ('1', '2'):
                    raise ValueError('请选择1或2。')
                args.probe_only = args.probe_only or mode == '2'
            args.port = args.port.upper()
            if not re.fullmatch(r'COM[1-9][0-9]*', args.port):
                raise ValueError('串口应为 COM7 这样的名称。')
            if args.port not in [p.device.upper() for p in ports]:
                raise ValueError('所选串口当前不存在：'+args.port)
            emit('接收机 PF00/TX（3号）→模块 RXD；PF01/RX（7号）→模块 TXD；9号→GND。')
            emit('仅接三根信号线，使用 3.3V 逻辑；模块电源脚不接。')
            emit('菜单进入固件升级，再按红键确认，设备屏幕和背光熄灭。')
            emit('本次操作：'+('仅握手，不写入' if args.probe_only else '握手成功后自动烧录'))
            input('按回车开始等待，然后抵稳触点；无需提前一直用手抵住。')
            with serial.Serial(port=None, baudrate=BAUD, timeout=0.05, write_timeout=3) as port:
                # These control lines are unused. Do not pulse a target reset.
                port.dtr = port.rts = False
                port.port = args.port
                port.open()
                port.reset_input_buffer()
                traced = TracedPort(port)
                last_percent = [-1]
                def progress(done, total):
                    percent = done*100//total
                    if percent//10 != last_percent[0]//10 or done == total:
                        emit('传输 %d%%（%d/%d 字节）' % (percent, done, total))
                        last_percent[0] = percent
                transfer(traced, image, args.probe_only, args.wait, emit, progress)
            return 0
        except (Exception, KeyboardInterrupt) as exc:
            emit('已取消。' if isinstance(exc, KeyboardInterrupt) else '失败：'+str(exc))
            if isinstance(exc, ImportError):
                emit('缺少 pyserial。当前项目已有 tools/serial_deps；请保留该目录。')
            if traced and any(int(cmd) != 0 for cmd in traced.commands):
                emit('升级尚未确认完成。请保持供电、重新进入升级后重试。')
            else:
                emit('未发送擦写命令，设备原固件未被本次操作改写。')
            log.write(traceback.format_exc())
            return 1
        finally:
            if traced:
                emit('串口统计 TX:%d 字节  RX:%d 字节  已读取:%d 次' %
                     (traced.tx_bytes, traced.rx_bytes, traced.read_calls))
                log.write('\nUART '+json.dumps(traced.summary())+'\n')
            emit('日志已保存：'+str(log_path))


if __name__ == '__main__':
    sys.exit(main())
