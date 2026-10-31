# FAT absolute-path file creation and overwrite

The original entry at 0x8280e4..0x82867e now executes C from
`kernel/fat_write.c`. Its 1,015-byte body fits the 1,434-byte slot and retains
the original address and cdecl ABI. It forwards root paths to the previously
recovered root writer. The installed kernel now has 398 declared intervals,
including 77 exact instruction intervals; these counts are not completion
percentages.

## Recovered behavior

The entry caches FAT1/FAT2 and computes the data-region LBA before validating
the path. A zero bytes-per-sector divisor faults even for a malformed path.
The original adjacent 16-byte short-name, 16-byte filename and 256-byte
parent buffers are retained: unchecked copies can reach neighboring
buffers. The path splitter and uppercase conversion execute before parent
selection. Only an exact uppercase `C:` parent forwards to the root writer.
Other parents require a signed positive directory lookup result and a
successful allocating directory read.

Within a subdirectory, a nonnegative file lookup result selects overwrite,
including index zero. Overwrite releases the old FAT chains, retains the
starting cluster and most record fields, sets modification time/date to
0x8edd/0x5836, and updates size. Creation prefers empty records over deleted
records, releases a reused deleted record's chain, and installs the original
8.3 name and fixed attributes/timestamps. A full directory frees its buffer
and returns zero without writing. Lookup and allocation failures return
zero with the original cleanup order.

Sector/cluster division occurs after record mutation. Arithmetic wraps at
32 bits. The first data transfer writes at most one cluster; every later
transfer writes an entire cluster, including data beyond EOF. Zero-length
files still retain an allocated cluster/end marker. Allocation failure,
exact 0xffff termination, cached FAT pointers, live BPB reads and FAT2
mirroring preserve the root writer's unchecked legacy behavior. The parent
descriptor's cluster is truncated to 16 bits for writeback. The entry writes
one live-BPB cluster of directory data, frees the directory buffer, flushes
FAT/root metadata, then returns the previously computed sector count.

## Directory-reader divide compatibility

The expanded tests exposed a compiler-dependent mismatch in the previously
recovered allocating directory reader at 0x827454. GCC could defer the
cluster-count division past the directory attribute check, suppressing an
original x86 divide fault when cluster size was zero. The directory/index
reader at 0x82711e had the same issue. Both computed counts are now volatile,
which preserves the original division before forcing directory counts to
one. The resulting bodies occupy 361/440 and 377/470 bytes respectively;
neither adds an interval or changes the interface.

## Differential validation

`tests/kernel_fat_write.py --installed` compares original x86, ordinary C,
and installed C in 846 scenarios / 2,538 executions across the splitter and
both writers. The 361 added scenarios exercise the absolute entry. Only
allocation/free and sector transfers are intercepted: path parsing,
directory lookup/read, string conversion, free-slot selection, cluster
release/allocation and metadata flushing execute real kernel code. The
ordinary variant uses original helpers; the installed variant uses the
complete production kernel and therefore also exercises recovered helpers.

Comparisons include returns and divide faults, ordered allocation/read/
write/free calls and transfer hashes, complete low memory, 2 MiB of fixture
memory, 2 MiB of allocation memory, caller arguments and preserved registers.
Cases include paths up to four directory levels, failure at every allocation,
index zero, empty/deleted/full records, file attributes, root forwarding,
invalid parents/drives/separators, overlapping local buffers, non-ASCII
names, varying BPB geometry, zero and wrapping sizes, exhausted clusters,
high parent-cluster words and geometry/global-pointer mutations. Mutations
at the real chain-release entry check that record timestamp/size writes
precede the subsequent divide fault. No successful lookup is supplied by
a test stub.

The expanded file-reader suite passes 2,422 scenarios / 7,266 executions,
including 12 added directory-attribute/divide-fault probes with checks that
allocation has not occurred. Focused installed regressions also pass FAT
path lookup, directory accessors, release/deletion, metadata flush, sector
writing, free-slot selection and X loading. No full `make verify` was run.

## Real QEMU disk writes and production boot

`tests/fat_write_path_guest.c` is a test-only X application temporarily
installed over COLOR.X in disposable copies. Both the original and C
kernels launch it through CMD using emulated PS/2 input. All filesystem,
allocator and ATA code executes in the guest without interception. The
runner verifies cached input metadata and entry/body execution in the
affected routines.

The fixture places a directory at root and a nested subdirectory within it.
It moves the independently specified root-write fixture records/data into
the nested directory and separately models the absolute entry's timestamp
updates and root forwarding. The diagnostic creates an 8,193-byte nested
file, overwrites the nested directory's index-zero file with one byte,
creates a zero-length nested file and writes a 513-byte root file through
the absolute entry. Missing paths, absent separators and a wrong drive
return zero. Absolute lookups after writing verify the resulting file sizes.
A deleted record remains unchanged because available empty slots take
priority. The existing root diagnostic separately exercises deleted-record
reuse and a three-cluster write.

Original and C nested-write results each equal their independent expected
33,554,944-byte disk, changing the same 17,958 bytes. The expected data
includes sectors written beyond EOF and retained data in a shortened file,
both FATs, the exact directory record fields, unchanged parent descriptors,
other files, slack and the VHD footer. Root-write regression also passes:
each complete disk matches its independent expectation and changes the same
41,415 bytes. QEMU writes temporary images only; source images are checked
unchanged afterward.

Production CLOCK boot, animation, window close/task exit and CMD reuse pass,
as do CMD/cpuid input, 128 MiB RAM/heap checks, real timer/TSS switches and
guest-memory equality for all 398 installed intervals. The original logo
is retained. Disk/app installers and inventory consistency checks pass.

    make -C rewrite build/OpenIceStarOS_c_apps.vhd \
      test-fat-write test-fat-files test-fat-path test-fat-directory \
      test-fat-mutation test-fat-metadata test-fat-io test-fat-slots test-x-loader
    make -C rewrite test-fat-write-path-qemu test-fat-write-qemu
    python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
      --clock-check --memory-check --task-check
    make -C rewrite inventory-check

Small summaries are committed in `../evidence/2026-10-31-fat-write-path/`.
Images, execution traces, memory dumps and screenshots remain in ignored
`build/`. These checks cover the specified fixtures and do not establish
arbitrary directory-chain or filesystem correctness. In particular, the
original empty-record helper scans the BPB root-entry count even in a
subdirectory; the writer writes back just its first cluster. Neither
behavior was repaired. General unchecked buffer overflow is not validated.

INIT/kernel remain partial, five applications are rebuilt and 32 program
paths remain unrecovered. The build still needs original machine code,
including the privileged ATA entries. The complete OS recovery is unfinished.
