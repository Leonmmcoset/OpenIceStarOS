# Font active edges, scanline coverage and edge sorting

Five additional entries now execute `kernel/font_scan.c` at their original
addresses in the 481-interval kernel. These counts are not completion ratios.

| Entry | Function | Compiled / slot bytes |
| --- | --- | --- |
| `0x830c5a` | initialize a fixed-point active edge | 335 / 362 |
| `0x830dc4` | nonzero-winding scanline coverage | 286 / 352 |
| `0x831340` | stable edge insertion sort | 142 / 320 |
| `0x831480` | edge quicksort partitioning | 509 / 984 |
| `0x831858` | quicksort then insertion sort | 35 / 44 |

Active edges use the real recovered arena allocator and kernel malloc. The
original slope calculation is performed even when allocation fails. The float32
slope store, x87 floor calls, signed 32-bit conversions, wrapping 10-bit fixed
point offsets, raw end-Y bits, zero next pointer and normalized winding sign
are preserved. Negative slopes use the original negated floor of the positive
magnitude, producing truncation toward zero in fixed point. Volatile source
reads retain the effects of arena objects overlapping the input edge.

Scanline filling accumulates the original wrapping 32-bit nonzero winding and
fills each closed span. Boundary coverage uses ten fractional bits and signed
arithmetic shifts after wrapping multiplication. Interior pixels add the weight
directly; all byte additions wrap rather than saturate. Left/right clipping,
same-pixel spans, zero winding contributions and unmatched final crossings keep
the executable's behavior. The active-list argument itself advances in the
caller's stack area and ends at zero. It is reread after output writes; output
that overlaps a later edge therefore changes that edge's subsequent X read.
No test replaces the linked allocator, floor or other nested helpers.

All sorting entries move complete 20-byte records without converting stored
float payloads. Ordering compares only the start-Y float. Insertion sort is
stable for ordered equal keys. Quicksort retains the median selection,
partition permutations, smaller-side recursion and 12-element cutoff; it
intentionally leaves small partitions for the insertion pass. The iterative
tail partition updates the original caller's array/count slots. Explicit GCC
frame access is restricted to these original ABI writes and the scanline's
live active-list argument. Quiet/unordered comparisons do not create a new
NaN ordering, and signaling/quiet NaN bit patterns survive record moves.

## Validation

`make -C rewrite test-font-scan` compares original x86, ordinary C and installed
C across 2,959 scenarios / 8,877 executions. It compares complete touched data
and heap pages, caller arguments, preserved registers, stack balance and x87
control. The suite covers both incoming stack alignments modulo eight, six
x87 precision/rounding controls, signed zero, infinities, NaNs, raw signaling
NaN payloads, allocator failure/reuse, source/output aliases, signed count
limits, wrapped fixed-point offsets, winding overflow, byte coverage wrap,
cutoff boundaries and exact record permutations.

Independent expectations compute per-pixel intersection lengths, active-edge
coordinates, stable insertion order and sorted record permutations. The
interval oracle works directly on pixel/span intersections instead of copying
the executable's edge traversal and clipping branches.

`make -C rewrite test-font-scan-qemu` loads one diagnostic X through actual
CMD/PS2 in original/C guests. It checks an arena-allocated sloping edge, packed
scanline bytes, insertion/partition/full-sort results, then calls the real
original raster driver on a rectangle with fractional X boundaries and four
vertical samples. All 128 output bytes match independent expectations. The
interior weight is `4 * floor(255/4) = 252`; each 3/4-covered boundary is
`4 * floor(63 * 3/4) = 188`. Execution traces cover all five installed entry
points/bodies and the original driver, including ordinary font drawing while
CMD runs. Entire disposable and source disks remain unchanged. The diagnostic
X is a test fixture and does not increase the rebuilt-app count.

The driver at `0x830f24..0x831340` itself remains original. Its clear/copy helpers
take DWORD counts even though the bitmap width is also used as the visible
pixel count. The QEMU rectangle uses width 8 and a 32-byte stride, checks the
24 cleared/copied padding bytes of every row, and retains that observed
behavior. This does not establish arbitrary-width or malformed-driver safety.
The edge-building driver, curve flattening, Type-2 interpreter and the complete
font renderer still require source recovery.

Evidence is in `../evidence/2026-11-01-font-scan/`: focused installed regressions,
original/C QEMU results, whole-disk installation checks and production COLOR,
CMD/cpuid, cursor, RAM and timer/TSS boot checks. This is not a complete
`make verify` for 481 intervals; the last complete full regression remains the
separately recorded 456-interval run. INIT/kernel remain partial; five apps are
rebuilt and 32 program paths remain unrecovered. The original logo is retained.
The complete OS rewrite is unfinished.
