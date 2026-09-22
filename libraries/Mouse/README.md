# Mouse for STC

## 示例程序

在 Arduino IDE 的 `文件 → 示例 → Mouse` 中打开。按钮均接到指定引脚与 GND 之间，USB 接芯片原生 D+/D−。

| 示例 | 演示内容 | 接线及操作 |
| --- | --- | --- |
| [ButtonMouse](examples/ButtonMouse/ButtonMouse.ino) | 最小左键按下/释放 | P3.2 按钮 |
| [ButtonMouseControl](examples/ButtonMouseControl/ButtonMouseControl.ino) | 四方向移动和左键拖动 | P3.2/P3.3/P3.4/P3.5 为上/下/左/右，P3.6 为左键 |
| [JoystickMouseControl](examples/JoystickMouseControl/JoystickMouseControl.ino) | 模拟摇杆、中心死区、按住使能、左键拖动 | X/Y 接 A0/P1.0、A1/P1.1；按住 P3.2 启用，P3.3 为左键 |
| [MouseWheel](examples/MouseWheel/MouseWheel.ino) | 滚轮和中键 | P3.2/P3.3 向上/向下滚动，P3.4 为中键；长按滚动按钮连续滚动 |

按钮与摇杆场景参考 Arduino 官方 [ButtonMouseControl](https://docs.arduino.cc/built-in-examples/usb/ButtonMouseControl/) 和 [JoystickMouseControl](https://docs.arduino.cc/built-in-examples/usb/JoystickMouseControl/)。摇杆使用 10 位 ADC，中心约为 512，中心附近 ±64 为死区；松开使能按钮时停止移动并释放鼠标按键。摇杆输出电压须在芯片 ADC 输入范围内。

## API 说明

移植 Arduino 官方 [Mouse](https://github.com/arduino-libraries/Mouse)，保留常用 API：

```cpp
Mouse.begin();
Mouse.move(10, -5, 1);  // X、Y、滚轮
Mouse.click(MOUSE_LEFT);
Mouse.press(MOUSE_RIGHT);
Mouse.release(MOUSE_RIGHT);
bool pressed = Mouse.isPressed(MOUSE_LEFT);
Mouse.end();
```

提供左、中、右三个按键、相对移动和滚轮，Report ID 为 1。X/Y 限幅到 -127～127；滚轮沿用官方 `signed char` 参数。`isPressed()` 返回本地按键状态，发送结果可查询 `HID().lastError()`。`end()` 尝试释放按键，不断开其他 HID 功能。

需调用 `Mouse.begin()` 启动 USB，并等待 `USBDevice.configured()`。打开 [ButtonMouse](examples/ButtonMouse/ButtonMouse.ino)，用 P3.2 到 GND 的按钮控制鼠标左键。芯片支持、接线和轮询要求见 [HID 说明](../HID/README.md)。当前按钮鼠标示例也超出 AI8051U-34K16 的 16KB Flash。

上游固定于 [6a04789](https://github.com/arduino-libraries/Mouse/tree/6a0478972dfb499ddb1b5b4cda9884fbdd1043ad)，见 `REVISION`。修改了 STC HID 初始化、初始报告注册和 `end()` 释放行为；保留上游版权及 [LGPL-2.1-or-later 许可证](LICENSE.txt)。
