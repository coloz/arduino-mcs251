# 兼容性与安装要求

本文件随平台安装包提供；完整 API、型号和接线说明见 [README](README.md) 与 [变体文档](variants/README.md)。

- 支持 Windows x64 和 Apple Silicon macOS 15+，Arduino 架构为 `mcs251`，FQBN 为 `stc:mcs251:<板卡ID>`。
- 支持的十种芯片以 `tools/variants/devices.json` 为准；所有型号仍处于实验验证阶段。
- 继续使用不可变的 `stcxx-toolchain` 0.3.0 和 `stc-cli` 0.1.0-stc.2。安装包包含分别在 Windows 和 Apple Silicon 原生构建的 Arduino 适配驱动。
- C++11 使用 Clang → LLVM-CBE → SDCC；ABI 为 16 位 `int`、32 位 `long` 和 24 位地址空间，不支持异常、RTTI 或完整桌面 STL。
- 编译时钟必须匹配实际 ISP 配置。AI8051U-34K64 可选 12/40 MHz，STC32G144K246 可选 12/48 MHz，其余当前固定 12 MHz。
- 更新平台后清理构建缓存并重新加载板卡数据。Aily 板卡包必须声明对应菜单类型，才会显示 SDK 新增的 EEPROM 菜单。
- 共享 Flash 型号使用 EEPROM 前必须选择预留容量；复位和中断向量始终位于 `FF:0000`。详情见 [EEPROM 分区](README.md#eeprom--iap-分区)。菜单配置链接分区，不替代 ISP 硬件选项或应用的 IAP 边界检查。
- EEPROM 可断电保存，但当前上传器重新烧录时可能整片擦除，不保证跨重新烧录保留数据。
- 编译与链接检查不能替代实板的启动、时钟、外设和 IAP 擦写验证。
