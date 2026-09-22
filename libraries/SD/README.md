# SD library for the STC C++ core

This clean-room MIT implementation supports SD version 1, SD version 2 and
SDHC cards in SPI mode. It requests 100 kHz for card initialization, then
4 MHz on bus-layout-1 devices or 400 kHz on other devices by default.
`begin(clock, cs)` selects a nonzero data-clock request; SPI chooses the
supported hardware divider or software fallback. It provides bounded `CMD17`/`CMD24`
single-sector I/O. Every card-response, data-token and busy wait has both a byte-attempt
limit and a `millis()` deadline; chip select is released on every exit path.

`SD.begin(cs)` also mounts a 512-byte-sector FAT16 or FAT32 volume. Sector zero
may be either a FAT boot sector (a superfloppy) or an MBR whose first partition
contains FAT. The file layer is intentionally small and deterministic:

If card initialization succeeds but FAT mounting fails, `begin` returns zero,
`fatType` remains `SD_FAT_NONE`, and bounded raw block I/O remains available for
diagnostics until `end` is called.

- one 512-byte cache and file contexts in XDATA, with a compile-time minimum
  of 1 KiB XDATA; budgeted hot volume state can reside in DATA (see below);
- one card, one shared sector cache, and independently allocated file contexts;
- nested directories with ASCII short 8.3 path components;
- file reads through `exists`, `open`, `peek`, `read`, `readBytes`, `available`,
  `seek`, `position`, `size`, and `close`;
- `FILE_WRITE` creation and append, seek-then-overwrite, `flush`/`close`
  writeback, and `remove`, including cluster allocation and release;
- recursive `mkdir`, empty-directory `rmdir`, `isDirectory`, `openNextFile`
  and `rewindDirectory`;
- no long-file names, FAT12, exFAT,
  formatting, sparse files, or power-loss-atomic metadata updates.

The library exposes `SDClass SD` and a writable `File : public Stream` facade, including
`Print`/`println` inherited from `Print`. The class layer provides multiple
open files on the same mounted card and `String` path overloads. A `File`
copy shares its original context and position; a separately opened file has
its own position. Concurrent mutable opens of the same directory entry, or
deleting a file/directory with a live handle, fail with `SD_ERROR_FILE_BUSY`.
Read-only duplicate opens are supported. `openNextFile()` skips dot entries,
deleted entries and FAT volume/long-name records; reaching the end returns an
invalid File with `SD_ERROR_NONE`.
A successful `setPins()` closes the active backend and invalidates every
outstanding `File` facade; a rejected pin configuration leaves the mounted
backend and its handles unchanged.

Closing the last live copy flushes its context; opening a different file
preserves existing handles. The C backend uses a fixed-address working context
and copies it at file switches, avoiding repeated far-pointer arithmetic on
every byte. Each live facade shares a small heap allocation until its last
owner releases it. `open()` returns an invalid File if allocation fails and
leaves existing files usable. A successful reinitialization or `end` invalidates
all handles. All calls remain foreground
operations; this is not a thread-safe or interrupt-safe filesystem API.

If the last handle's explicit `close()` fails to flush, the handle stays valid
and `getWriteError()` records the failure. Correct the I/O problem, call
`clearWriteError()`, and retry `flush()`/`close()`. A failed close also prevents
`SD.open()`/`begin()`/`end()`/`remove()` from discarding the existing handle.
A destructor cannot preserve its own handle, but pending backend state is
retained for the next open or reconfiguration to retry. This is recovery from
a transient I/O failure, not a guarantee against power loss. `File.read(buffer,
length)` returns at most `INT_MAX` bytes per call (32767 on MCS251).

Directory scans have a hard 4096-sector ceiling in addition to the FAT
cluster-count bound, so a corrupt cyclic directory chain cannot cause an
effectively unbounded lookup. File reads are bounded by the directory entry's
declared size and the volume cluster count. File growth updates mirrored FAT
copies (or the selected active FAT when FAT32 mirroring is disabled) and the
file's directory entry, but those writes are not a transaction and an I/O
failure or sudden power loss can leave a damaged filesystem. This small
implementation is not a FAT repair tool or full `fsck`; do not treat data from
a damaged volume as trusted.

The raw `readBlock` and `writeBlock` calls always transfer exactly 512 bytes.
`writeBlock` bypasses FAT consistency and can irreversibly corrupt the mounted
volume; use it only when the caller owns the on-card layout. Normal writable
file access instead maintains FAT16/FAT32 file and directory structures.

The complete filesystem requires substantial Flash. A read-only open retains
the writable `File` virtual interface, so read-only behavior does not imply a
small binary. The 1 KiB XDATA check alone does not guarantee sufficient Flash.

The default hot volume state occupies 68 bytes of DATA on STC32G12K128,
STC32G144K246, AI8051U-34K64, STC32G12K64, STC32G8K64 and STC32CL8K64.
Other targets retain XDATA placement; no state is placed in IDATA. Applications
needing that DATA space can pass `-DSTC_SD_STATE_IN_DATA=0` to every translation
unit, for example Arduino CLI `--build-property
build.extra_flags=-DSTC_SD_STATE_IN_DATA=0`. A sketch-only `#define` cannot
configure the separately compiled C backend.

`File.read(buffer, length)` copies contiguous cached ranges per sector. It
preserves dirty-cache reads, EOF, partial success and retry position after an
I/O error. Inherited `Stream.readBytes()` keeps its bytewise timeout behavior;
use `File.read(buffer, length)` to request bulk transfers.
Directory iteration retains a cursor in the existing file context and resets
it on `rewindDirectory`; independent directory handles have independent cursors.

Cards and breakout boards must use 3.3 V signaling. Provide proper level
translation when the selected STC device is operated at 5 V.
