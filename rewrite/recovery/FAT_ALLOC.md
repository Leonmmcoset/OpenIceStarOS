# FAT16 directory-index and bounded-cluster allocating readers

`kernel/fat_alloc.c` recovers two previously original-only service bodies:

| Address | Service index | Operation | Original slot bytes | Compiled C bytes |
| --- | ---: | --- | ---: | ---: |
| `0x82711e` | 95 | Allocate/read a record from an arbitrary directory base and signed index | 470 | 381 |
| `0x8272f4` | 96 | Allocate/read a requested number of clusters from a starting cluster | 352 | 286 |

The indexed reader rejects negative indices before touching BPB/record data.
Index zero is accepted; record addressing uses wrapping 32-bit `base + index*32`.
Allocation rounds file size upward using unsigned arithmetic, including its
original overflow behavior. Directory records allocate one cluster, but the
reader still follows the complete FAT chain. File size does not bound reads.

The bounded reader allocates `requested * sectors_per_cluster * bytes_per_sector`
with wrapping arithmetic. After successful allocation it always reads the
first cluster, even for a zero request. Further reads require both an exact
non-`0xffff` FAT link and an unsigned count below the request. Other end-of-chain
values, including `0xfff8`, retain their original treatment as cluster indices.

Both routines cache the FAT pointer and calculated data LBA after allocation,
read the initial next link before the first sector call, retain live BPB reads
for subsequent strides and sector counts, ignore hardware-read return values,
and return the allocation pointer. The root-sector calculation uses signed
division. The indexed reader can divide by zero before allocation; the bounded
reader first calls the allocator and returns immediately on allocation failure.
These compatibility behaviors are not independently hardened by this recovery.

## Source validation

```sh
python3 rewrite/tests/kernel_fat_files.py
```

The expanded fourteen-entry suite passes **2,410 scenarios / 7,230 executions**
against original x86, ordinary C and independently linked fixed-address C.
Only allocation, existing pathname lookups and hardware sector reads are
intercepted; the two new entries make no pathname lookup calls. FAT traversal,
geometry arithmetic, record selection, read bounds and return paths execute
as x86. All 39 original program paths exercise the indexed reader and six
requested-count configurations of the bounded reader using real FAT sectors.
Bounded reads check complete cluster bytes, including disk slack, and 32 bytes
past the last written cluster remain unchanged.

Synthetic cases independently check allocation sizes, signed negative-index
rejection, arbitrary/unaligned directory bases, index-address wraparound,
directory attributes, overflowing file sizes/counts, zero requests, unsigned
large requests, exact FAT termination, allocation failure, division faults,
ignored I/O status, and BPB/FAT/record/partition mutations across calls.
Ordered boundary arguments, return values, cdecl stack/register preservation,
and checked memory arenas are compared. Unbounded FAT cycles and arbitrary
unmapped pointers are excluded; the original routines can overrun undersized
allocations, so native fixtures provide mapped guarded storage without
claiming those original behaviors are safe for ordinary callers.

Preview evidence is in `../evidence/2026-10-29-fat-alloc/preview.json`.
The preview checkpoint was committed before production installation. The
kernel/INIT remain partial and no application is newly classified as rebuilt.

## Integrated validation

Both bodies are now installed at their original exported service addresses.
The kernel contains **368 declared intervals**. The installed fourteen-reader
test passes the same 2,410 scenarios / 7,230 executions and checks installed
bytes against its independent link. Absolute-path/name tests pass 347 / 1,041;
X-loader tests pass 430 / 1,290. Installed icon/bitmap file callers, exact
kernel/application disk checks, six branding tests, residual audit and
inventory validation also pass.

```sh
make -C rewrite build/OpenIceStarOS_c_apps.vhd \
  build/OpenIceStarOS_fat_diagnostic.vhd test-fat-files test-fat-path test-x-loader
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_fat_diagnostic.vhd \
  --fat-relative-check --memory-check --task-check
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_c_apps.vhd \
  --clock-check --fat-files-check --memory-check --task-check
```

The separate diagnostic X checks eight actual production service addresses and
16 calls against the original four-cluster `/INIT.X`, using real disk I/O and
PS/2 input. The new indexed reader runs against both the loaded root directory
and a copied directory record at another base, and rejects a negative index.
The bounded reader loads one, two and up to six requested clusters, stopping
at the actual four-cluster chain end. All **524,800 data/guard bytes**, X
relocations and allocation/free results pass. QEMU traces observe three entries
into each new function and execution inside both C bodies. The diagnostic is
installed only into a separately named image, replacing COLOR.X; it is never
classified as a recovered application or included in the production image.

Zero-count and directory-chain-overallocation behaviors are tested only in
mapped native fixtures. The real allocator can return a small allocation for
those cases, and the original reader can then write beyond it. The QEMU
diagnostic deliberately uses requests that contain their actual data writes.

The ordinary production image separately boots to the desktop and branded
CMD/cpuid, runs the C CLOCK with three changing complete frames, closes its
window through PS/2, terminates its task, and reuses CMD. RAM/heap, real TSS
switching and all 368 installed guest intervals pass. The new services have
**zero observed entries in this production workload**; the separate diagnostic
provides their runtime proof. Both boots use snapshots. The two committed PNGs
were captured directly by QEMU and visually inspected.

Exactly **786 bytes** differ from the previous 366-interval production image,
all within the two new slots. Restoring just those kernel bytes reproduces the
entire previous image byte for byte. Boot stages, application binaries,
FAT/directory structures, slack, the retained original logo and other resources
are unchanged. The original VHD's hash remains unchanged. Evidence is in
[`../evidence/2026-10-29-fat-alloc/`](../evidence/2026-10-29-fat-alloc/).

This is focused integration validation; a full 368-interval `make verify` was
not run. Only five applications are rebuilt; INIT/kernel are partial and 32
program paths remain unrecovered. Relative filename lookup, other directory
operations, hardware I/O, drivers and remaining applications still require
source recovery.
