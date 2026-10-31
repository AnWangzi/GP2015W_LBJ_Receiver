# 固件更新

## UART更新

设备需已安装本工程IAP，首次安装见 [构建](build.md)。模块RX接3号PF00、TX接7号PF01、GND接9号；3.3V逻辑、38400/8N1、设备电池供电，不接模块电源。

```powershell
python -m pip install pyserial==3.5
python tools/serial_flash_wizard.py
```

也可双击 `UART_Update.bat`，默认用PATH中的Python，可通过 `LBJ_PYTHON` 指定解释器。原有本地 `tools/serial_deps` 如存在仍可用，但不随仓库分发。

1. 选择实际USB串口，不预设COM号，再选更新或只握手。
2. 默认使用构建生成的 `keilproj/iap/release/lbj-update.lbju`。
3. 已验证的入口：按住绿+红，短接4号RESETB到9号GND，释放RESET后松开按键。
4. 电脑按回车开始等待，再接稳触点；等待60秒，握手后继续，Ctrl+C可取消。
5. 完成需有CRC、安装及启动确认，随后核对实际页面和接收。

菜单“固件升级”也保留确认入口，确认后黑屏；此入口的实物行为需单独核对，无法握手时使用强制复位入口。

## 检查和日志

```powershell
python tools/serial_flash_wizard.py --check
python tools/serial_flash_wizard.py --list
python tools/serial_flash_wizard.py --probe-only
```

`--check`只检查包，不打开串口。没有板型匹配的有效握手，不发送擦写。工具拒绝普通BIN/HEX及完整安装镜像。

本地日志在 `keilproj/diagnostics/uart-updates/`，可能含路径和设备信息，不提交。`RX=0`表示没有读到回复，读取次数增加不能代替握手成功。协议启动应答不能代替应用运行验证。

## Flash布局

| 区域 | 地址/容量 |
| --- | --- |
| 引导 | `0x00000`，8KiB |
| 应用 | `0x02000`，56KiB |
| 暂存 | `0x10000`，56KiB |
| 活动/待安装元数据 | `0x1E000`/`0x1E200`，各512字节 |

暂存区上传完成并校验后安装。未提交的中断上传保留旧应用；安装中断可恢复。日常UART只更新应用。
