# Contiguous disk, X-header and directory-cluster readers

Eight original entry points at 0x8252b4..0x8258a6 now execute C from
`kernel/fat_block.c`. They retain the original addresses and cdecl ABI.
The kernel installs 406 declared intervals, including 77 exact instruction
intervals. These counts are not OS completion percentages.

| Entry | Recovered behavior | Compiled / slot bytes |
| --- | --- | --- |
| 0x8252b4 | Allocate and read contiguous sectors using the X header size | 110 / 234 |
| 0x82539e | Read contiguous sectors using the X header size into a supplied buffer | 97 / 216 |
| 0x825476 | Allocate and read a supplied sector count | 64 / 84 |
| 0x8254ca | Read a supplied sector count into a supplied buffer | 44 / 60 |
| 0x825506 | Read a partition BPB, FATs and root directory into a cache | 145 / 168 |
| 0x8255ae | Read a root indexed directory's first cluster into a supplied buffer | 229 / 250 |
| 0x8256a8 | Allocate/read a root indexed directory's first cluster | 254 / 276 |
| 0x8257bc | Allocate/read a supplied directory's indexed directory first cluster | 204 / 234 |

## Preserved semantics

All contiguous reads use a fixed 512-byte stride, even when the BPB reports
a different bytes-per-sector value. Loop counts are signed; zero and
negative counts issue no sector transfers. Arithmetic wraps at 32 bits.
The X readers first read one sector into scratch address 0x18a00, interpret
the size field as signed, divide by 512 with truncation toward zero and add
one only for a positive remainder. They reread the size after the remainder
test. The header sector is read again as part of a positive-length transfer.

The count-based allocating entry reads only when the allocation result is
signed positive. The X allocating entry and both directory allocating
entries do not check allocation failure before reading. These are original
behaviors, not added error-handling policies. Hardware return codes do not
change the loops.

The partition entry accepts an unchecked partition-table index. It reads
the BPB, uses its bytes-per-sector/reserved values to locate the destination
cache, computes FAT/root counts with signed root-sector division, rereads
the partition LBA, then invokes the recovered contiguous reader. A zero BPB
divisor raises the original x86 divide exception. The entry returns the
contiguous reader's destination pointer.

Directory entries test only attribute bit 0x10. They do not filter deleted,
empty or volume entries. They read only the low cluster word and one
cluster, using the global BPB and first partition's data-region LBA.
Non-directories return zero before allocating or dividing. For allocating
entries the record address is cached before allocation, while BPB geometry,
partition LBA and record cluster are read after allocation. Entry 0x8257bc
takes the index before the directory-buffer argument. This differs from
some previously recovered directory APIs.

The real ATA single-sector entry at 0x828b44 remains original machine code.
The new 0x8254ca reader is distinct from the previously recovered writer at
0x827c72, whose stride follows the live BPB.

## Differential and guest validation

`tests/kernel_fat_block.py --installed` passes 882 scenarios / 2,646
executions across original x86, ordinary C and fixed-address C. The fixed
variant uses the complete production kernel; reader-to-reader calls execute
in both C variants. Only allocation and ATA single-sector transfers are
intercepted. Comparisons cover ordered boundary calls, transfer contents,
all touched data pages, results/exceptions, callee-saved registers and
unchanged caller arguments. Different stack-frame scratch contents are
excluded from data comparisons.

Cases include all 39 original program paths, the original BPB/FAT/root
cache, directory/file attributes, signed size/count boundaries, zero and
high-bit allocation results, LBA/buffer/index wrapping, nonstandard BPBs,
ignored error statuses and absent transfers. Mutation probes change live
BPB fields, partition LBA and directory records during allocation or ATA
calls, and check original divide faults. The corpus expectations for the X
entries are contiguous disk bytes, not FAT-chain traversal.

`tests/fat_block_guest.c` is a diagnostic X installed temporarily over
COLOR.X in disposable disk copies. Both original and recovered kernels
launch it through CMD using emulated PS/2 input. No allocator, filesystem
or ATA routine is intercepted in QEMU. Traces verify entry and body
execution for all eight recovered functions and the real ATA reader.

Two otherwise free clusters hold an X-size fixture and directory data.
Host-calculated FNV-1a values independently verify each returned payload,
the partition BPB and both FATs/root cache. Non-directory and negative-count
returns are also checked. Each complete 33,554,944-byte disk remains equal
to its seeded image, including slack, other files and VHD footer. The input
images remain unchanged. The diagnostic is not registered as a rebuilt app.

Focused installed regressions pass FAT file/relative readers, paths,
directory accessors, writers, release/deletion, metadata flush, sector
writing, free slots and X loading. Kernel/app installers verify allowed
disk extents. Inventory consistency passes. Production CLOCK startup,
changing frames, PS/2 close, task exit and CMD reuse pass, together with
CMD/cpuid input, 128 MiB RAM/heap checks, real timer/TSS switches and
guest-memory equality for all 406 installed intervals. CLOCK was also
visually inspected. The original logo is retained.

    make -C rewrite test-fat-block test-fat-block-qemu
    make -C rewrite test-fat-files test-fat-path test-fat-directory \
      test-fat-write test-fat-mutation test-fat-metadata test-fat-io \
      test-fat-slots test-x-loader
    python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
      --clock-check --memory-check --task-check
    make -C rewrite inventory-check

Small summaries are committed in `../evidence/2026-10-31-fat-block/`.
Builds, disk images, traces, memory dumps and screenshots stay in ignored
`build/`. Full `make verify` was not run for this revision. These fixtures
do not verify arbitrary invalid pointers, unbounded positive transfer
counts, hardware error recovery or general directory-chain correctness.

INIT/kernel remain partial, five applications are rebuilt and 32 program
paths remain unrecovered. The build still uses original machine code to
supply unrecovered components. Complete OS recovery remains unfinished.
