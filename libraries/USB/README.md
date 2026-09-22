# STC 原生 USB CDC 串口

使用芯片 USB 外设生成电脑上的虚拟 COM 端口，不需要 USB 转 UART 模块。
接口参考 Arduino 原生 USB 串口和 ESP32 Arduino 的 `USB CDC On Boot` / `USBSerial`。
实现随核心编译，不依赖 TinyUSB 或厂商预编译库。

## 直接使用 Serial

选择正确芯片，设置 **工具 → USB CDC On Boot → Enabled (Serial = USB CDC)**：

```cpp
void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) delay(1);
}
void loop() {
  Serial.println("你好啊");
  delay(1000);
}
```

烧录后选择新出现的 USB 串口，打开串口监视器；中文需按 UTF-8 显示。
`begin(115200)` 保留 Arduino 用法，不设置 USB 物理速率。`if (Serial)` 表示设备已配置、
未挂起、串口已启用且主机置位 DTR。未打开串口时写入返回 0，并设置 `getWriteError()`。
避免无期限 `while (!Serial)`，否则未连接电脑时程序会一直等待。
默认 **Disabled** 保持原来的 UART1 行为。CLI 板型参数为 `stc:mcs251:stc32g144k246:cdc=enabled`。

## 接口

| 接口 | 含义 |
| --- | --- |
| `Serial`，CDC On Boot 启用 | 原生 USB CDC |
| `Serial`，CDC On Boot 禁用 | UART1 |
| `USBSerial` / `SerialUSB` | 同一个 USB CDC 对象 |
| `Serial1` / `Serial0` | 同一个 UART1 对象，RX=P3.0、TX=P3.1 |
| `USB` / `USBDevice` | 同一个 USB 设备控制对象 |

显式 USB 串口可以保持菜单 Disabled（16 KB 型号除外）：

```cpp
#include <USB.h>
void setup() {
  USBSerial.begin(115200);
  USB.begin(); // 可省略，USBSerial.begin() 已启动 USB
}
void loop() {
  while (USBSerial.available()) USBSerial.write((uint8_t)USBSerial.read());
}
```

支持 `begin()` / `begin(baud)` / `begin(baud, config)`、`end()`、`available()`、`peek()`、
`read()`、`write()`、`availableForWrite()`、`flush()`、`print()` / `println()` 和 Stream 的
`readBytes()`、`readStringUntil()`、`parseInt()`、`setTimeout()` 等。
`begin(baud, config)` 不改变主机的 line coding，遵循 Arduino 原生 USB 用法；
用 `baud()`、`stopbits()`、`paritytype()`、`numbits()`、`dtr()`、`rts()`、`readBreak()` 查询主机设置。
本机 Windows 实测单独改变 RTS 时状态暂不更新，切换一次 DTR 后 RTS 才传到设备；
这是本次主机组合的实测限制，不应依赖 RTS-only 操作实现即时控制。

RX/TX 各使用 128 字节固定环形缓冲；RX 满时通过 USB NAK 让主机等待，不覆盖旧数据。
`write()` / `flush()` 默认最多等待约 100 ms，另有轮询次数上限。
`setTxTimeoutMs()` 调整发送等待；`setTimeout()` 调整 Stream 读取等待。
大块 `write()` 可能短写，请检查返回值。`flush()` 不清空 RX。
`end()` 停止 CDC 收发，保留接口枚举和 HID 功能；`USBDevice.detach()` 断开整个设备。

## 型号与容量

| 型号 | CDC |
| --- | --- |
| STC32G12K64 / STC32G12K128 | 支持，12 MHz |
| STC32G144K246 | 支持，12 / 48 MHz |
| AI8051U-34K32 / AI8051U-34K64 | 支持；34K64 另支持 40 MHz |
| AI8051U-34K16 | 16 KB 精简配置，启用 CDC 后不支持同时加入 HID；使用 CDCMinimal 示例 |
| STC32G8K48 / G8K64、STC32CL8K48 / CL8K64 | 无应用原生 USB，不提供 CDC 菜单 |

16 KB 型号的 CDC 配置启用 C++ 大小优化，最小回显已接近 Flash 上限；打印、闪灯等较大
程序可能超限。禁用 CDC 时保留独立 HID；显式 `USBSerial` 也需要启用 CDC 菜单。
G12 启用 CDC 时默认动态堆从 6 KB 调整为 4 KB，给 CDC + HID 静态缓冲预留空间。

2026-09-17 的 G144 实板在默认低地址 XRAM 布局下出现二进制数据位错误；仅将 XRAM
改为高地址区后，CDC 和 CDC + HID 收发测试通过。为这块板选择
**工具 → XRAM layout → High bank 64 KiB (0x020000)**，CPU clock 选择实测的 **48 MHz**。
此选项将可用 XRAM 限制为 64 KiB，不改变 Flash 或芯片硬件选项；默认仍保留 128 KiB 布局。
这是当前样板的已验证绕过方式，尚不能据此判定所有 G144 都存在同样问题，或认定芯片物理损坏。
对应 CLI 参数为 `stc:mcs251:stc32g144k246:cdc=enabled,clock=48m,xram=high`。

USB 使用独立 IRC48M，不改变 CPU 时钟；接线以型号和封装引脚图为准。
**P3.0/P3.1 也是默认 UART1 引脚，不能同时运行 UART1 和原生 USB。**
核心拒绝后启动的冲突外设：UART 检查 `configurationError()`，USB 返回 `STC_USB_BUSY`。
若还需要连接串口模块，应使用另一套串口实现和独立引脚。

## CDC 与 HID 共存

包含 `Keyboard.h` / `Mouse.h` 并调用对应 `begin()` 即可，参考
[CDCKeyboardMouse](examples/CDCKeyboardMouse/CDCKeyboardMouse.ino)。CDC 使用接口 0/1、
数据端点 2 和通知端点 3；HID 使用接口 2、端点 1。独立 HID 仍用接口 0、端点 1。

自定义 HID 描述符必须在首次 USB 启动前注册。CDC On Boot 自动连接推迟到 `setup()` 中
首次 USB 轮询、`delay()` / `yield()`，或 `setup()` 返回后，因此 RawHID 可以在 `setup()`
开头注册描述符。连接后再添加描述符会报忙。不要在全局构造函数中调用 `begin()`；
库自带构造函数只注册接口，不操作硬件。

USB 由主循环、`delay()`、`yield()` 和串口 API 协作轮询。长时间阻塞循环需主动调用
`yield()` 或 `USB.poll()`；不要在 ISR 中使用 USB API。
不提供 ESP32 的 FreeRTOS 事件回调、动态缓冲区、多 CDC 实例或 USB Host API。

## 再次烧录

首次烧录或程序不响应时，用 P3.2 配合冷启动进入厂商 USB ISP。
默认支持 **1200 bps + DTR 从 1 变 0** 进入 ISP，约 120 ms 后执行；期间重新拉高 DTR
或切换速率可取消。`USBSerial.enableReboot(false)` 可关闭。
数据流不解释 `@STCISP#`，避免把用户数据误当下载命令。

配套更新的 `stc-cli` 在 G144 的 `Automatic (USB CDC or UART)` 路径中识别新 CDC
并执行 1200 bps 复位，旧 `34BF:FF02` 固件仍使用 `@STCISP#`，进入厂商 HID 后验证型号再下载。
`Native USB` 用于已经手动进入 ISP、没有 COM 的情况。
其他型号支持 CDC 通信，但 `stc-cli` 原生 HID 下载仍限 G144；其余使用官方 ISP 或已支持的
UART 下载方式，不能把应用 CDC 端口当作 UART 下载器。

默认 VID:PID：独立 HID `1209:0001`，独立 CDC `1209:0002`，CDC + HID `1209:0003`，避免
主机缓存不同接口布局。这些是 [pid.codes 私人实验 PID](https://pid.codes/1209/0002/)，不具有
全局唯一性，不能用于制造、销售或分发的设备。正式设备须用自有 VID/PID。
全局构建参数 `-DUSB_VID=... -DUSB_PID=...` 可覆盖；仅在草图中定义不会传给核心 C 文件。
自定义 VID/PID 不会被当前上传器自动识别，需手动进入 ISP。

## 示例

- [CDCMinimal](examples/CDCMinimal/CDCMinimal.ino)：最小回显，适合 16 KB 型号。
- [CDCSerial](examples/CDCSerial/CDCSerial.ino)：闪灯并通过 Serial 打印。
- [CDCSerialEcho](examples/CDCSerialEcho/CDCSerialEcho.ino)：显式 USBSerial，批量回显。
- [CDCKeyboardMouse](examples/CDCKeyboardMouse/CDCKeyboardMouse.ino)：CDC 与键鼠共存。

接口参考
[Arduino CDC](https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/CDC.cpp) 和
[ESP32 Arduino CDC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb_cdc.html)。
