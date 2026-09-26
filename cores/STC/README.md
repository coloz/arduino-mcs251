# STC MCS251 core

当前核心只支持 STC32 和 AI8051U 的 MCS251 执行配置。能力来自生成的 board flags，不根据不存在的旧系列猜测寄存器布局。公共接口见 [Arduino.h](Arduino.h)，具体引脚与外设能力以各型号的 `pins_arduino.h` 为准。

## 源码职责与依赖

| 位置 | 职责 | 编译方式 |
| --- | --- | --- |
| 根目录公共头文件、`Print.cpp`、`Stream.cpp` 等 | Arduino API 与平台无关的类实现；保留官方常用 include 名称 | C++ 经 Clang / LLVM-CBE / SDCC |
| `main.c` | 堆、全局构造函数、Arduino 生命周期与中断向量声明 | 原生 SDCC C |
| `hal/` | Wiring、UART、USB、ISR、私有状态及控制台适配 | 原生 SDCC C |
| `runtime/include/` | STCXX ABI 声明与 Clang 专用标准头适配 | 仅加入 C++ 头文件搜索路径 |
| `runtime/src/` | 分配器、构造函数、new/delete、libc 桥接 | 按 `.c` / `.cpp` 分流 |
| `../../libraries/<库名>/src/` | 库自己的类接口与驱动，包括 SPIClass / TwoWire | 由 Arduino 库发现决定是否编译 |

依赖方向为 Arduino API → C HAL / STCXX runtime。共享 runtime 不包含
Arduino 头文件，也不直接调用 `Serial_*`；`hal/stcxx_console.c` 提供原生 C
控制台钩子，保留 UART1 输出及 `getchar()` 等待期间调用 `yield()` 的行为。

`runtime/include` 与 `runtime/src` 的唯一维护源是相邻仓库
`stcxx/sdk/runtime`。运行 `node scripts/sync-stcxx-sdk.mjs` 同步，
`--check` 检查副本。不要直接修改这些同步副本。
`runtime/runtime-manifest.json` 记录 Arduino 的运行时集成契约。

公共代码使用 `<Arduino.h>`、`<Print.h>`、`<WString.h>`、`<SPI.h>`、
`<Wire.h>` 等；旧 `cpp/...` 路径属于原内部结构，已移除。
`wiring_private.h` 的历史兼容入口仍保留。目录变化不改变 MCS251 的
数据布局、调用约定或构造函数执行顺序。

## 构建与兼容范围

公共类以 ArduinoCore-API 1.5.2 为兼容基线，已实现接口和硬件限制见
[平台 README](../../README.md) 和各库 README。

目录迁移后必须重建原生驱动，并清理旧 Arduino core 缓存。平台配方中的
`STC_CORE_LAYOUT=2` 参与缓存身份；发布包需分别重建 Windows 与 macOS 驱动。
