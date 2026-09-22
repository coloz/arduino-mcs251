# Native STCXX packages

Clone `stcxx` beside this repository, then build the shared native driver with
Rust and Node (Node is only a maintenance tool):

```text
node scripts/build-native-driver.mjs
tools/stcxx-driver/stcxx.exe package-platform . dist/arduino-mcs251-native.zip
tools/stcxx-driver/stcxx.exe package-toolchain <existing-toolchain-root> dist/stcxx-toolchain-native.zip
```

The existing toolchain root supplies the locked Clang/LLVM-CBE and SDCC binaries.
The native packager includes frontend/bin, frontend/lib, sdcc/bin,
sdcc/include, sdcc/lib, native libexec helpers, `bin/stcxx`, the independent SDK
under `share/stcxx/sdk`, and the corresponding license notices. It does not run
or package the old embedded interpreter, scripts, bytecode or build-input trees.
The platform includes the native driver, its dependency licenses, lock files,
core, variants, libraries and examples. Arduino invokes stcxx directly.

Unified toolchains use the shared driver's 0.3.0 version. Published 0.0.7
platforms retain their original 0.2.0 dependency. Do not replace an existing
published asset.

This SDK consumes prebuilt compiler packages. Compiler source patches, source
rebuild scripts and their dedicated notices are not SDK build inputs and have
been removed. Compiler development belongs to the separate `stcxx` project;
the host locks retain binary, ABI and source digest bindings. Packaging an
existing toolchain still includes the component licenses from that toolchain.

Archives use stable entry order and timestamps. Each ZIP has an adjacent JSON
report containing its size and SHA-256, and includes a file manifest. Existing
output ZIPs are never overwritten. Windows binaries statically link the C runtime.

The 0.0.9 Boards Manager release supplies Windows x64 and Apple Silicon native
packages with stcxx-toolchain 0.3.0 and stc-cli 0.1.0-stc.2. The index retains
previous versions and their original dependencies for rollback. Qualify a native build on Apple Silicon
before including that host in a release; Windows verification does not qualify macOS.

New platform archives use `arduino-mcs251-<version>.zip` and the matching ZIP root.
The renamed `package_mcs251_index.json` retains published archive filenames,
sizes and checksums, including older `arduino-stc51-<version>` archives.

For native releases, package the platform as `arduino-mcs251-<version>.zip`,
record tool archive sizes, hashes, roots and pinned release URLs in
`tools/toolchain-manifest.json`, then run:

```text
python scripts/create-native-release-index.py --platform dist/release-0.0.9/arduino-mcs251-0.0.9.zip --assets dist/release-0.0.9 --previous dist/release-0.0.9/previous-index.json --output package_mcs251_index.json
```

Save the published index as `previous-index.json` before generating the new one.
Generating `package_mcs251_index.json` also writes an identical
`package_arduino-stc51_index.json` compatibility copy for existing subscriptions.
The native index generator verifies every ZIP payload manifest and tool archive
binding, requires matching host dependencies and driver binaries, and retains
previous versions. Uploader ZIPs include licenses, `MANIFEST.sha256` and a
`build-info.json` recording the source commit; publish a source snapshot alongside
the uploader. Unchanged tool versions reuse their published URLs and must exactly
match the previous index, including every host, size and checksum. Only new tool
versions use URLs under the new release; never replace an existing tool archive.
Validate installation with Arduino CLI in a separate data directory,
then compile a supplied example for the selected board.
No packaging or index command publishes a GitHub release automatically.

To prepare an unpacked native compiler payload:

```text
tools/stcxx-driver/stcxx.exe stage-toolchain <existing-toolchain-root> <new-directory>
```

After extracting and validating a platform ZIP, the Windows local installer can
replace an existing published installation. Both arguments are unpacked directories:

```text
node scripts/install-native-driver.mjs <native-platform-directory> <native-toolchain-directory>
```

The installer retains the previous platform and toolchain under a unique
Arduino15-native-backups directory beside Arduino15 and records the paths in
installation.json. Platform and toolchain versions are read from the payloads.
The installed `platform.local.txt` selects the unified toolchain's backend and
include paths, independently of older dependencies in the published index.
The public compiler remains the SDK's native Arduino adapter.
Existing user overrides and Boards Manager metadata are preserved.
Reinstalling from Boards Manager restores published
payloads, so do not confuse this local candidate with a published release.

The optional
scripts/build-example.ps1 maintenance entry point delegates packaging to the
native driver and stages only native compiler components. It is not called by
Arduino recipes. See tools/stcxx-driver/README.md for driver details.
