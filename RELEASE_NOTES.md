# 0.0.9（2026-09-22）

- 重整核心目录：公共 Arduino API 位于 `cores/STC`，硬件适配位于 `hal/`，共享运行库位于 `runtime/`；SPIClass 和 TwoWire 接口随各自库维护。
- 对齐 ArduinoCore-API 1.5.2 的常用接口，补充 `arduino::` 类名、`pin_size_t`、`bitToggle` 和字符串反向比较；修正 String 进制转换及 Stream 小数解析行为。
- 分离运行库与 Arduino 串口依赖，由 HAL 提供控制台钩子；更新原生数学兼容实现，保持 MCS251 ABI 和现有硬件能力边界。
- 为新核心布局分别重建 Windows x64 与 Apple Silicon macOS 15+ 原生驱动；继续使用已发布的 `stcxx-toolchain` 0.3.0 和 `stc-cli` 0.1.0-stc.2。
- FQBN 和公共头文件名称保持不变；旧内部 `cpp/...` 路径已移除。升级后请清理旧构建缓存，索引保留历史版本供回退。
- 编译与安装验证不代替实板验收；G12K128 的 CDCKeyboardMouse 示例需选择 `USB CDC On Boot: Enabled`，以使用适合该示例的堆预留配置。

# 0.0.8（2026-09-22）

- 平台更名为 arduino-mcs251，开发板索引为 `package_mcs251_index.json`；FQBN 保持 `stc:mcs251:<variant>`，继续提供全部 10 个 MCS251 型号。
- 升级到 STCXX 0.3.0，Arduino 和独立 C/C++ 编译共用原生编译器实现、运行库及宿主工具锁；完整工具链提供 `bin/stcxx` 独立入口。
- SDK 通过标准构建配方兼容未经修改的 aily-builder 1.2.17，支持混合 C/C++ 工程、`src/` 源文件、完整对象/归档缓存及 ELF 到 HEX 导出。
- 修复 CAN 未使用变量警告，调整小容量芯片的 CheckedWire 示例，并增加 C/C++ 混合调用示例。
- Windows x64 和 Apple Silicon macOS 15+ 使用原生驱动与编译组件；运行时不依赖 Node、Python 或 shell。
- 对象和归档格式已更新：升级后请清理旧工程构建缓存。旧版本及其工具依赖保留，烧录工具继续使用 stc-cli 0.1.0-stc.2。

# 0.0.7（2026-09-20）

- SD 增加 FAT16/FAT32 短文件名路径、嵌套目录、目录枚举、多文件上下文和共享句柄；支持初始化后选择 SPI 数据时钟，并补充目录示例与容量限制说明。
- Wire 增加硬件 I²C 从机模式、`onReceive` / `onRequest` 回调及 SlaveEcho 示例；统一外设中断入口，保留主机模式。
- 新增 Timer2 非阻塞 `tone` / `noTone`、带错误码的 `toneChecked` 与 CheckedTone 示例。
- 原生驱动更新为 0.2.1，默认启用 C++ Oz 和链接优化；改进原生 C 源码裁剪、对象/归档缓存及依赖路径处理，校验预编译库和 C++ 产物的完整性。
- 完善 `.S` 编译、预编译库链接、Arduino IDE 警告级别和分类内存报告；拆分可选 libc/堆查询对象以减少未使用代码。
- STC32G144K246 默认 XRAM 改为高地址 64 KiB；128 KiB 保留为实验选项。升级后请检查已有工程的 XRAM 菜单配置。
- 提供 Windows x64 和 Apple Silicon macOS 15+ 原生平台包；继续使用已发布的 `stcxx-toolchain` 0.2.0 和 `stc-cli` 0.1.0-stc.2，索引保留历史版本供回退。
- 清理旧验证脚本和过时文档，新增 [兼容性说明](COMPATIBILITY.md)。本次发布验证覆盖原生驱动测试、安装及示例编译，不代替实板外设验收。

# 0.0.6（2026-09-18）

- 本次原生安装包支持 Windows x64 和 Apple Silicon macOS 15+；两端分别构建原生驱动和上传器，索引保留 0.0.5 供回退。
- 工具依赖更新为 `stcxx-toolchain` `0.2.0` 和 `stc-cli` `0.1.0-stc.2`；上传器增加新 CDC/复合设备的 1200 bps 自动复位支持。FQBN 不变。
- 移除随附的实验性 Adafruit NeoPixel 库。

- 原生 USB 变体增加 CDC ACM、USB CDC On Boot、USBSerial/SerialUSB 和 Serial1/Serial0；支持 CDC + HID 复合设备、固定收发缓冲和 1200 bps 进入 ISP。16 KB 型号提供独立 CDC 精简配置；G12 启用 CDC 时预留更多 USB 静态 RAM。UART1 与 USB 引脚冲突会被拒绝。
- G144 增加可选 High bank 64 KiB XRAM 布局。本机样板默认布局出现数据位错误，高地址布局通过 CDC 与 CDC/HID 实测；保留 128 KiB 默认布局，实测范围和 RTS 限制见通信验证记录。

- Arduino 的编译、预处理、归档、链接及大小统计直接调用 Rust 原生 `stcxx.exe` / `stcxx`，输出内部编译命令，构建路径不再启动 PowerShell、shell 或 Python。
- 增加原生平台/工具链打包入口，安装包不再集成 Python 解释器或 Python 辅助脚本；保留原生 Clang、LLVM-CBE 和 SDCC。
- 清理旧驱动目录残留、编译器源码补丁及其重建脚本；SDK 使用锁定的预编译工具链，编译器开发归属独立的 `stcxx` 项目。
- 修复 LLVM IR-only 优化阶段尝试创建 MSP430 后端的警告、SDCC 简单整数内联辅助函数的初始化误报，以及 `--stack-loc` 弃用警告；继续核验 STC ABI 与最终栈布局。
- 保留编译器 stdout/stderr 通道，使普通 ASlink 回显使用正常输出，真实警告和错误继续显示为诊断。
- 修复 IDE 第一次编译成功、再次编译复用核心缓存时报告 `SHA-256 mismatch: ...core.lib`：缓存现在携带 C++ 中间文件和堆对象，支持跨 sketch 复用，保留哈希及链接审计。
- 补充默认共享缓存下连续编译、删除首次构建目录后复用缓存的回归验证。

# 0.0.5（2026-09-17）

- 将 `sdcc-mcs251` 和 `stcxx-frontend` 合并为 `stcxx-toolchain` `0.1.0`，Windows x64 和 Apple Silicon 共用相同的内部目录布局。
- Arduino 编译、归档和前端发现均使用统一工具依赖；新增 `STCXX_TOOLS_ROOT`，可一次指定完整工具包。
- 保留组件二进制、运行库、许可证、macOS 内部链接及原有哈希校验；添加统一包清单和确定性打包入口。
- 平台和工具链使用新版本，升级时不会覆盖旧包；`stc-cli` 上传器和 FQBN 保持不变。

# 0.0.4（2026-09-17）

- 新增原生 USB HID、自定义报告收发及 Arduino 官方 Keyboard/Mouse 移植，支持键盘指示灯和键鼠组合；通过 `yield()` / 主循环服务 USB。
- 新增双路原生 CAN，适配经典 CAN 和 G144 CAN-FD 控制器的经典帧收发；提供官方 `HardwareCAN` / `CanMsg` 风格 API 和独立 `CANPacketClass`。
- 补充 15 个 CAN/USB 示例，覆盖独立收发、过滤、扩展帧、远程帧、双路 CAN、键盘文字/组合键/指示灯、串口转键盘、按钮/摇杆鼠标、滚轮、媒体键和 HID 游戏手柄；各库 README 提供示例索引与接线说明。
- 按型号生成 USB/CAN 能力标志，补充芯片支持说明和上游许可证。
- 仅更新 core 平台包；SDCC、C++ 前端、上传器及编译器源码继续复用 v0.0.2 已发布资源，工具版本、下载地址和校验值保持不变。
- 已安装 0.0.3 的用户刷新开发板索引后升级至 `stc:mcs251@0.0.4`；FQBN 和工具依赖保持不变。

# 0.0.3（2026-09-17）

- 包标识改为 `stc`，FQBN 统一为 `stc:mcs251:<variants>`；板卡 ID 和菜单参数保持不变。
- 同步工具依赖、Windows/macOS 前端锁文件、源码安装路径及构建和验证脚本；平台包和索引同步更新。
- 旧包用户刷新索引后安装 `stc:mcs251@0.0.3`，重新选择板卡，并将项目中的 FQBN 前缀改为 `stc:mcs251:`。包标识已变化，不会通过旧包的版本升级自动迁移。
- 沿用 0.0.2 的编译器、前端和上传器归档，继续支持 Windows x64、Apple Silicon macOS 15+ 和 10 个 MCS251 型号。

# 0.0.2（2026-09-16）

- 清理未使用的上游 SDK 清单、Linux 容器配置及 Linux 分发锁文件，安装包不再包含 SDK 资料。
- 保留仍使用的打包、代码生成、源码锁定与发布校验工具；源码构建的工具缓存改至 `dist/toolchain-cache`。
- 移除 GitHub Actions 工作流及历史库兼容性报告；保留可手动运行的本地验证脚本。
- 打包和索引脚本默认读取平台版本，原生安装验证从候选索引读取版本，支持 0.0.2 及后续版本。
- 沿用 0.0.1 的编译器、上传器、C++ core 与库实现，继续支持 Windows x64、Apple Silicon macOS 15+ 和 10 个 MCS251 型号。
- 刷新开发板管理器索引后可由 0.0.1 升级至 0.0.2。编译、链接和离线 HEX 校验不代表实板验收。

# 0.0.1（2026-09-16，历史记录）

平台现仅提供 C++11 模式，已移除语言菜单和纯 C 兼容接口。旧 FQBN 中的 `cppcore=enabled` / `cppcore=plain` 参数需删除。所有板项默认 12 MHz，AI8051U-34K64 可选 40 MHz、STC32G144K246 可选 48 MHz；实际芯片时钟须与所选配置一致。

- 修复 U8g2 在 Windows 长路径下的函数归档命令超长，以及并行依赖扫描时的临时文件冲突；无需缩短构建路径或强制单线程。
- 修复 DHT 的清理分支返回值分析和 RTClib 常量字节读取链，保留 SDCC 告警审计。
- 补齐 `fminf/fmaxf`、`itoa/utoa/ltoa/ultoa`；将合法 C++ 的匿名 typedef 兼容性提示、整数字面量到浮点的舍入提示保留为警告，TFT_eSPI 可使用通用 SPI 路径。
- 随附 Adafruit NeoPixel `1.15.5-stc.1` 实验移植：目前仅 STC32G144K246、12/48 MHz、P0～P7 有效引脚、800 kHz。编译与指令模型验证不代替实板时序验收，详见 [移植说明](libraries/Adafruit_NeoPixel/README-STC.md)。
- 以 0.0.1 重新发布 Windows x64 和 Apple Silicon Mac（macOS 15+）安装包及独立 `stc-cli` 烧录工具。
- 修复编译过程反复出现的 `__has_builtin`、`__STDC_HOSTED__` 重复定义警告；Windows、macOS 工具包同时包含修复，保留真正的源码警告和错误。
- 修复 Arduino IDE 上传时报 `Property 'upload.tool.serial' is undefined`：开发板管理器自动安装原生 `stc-cli`，所有型号均配置上传配方。无法返回型号 ID 的 UART 协议默认允许上传；可核验的型号 ID 仍执行匹配检查。
- 上传显示按芯片确认接收量更新的百分比。普通提示和进度使用正常文字颜色，错误继续以红色显示；详细上传模式不再默认输出原始 ISP 报文。
- STC32G144K246 支持自动识别所选 USB CDC／UART 端口，以及 `工具 → Upload method → Native USB` 手动选项。工厂 HID 模式没有 COM 口时也可上传。
- 更新后若未出现 `Upload method`，执行 `工具 → Reload Board Data（重新加载开发板数据）` 并检查板型参数。
- SDCC 工具依赖更新为 `4.6.0-stc.0.0.1-r1`，上传器更新为 `0.1.0-stc.1`，避免复用旧版工具缓存；平台版本仍为 0.0.1。
- Windows 的 C 和 C++ 均使用原生 `.exe` 工具，前端包自带 Python，通过系统 PowerShell 5.1 启动，无需 WSL、Ubuntu 或 Git Bash。
- 移除 `sdcc-mcs251-wsl` 依赖和 `stc51-native-macos-host` 占位包；Windows 与 macOS 的 C/C++ 共用各自原生 SDCC，前端工具依赖版本更新为 `20.1.8-stc.3`。
- macOS C++ 使用原生 ARM64 工具，需要 Bash 4.4+、GNU coreutils 和 Python 3。
- 支持 10 个 STC32/AI8051U 型号，统一使用 MCS251；原生 SDCC 支持各型号的完整 Flash 布局。
- 提供 C++ `String`、`Print`、`Stream`、串口及 Wire、SPI、SoftwareSerial、LiquidCrystal、Stepper、受限 SD 库。
- STC32CL8K48／64 支持硬件 I²C、SPI、PWM、独立上拉和存储器访问加速。
- 安装包只包含运行所需的 core、variants、库、示例、编译驱动及许可证；维护脚本、源码补丁和生成器保留在源码仓库。
- 版本号保持 0.0.1，替换已有发布包并更新索引。包括已安装旧 0.0.1 的用户，均需卸载、更新索引后重新安装，并清理旧构建缓存；板型使用 `arduino-stc51:mcs251:…`。此前的 0.0.2、0.0.3 保持撤下。
- 编译、链接及离线 HEX 校验不等于实板验收。使用方式、宿主要求和已知限制见 [README.md](README.md)。
