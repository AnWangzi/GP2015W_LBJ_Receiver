# 硬件接线

本表适用于当前已核对板型，不代表其他批次通用接法。

| 器件 | 配置 |
| --- | --- |
| MCU | HC32L176KATA，128KiB Flash / 16KiB RAM，RCH 4MHz |
| 射频 | Si4463，26MHz时钟，821.2375MHz / 1200bps |
| 屏幕 | 128×64点阵，8080并口 |
| 外置存储 | W25Q80，1MiB，当前应用未使用 |

821.2375MHz属于Si4463保证频段外的使用；实测能接收，不等于保证规格外灵敏度和温度性能。

## 板级引脚

| 功能 | MCU引脚 |
| --- | --- |
| 射频SCLK/MOSI/MISO/CS | PB01/PC05/PB00/PC04 |
| 射频SDN/电源控制 | PA06/PA02 |
| 接收时钟/数据 | PB10/PB02 |
| Si4463 GPIO2 | PA07；GPIO3未确认 |
| 绿/上/下/红 | PB07/PB06/PB05/PB04，低有效、上拉 |
| 背光 | PB03，低有效 |
| 蜂鸣器 | PC08，PCA_CH2 |
| 电池/外部电源/充电 | PA00 ADC0 / PC13 / PC00 |

屏幕RS/WR/RESET为PA09/PA12/PA11；数据线依次为PA08、PC09、PC07、PC06、PB15、PB14、PB13、PB12。

## 后盖触点

沿实测时背面从左到右：

| 序号 | 功能 |
| --- | --- |
| 1 | 5V充电输入，连接充电芯片 |
| 2 | PC11，外置Flash SCLK |
| 3 | PF00，UART1 TX |
| 4 | RESETB，低有效 |
| 5 | PC12，外置Flash CS |
| 6 | PA15，外置Flash MISO |
| 7 | PF01，UART1 RX |
| 8 | PC10，外置Flash MOSI |
| 9 | GND |

UART用3.3V逻辑，交叉TX/RX、共地，不接模块电源。外部SPI访问需确认电平和总线控制；1号5V不能直接给Flash供电。
