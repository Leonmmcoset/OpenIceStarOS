# Font rasterizer object arenas

Three original functions now execute `kernel/font_heap.c` in the 472-interval
kernel. The arena implementation is C; other rasterizer functions still need
recovery. The interval count is not an OS recovery percentage.

| Entry | Function | Compiled / slot bytes |
| --- | --- | --- |
| `0x830b4c` | allocate or reuse arena object | 118 / 194 |
| `0x830c0e` | put object on free list | 19 / 26 |
| `0x830c28` | free arena chunks | 31 / 50 |

Allocation first pops a free object, then uses remaining objects in the current
chunk. A new chunk has 2,000 objects for sizes below 32, 800 for sizes 32..127,
and 100 for larger unsigned sizes. Chunk size and object addresses use the
original unchecked wrapping 32-bit arithmetic. The real recovered kernel
malloc/free entries allocate and release the chunks; no allocator stubs are
used. Allocation failure leaves the descriptor unchanged. Free-list push/pop
and remaining-count accesses preserve the original live descriptor reads,
including overlapping object/descriptor fields.

Cleanup caches the next chunk before freeing the current one and leaves the
arena descriptor stale, matching the original. It does not repair the kernel
allocator's existing coalescing behavior: the three-adjacent-chunk fixture
retains the original stale forward link after cleanup. The unused userdata
parameters retain their ABI positions and have no effect.

`make test-font-heap` passes 184 scenarios / 552 executions across original
x86, ordinary C and installed C. It executes real malloc/free, compares all
touched data/heap-header pages and caller state, and checks count thresholds,
empty/existing chunks, allocation failure, wrapped sizes/counts, free-list
aliases and zero-to-three-chunk cleanup. Independent expected object addresses,
counts and heap-header effects supplement the original/C comparisons.

`make test-font-heap-qemu` uses real guest allocation to check all three chunk
size tiers, decreasing counts, free-list reuse and cleanup through original/C
kernels. Reports match fixed expectations, execution traces cover all three
installed bodies and complete temporary/source disks remain unchanged. The
small diagnostic X is not a rebuilt production application.

Evidence is in `../evidence/2026-10-31-font-outline-heap/`, alongside the seven
outline/bounds functions installed in the same step. Focused original/C tests,
whole-disk installation checks, inventory consistency and production boot are
recorded there. Full `make verify` is established for 456 intervals separately,
not for this 472-interval checkpoint. The complete font renderer, kernel and
OS are unfinished; five applications are rebuilt and 32 program paths remain
unrecovered. The original logo is retained.
