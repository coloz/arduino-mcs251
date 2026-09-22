# STC USB HID

在支持的 64 KiB 及以上配置中，7 字节标量状态放入 DATA；报告 ID 数量、
描述符容量和报告缓冲区保持不变。可通过平台级参数 `-DSTC_HID_STATE_IN_DATA=0`
恢复 XDATA 布局；包含堆预留的 XDATA 分配可能接近设备容量。

使用芯片原生 USB 全速控制器，提供键盘、鼠标和自定义 HID 报告。键鼠上层分别使用随平台提供的 [Keyboard](../Keyboard/README.md)、[Mouse](../Mouse/README.md)，可以同时使用；设备只有一个 HID 接口，通过 Report ID 区分功能。

## 示例程序

在 Arduino IDE 的 `文件 → 示例 → HID` 中打开；支持型号见各示例开头。

| 示例 | 报告 | 接线及操作 |
| --- | --- | --- |
| [RawHID](examples/RawHID/RawHID.ino) | ID 3，8 字节输入/输出 | 主机发来的报告原样回传；主机缓冲区首字节为 ID，随后是 8 字节数据 |
| [ConsumerControl](examples/ConsumerControl/ConsumerControl.ino) | ID 3，16 位 Consumer Usage | P3.2/P3.3/P3.4/P3.5 按钮分别为播放暂停/音量增/音量减/静音，按钮另一端接 GND |
| [HIDGamepad](examples/HIDGamepad/HIDGamepad.ino) | ID 4，4 个按钮和两个有符号绝对轴 | 摇杆 X/Y 接 A0/P1.0、A1/P1.1；P3.2～P3.5 为按钮，各接 GND |

媒体键和游戏手柄示例演示 `HIDSubDescriptor`、`AppendDescriptor()`、`RegisterReport()` 和 `SendReport()`，Usage 定义参考 [USB-IF HID Usage Tables](https://www.usb.org/sites/default/files/hut1_3_0.pdf)。媒体键同一时间报告一个按键，松开时发送零；游戏手柄可在系统的游戏控制器面板观察轴和按钮。摇杆输出电压须在芯片 ADC 输入范围内。

多个自定义功能组合时为它们分配不同的 Report ID，并在首次 `begin()` 前注册所有描述符。RawHID 和 ConsumerControl 各自独立使用 ID 3；键盘保留 ID 2，鼠标保留 ID 1。

以上功能尚未发布到开发板管理器，需从当前源码构建。

## 芯片与接线

| 型号 | 应用程序 USB HID |
| --- | --- |
| STC32G144K246 | 支持，12/48 MHz 主时钟 |
| STC32G12K64 / G12K128 | 支持，12 MHz 主时钟 |
| AI8051U-34K32 / 34K64 | 支持；34K64 可选 40 MHz |
| AI8051U-34K16 | 有原生 USB，但当前键鼠示例超出 16KB Flash；自定义 HID 的占用取决于草图 |
| STC32G8K48 / G8K64、STC32CL8K48 / CL8K64 | 不支持应用程序原生 USB；`HID().begin()` 返回 0，错误为 `STC_USB_UNSUPPORTED` |

USB 使用独立 IRC48M 时钟，不改变 CPU 主时钟。把 USB 连接器接到对应芯片和封装的 D+/D− 引脚，并按芯片手册连接电源、地线和外围元件。支持 USB ISP 下载不等于具备应用程序 USB 外设；USB 转串口模块也不能代替原生 USB 接线。

## 键盘、鼠标

打开 [KeyboardMouse](../Keyboard/examples/KeyboardMouse/KeyboardMouse.ino) 示例。P3.2 与 GND 之间接按钮，每次按下输出一行文字并移动鼠标。主要调用与 Arduino 库一致：

```cpp
#include <Keyboard.h>
#include <Mouse.h>

void setup() {
  Keyboard.begin();
  Mouse.begin();
}
void loop() {
  // 在自己的按钮或其他事件中调用：
  // Keyboard.print("Hello");
  // Keyboard.press(KEY_LEFT_CTRL);
  // Keyboard.releaseAll();
  // Mouse.move(10, -5, 1);
  // Mouse.click(MOUSE_LEFT);
}
```

`begin()` 才会连接 USB。先用 `USBDevice.configured()` 确认主机已配置设备，再发送操作；`begin()` 成功仅代表外设已启动。`Keyboard.end()` / `Mouse.end()` 释放相应按键，不断开其他 HID 功能；断开整个设备使用 `USBDevice.detach()`。

## 自定义报告

示例 [RawHID](examples/RawHID/RawHID.ino) 提供 8 字节收发回显。

1. 定义 HID 报告描述符及 `HIDSubDescriptor node(descriptor, sizeof(descriptor))`。
2. 在首次 `begin()` 前调用 `HID().AppendDescriptor(&node)`；多个描述符总长最多 512 字节。
3. 用 `HID().RegisterReport(id, inputSize)` 注册输入报告长度，使主机在首次发送前也能 `GET_REPORT`；注册后发送长度必须一致。
4. 调用 `HID().begin()`，随后使用 `HID().SendReport(id, payload, size)`。

| API | 行为 |
| --- | --- |
| `HID().begin()` | 1：已启动；0：失败；重复调用不重新枚举 |
| `HID().SendReport(id, data, size)` | 成功返回 `size + 1`，含 Report ID；失败返回负错误码；最多等待约 100ms，并有轮询次数上限 |
| `HID().available()` | 待读 OUT 报告的字节数，含 Report ID |
| `HID().read(buffer, capacity)` | 读取一个完整 OUT 报告；无报告返回 0；空间不足返回负错误码并保留报告 |
| `HID().configured()` / `USBDevice.configured()` | 已配置且未挂起 |
| `HID().poll()` / `USBDevice.poll()` | 主动处理 USB 事务 |
| `HID().lastError()` | 最近错误，常量定义见 [USB_backend.h](src/USB_backend.h) |
| `USBDevice.attach()` / `detach()` | 启动连接 / 断开；保留已注册描述符 |

报告必须带 ID：1 保留给鼠标，2 保留给键盘，自定义使用 3～15。每个报告有效数据最多 63 字节，连同 ID 共 64 字节。`SendReport()` 的数据不含 ID；`read()` 返回的数据首字节为 ID。传给 `AppendDescriptor()` 的同一个节点只注册一次；连接后再追加会报忙。

支持 EP0 `SET_REPORT` 和中断 OUT 端点，只有一个待读输出报告缓冲区，请及时读取。键盘 LED 输出单独消费，可用 `Keyboard.leds()` 获取；不会占用应用程序的待读报告。`GET_REPORT` 提供最近的输入报告；鼠标相对位移在发送后清零，避免空闲重发造成重复移动。

## 运行约束与兼容范围

- 使用轮询，不占用 USB CPU 中断向量。主循环返回、`delay()` 和 `yield()` 会服务 USB。长时间阻塞的自定义循环必须经常调用 `yield()` 或 `USBDevice.poll()`，否则枚举和通信可能超时。API 仅用于主循环，不在 ISR 中调用。
- 提供 HID report protocol；可与核心的 [USB CDC / SerialUSB](../USB/README.md) 组合。不提供 BIOS/UEFI boot protocol、USB Host、MSC、Feature 报告或远程唤醒。没有完整的 AVR `PluggableUSB` / `USB_*` 底层接口，因此依赖这些内部接口的第三方库需要单独移植。
- `AppendDescriptor()`、`SendReport()` 和键鼠常用 API 尽量兼容 Arduino；这里 `HID().begin()` 的返回值采用上述成功/失败约定。`RegisterReport()`、`lastError()` 和原始 OUT 读取是 STC 扩展。
- 独立 HID 不产生 COM，也不实现 `@STCISP#`。启用 USB CDC On Boot 后可同时保留 COM 和 1200 bps 下载入口。16 KB 型号的精简配置只能选择独立 HID 或独立 CDC。

默认 VID:PID 为 `1209:0001`，仅用于私人实验，不能用于发布或制造产品，见 [pid.codes 的该 PID 说明](https://pid.codes/1209/0001/)。自有设备通过全局构建选项覆盖 `USB_VID` / `USB_PID`；必须传给库的 C 编译单元，不能只在 `.ino` 中 `#define`。例如，在已有 `build.extra_flags` 上追加 `-DUSB_VID=<自己的VID> -DUSB_PID=<自己的PID>`。

## 实现参考

协议依据 [USB-IF HID 1.11](https://www.usb.org/sites/default/files/hid1_11.pdf)；寄存器依据 STC 官方手册和示例，包括 [STC32G144K246 第 21～39 章](https://www.stcaimcu.com/data/download/Datasheet/STC32G144K246/STC32G144K246-4.pdf)及 [STC32G12K128](https://www.stcmicro.com/stc/stc32g12k128.html)。底层为本项目 MIT 实现，不依赖厂商预编译 `.LIB`。
