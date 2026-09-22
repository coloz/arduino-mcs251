# 工具目录

当前 Arduino 编译入口是 `stcxx-driver/stcxx.exe`（Windows）或
`stcxx-driver/stcxx`（macOS），由 `platform.txt` 直接调用。

| 路径 | 保留用途 |
| --- | --- |
| `stcxx-driver/` | 原生驱动源码、Cargo 配置与依赖锁、宿主工具链锁和构建说明。`stcxx.exe` / `stcxx` 及 `LICENSES/` 由构建脚本生成并由 Git 忽略，供源码使用和打包。 |
| `variants/devices.json` | 板型、引脚与存储配置的来源；发布校验也读取该文件。 |
| `variants/generate.mjs` | 生成并校验 `boards.txt` 和各型号的 variant。 |
| `toolchain-manifest.json` | 已发布工具归档的版本、下载地址及摘要；源码示例构建和发布打包使用它。 |

旧 Python/shell 编译驱动、CBE 适配脚本和包装入口已由原生驱动取代并移除。
SDK 使用预编译的 Clang、LLVM-CBE 和 SDCC，编译与打包均不应用 `.patch`。
编译器补丁、源码重建脚本和专用许可副本已从本仓库移除；编译器本身的开发
与重建属于独立的 `stcxx` 项目。宿主锁文件继续保留二进制、ABI 和来源摘要，
工具链安装包继续携带其组件的许可证。

原生驱动构建和打包见 [驱动说明](stcxx-driver/README.md)。
Cargo 的 `target/` 和 Python 的 `__pycache__/` 是可重新生成的本地缓存，
不属于必要源码。
