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

## Production integration and QEMU

The production manifest installs all six C bodies at their original ABI
addresses, increasing the declared kernel intervals from 358 to 364.
`make test-fat-files test-x-loader` passes in installed mode: all 1,926 FAT
scenarios / 5,778 executions, plus 430 X-loader scenarios / 1,290 executions.
Installed bytes match an independent fixed-address link, and every new body
fits its original slot. The installed icon-file and bitmap-file differential
checks, exact kernel/application disk checks, six branding tests, residual
audit and inventory validation pass.

```sh
make -C rewrite build/OpenIceStarOS_c_apps.vhd test-fat-files test-x-loader
python3 rewrite/tools/boot_smoke.py \
  rewrite/build/OpenIceStarOS_c_apps.vhd \
  --clock-check --fat-files-check --memory-check --task-check
make -C rewrite build/OpenIceStarOS_fat_diagnostic.vhd
python3 rewrite/tools/boot_smoke.py \
  --fat-relative-check --memory-check --task-check
```

The ordinary production image boots and passes CMD/cpuid, 128 MiB RAM/heap,
real TSS switching, changing CLOCK frames, PS/2 close, task exit and CMD reuse.
All 364 installed intervals match guest memory. This ordinary boot does not
call the six new relative entries, so it is not their runtime execution proof.

`tests/fat_relative_guest.c` is a **test-only X application**, installed only
in the separate `OpenIceStarOS_fat_diagnostic.vhd` image's COLOR.X extents.
It loads through CMD and the normal X loader, calls the unchanged service-table
slots 89..94 and uses real FAT and disk-controller I/O to read `/INIT.X`.
It checks both whole-file entries, both allocating entries and both single-
cluster entries, including the distinct past-EOF and negative-offset behavior.
The host compares 328,000 buffer/guard bytes independently with all four
8192-byte on-disk INIT clusters. Both allocations are copied and freed by the
guest. The diagnostic's own relocations and original service addresses are
verified. It changes no shared FAT/service tables and contributes no rebuilt
application to the recovery inventory.

The real QEMU execution trace observes entry and C-body blocks for every new
routine: whole-chain 4, named whole-chain 1, indexed cluster 3, named cluster
3, named allocation 1 and indexed allocation 1. Ten top-level diagnostic calls
include internal whole-chain calls from the wrappers. The diagnostic image's
kernel is exactly the production kernel. Both QEMU runs use snapshots and
leave the original and production disks unchanged.

Byte comparison against the 358-interval baseline confirms exactly 1,532
changed bytes, all inside the six new kernel intervals. Boot stages, all
applications, original logo/resources, FAT/directory structures and disk slack
are identical. The source and installed C tests include the existing six
record/header readers, whose source remains unchanged.

Evidence and inspected production CMD/CLOCK screenshots are preserved in
[`../evidence/2026-10-29-fat-relative/`](../evidence/2026-10-29-fat-relative/).
This is a focused integration checkpoint, **not a full 364-interval `make
verify` regression**. There remain five rebuilt applications, two partial
programs and 32 unrecovered program paths. Neither these six functions nor
the diagnostic complete the remaining filesystem, drivers or OS recovery.
