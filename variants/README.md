# 串口、IIC 与 SPI 配置

按 STC 官方资料核对，下面的数量指独立硬件控制器；同一控制器的多组复用引脚不增加控制器数量。

| 变体 | 硬件 UART / USART | 硬件 IIC | 独立硬件 SPI | Arduino 对象 |
| --- | ---: | ---: | ---: | --- |
| STC32G12K128、STC32G12K64 | 4 | 1 | 1 | `Serial1`～`Serial4`、`Wire`、`SPI` |
| STC32G8K64、STC32G8K48 | 4 | 1 | 1 | `Serial1`～`Serial4`、`Wire`、`SPI` |
| STC32CL8K64、STC32CL8K48 | 4 | 1 | 1 | `Serial1`、`Serial3`、`Serial4`、`Wire`、`SPI` |
| AI8051U-34K64、AI8051U-34K32、AI8051U-34K16 | 4 | 1 | 1 | `Serial1`～`Serial4`、`Wire`、`SPI` |
| STC32G144K246 | 8 | 2 | 3 | `Serial1`～`Serial8`、`Wire`、`Wire1`、`SPI`、`SPI1`、`SPI2` |

STC32CL 的当前变体采用 TSSOP20 引脚表，UART2 的两组引脚均未引出，因此不声明 `Serial2`。`STC_VARIANT_UART_COUNT` 保留硅片的控制器数量；`STC_CORE_UART_AVAILABLE_MASK` 和各 UART 的 `ROUTE_COUNT` 表示当前封装实际可用的接口。其他封装应建立对应引脚表，不能直接使用未引出的端口。

## 默认引脚

| 对象 | RX / SDA | TX / SCL | 说明 |
| --- | --- | --- | --- |
| `Serial1` | P3.0 | P3.1 | 所有型号 |
| `Serial2` | P1.0 | P1.1 | STC32G 系列 |
| `Serial2` | P1.2 | P1.3 | AI8051U 系列 |
| `Serial3` | P0.0 | P0.1 | 所有型号 |
| `Serial4` | P0.2 | P0.3 | 所有型号 |
| `Serial5` | P0.4 | P0.5 | STC32G144K246 |
| `Serial6` | P0.6 | P0.7 | STC32G144K246 |
| `Serial7` | P5.0 | P5.1 | STC32G144K246 |
| `Serial8` | P5.2 | P5.3 | STC32G144K246 |
| `Wire` | P3.3 | P3.2 | STC32 主机/从机、AI8051U 硬件从机 |
| `Wire` | P3.2 | P3.3 | AI8051U 软件主机的既有默认值 |
| `Wire1` | P2.6 | P2.7 | STC32G144K246 的 IIC2 |

完整复用表见各型号的 `variant.json` 中 `peripherals.uart` / `peripherals.i2c`，以及 `pins_arduino.h` 中的 `STC_VARIANT_UARTn_*` / `STC_VARIANT_I2Cn_*`。表中已过滤封装未引出的引脚。默认引脚宏为 `PIN_SERIALn_RX/TX`、`PIN_I2Cn_SDA/SCL`。

## 使用方法

```cpp
#include <Arduino.h>

void setup() {
  Serial3.begin(115200); // UART3 默认 P0.0 / P0.1
  Serial4.begin(9600);  // UART4 默认 P0.2 / P0.3
  if (Serial3.configurationError() || Serial4.configurationError()) return;
}

void loop() {
  while (Serial3.available()) Serial4.write(Serial3.read());
}
```

在 `begin()` 前或 `end()` 后调用 `Serialn.setPinsChecked(rx, tx)` 选择完整的硬件引脚组；返回 `false` 表示引脚组无效或端口仍在运行。不能任意组合不同组的 RX/TX。`Serial` 仍按板卡菜单映射到 UART1 或 USB CDC，`Serial0` 仍是 `Serial1` 的别名。建议使用预定义的对象；无参数 `HardwareSerial()` 对应 UART1，带端口号的构造函数供 UART2～UART8 使用，多个同端口对象共享同一硬件状态。

各串口支持 `SERIAL_8N1`、独立中断接收、`available/read/peek/write/flush` 和读取后清除的 `overflow()`。默认接收环形缓冲为 16 字节，可容纳 15 字节；`SERIAL_RX_BUFFER_SIZE` 可设为 2～255。发送同步等待完成，关闭全局中断时仍可轮询完成；不要从中断回调调用阻塞串口 API。

UART1 使用 Timer1，UART2～UART8 分别使用 Timer2～Timer8。新增串口会拒绝占用已运行或已启用中断的定时器；例如 `Serial2` 与 `tone()` 不能同时使用 Timer2。无效波特率、超过 3% 的波特率误差或资源冲突会设置 `configurationError()`。复用引脚与其他串口、USB、SPI、PWM、IIC 的物理冲突需由应用避免。

AI8051U-34K16 的串口配置状态默认使用内部 DATA RAM，以减少 Flash 开销；可用全局编译参数 `-DSTC_SERIAL_STATE_IN_DATA=0` 改回 XDATA。接收缓冲仍在 XDATA。该型号的三路串口转发示例约占 15 KB，叠加其他接口或库前应检查编译容量报告；示例在 16 KB 型号上不启动 UART1。

STC32G144K246 的第二路 IIC 使用 `Wire1`，API 与 `Wire` 相同，主机/从机模式、缓冲、回调和超时状态分别独立：

```cpp
#include <Wire.h>

void setup() {
  Wire.begin();              // IIC1，P3.3 / P3.2
  Wire1.begin();             // IIC2，P2.6 / P2.7
  Wire1.setClock(400000);
  // 也可在初始化前：Wire1.setPinsChecked(P1_6, P1_7);
  // 从机模式：Wire1.begin(0x42);
}
void loop() {}
```

硬件从机允许选用该控制器的任一已引出复用组。STC32 的硬件主机也支持这些组；其他合法 GPIO 保留软件主机回退。AI8051U 保留现有软件主机选择及硬件从机实现，软件主机的 `PIN_WIRE_SDA/SCL` 与硬件默认宏 `PIN_I2C1_SDA/SCL` 不同；无自定义引脚时，`begin(address)` 会切换到硬件从机默认组。本次没有将其主机实现改为硬件。IIC 总线需要外部上拉电阻。更多约束见 [Wire 文档](../libraries/Wire/README.md)。

## SPI 与其他硬件接口

SPI 硬件编号从 1 开始，Arduino 对象从 `SPI` 开始：G144 的 `SPI1` 对应硬件 SPI2，`SPI2` 对应硬件 SPI3。`STC_VARIANT_SPI_COUNT` 与 `peripherals.spi` 描述独立 SPI 控制器；USART 的同步 SPI 模式和 QSPI 不计入该数量，也没有由本库实现。

保留原 `SPI` 默认接线，避免更换版本后改变已有硬件连接。显式调用 `SPI.setPinsChecked(PIN_SPI1_MOSI, PIN_SPI1_MISO, PIN_SPI1_SCK, PIN_SPI1_SS)` 可选择第一路硬件默认组；AI8051U 的硬件默认数据组为 P1.5/P1.6/P1.7，与 STC32 的 P1.3/P1.4/P1.5 不同。G144 第二、三路对象默认数据组为 P6.5/P6.6/P6.7 和 P2.3/P2.4/P2.5。

GPIO 片选可由应用另选；未匹配硬件组、时钟不可确认或无法分频时使用软件 SPI。G144 硬件需有效的既有 HSIO/PLL 配置，本库不更改共享 PLL。用 `usingHardware()` 查询当前方式。更多用法和限制见 [SPI 文档](../libraries/SPI/README.md) 与 [HardwareBuses 示例](../libraries/SPI/examples/HardwareBuses/HardwareBuses.ino)。

其他片上功能并未全部提供 Arduino 库，包括 EEPROM/IAP、RTC、G144 完整 CAN-FD、ADC2 和额外 PWM 等。

## 配置来源与验证

数量、引脚及寄存器依据以下官方资料（核对日期：2026-09-23）：

- [STC32G 技术参考手册](https://www.stcmicro.com/datasheet/stc32g-cn.pdf)：STC32G/CL 产品与封装说明、外设切换、串口及 IIC 章节。
- [STC32G144K246 技术参考手册](https://www.stcmicro.com/datasheet/STC32G144K246-cn.pdf)：USART1～8、Timer2～8、IIC1/2 及 P_SWX 复用表。
- [AI8051U 官方功能与引脚资料](https://www.stcmicro.com/datasheet/Ai8051U_Features.pdf)：4 路串口、1 路 IIC、UART2 与 IIC 的专用复用表。

编辑源数据 [devices.json](../tools/variants/devices.json) 与 [peripherals.json](../tools/variants/peripherals.json)，运行 `node tools/variants/generate.mjs`。生成器校验数量、默认复用组和封装引脚；`--check` 检查生成文件是否同步。

[MultipleSerial](../examples/Practical/MultipleSerial/MultipleSerial.ino) 与 [DualBus](../libraries/Wire/examples/DualBus/DualBus.ino) 可用于目标编译。实际中断时序、波特率和总线电气行为仍需实板验证。
