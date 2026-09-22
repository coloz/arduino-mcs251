# 硬件接口适配检查（2026-09-23）

本表区分芯片拥有的资源与本项目实际提供的 Arduino API。变体里有数量、引脚或容量宏，并不代表对应驱动已经实现；目标编译和寄存器模拟也不能替代实板验证。

| 接口 | 当前实现 | 尚未覆盖的硬件能力 |
| --- | --- | --- |
| UART/USART | 按型号提供 `Serial1`～`Serial4`，G144 扩展到 `Serial8`；独立接收中断、8N1、同步发送、硬件引脚组切换 | 其他帧格式、USART 同步通信、LIN、DMA；CL 当前封装未引出的 UART2 不可使用 |
| IIC | `Wire` 主机/从机；G144 第二控制器为独立 `Wire1` | AI8051U 主机仍为软件实现；DMA 和 10 位地址未提供 |
| SPI | 各型号 1 路独立硬件 SPI，G144 共 3 路，分别通过 `SPI`、`SPI1`、`SPI2` 使用；主机四种模式、位序、事务保护和软件回退 | SPI 从机、DMA、FIFO 批量传输、USART 模拟 SPI；QSPI 是另一类控制器，未包含在这里 |
| CAN | G12/G8/CL 两路经典 CAN；G144 的两个 CAN-FD 控制器可用经典 CAN 帧；AI8051U 没有原生 CAN | G144 的 FD 帧、64 字节数据及 BRS；硬件接收回调、DMA |
| USB | G12/G144/AI 的设备 CDC、HID、Keyboard、Mouse；G8/CL 没有应用 USB 支持 | MSC 类、完整通用 PluggableUSB 扩展 |
| ADC | ADC1 外部引脚的同步 `analogRead()`，支持 10/12 位返回值 | G144 的 ADC2、内部通道、DMA 连续采样 |
| PWM | PWMA/PWMB 的 8 路正向输出，`analogWrite()` 为 8 位、固定频率 | G144 其余 16 路 PWM；互补输出、死区、捕获和可调频率 API |
| 外部中断 | P3.2/INT0 与 P3.3/INT1 的 `attachInterrupt()` | INT2～INT4、全 GPIO 中断与唤醒；当前不支持 LOW 模式 |
| 定时器 | Timer0 计时、串口波特率发生器、Timer2 的 `tone()` | 通用定时器、捕获/比较库；额外定时器不是自动可用的 Arduino API |
| EEPROM/IAP | 没有数据存储库；当前 IAP 操作仅用于进入 ISP | Arduino EEPROM、FlashStorage、数据区读写/擦除 |
| 其他 | 未提供公开驱动 | RTC、WDT、DMA、比较器；有相应硬件的型号还缺 I2S、QSPI、DAC、OPA、LCD/TFT 并口等接口 |

`SD` 通过 SPI 访问存储卡，不是片内 EEPROM/IAP 驱动；`LiquidCrystal`、`Stepper` 和 `SoftwareSerial` 是基于 GPIO 的实现，不代表专用硬件控制器已经适配。新增上述专用接口需要驱动和独立验证，不能只补变体宏。

## 本轮确认并修正的问题

- IIC 主机必须读取 `I2CxMSST.MSACKI`（bit 1）判断收到的 ACK/NACK。bit 0 是主机发送的 ACK/NACK；原代码及旧模拟模型混淆了两者。现已区分“地址/数据 NACK”和“读末尾主动发送 NACK”。
- UART1 初始化时保存并清零 Timer1 的 TM1PS 预分频，结束后恢复；避免已有预分频设置使实际波特率偏低。对三类芯片分别验证了中断开关、XFR 窗口和定时器状态恢复。
- SPI 需要独立的能力配置，不能仅由其他总线的 `bus_layout` 推断。AI8051U 的 SPI 数据引脚与 STC32 不同，G144 的 3 个 SPI 控制器需要各自寄存器、复用和软件状态。
- SPI 的 `/2` 时钟需要高速使能；普通字节轮询必须关闭 FIFO，并清除可能遗留的 MOSI/MISO 交换。复用寄存器的修改只影响所选控制器。
- G144 的 SPI 高速源为 HSIO，不等于 CPU 时钟。驱动验证现有 PLL 输入和分频后才使用硬件；PLL 未开启或时钟不可确认时回退 GPIO，通过 `usingHardware()` 可查询。驱动不配置共享 PLL，因此这部分仍需要板级时钟初始化配合。
- 公开总线对象的非法编号不应静默转而操作第一个控制器；错误对象必须返回配置错误且不访问硬件。
- 更新了主 README 的多串口说明及 USB 文档中过时的 G144 默认 XRAM 配置。

## 资料和实现入口

- [变体数量、引脚和示例](README.md)、[SPI](../libraries/SPI/README.md)、[Wire](../libraries/Wire/README.md)、[CAN](../libraries/CAN/README.md)、[USB](../libraries/USB/README.md)。
- [STC32G 官方手册](https://www.stcmicro.com/datasheet/stc32g-cn.pdf)：UART、SPI/IIC、CAN、定时器和封装复用。
- [STC32G144K246 官方手册](https://www.stcmicro.com/datasheet/STC32G144K246-cn.pdf)：第 25 章为 3 组 SPI，第 26 章为独立 QSPI，第 27 章为 2 组 IIC；另含 ADC1/2、PWM、DAC 和 DMA 资源。
- [AI8051U 官方资料](https://www.stcmicro.com/datasheet/Ai8051U_Features.pdf)：独立 SPI、USART 同步接口、IIC、QSPI 等资源与引脚表。
- [AI8051U 官方 32 位库](https://www.stcmicro.com/rar/demo/AI8051U-32bit%d7%a8%d3%c3%bf%e2%ba%af%ca%fd.zip)：芯片专用头文件交叉核实 Timer1～4 预分频及 SPI/时钟寄存器地址。
- [G12 产品页](https://www.stcmicro.com/cn/stc/stc32g12k128.html)、[G8 产品页](https://www.stcmicro.com/cn/stc/stc32g8k64.html)、[CL 产品页](https://www.stcmicro.com/cn/stc/stc32cl8k64.html)用于交叉核对型号功能。
