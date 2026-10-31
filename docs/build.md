# 构建说明

需要Windows/PowerShell、Python 3.7+、Keil MDK/ARMCC 5及HC32L17x设备包。主机测试另需GCC；PNG预览推荐Python 3.9+和Pillow。

仓库已提供当前CMSIS、厂商源码和字形子集，不需要本地备份或完整SDK：

```powershell
.\tools\build_firmware.ps1 -Iap -KeilRoot '<Keil安装目录>'
```

`KeilRoot` 包含 `UV4/UV4.exe` 和 `ARM/ARMCC/Bin/fromelf.exe`。脚本生成UI文字、字模、工程列表、引导、重定位应用及完整安装目标，要求零错误零警告。不带 `-Iap` 只生成地址0独立应用。

| 产物 | 用途 |
| --- | --- |
| `keilproj/Objects/lcdtest.bin` | 地址0独立应用，不适用于已有IAP的设备 |
| `keilproj/iap/release/lbj-update.lbju` | UART应用更新包 |
| `keilproj/iap/release/first-install.hex` | 首次SWD完整安装镜像 |
| `keilproj/iap/install.uvprojx` | 完整安装镜像的Keil工程 |
| `keilproj/iap/release/build-info.json` | 版本、Git提交、工作区状态和校验值 |

构建不操作设备。首次安装需在Keil选择自己的调试器配置并单独下载。生成产物和个人设置不进Git。

```powershell
python tools/verify_iap_artifacts.py
python tools/check_realtime.py
```

文字在 `tools/gen_ui_text.py`；新增汉字先补 `assets/fonts/unifont-subset.hex` 中的GNU Unifont字形并保留许可。缺字使构建失败，避免静默漏字。外置字库及线路显示尚未实现，GBK原始字节不能直接交给UTF-8接口。
