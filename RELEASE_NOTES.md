# arduino-mcs251 0.0.3（2026-11-04）

- 修复用户 EEPROM 分区未从链接空间排除的问题，对应 [aily-blockly #780](https://github.com/ailyProject/aily-blockly/issues/780)。新增 `User EEPROM size / ROM start` 菜单，可选关闭、1、2、4、8、16、32 KiB。
- STC32G12K128、STC32G144K246 从 Flash 前部预留 EEPROM，移动启动代码和链接窗口下限；STC32G8K64、STC32CL8K64、AI8051U-34K64 从尾部预留，限制链接窗口上限。复位与中断向量保持 `FF:0000`，程序容量随选择扣减，固定代码侵入 EEPROM 时链接失败。
- 核对其余五款型号的独立 EEPROM 区域，补齐全部十款的分区元数据；向 C、C++、汇编和预处理阶段提供 EEPROM 容量、物理地址和扇区大小宏。
- 包含上一版本发布后的 GPIO、Print/Stream/String 优化和时钟菜单修订；重建原生 Arduino 驱动，包含 HEX/ELF 输入校验及 sketch 文件镜像冲突保护修正。
- 新增可重复的双宿主构建和发布包校验。保留旧版本安装索引，工具链与上传器继续使用原有不可变归档。

例如预留 2 KiB：`stc:mcs251:stc32g12k128:eeprom=k2`。该型号的启动代码从 `FE:0800` 开始，程序容量为 129024 字节。

升级后请清理构建缓存并重新加载板卡数据。Aily 还需更新带 `eeprom` 菜单声明的板卡包。外部 EEPROM 库必须限制访问和整扇区擦除范围，ISP 硬件分区应与所选容量一致；重新烧录不保证保留 EEPROM 数据。

验证覆盖全部十款芯片及 40 个分区组合，并包含五款共享 Flash 型号的越界拒绝测试。发布资产中的 `release-validation.json` 记录各宿主的实际验证结果。尚未进行实板烧录和 IAP 擦写验收。
