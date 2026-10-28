# FAT16 file-reader recovery

The original kernel bodies at `0x827454..0x82798c` implement six callers used
by startup resource loading, icon loading and the recovered X loader:

| Address | Operation | Original bytes | Compiled preview bytes |
| --- | --- | ---: | ---: |
| `0x827454` | Allocate and read a directory record's file | 440 | 362 |
| `0x82760c` | Read a record's file into a supplied buffer | 396 | 332 |
| `0x827798` | Find exact-success pathname and allocate/read | 68 | 42 |
| `0x8277dc` | Find exact-success pathname and read into buffer | 74 | 46 |
| `0x827826` | Read first sector by loaded root-directory index | 206 | 202 |
| `0x8278f4` | Read first sector by directory record | 152 | 142 |

`kernel/fat_files.c` recovers these bodies in C. The BPB is at `0x520000`,
partition LBA at `0x7dc6`, and loaded FAT pointer at `0x1a83a`. The root
sector count is rounded up from `root_entries * 32`. Data LBA is partition
start + reserved sectors + FAT count * sectors/FAT + root sectors. Relative
header reads locate the loaded root record before computing its first LBA.

The original traverses to **exactly `0xffff`**, including continuation through
other nominal FAT end markers. It caches the FAT pointer and data LBA, re-reads
sector/byte geometry when calculating each destination, ignores disk status,
and reads the first successor before issuing the first read. Allocation sizes
use the original wrapping unsigned ceiling calculation; directory allocations
use one cluster. The supplied-buffer path still evaluates its otherwise-unused
division before checking for a null destination. Path wrappers require lookup
result exactly 1, unlike the X loader's positive-result check. These behaviors
are retained for compatibility, including the original unsafe cases.

`python3 tests/kernel_fat_files.py` compares original machine code, ordinary C
and fixed-address C in Unicorn: 692 scenarios / 2,076 executions. All 39 original
program paths contribute real directory records, geometry, FAT chains and disk
sectors; complete file contents are checked independently against FAT extraction.
Synthetic cases cover geometry, allocation failure, null destinations, directories,
size wrapping, lookup return boundaries, unusual cluster IDs, ignored read errors,
live BPB/FAT mutations, cdecl registers/stack and division faults. Only allocator,
pathname lookup and hardware sector-read boundaries are intercepted. The traversal
and all six bodies execute as real x86 in every variant. Fault cases use fresh
Unicorn CPUs because delivered exceptions retain internal exception state.

Preview evidence: `build/fat_files_summary.json`, `build/fat_files_preview.log`.
This checkpoint verifies source recovery; production installation and real boot
validation are recorded next. It does not recover the underlying disk controller
or pathname-lookup implementations, and does not complete the OS.

## Integrated validation

The production manifest now installs all six intervals, taking the total from
352 to 358. `make test-fat-files test-x-loader` verifies the actual installed
slots, including overflow assertions and exact agreement with the independent
preview link. Kernel whole-disk verification, five application installer tests,
whole-application disk comparison, six branding safety tests, residual scan and
inventory check pass in a fresh worktree build. The inventory retains five
rebuilt applications, two partial programs and 32 unrecovered program paths.

`tools/boot_smoke.py build/OpenIceStarOS_c_apps.vhd --clock-check
--fat-files-check --memory-check --task-check` boots the installed image. QEMU
records C entry/body execution for allocating a directory record (78 entries),
reading a record (6), allocating by path (34) and reading a program-header
sector (4). Named buffered reads and relative headers are not observed in this
boot; their differential fixtures remain the evidence for those two entries.
The 128 MiB RAM/59-header heap, real TSS switches, branded CMD/cpuid, changing
CLOCK frames, PS/2 close, task exit and CMD reuse all pass, and all 358 installed
intervals match live guest memory. CMD and CLOCK screenshots were inspected.

Evidence is committed in `../evidence/2026-10-28-fat-files/`. Underlying pathname
lookup and hardware sector readers remain original code. This is a focused
358-interval integration check, distinct from the separate full 352-interval
branding regression.
