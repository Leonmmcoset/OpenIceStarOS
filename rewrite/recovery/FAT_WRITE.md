# FAT root-file creation and overwrite

This document records the 397-interval checkpoint. The subsequent
[absolute-path writer recovery](FAT_WRITE_PATH.md) extends the same module
and its tests; the counts and evidence below describe the earlier checkpoint.

Two original-only entries now execute recovered C in `kernel/fat_write.c`.
Both retain their fixed entry addresses and cdecl ABI.

| Address | Operation | Slot bytes | C bytes |
| --- | --- | ---: | ---: |
| 0x828018 | Split a path into parent and filename | 204 | 133 |
| 0x82867e | Create or overwrite a root-directory file | 1,207 | 954 |

The splitter searches backward for a backslash at a signed positive index.
A backslash at index zero does not qualify. Failure leaves both outputs
untouched; success copies and terminates the parent before copying the
filename. Output/input overlap follows that original copy order. There are
no added buffer-length checks.

The root writer caches both FAT pointers and the data-region LBA, copies
the name into its original 128-byte temporary buffer, uppercases it, and
uses the existing root lookup. Only a signed positive lookup result selects
overwrite; index zero instead enters the creation path. It releases an
existing file's chains and changes only its size, retaining the starting
cluster, attributes and timestamps. Creation prefers any zero record over
deleted records, releases a reused deleted record's chain, formats the
original input as an 8.3 name, and installs the original fixed attributes
and date/time values.

Sector/cluster counts preserve 32-bit wrap and unsigned division. Zero BPB
divisors still fault. The first cluster writes the smaller of one cluster
and the requested sector count; every later cluster writes a full cluster,
including data beyond the declared EOF. A zero-length file still acquires
one cluster and a FAT end marker, while returning zero sectors. Allocation
failure preserves the original unchecked -1-to-0xffff behavior; subsequent
allocation rejects only zero. FAT2 receives the resulting FAT1 chain only
after data transfers, and metadata flush occurs last. BPB reads that were
live in the original remain live. These behaviors have not been repaired.

## Differential and installation checks

`tests/kernel_fat_write.py --installed` compares original x86, ordinary C,
and the installed fixed-address C in 485 scenarios / 1,455 executions.
Filesystem and string helpers execute their real machine code; only the
sector-transfer boundary is intercepted. Comparisons include return/fault
results, data-write argument order and source hashes, complete low-memory
and 2 MiB fixtures, preserved caller arguments and callee-saved registers.
Cases cover split-buffer overlap, new/existing/deleted/full directories,
sector/cluster boundaries, zero sizes, wrapping sizes/LBAs, multiple BPB
geometries/FAT counts, exhaustion, attributes/index zero, signed character
names, and BPB/global-pointer changes after the first transfer.

Focused installed regressions cover FAT release/deletion, metadata flush,
contiguous writing, free-slot scans, absolute path/name formatting,
directory accessors/lookups, file reading and X loading. The disk installer,
five-application installer and inventory checks are also checked separately.
The production kernel now has 397 declared intervals; interval counts and
padding bytes are not a measure of complete OS recovery.

## QEMU disk comparison

`tests/fat_write_guest.c` is a test-only X application, temporarily installed
over COLOR.X by `tests/kernel_fat_write_qemu.py`. It runs under both the
original and installed C kernels through CMD using emulated PS/2 input.
The runner checks that the cached FAT/root data equals the fixture before
execution, verifies entry/body execution for the recovered routines and
the original ATA writer, and checks the diagnostic result words.

The disposable fixture has fragmented allocation, a two-cluster existing
file, a two-cluster deleted file and precisely two empty root records. It
creates an 8,193-byte file, overwrites the existing file with one byte,
creates a zero-length file and reuses the deleted record for a 16,385-byte
file. The expected image specifies both FATs, exact record fields, preserved
old data and all additional sectors written beyond EOF. The original disk
has five unused entries free only in FAT1; the fixture explicitly reserves
these in both copies so its chosen allocation area starts in common free
space. The production disk's FAT discrepancy is retained.

Each entire 33,554,944-byte result equals its independently constructed
expected image. Both kernels change the same 41,415 bytes. Boot sectors,
other files, unused data, slack and VHD footer are included in the
comparison. QEMU writes only temporary copies; source images are rechecked
unchanged. Neither filesystem helpers nor disk I/O are intercepted here.

    make -C rewrite build/OpenIceStarOS_c_apps.vhd test-fat-write
    make -C rewrite test-fat-write-qemu test-fat-mutation-qemu
    make -C rewrite test-fat-mutation test-fat-metadata test-fat-io \
      test-fat-slots test-fat-path test-fat-directory test-fat-files test-x-loader
    python3 rewrite/tools/kernel_image.py verify IceStarOS_x86_32M.vhd \
      rewrite/build/OpenIceStarOS_c_init.vhd rewrite/build/openicestaros_kernel.x \
      rewrite/kernel/patches.json rewrite/build
    python3 rewrite/tools/app_image.py verify rewrite/build/OpenIceStarOS_c_init.vhd \
      rewrite/apps/images.json rewrite/build rewrite/build/OpenIceStarOS_c_apps.vhd
    python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
      --clock-check --memory-check --task-check
    make -C rewrite inventory-check

Small summaries are retained in `../evidence/2026-10-31-fat-write/`.
Full images, traces and dumps remain ignored in `build/`. No full
`make verify` was run for this checkpoint. These fixtures do not establish
arbitrary filesystem correctness. At that checkpoint the absolute-path writer at 0x8280e4,
ATA entry assembly and other unrecovered paths still require original
machine code. INIT/kernel remain partial; five applications are rebuilt and
32 original program paths remain unrecovered. The original logo is retained.
