# Native STC compiler driver

The compiler implementation is maintained in the sibling `stcxx/compiler` crate.
This directory owns Arduino build-system adaptation and has a Cargo path
dependency on that shared compiler. Clone `stcxx` beside `arduino-mcs251` when rebuilding;
installed platform ZIPs contain the native executable and require no checkout.
Run shared unit tests with `cargo test --manifest-path ../stcxx/compiler/Cargo.toml`.

The canonical runtime and host locks are in `stcxx/sdk`. Run
`node scripts/sync-stcxx-sdk.mjs` to refresh the checked platform copies, or
append `--check` to verify them. The native build script performs this sync.
The same driver supports independent `--chip ... main.cpp -o firmware.hex`
builds; see the sibling `stcxx/sdk/README.md` for installation and usage.

`stcxx.exe` (Windows) / `stcxx` (macOS) directly implements the Arduino recipe
adapter, C/C++ compilation, core cache, LLVM archive selection, CBE adaptation,
constructor bridge, member-function alignment, native storage checks, heap and
stack validation, linking and memory reporting. It launches compiler executables
using argument arrays. No shell or interpreter is loaded by the driver.

Command traces and the link success marker use stdout so Arduino IDE displays
them as ordinary build information. Preprocessor recipes omit traces to keep
source/dependency stdout machine-readable. Compiler stdout/stderr and exit status
retain their meaning; a failed link removes the HEX output.

Compilation follows these native stages:

```text
Arduino -> stcxx -> Clang -> LLVM link/optimization -> LLVM-CBE -> SDCC
                -> SDCC (C sources), SDAR (archives), assembler/linker
```

The default C++ frontend and whole-program optimizer now use Oz. LLVM verifies
each link optimization pass; `stcxx/optimization.json` records the requested
frontend mode and the actual pipeline. The 24-bit target layout, constructor,
CODE/XDATA, member-pointer alignment and diagnostic audits still apply.
Global vtable byte-offset initializers are rewritten only after checking the
table's pointer layout and bounds.

Use platform-wide `build.extra_flags` for controlled comparisons:
`-DSTCXX_CPP_OPT=0` restores the original O0/closure-only pipeline;
`-DSTCXX_CPP_OPT=0 -DSTCXX_LINK_OPT=z` optimizes only at link time;
`-DSTCXX_LINK_OPT=0` leaves only frontend Oz. A sketch-local define cannot
configure all compiled sources or the link recipe. Nonzero frontend modes
default to link Oz; an explicit link override accepts only `0` or `z`.
Rebuild old precompiled C++ libraries to obtain the new frontend attributes.

The driver retains constructor order, whole-module C++ archive selection,
member-function pointer alignment and CODE/XDATA checks. Optional native C
pruning keeps each translation unit's private state together. Hardware code
that cannot be analyzed safely is retained; an oversized unsplittable object
fails with a diagnostic. No helper script is invoked as a fallback.

Source-closure pruning also handles native C members of source-backed archives,
including libraries selected with `--library` outside the sketchbook. Source,
dependency-header and original-object hashes must match. Binary-only, relocated
or changed-source archives retain their original native members. Recompiled
members remain inside an archive, preserving normal extraction and shared
translation-unit state. `stcxx/native-source-closure.json` records the selected
sources and rebuilt archives.

Native C dependency rules target Arduino's `.o` file while preserving SDCC's
path spelling, encoding and make escaping; explicit `-MF` destinations are
honored too. Windows ANSI dependency paths are decoded only for internal
metadata; the file read by Arduino retains SDCC's original encoding.
Unchanged builds reuse these objects. Native C compilation no longer starts
Clang just to discover its resource directory. Toolchain verification reuses
each file's hash within one invocation, still checking every lock entry.

Trimmed native RELs and listings are cached under the build's
`stcxx/native-source-closure/` directory. Reuse requires matching source/header
contents, original object metadata, selected functions, generated C, arguments,
toolchain lock and driver executable fingerprint, plus matching output hashes.
Missing or damaged local entries are rebuilt; symbol audits still run on hits.
`native-source-closure.json` records `compile_cache_hit` for each trimmed unit.
Shared-compiler archive updates validate all members and reuse the compressed
bytes of unchanged members. The SDK's public bundle format is described below.

## aily-builder

The public `compiler.path` and `compiler.*.cmd` properties identify STCXX.
`compiler.backend.path` and `compiler.backend.*.cmd` identify SDCC/SDAR only
inside recipes. Local toolchain overrides must set these paths separately.

The SDK supports unmodified aily-builder 1.2.17 through standard recipes.
No `build.aily.*` properties or builder patches are required. The SDK executable
must be rebuilt from this directory; `compiler.path` must not point directly to
the standalone toolchain's `bin/` directory, which has no Arduino adapter.

- `compile-unit` stores the REL, C++ bitcode/IR, metadata and source together in
  one checksummed `.o` bundle. Individual-object caches cannot lose companions.
- `rcs archive.a objects...` accepts the builder's fixed archive command and
  replaces the complete member set. Arduino CLI's `archive-unit` recipe updates
  one member at a time. Both produce self-contained `.a` bundles.
- `link-units` relocates and verifies these bundles, then calls the shared
  STCXX compiler. Its ELF32 output contains the actual flash load segments
  (`EM_8051`, without debug symbols). The standard `recipe.objcopy.hex.pattern`
  exports those bytes as Intel HEX for upload. Memory reporting still uses
  STCXX's `.mem` report.
- A standard prebuild hook copies sibling C/C++ files and recursive `src/`
  inputs into the builder's existing generated-sketch directory before analysis.
  Arduino CLI already handles these files, so the hook is inactive there.

Existing raw `.o`/`.a` build caches must be cleaned once after this format change.
The standalone compiler's artifact format and C/C++ implementation are unchanged.
Run SDK adapter tests with `cargo test --manifest-path tools/stcxx-driver/Cargo.toml`.
Windows x64 and Apple Silicon macOS 15+ use separately built native binaries.
Qualify both hosts before publishing a combined SDK archive.

## Build

Install a Rust toolchain and the host linker, then run from the repository root:

```text
node scripts/build-native-driver.mjs
```

The build script also copies dependency license notices from Cargo's package
sources into `LICENSES/`. This generated directory is ignored by Git and is
included in platform packages. It can be removed locally and regenerated by
running the build script before packaging.

For driver development alone, run `cargo build --release --locked` inside this
directory and copy `target/release/stcxx.exe` here. Use the Node build script before
packaging to also generate dependency notices. Windows builds statically link the C runtime.
The macOS binary must be built on the supported Apple Silicon host; Windows tests
do not qualify a macOS release.

Arduino invokes the binary through `platform.txt`, passing `--platform` so an
installed build never depends on a source checkout path. `STCXX_TOOLS_ROOT` can
select an unpacked native toolchain during development. The native lock files bind
compiler executables, resource headers and SDCC libraries to their SHA-256 values.

## Package

```text
tools/stcxx-driver/stcxx.exe package-platform . dist/arduino-mcs251-native.zip
tools/stcxx-driver/stcxx.exe package-toolchain <existing-toolchain-root> dist/stcxx-toolchain-native.zip
tools/stcxx-driver/stcxx.exe stage-toolchain <existing-toolchain-root> <new-directory>
```

The platform ZIP contains the native driver and lock files. The toolchain operation
copies only native compiler components, headers, libraries and their licenses,
with fresh manifests. It excludes the old embedded interpreter and build scripts.
ZIP timestamps and entry order are deterministic. Existing output archives are
never overwritten. Each ZIP has an adjacent JSON report with its size and SHA-256.
Supply both host driver binaries in this directory before making a combined
Windows/macOS release. These commands create local artifacts; they do not publish
or update the public Boards Manager index.
