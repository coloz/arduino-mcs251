# 0.0.1（2026-09-22）

- 编译容量输出改为 Arduino 标准的程序空间、动态内存两行摘要；动态内存包含预留堆，容量随 XRAM 菜单切换，独立栈与内部 RAM 明细保留在 `advanced-size` 诊断命令中。

- 在 [coloz/arduino-mcs251](https://github.com/coloz/arduino-mcs251) 发布首个版本。
- 支持 10 个 MCS251 型号，提供 Arduino C++11 API、原生编译驱动及核心运行库；FQBN 为 `stc:mcs251:<variant>`。
- 提供 Windows x64 和 Apple Silicon macOS 15+ 原生安装包，编译运行时无需 Node、Python 或 shell。
- 开发板管理器安装 `stcxx-toolchain` 0.3.0 与 `stc-cli` 0.1.0-stc.2，支持 UART 和原生 USB 上传。
- 安装索引为 `package_mcs251_index.json`，平台和工具资源均由本仓库的 v0.0.1 Release 提供。
- 当前版本尚未完成实板验收，硬件能力与容量限制见 [COMPATIBILITY.md](COMPATIBILITY.md)。
