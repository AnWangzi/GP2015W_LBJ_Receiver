# GP2015W 固件工程

本工程针对GP2015W当前已核对的硬件。主应用在 `src/`，厂商依赖在 `common/`，引导在 `iap/`。采用定时节拍、有界短任务和中断采样，接收期间不使用延时函数。

在仓库根目录运行：

```powershell
.\tools\build_firmware.ps1 -Iap -KeilRoot '<Keil安装目录>'
```

脚本生成字模、工程及安装镜像，不烧写设备。`1.uvprojx` 是地址0的独立应用；已安装IAP时用 `iap/release/lbj-update.lbju`，不能覆盖引导。

详见 [构建](../docs/build.md) 和 [更新](../docs/firmware-update.md)。个人调试器设置、生成目录和安装状态只留在本地。
