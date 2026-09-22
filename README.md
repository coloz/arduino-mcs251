# arduino-mcs251

编译驱动由配套 [stcxx/compiler](../stcxx/compiler/) 统一维护，基础运行库与宿主锁来自 [stcxx/sdk](../stcxx/sdk/)。重建驱动时需将两个仓库并列放置；已安装的开发板包无需源码仓库。独立于 Arduino 的 C/C++ 编译见 [STCXX SDK 使用说明](../stcxx/sdk/README.md)。

面向 STC **MCS251** 芯片的 Arduino core。当前版本 **0.0.9**，支持 Windows x64 和 Apple Silicon macOS 15+，仅维护 10 个型号；所有型号固定使用 `-mmcs251`，Arduino 架构标识为 `mcs251`。

MCS51 芯片、板项和 C++ 适配路径已移除。旧的 `arduino-stc51:mcs51:…` FQBN 不再适用，需要重新安装当前源码并选择新板项。工程目录为 `arduino-mcs251`。

FQBN 统一为 `stc:mcs251:<variants>`，其中 `<variants>` 是 `boards.txt` 中的板卡 ID（对应 `devices.json` 的 `id`），例如 `stc:mcs251:stc32g8k64`、`stc:mcs251:ai8051u_34k64`。时钟等菜单参数仍可附加在末尾，例如 `stc:mcs251:ai8051u_34k64:clock=40m`。包标识为 `stc`，开发板管理器中的显示名称为 `mcs251`。

当前版本供开发和有限场景验证使用，尚未完成实板验收。产品板需验证实际时钟、接线和外设功能；编译成功不代表实板验收通过。

当前能力与容量限制见 [兼容性说明](COMPATIBILITY.md)。G144 默认 XRAM 为高地址 64 KiB，128 KiB 配置保留为实验选项。

Arduino 构建入口为 Rust 原生 **`stcxx.exe` / `stcxx`**，直接调度 Clang、LLVM-CBE、SDCC，并输出内部命令。编译运行时不使用 PowerShell、shell 或 Python，原生安装包不集成 Python。Windows x64 和 Apple Silicon 分别构建并验证各自的原生驱动。详见 [原生驱动说明](tools/stcxx-driver/README.md)。

SDK 通过原生构建适配入口兼容未经修改的 aily-builder 1.2.17：对象文件封装完整编译数据，归档支持标准 rcs 调用，并通过标准配方导出 HEX。Arduino CLI 使用同一入口。升级后请清理旧构建缓存。实现与格式约定见 [aily-builder 适配说明](tools/stcxx-driver/README.md#aily-builder)。Windows x64 和 Apple Silicon macOS 15+ 使用各自的原生构建。

0.0.9 提供 Windows x64 和 Apple Silicon 原生安装包，索引同时保留 0.0.7 及更早版本。源码构建使用 `node scripts/build-native-driver.mjs`。

## 安装

在 Arduino IDE 的“附加开发板管理器网址”中加入：

```text
https://raw.githubusercontent.com/coloz/arduino-stc51/main/package_mcs251_index.json
```

在开发板管理器中安装 **mcs251 0.0.9**，然后选择对应型号。Arduino CLI 可使用：

```powershell
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/coloz/arduino-stc51/main/package_mcs251_index.json
arduino-cli core install stc:mcs251@0.0.9 --additional-urls https://raw.githubusercontent.com/coloz/arduino-stc51/main/package_mcs251_index.json
```

安装资源见 [v0.0.9 Release](https://github.com/coloz/arduino-stc51/releases/tag/v0.0.9)。刷新开发板索引即可升级，并重新编译项目；FQBN 不变。需要回退时可显式安装 `stc:mcs251@0.0.8`。

从旧包标识 `arduino-stc51` 迁移时，需安装 `stc:mcs251` 并重新选择板卡，将项目和脚本中的旧 FQBN 前缀改为 `stc:mcs251:`；旧包的版本升级不会自动完成迁移。确认新包可用后，可卸载旧包。手动安装源码时，平台目录应为 `<sketchbook>/hardware/stc/mcs251`；开发板管理器安装路径为 `packages/stc/hardware/mcs251/<version>`。

0.0.7 开发板管理器包安装 `stcxx-toolchain` `0.2.0` 和 `stc-cli` `0.1.0-stc.2` 两项工具。原生打包器只保留 Clang、LLVM-CBE、SDCC 及其原生辅助程序、头文件、运行库和许可证，不携带解释器。C++ 编译流程仍为 Clang → LLVM-CBE → SDCC，`stc-cli` 继续负责 UART 和原生 USB 上传，并支持新 CDC 的 1200 bps 复位。

## 支持型号

| 型号 | 物理程序 Flash | 当前构建上限 |
| --- | --- | --- |
| STC32G12K128 | 128 KiB | 128 KiB |
| STC32G144K246 | 246 KiB | 246 KiB |
| STC32G8K64 | 64 KiB | 64 KiB |
| STC32CL8K64 | 64 KiB | 64 KiB |
| AI8051U-34K64 | 64 KiB | 64 KiB |
| STC32G12K64 | 64 KiB | 64 KiB |
| STC32G8K48 | 48 KiB | 48 KiB |
| STC32CL8K48 | 48 KiB | 48 KiB |
| AI8051U-34K32 | 32 KiB | 32 KiB |
| AI8051U-34K16 | 16 KiB | 16 KiB |

型号、存储布局和引脚掩码以 [devices.json](tools/variants/devices.json) 为准。物理 Flash 与当前已验证的可执行窗口不一定相同。具体封装也可能引出更少的 GPIO。

STC32CL8K48／64 提供硬件 Wire、SPI、PWM 和访问加速，默认 Wire 接线为 SDA=P3.3、SCL=P3.2。各型号外设能力和引脚配置见对应的 `variants/<型号>/pins_arduino.h`。

## 上传

Arduino 上传时，AI8051U 等 ISP 不提供型号 ID 的板项默认使用 IDE 中选择的型号，不再要求手动打开额外确认开关。`工具 → Upload model check` 可切换到 `Require detected model ID`；该严格模式会拒绝无法提供 ID 的协议。具有可识别 ID 的型号继续核对实物与所选型号是否匹配。

正常上传在输出窗口显示等待上电提示和 `Writing... 0%` 至 `100%` 的进度，按芯片确认接收的数据量更新。普通信息和进度使用正常文字颜色，真正的错误保留红色及失败状态。IDE 的“详细上传输出”不会开启原始 ISP 报文；需要排查协议时可单独运行 `stc-cli flash ... --debug`。

STC32G144K246 的 `工具 → Upload method` 默认为 `Automatic (USB CDC or UART)`。所选端口为 STC 原生 USB CDC（`34BF:FF02`）时，通过 `@STCISP#` 切换到工厂 HID 下载；USB 转串口或普通串口使用 UART ISP。`UART ISP` 可显式指定串口下载。芯片已经处于工厂 HID 模式、应用 COM 口不再存在时，选择 `Native USB`，并且只连接一个待下载的 STC HID 设备。USB 路径具有可验证的型号 ID，保留对应的检查。

带原生 USB 的型号新增 `工具 → USB CDC On Boot`。默认 Disabled 保持 `Serial` 为 UART1；选 Enabled 后 `Serial` 通过 USB CDC 通信，普通 Blink 也会保留 USB COM 口。显式接口为 `USBSerial` / `SerialUSB`，硬件 UART1 为 `Serial1` / `Serial0`，详见 [USB CDC](libraries/USB/README.md)。配套更新的上传器识别新 CDC `1209:0002` 和复合设备 `1209:0003`，通过 1200 bps 关闭端口进入 ISP；旧 STC CDC 仍用 `@STCISP#`。未启用 CDC 的普通 Blink 不产生 COM，再次下载需手动进入 USB 下载模式并选择 `Native USB`。USB 上传不修改已有时钟及硬件选项。

本地替换平台文件后，如果新菜单没有出现，在 IDE 中执行 `工具 → Reload Board Data（重新加载开发板数据）`。IDE 会持久化板型菜单，普通重启未必更新已有缓存；重新加载后检查所选时钟等板型参数。

## 从源码构建

准备 Arduino CLI、Rust、Node.js 及所需编译工具，先运行 `node scripts/build-native-driver.mjs`。原生打包和本机安装方法见 [打包说明](scripts/TOOLCHAIN-PACKAGING.md)。以下是保留的可选 PowerShell 维护入口（另需 tar），Arduino 编译本身不调用它。命令在源码仓库根目录执行；开发板管理器安装包不包含 `scripts` 维护工具：

```powershell
$build = .\scripts\build-example.ps1 `
    -Fqbn 'stc:mcs251:stc32g12k128:clock=12m' `
    -WorkDirectory D:\stc51-work `
    -ToolCacheDirectory D:\stc51-tools
$build.firmware
```

脚本将当前源码打包到独立目录，校验工具包的 SHA-256，然后编译示例。`-SketchPath` 可指定含同名 `.ino` 的 sketch 目录；SDCC 构建路径应避免空格，Windows 下应使用较短的构建目录，避免中间文件超过系统路径长度限制。默认 Blink 使用 P3.2，按电路修改引脚，不假设存在板载 LED。

平台只提供 C++11 模式，所有板项默认启用 C++ 编译链，无需选择语言或附加 `cppcore` 参数。安装脚本不传 FQBN 时默认编译 STC32G8K64、12 MHz 的 C++ Blink。G12K128、G144K246 等完整 Flash 布局依赖发布包中重建的 SDCC 分区功能；旧原版工具包不满足时构建会明确拒绝。详见 [原生驱动说明](tools/stcxx-driver/README.md)。

源码安装脚本用于维护者调试。脚本会下载并校验锁定的统一工具链和上传器归档，也可通过 `-ToolCacheDirectory` 复用本地归档，或通过 `-ToolManifestPath` 指定已验证的工具清单。发布前将本地工具归档目录传给 `-ToolCacheDirectory`。统一工具链打包步骤见 [打包说明](https://github.com/coloz/arduino-stc51/blob/main/scripts/TOOLCHAIN-PACKAGING.md)。打包不会自动发布。版本变化见 [RELEASE_NOTES.md](RELEASE_NOTES.md)。

## C++ 与 Arduino API

C++ 使用 Clang → LLVM-CBE → SDCC，提供 `String`、`Print`、`Stream`、`HardwareSerial`、`SPIClass`、`TwoWire` 等接口。所有板项默认使用 12 MHz，时钟菜单仅列出编译链支持的配置；AI8051U-34K64 另有 40 MHz、STC32G144K246 另有 48 MHz。编译时钟必须与芯片实际时钟一致，菜单不会替代 ISP 时钟配置。

支持 GPIO、计时、UART1、按型号提供的 ADC/PWM/外部中断，以及 Wire、SPI、SoftwareSerial、LiquidCrystal、Stepper 和受限的 SD。接口以 [Arduino.h](cores/STC/Arduino.h) 和各库头文件为准，使用示例位于 `libraries/<库名>/examples`。总线和 GPIO 共用引脚，使用前核对型号和封装。

0.0.4 新增 [USB HID](libraries/HID/README.md)、[Keyboard](libraries/Keyboard/README.md)、[Mouse](libraries/Mouse/README.md) 和 [CAN](libraries/CAN/README.md) 通信库，随 core 平台包一同安装。键鼠 API 移植自 Arduino 官方库；CAN 提供 `HardwareCAN` / `CanMsg` 风格接口和独立的分包适配器。HID 按型号支持 G12、G144、AI8051U，CAN 支持 G12、G8、CL、G144；AI8051U-34K16 当前只能容纳较小的自定义 HID 示例，键鼠示例超出 Flash。各库说明包含支持型号、接线和示例索引。

ABI 使用 16 位 `int`、32 位 `long`/`size_t`/`ptrdiff_t`、24 位指针，大端布局，`double` 与 `float` 均为 32 位。异常、RTTI、线程和完整 STL 不在支持范围内。不要直接发送结构体内存作为外部协议；可使用 `STCByteOrder.h`。运行时配置见 [runtime-manifest.json](cores/STC/runtime/runtime-manifest.json)。

0.0.6 已移除随附的实验性 Adafruit NeoPixel 库。

## 烧录

Arduino IDE 中选择实际芯片型号和串口，点击“上传”即可调用开发板管理器安装的原生 `stc-cli`。出现等待连接提示后，将芯片断电再上电以进入 ISP；默认等待 60 秒。请关闭占用该串口的串口监视器或其他程序。

AI8051U、STC32CL8K48/64、STC32G12K64 和 STC32G144K246 的 UART ISP 当前无法提供可核验的型号 ID，默认使用 IDE 所选型号；请核对芯片丝印。需要严格校验时选择“工具 → Upload model check → Require detected model ID”。STC32G12K128、STC32G8K48/64 及可识别的 USB ISP 路径继续核验实际型号。

若仍提示 `Property 'upload.tool.serial' is undefined`，请刷新索引、确认已安装当前宿主支持的版本并选择 `stc:mcs251` 下的板卡，然后重启 IDE。

也可从 [v0.0.7 安装资源](https://github.com/coloz/arduino-stc51/releases/tag/v0.0.7) 下载对应宿主的独立 `stc-cli`，先检查 HEX 再手动烧录，例如：

```powershell
$stc = '..\stc-cli\target\release\stc-cli.exe'
& $stc validate --expect STC32G12K128 --execution-mode mcs251 --file $build.firmware
& $stc flash --port COM5 --expect STC32G12K128 --execution-mode mcs251 `
    --file $build.firmware --reset manual --allow-experimental
```

不同目标所需的实验协议参数以当前 `stc-cli` 的说明为准。支持选择执行模式的芯片应先由官方 ISP 配置为 MCS251；`stc-cli --execution-mode` 用于镜像处理，不会切换芯片硬件模式。

## 维护

```powershell
node .\tools\variants\generate.mjs --check
node .\tools\variants\generate.mjs
node .\scripts\build-native-driver.mjs
.\tools\stcxx-driver\stcxx.exe package-platform . dist/arduino-mcs251-native.zip
```

生成器仅接受 MCS251 型号，并拒绝遗留的孤立 variant 目录。平台和库架构声明统一为 `mcs251`。修改 [devices.json](tools/variants/devices.json) 后重新生成配置。工具链二进制与 ABI 由 `tools/stcxx-driver` 中的宿主锁文件校验；上传配方及安装验证脚本保留在 `scripts`。

`scripts` 保留 core 与原生驱动构建、现有工具链打包、索引生成、代码生成和发布校验入口。用户安装包保留 core、variants、库、示例、编译驱动、锁文件及许可说明；维护脚本和生成器只放在源码仓库。SDK 使用预编译工具链，编译器源码补丁及重建脚本已移除，编译器本身由独立的 `stcxx` 项目维护。工具目录用途见 [tools/README.md](tools/README.md)。上游 `sdk` 资料清单已移除；源码构建的工具下载缓存默认位于 `dist/toolchain-cache`。

第三方 SDK、SDCC 的公共 `include/mcs51` 目录和固定上游归档名属于 MCS251 仍需的依赖或来源记录，不代表继续提供 MCS51 Arduino 支持。项目采用 [MIT 许可证](LICENSE)，第三方许可保留于 [LICENSES](LICENSES)。
