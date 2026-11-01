# Font curve subdivision and contour flattening

Four additional original entries now execute `kernel/font_flatten.c` in the
485-interval kernel. No diagnostic X or branding patch is counted as a rebuilt
application. The kernel and INIT are still partial.

| Entry and end (exclusive) | Recovered behavior | Compiled / slot bytes |
| --- | --- | --- |
| `0x831ba8..0x831bdc` | write one two-float point | 41 / 52 |
| `0x831bdc..0x831d64` | recursively subdivide a quadratic curve | 381 / 392 |
| `0x831d64..0x832000` | recursively subdivide a cubic curve | 579 / 668 |
| `0x832000..0x83244c` | count, allocate and expand contours into polylines | 1058 / 1100 |

The original cdecl coordinates occupy 32-bit stack words. C represents copied
float arguments as integer bit patterns so a signaling NaN endpoint is not
quieted merely by copying it. Arithmetic interprets those same words as floats.
Point writes reload pointer/index/Y after the first store. Recursive halves
reread live caller arguments, and the endpoint increment reloads the count
pointer after writing the point, including when that write changes the pointer.
Point counts and addresses retain unsigned 32-bit wrap.

Quadratics split at the midpoint when the squared deviation from the endpoint
midpoint exceeds the supplied squared tolerance. Cubics compare squared control
polygon length minus squared endpoint chord length against that tolerance. The
four square roots use the real kernel helper. Float32 stores for coordinate
differences, midpoints, lengths and errors, plus the two intermediate double
stores in cubic length summation, preserve the original x87 precision boundaries.
Ordered comparisons retain the original unordered/NaN branch behavior.

Both recursive functions compute their intermediates before testing the depth.
Depth greater than 16 drops the segment without appending its endpoint. The
quadratic function nevertheless returns one, including at that cutoff. A count
pass with a null point buffer still increments counts for accepted segments.

Flattening first counts move vertices and allocates one length per contour. It
then traverses move, line, quadratic and cubic vertices twice: once to count
points, and once to write the allocated point array. Unknown vertex kinds are
ignored. No-contour input leaves the lengths pointer unchanged. First-allocation
failure clears the contour count. Second-allocation failure calls the original
free wrapper on the null point pointer and allocated lengths, then clears both
outputs. Original allocation and free/merge logic execute in the tests.

Two internal C helpers occupy spare space inside these replaced slots: endpoint
append/increment follows the cubic body, and cubic error calculation follows the
flattening body. They introduce no new original entry or replacement interval.
The linker fixes all four public entry addresses, checks section sizes and
rejects unexpected allocated data. Quadratic compilation disables GCC temporary
expression substitution to fit its original slot; explicit float spills define
the precision boundaries. Normal and installed differential builds use the same
source attributes and flags. No original machine-code inclusion implements these
four functions.

## Validation

`make -C rewrite test-font-flatten` passes **7,041 scenarios / 21,123 executions**
comparing original x86, ordinary C and installed C. It compares every touched
data/heap page, caller words, preserved registers, stack balance and x87 control.
The suite includes both incoming stack alignments, six x87 controls, signed
zero, infinities, signaling/quiet NaN payloads, depth 16/17, null count passes,
wrapped point indices/counters, data/caller aliases and both allocation failures.

Independent expectations use exact rational Bernstein polynomials for quadratic
endpoints, a cubic midpoint, and direct line/contour geometry. A separate TrueType
table parser supplies **all 4,638 simple Fira outlines** to the two-pass flattener.
Compound outline decoding remains covered by the separate shape suite, including
its observed original copy/heap defects; this test does not claim successful
flattening of corrupted compound heaps or exhaustive malformed-input safety.

`make -C rewrite test-font-flatten-qemu` launches a diagnostic X through actual
CMD/PS2 in original and recovered guests. Both reports agree on raw point words,
quadratic/cubic coordinates, cutoff counts, two-contour lengths and points.
It also calls the original raster entry, edge builder and raster driver on a
rectangle: **all 128 output bytes** match independent expectations, including
24 cleared/copied padding bytes per row. Height four selects fifteen vertical
samples, each weighing `floor(255/15)=17`, so interior coverage is 255. Traces
show execution of all four installed entries and bodies, including during normal
CMD text rendering. Entire disposable and source disks remain unchanged.

Focused installed regressions cover scan/sort, shape, outline/bounds, font heap,
CFF context, X loading and branding. Whole-disk installation checks and inventory
consistency pass. Production QEMU checks cover CLOCK's changing hands and labels,
PS/2 close, task exit/CMD reuse, CMD/cpuid input, cursor callbacks, 128 MiB RAM,
timer/TSS switching and all 485 installed intervals against guest memory. The
unaltered CLOCK and CMD screenshots were visually inspected.

Evidence: `../evidence/2026-11-01-font-flatten/`. These are focused checks; **full
`make verify` was not run for 485 intervals**. The last complete full regression
remains the separately committed 456-interval run.

The raster driver, edge builder, higher raster entry, CFF Type-2 interpreter and
other kernel logic still contain original machine code. Four drivers and 26
distinct applications remain unrecovered; five applications are rebuilt. The
complete OS rewrite is unfinished, and the original logo is retained.
