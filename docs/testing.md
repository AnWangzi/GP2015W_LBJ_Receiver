# 测试说明

主机测试编译实际C源码，模拟GPIO、时钟、Flash和串口，不连接设备：

```powershell
python tools/run_native.py
```

需要GCC在PATH，或用 `--cc '<gcc路径>'` 指定。产物在本地 `keilproj/diagnostics/`，不进Git。默认不需要Pillow或pyserial。

| 类别 | 覆盖 |
| --- | --- |
| POCSAG/BCH | 纠错、拒绝、极性、跨批次与损坏 |
| LBJ | 字段、车型、交错、详情替换、超时和回绕 |
| 历史 | 100项、去重、淘汰、补全和稳定选择 |
| 界面 | 按键、逐级返回、背光、电池和实际字形像素 |
| 低功耗 | 入口寄存器、IRQ竞态和长短按边界 |
| IAP | 真实C与Python、丢应答重试、CRC和398个断电点 |

```powershell
python -m pip install -r tools/requirements.txt
python tools/run_native.py --visual
```

`--visual`用合成数据生成PNG，需要Pillow。ARM构建后再运行 `verify_iap_artifacts.py` 和 `check_realtime.py` 检查向量、邮箱、元数据、包及链接调用图。未构建ARM时，主机测试不声称验证过本次ARM包。

提交前运行 `python tools/audit_repository.py --staged`，扫描Git索引的实际内容。风险只输出文件/行号/类型，不回显凭证；自动检查不能代替新增资料的人工审查。
