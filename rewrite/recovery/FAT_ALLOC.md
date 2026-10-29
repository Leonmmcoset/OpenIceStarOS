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
This source checkpoint precedes production installation. The kernel/INIT
remain partial and no application is newly classified as rebuilt.
