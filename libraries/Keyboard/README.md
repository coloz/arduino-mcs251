# Keyboard for STC

移植 Arduino 官方 [Keyboard](https://github.com/arduino-libraries/Keyboard)；保留 `begin(layout)`、`end()`、`write()`、`print()`、`println()`、`press()`、`release()`、`releaseAll()` 及 `KEY_*` 常量。默认 US 布局，附带官方各语言布局；文本编码与字符支持范围沿用原库，不直接输入 Unicode。

## 示例程序

在 Arduino IDE 的 `文件 → 示例 → Keyboard` 中打开。按钮接到对应引脚与 GND 之间，使用内部上拉；USB 接芯片原生 D+/D−。

| 示例 | 演示内容 | 接线及操作 |
| --- | --- | --- |
| [KeyboardMessage](examples/KeyboardMessage/KeyboardMessage.ino) | 按钮消抖、按下沿检测、`print()` / `println()` | P3.2 按钮，每次按下输入一次计数文字 |
| [KeyboardSerial](examples/KeyboardSerial/KeyboardSerial.ino) | UART ASCII 转 USB 键盘，处理 Enter/Tab/Backspace | UART1 P3.0/P3.1，115200 baud；按住 P3.2 按钮启用输入 |
| [KeyboardModifiers](examples/KeyboardModifiers/KeyboardModifiers.ino) | 修饰键组合、`press()` / `releaseAll()` | P3.2 按钮发送 Ctrl+A；macOS 可将修饰键改为 `KEY_LEFT_GUI` |
| [KeyboardLeds](examples/KeyboardLeds/KeyboardLeds.ino) | 读取主机 Num/Caps/Scroll Lock 指示灯 | P2.0/P2.1/P2.2 分别经限流电阻、LED 接 GND，高电平点亮 |
| [KeyboardMouse](examples/KeyboardMouse/KeyboardMouse.ino) | 键盘与鼠标同时使用 | P3.2 按钮输入文字并移动鼠标 |

按钮文字与串口输入场景参考 Arduino 官方 [KeyboardMessage](https://docs.arduino.cc/built-in-examples/usb/KeyboardMessage/) 和 [KeyboardSerial](https://docs.arduino.cc/built-in-examples/usb/KeyboardSerial/)。`KeyboardSerial` 使用 STC UART1 接收，先选择目标文本窗口，再从串口侧发送字符；每个 LF 产生一次 Enter，CR 被忽略。

## API 说明

支持 6 个普通键和 8 个修饰键同时按下。发送失败时，`press()` / `release()` / `write()` 返回 0 并设置 `Print` 写入错误，可用 `getWriteError()` 查询、`clearWriteError()` 清除。`releaseAll()` 和 `end()` 返回 `void`；调用后也可检查写入错误。

新增 `Keyboard.leds()` 读取主机指示灯位：bit0 Num Lock、bit1 Caps Lock、bit2 Scroll Lock、bit3 Compose、bit4 Kana。USB Report ID 固定为 2。`end()` 尝试释放所有键，不断开 USB。

从 [KeyboardMouse](examples/KeyboardMouse/KeyboardMouse.ino) 按钮触发示例开始。需调用 `Keyboard.begin()` 启动 USB，并等待 `USBDevice.configured()`。芯片支持、接线、轮询要求及 VID/PID 见 [HID 说明](../HID/README.md)。当前键鼠固件不适用于 16KB Flash 的 AI8051U-34K16。

上游固定于 [3f7bad0](https://github.com/arduino-libraries/Keyboard/tree/3f7bad0a41839689684e3b46ce9deb0232f8ec2d)，版本记录见 `REVISION`。修改包括 STC HID 初始化、初始报告注册、LED 输出描述符、错误传播和公开 `Print::write` 重载。上游版权和 LGPL-2.1-or-later 声明保留于源码及 [LICENSE](LICENSE)。
