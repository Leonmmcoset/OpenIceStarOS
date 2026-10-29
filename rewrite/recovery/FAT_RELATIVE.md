# FAT16 relative-reader recovery

`kernel/fat_relative.c` recovers six previously original-only kernel bodies:

| Address | Operation | Original slot bytes | Compiled C bytes |
| --- | --- | ---: | ---: |
| `0x826ade` | Read a loaded-root record's whole FAT chain | 348 | 316 |
| `0x826c3a` | Find a relative name and read the whole chain | 52 | 29 |
| `0x826c6e` | Read one cluster by directory index and chain offset | 354 | 311 |
| `0x826dd0` | Find a relative name and read one chain-offset cluster | 360 | 316 |
| `0x826f38` | Find a relative name, allocate and read | 250 | 198 |
| `0x827032` | Allocate and read by loaded-root directory index | 236 | 172 |

The record address is `0x520000 + (reserved + fats * sectors_per_fat) *
bytes_per_sector + index * 32`, with 32-bit wrapping address arithmetic.
The whole-file entry returns the number of clusters read, while the allocating
entries return the allocation address. They retain the original exact `0xffff`
terminator, ignored disk status, cached FAT pointer/data LBA, and live BPB
geometry and FAT reads. Relative data-LBA rounding uses signed division; the
allocation size uses the original unsigned wrapping ceiling calculation and
does not special-case directory attributes.

Important original differences are preserved:

- The direct whole-chain reader has no negative-index or null-buffer gate.
- Its named wrapper only reads for a signed lookup result greater than zero;
  lookup zero or negative is returned unchanged.
- Both single-cluster entries require a positive directory index and return
  the number of FAT links traversed, even if no read occurs.
- The indexed single-cluster entry reads only when the requested offset was
  reached. Negative offsets and offsets past EOF cause no disk read.
- The named single-cluster entry reads the last reachable cluster on past-EOF
  requests, and the first cluster for negative offsets. The destination is
  advanced by the number of links actually traversed.
- Both allocating entries accept directory index zero, reject negative
  indices, and still divide before observing allocation failure.

These are compatibility behaviors of the original machine code, including its
unsafe inputs; they are not new filesystem policy.

## Source checkpoint

`python3 tests/kernel_fat_files.py` compares original x86, ordinary C and
fixed-address C for the six new entries together with the six previously
recovered record/header readers. It passes **1,926 scenarios / 5,778 executions**.
The original six C bodies are unchanged. Only allocator, pathname lookup and
hardware sector-read boundaries are intercepted; internal calls and cluster
traversal execute as real x86 in all three variants.

All 39 original `.X`/`.DX` paths contribute real BPBs, FAT chains, directory
records and sectors. Each complete file is checked independently against FAT
extraction. Single-cluster reads check first/middle/last/past-EOF/negative
offsets and independently compare the selected disk cluster and destination.
Synthetic cases check zero/negative indices and lookup results, unusual FAT
IDs, exact-end semantics, allocation and read failures, null destinations,
size/address wrapping, geometry changes, cached-state mutations, cdecl
registers/stack, and division faults. Fault cases use fresh Unicorn instances.
Unbounded cyclic FAT chains and arbitrary unmapped pointers are not covered.

Preview evidence: `build/fat_relative_preview.log` and
`build/fat_files_summary.json`. This source checkpoint precedes production
installation. The underlying relative/path lookup, controller I/O and the
remaining filesystem implementations are still original code. Full OS source
recovery remains unfinished.
