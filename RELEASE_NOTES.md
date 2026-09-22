# 0.0.2（2026-09-23）

- 复查修正硬件 IIC 的 ACK 输入状态位；非法总线编号不再操作默认控制器。补齐 SPI 独立控制器数量和复用表，G144 新增 `SPI1`/`SPI2`，修复高速使能、验证时钟源并提供 `usingHardware()`；新增接口适配清单及模拟回归测试。
- 按官方手册补齐全部 10 个变体的 UART/IIC 数量、默认引脚和硬件复用表；STC32CL 当前 TSSOP20 封装不暴露未引出的 UART2。
- 新增 `Serial2`～`Serial4`，STC32G144K246 扩展至 `Serial8`，支持独立中断接收、波特率检查和定时器占用检测；新增串口引脚组选择接口。
- STC32G144K246 新增独立的 `Wire1`（IIC2），支持主机、从机、回调和超时；Wire 硬件引脚选择使用各变体的复用表。新增多串口、双 IIC 示例及寄存器模拟测试，详见 [变体文档](variants/README.md)。
- 修正 UART1 初始化时未清零 Timer1 预分频的问题，并在结束后恢复原状态。
- SPI 保留现有默认接线；AI8051U、G144 需显式选择硬件引脚，G144 还需有效的 HSIO/PLL 时钟，否则回退软件实现。详见 [SPI 用法](libraries/SPI/README.md)。
- 全部 10 个变体的 SPI 示例编译和 UART/Wire/SPI 模拟回归通过；尚未实板验收。16KB 型号组合 SPI 与 Serial 时可能超出 Flash，其他未覆盖外设见 [硬件接口适配清单](variants/HARDWARE_INTERFACES.md)。
- 平台版本升级至 0.0.2；继续使用已发布的 `stcxx-toolchain` 0.3.0 和 `stc-cli` 0.1.0-stc.2，保留 0.0.1 安装索引。

# 0.0.1（2026-09-22）

- 编译容量输出改为 Arduino 标准的程序空间、动态内存两行摘要；动态内存包含预留堆，容量随 XRAM 菜单切换，独立栈与内部 RAM 明细保留在 `advanced-size` 诊断命令中。

- 在 [coloz/arduino-mcs251](https://github.com/coloz/arduino-mcs251) 发布首个版本。
- 支持 10 个 MCS251 型号，提供 Arduino C++11 API、原生编译驱动及核心运行库；FQBN 为 `stc:mcs251:<variant>`。
- 提供 Windows x64 和 Apple Silicon macOS 15+ 原生安装包，编译运行时无需 Node、Python 或 shell。
- 开发板管理器安装 `stcxx-toolchain` 0.3.0 与 `stc-cli` 0.1.0-stc.2，支持 UART 和原生 USB 上传。
- 安装索引为 `package_mcs251_index.json`，平台和工具资源均由本仓库的 v0.0.1 Release 提供。
- 当前版本尚未完成实板验收，硬件能力与容量限制见 [COMPATIBILITY.md](COMPATIBILITY.md)。
