# GP2015W_LBJ_Receiver

用于 **GP2015W 手表式接收机**的 LBJ 固件，基于该设备的 HC32L176、Si4463、屏幕及板级接线实现，包含 POCSAG 解码、列车信息显示、按键菜单、低功耗关机和后盖串口升级。

本工程针对 GP2015W 当前已核对的硬件，不能直接用于任意 HC32L176 + Si4463 开发板；其他板型需要重新适配引脚、屏幕、电源和射频配置。

当前工程版本：**0.1.1**，以 [VERSION](VERSION) 为准。每次版本更新同步维护中文 [CHANGELOG.md](CHANGELOG.md)，Git 提交说明使用中文；明确发布时再创建版本标签和 GitHub Release。

## 硬件与功能

- HC32L176KATA，128KiB Flash / 16KiB RAM；当前使用RCH 4MHz。
- Si4463接收、POCSAG 1200bps和BCH纠错，车次、速度、公里标、前缀、机车型号及车号显示。
- 128×64点阵屏，中文菜单、背光、提示音、电量和充电状态。
- 最近100个不同车次的RAM历史，同车次合并最新信息，重新开机清空。
- 绿键长按1.5秒开机，关机进入DeepSleep，提前松开则重新休眠。
- UART IAP分段上传、CRC校验、暂存后安装和断电恢复。

接收链路和后盖UART更新已有实物验证。最新交错接收保护、DeepSleep唤醒和整机待机电流仍需针对当前构建核对，不能把主机测试等同于现场验证。见 [当前状态](docs/status.md)。

## 构建

需要Windows、Python 3.7+、Keil MDK/ARMCC 5和HC32L17x设备包。在工程根目录执行：

```powershell
python -m pip install pyserial==3.5
.\tools\build_firmware.ps1 -Iap -KeilRoot '<Keil安装目录>'
```

脚本只生成文件。已有IAP的设备使用 `keilproj/iap/release/lbj-update.lbju`；首次安装使用单独生成的完整镜像。详见 [构建说明](docs/build.md)。

源码、最小厂商依赖和283个字形的源数据随仓库提供；正常构建和主机测试不需要设备原始备份、调试日志或本地参考资料。

## 后盖串口升级

双击 `UART_Update.bat`，或运行：

```powershell
python tools/serial_flash_wizard.py
```

| 后盖触点 | 设备功能 | 接3.3V USB串口模块 |
| --- | --- | --- |
| 3号 PF00 | TX | RXD |
| 7号 PF01 | RX | TXD |
| 9号 | GND | GND |

参数为38400、8N1，设备电池供电，模块电源脚不接。已验证的恢复入口是按住绿键和红键，再将4号RESETB短接到9号GND，释放复位后松开按键。菜单进入升级的实物行为仍需单独核对。见 [固件更新](docs/firmware-update.md)。

## 验证

主机测试需要Python和GCC，不需要连接设备：

```powershell
python tools/run_native.py
python tools/audit_repository.py --staged
```

覆盖真实C解码、连续接收关联、历史、菜单、低功耗入口和真实C引导与Python上传器的交互。安装Pillow后可加 `--visual` 生成合成数据预览。见 [测试说明](docs/testing.md)。

## 工程目录

| 目录 | 内容 |
| --- | --- |
| `keilproj/src/` | 应用、板级驱动、解码和界面 |
| `keilproj/iap/` | 引导、升级协议、链接布局 |
| `keilproj/common/` | CMSIS与厂商依赖 |
| `assets/fonts/` | 当前界面字形源数据 |
| `tools/` | 构建、测试、更新和仓库检查 |
| `docs/` | 架构、接线、构建、验证和状态 |

运行逻辑见 [架构](docs/architecture.md)，引脚见 [硬件](docs/hardware.md)，后续计划见 [ROADMAP.md](ROADMAP.md)。

## 数据与许可证

仓库不收录设备原始镜像、现场照片、接收记录、调试日志、凭证、个人路径或IDE用户设置。白名单式 `.gitignore` 将本地资料排除出版本范围，见 [仓库约定](docs/repository-policy.md)。

应用和工具按GPL-3.0分发，见 [LICENSE](LICENSE)。移植参考、字库、CMSIS和厂商代码保留各自声明，见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
