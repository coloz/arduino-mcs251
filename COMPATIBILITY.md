# Arduino 兼容范围

平台面向 STC MCS251，具体硬件接线、时序和容量需按应用确认。各接口已覆盖和未适配的硬件资源见 [接口适配检查](variants/HARDWARE_INTERFACES.md)。

公共类的对照基线为 [ArduinoCore-API 1.5.2](https://github.com/arduino/ArduinoCore-API/tree/1.5.2)，
传统入口、引脚与串口常量保留现有 Arduino AVR 风格。不同官方架构的
寄存器、串口配置编码、类型宽度和扩展接口并不相同，因此兼容验证以公开调用
和已实现功能的行为为准，不能从一次构建推导“所有官方架构、全部 API 绝对兼容”。

目录重构保留公共 include 名称与既有 STC 扩展，增加 `arduino::String`、
`arduino::Print`、`arduino::Printable`、`arduino::Stream`、
`arduino::HardwareSerial` 等类名、`pin_size_t`、`bitToggle` 及字符串反向比较。
`String` 进制转换使用与目标 `itoa/utoa` 一致的小写字母，`Print` 仍输出大写字母；
`parseFloat` 按官方算法逐位累加小数，避免整体整数乘缩放因子引入额外误差。
这些是有测试依据的原有兼容性修正。

保留的目标限制包括：UART 目前仅支持 `SERIAL_8N1`；无网络协议栈；
`F()` / `PROGMEM` 目前不节省 RAM；没有完整 AVR `PluggableUSB` 或桌面 STL。
这些限制不会因目录重构而消失。

| 项目 | 当前实现与边界 |
| --- | --- |
| 编译入口 | Arduino 标准 recipes；支持 `.c`、`.cpp`、`.cc`、`.cxx`、`.S`；`.S` 经预处理后使用 SDAS251 语法 |
| C++ | GNU C++11；Clang → LLVM-CBE → SDCC；禁用异常、RTTI；24 位指针 ABI，不能直接链接 AVR/ARM/GCC 对象 |
| 编译缓存 | C++ 对象及 bitcode、REL、IR、元数据校验；缺失或损坏明确失败并要求清理缓存，避免静默丢失构造函数 |
| 预编译库 | `dot_a_linkage`、`precompiled=true/full`、`compiler.libraries.ldflags`；C++ `.a` 为包含源码快照与哈希的独立封装；原生 C 接受 SDAR 归档 |
| 诊断 | 普通命令与成功信息使用 stdout；真实警告/错误使用 stderr；IDE 警告级别传给前端，显式 `-Werror` 仍生效 |
| 内存报告 | 默认使用 Arduino 标准两行摘要；动态内存统计 XDATA/PDATA（含堆预留），总容量随 XRAM 菜单变化；内部静态 RAM 和 EDATA 栈独立，不计入该比例；`advanced-size` 可查看分区明细 |
| tone | 单路 Timer2 非阻塞方波，支持时长及 `noTone`；已有 Timer2 占用时拒绝启动 |
| UART | `Serial1`～`Serial4`，STC32G144K246 扩展至 `Serial8`；STC32CL 当前封装不引出 UART2；支持完整硬件引脚组选择、独立中断接收及同步发送 |
| Wire | 主机和硬件从机、接收/请求回调；支持变体列出的硬件复用组，默认缓冲 32 字节；STC32G144K246 提供独立的第二路 `Wire1` |
| SPI | 所有型号 1 路，G144 提供 `SPI`/`SPI1`/`SPI2` 三路主机；支持硬件引脚组和 GPIO 回退；G144 需可验证的现有 HSIO 时钟，`usingHardware()` 查询实际方式 |
| SD | FAT16/FAT32、8.3 路径、嵌套目录、目录枚举、多文件、共享句柄；不支持长文件名、exFAT、断电事务保证 |

C++ 不支持变长栈数组、`alloca`、异常和 RTTI；第三方库必须满足目标 ABI 和内存限制。

`toneChecked(pin, frequency, duration)` 返回 `STC_TONE_OK` 或配置错误，
`toneConfigurationError()` 可查询错误。频率范围为 31 Hz 至
`STC_TONE_MAX_FREQUENCY`（`F_CPU/4096`）；该上限限制中断负荷，不是实测
最高可靠输出频率。使用 Timer2 不改变 Timer0 计时和默认 UART1/Timer1。
`Serial2` 同样使用 Timer2，不能与 `tone()` 同时运行。串口数量、默认引脚、
IIC 配置与小容量型号的 Flash 限制见 [变体文档](variants/README.md)。
持续时间以 `millis()` 判断；关闭中断、耗时回调会影响实际波形与停止时间。
同一时刻只能输出一个引脚，另一引脚请求会返回定时器忙。

STC32G144K246 默认 XRAM 为高地址 `0x020000` 的 64 KiB。128 KiB 布局
保留为实验选项 `xram=default`；更新板卡数据后应检查已有工程的菜单选择。

完整 SD 文件系统需要较大的 Flash，不能只根据 XDATA 容量判断是否适用。
可用全局编译参数 `-DSTC_SD_STATE_IN_DATA=0` 将热状态放回 XDATA，代价是
Flash 增加。目录、多文件和写入失败恢复见 [SD 文档](libraries/SD/README.md)，
从机回调约束见 [Wire 文档](libraries/Wire/README.md)。

标准预编译库放在 `src/{build.mcu}/libName.a`，这里 `build.mcu` 是小写板卡
ID，例如 `ai8051u_34k64`。库由匹配的 STC ABI 和工具链生成；不能把仅含
C++ 占位 REL 的普通 SDAR 库当成完整 C++ 库。Arduino CLI 自动生成的库路径
支持空格；CLI 将自定义 `library.properties` 的 `ldflags`
按空白切分，自定义 `-L` 路径应避免空格和引号，优先使用标准 MCU 目录。
