# STC CAN

使用片上 CAN 控制器，实现 Arduino 官方 [HardwareCAN / CanMsg](https://github.com/arduino/ArduinoCore-API/blob/master/api/HardwareCAN.h) 风格的经典 CAN 2.0 API。支持标准/扩展 ID、0～8 字节数据帧及 RTR 远程帧；`CAN` 对应芯片 CAN1，`CAN1` 对应芯片 CAN2。

## 示例程序

在 Arduino IDE 的 `文件 → 示例 → CAN` 中打开，默认波特率为 500 kbit/s，UART1 日志为 115200 baud。芯片列表位于各示例开头。

| 示例 | 演示内容 | 使用方式 |
| --- | --- | --- |
| [CANWrite](examples/CANWrite/CANWrite.ino) | 标准帧、4 字节小端计数器、发送结果 | 与另一节点的 CANRead 配对 |
| [CANRead](examples/CANRead/CANRead.ino) | `available()` / `read()`、打印帧、计数器解码 | 与 CANWrite 配对 |
| [CANReadFilter](examples/CANReadFilter/CANReadFilter.ino) | 标准 ID 掩码过滤、扩展 ID 过滤 | 默认接收 0x120～0x12F；修改 `useExtendedIds` 接收 0x18FF5000～0x18FF50FF |
| [CANExtendedFrames](examples/CANExtendedFrames/CANExtendedFrames.ino) | 29 位扩展帧发送及 ID 类型识别 | 周期发送 0x18FF5010，可与扩展过滤示例配对 |
| [CANRemoteFrames](examples/CANRemoteFrames/CANRemoteFrames.ino) | RTR 请求、DLC、响应及发送忙重试 | 两块板分别设置 `requester=true` / `false` |
| [CANDualBus](examples/CANDualBus/CANDualBus.ino) | `CAN` / `CAN1`、引脚配置、同一 MCU 双路收发 | 两路各接一个收发器，再连接到同一条 CAN 总线 |
| [CANSendReceive](examples/CANSendReceive/CANSendReceive.ino) | 单路周期发送并接收 | 与另一 500 kbit/s 节点连接 |
| [CANPackets](examples/CANPackets/CANPackets.ino) | 分包 `Stream` 适配器 | 将接收数据以标准 ID 0x321 回传 |

独立收发和过滤场景参考 Arduino 官方 [Arduino_CAN 示例](https://github.com/arduino/ArduinoCore-renesas/tree/main/libraries/Arduino_CAN/examples)，代码按 STC 的 C++11、16 位 `int` 和大端 ABI 编写。过滤示例使用本库的 `filter()` / `filterExtended()`；计数器明确按字节编码。默认接线见下节。

## 支持范围与接线

| 型号 | 控制器 |
| --- | --- |
| STC32G12K64 / G12K128、STC32G8K48 / G8K64 | 两路经典 CAN |
| STC32CL8K48 / CL8K64 | 两路经典 CAN |
| STC32G144K246 | 两路 CAN-FD 控制器，以经典 CAN 帧接口运行 |
| AI8051U-34K16 / 34K32 / 34K64 | 无原生 CAN，`begin()` 返回 false、错误为 `STC_CAN_UNSUPPORTED` |

默认 `CAN` RX/TX 为 P0.0/P0.1，`CAN1` 为 P0.2/P0.3。RX 接外部 CAN 收发器 RXD，TX 接 TXD；CANH/CANL 由收发器连接总线，不能直接接 MCU GPIO。需匹配逻辑电平、共地、解除收发器待机，并在总线两个物理端点各接 120Ω 终端电阻。发送需要总线上其他节点提供 ACK。

可在 `begin()` 前调用 `CAN.setPins(rx, tx)`，支持的组合如下；封装必须实际引出这些引脚：

| 控制器 | 所有支持 CAN 的型号 | G144 额外组合 |
| --- | --- | --- |
| `CAN` | P0.0/P0.1、P4.2/P4.5、P7.0/P7.1；经典控制器另有 P5.0/P5.1 | P3.0/P3.1、P3.6/P3.7、P1.4/P1.5、P4.3/P4.4 |
| `CAN1` | P0.2/P0.3、P5.2/P5.3、P4.6/P4.7、P7.2/P7.3 | P1.0/P1.1、P8.4/P8.5、PA.0/PA.1 |

G144 的 CAN1 第 1 组备用引脚在官方手册与示例中有 P5.2/P5.1 的差异，因此暂不开放该组。库不控制 CANSTB 引脚，按实际收发器另行配置。`end()` 停止控制器并恢复外设复用选择，GPIO 模式由应用程序后续重新设置。不要和其他驱动同时操作同一路 CAN。

## 官方消息 API

```cpp
#include <Arduino_CAN.h>  // 也可以包含 CAN.h

void setup() {
  Serial.begin(115200);
  if (!CAN.begin(CanBitRate::BR_500k)) {
    Serial.println(CAN.lastError());
  }
}
void loop() {
  if (CAN.available()) {
    CanMsg message = CAN.read();
    Serial.println(message);
  }
}
```

发送用法：

```cpp
const uint8_t bytes[] = {0x12, 0x34};
CanMsg message(CanStandardId(0x123), sizeof(bytes), bytes);
int result = CAN.write(message);  // 1：已提交；负值：错误
// 扩展 ID：CanExtendedId(0x1234567)
// RTR：message.id |= CanMsg::CAN_RTR_FLAG; data_length 表示请求的 DLC
```

`CanBitRate` 支持 `BR_125k`、`BR_250k`、`BR_500k`、`BR_1000k`；STC 扩展 `CAN.begin(uint32_t bitrate)` 接受 10k～1M bit/s 内可精确分频的速率。无法精确配置的速率返回失败，不静默近似。经典控制器以 `F_CPU / 2`、G144 以 `F_CPU` 计算位时序，采样点尽量接近 80%；CPU 实际时钟必须与所选板卡菜单一致。

| API | 行为 |
| --- | --- |
| `begin(rate)` | true 成功；再次调用先停止旧配置，失败后保持停止 |
| `end()` | 停止、清空软件队列和过滤条件 |
| `write(const CanMsg&)` | 1 已提交硬件；负错误码表示未提交。成功不代表已获得总线 ACK |
| `available()` | 待接收帧数，内部轮询硬件并填充最多 4 帧的软件队列 |
| `read()` | 返回一帧；为空时返回零初始化 `CanMsg`，应先检查 `available()` |
| `read(CanMsg&)` | STC 扩展，返回 bool，能直接区分空队列与合法零字节帧 |
| `filter(id, mask)` / `filterExtended(id, mask)` | STC 软件过滤，按 ID 类型及 `(received & mask) == (id & mask)` 接收；更新时清空软件队列 |
| `lastError()` | 错误常量见 [CAN_backend.h](src/CAN_backend.h)：UNSUPPORTED、INVALID、BUSY、BUS_OFF、STOPPED、OVERFLOW |

软件过滤不阻止控制器 ACK 其他合法帧，也不降低硬件接收负担；`end()` / `begin()` 恢复接收所有 ID。接收队列满后暂缓搬运，硬件缓冲仍可能溢出；请及时调用 `available()` / `read()`。写入为非阻塞单硬件发送槽，BUSY 时应用自行重试；bus-off 可处理故障后重新 `begin()`。这些 API 仅用于主循环，不提供中断回调。

`CanMsg` 有 `id`、`data_length`、`data[8]`，可直接交给 `Serial.print()`；其构造函数沿用官方对长度上限的裁剪。驱动按字节序列化，不把 MCS251 大端结构体直接发到总线。STC 的 `CanBitRate` 使用 32 位枚举底层类型；因当前 C++ 编译桥不能在虚表中返回结构体，`HardwareCAN::read()` 内部通过虚函数 `read(CanMsg&)` 转接。普通草图调用方式不变，自定义继承类需实现输出参数版本。

## 常见分包 API

第三方 `arduino-CAN` 的 `read()` 返回字节，与官方消息 API 同名但含义不同。要迁移这种代码，请显式创建 [CANPacketClass](src/CANPacket.h)：

```cpp
#include <CANPacket.h>
CANPacketClass packets(CAN);
// setup(): packets.begin(500000);
// 发送：
// packets.beginPacket(0x123);
// packets.print("hello");
// packets.endPacket();
// 接收：
// if (packets.parsePacket()) while (packets.available()) Serial.write(packets.read());
```

支持 `beginExtendedPacket()`、DLC/RTR 参数、`packetId()`、`packetExtended()`、`packetRtr()`、`packetDlc()`、`peek()` 和 `Stream` 方法。RTR 不提供数据字节；指定 DLC 大于已写数据时补零。`parsePacket()` 的返回值不能区分零 DLC 帧与无帧，此时使用消息 API。未实现第三方库的 `onReceive()`、loopback、sleep/wakeup、SPI/MCP2515 后端；`setPins()` 在这里表示原生 RX/TX。

## 实现参考

G144 底层不会把 CAN-FD 数据帧截断成经典 CAN 帧，而是丢弃；本次不提供 CAN-FD 发送或速率切换 API。

寄存器参考 [G144 官方手册](https://www.stcaimcu.com/data/download/Datasheet/STC32G144K246/STC32G144K246-4.pdf)、[G12](https://www.stcmicro.com/stc/stc32g12k128.html)、[G8](https://www.stcmicro.com/stc/stc32g8k64.html) 和 [CL](https://www.stcmicro.com/stc/stc32cl8k64.html) 官方资料。驱动和 API 实现采用本项目 MIT 许可证。
