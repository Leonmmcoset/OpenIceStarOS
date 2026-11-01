# Character baking and packed atlas pixel rendering

Ten additional original entries now execute `kernel/font_bake.c`, increasing
the installed kernel from 507 to 517 intervals. Six entries contain baking,
filter dispatch, packed-range rendering, range orchestration, range descriptor
construction and scaled metrics; four preserve the public font API forwarding
entries. INIT/kernel remain partial. Application recovery counts do not change.

| Original range (end exclusive) | Recovered behavior | Compiled / slot bytes |
| --- | --- | --- |
| `0x832a98..0x832dbe` | contiguous character baking and 20-byte metadata | 630 / 806 |
| `0x833bbc..0x833c94` | subpixel rendering and prefilter dispatch | 210 / 216 |
| `0x833c94..0x8342c4` | packed rectangle pixels, metadata and missing-glyph reuse | 1212 / 1584 |
| `0x8342e8..0x8345c2` | range initialization/allocation/gather/pack/render/release | 241 / 730 |
| `0x8345c2..0x834612` | single-range descriptor and forwarding | 59 / 80 |
| `0x834612..0x8346e2` | scaled vertical metrics | 181 / 208 |
| `0x836f70..0x836fbc` | public bake API | 37 / 76 |
| `0x836fbc..0x836fd6` | public font collection offset API | 16 / 26 |
| `0x836fd6..0x836fea` | public font count API | 13 / 20 |
| `0x836fea..0x83700c` | public font initialization API | 19 / 34 |

Baking initializes a real local SFNT context, returns -1 on initialization
failure, clears width*height DWORDS and places characters with a one-pixel
margin. It wraps rows on **>=** width and rejects a glyph on **>=** height,
returning the negative number of characters already processed (zero if the
first glyph fails). Successful output records keep the four uint16 texture
coordinates, scaled advance and unscaled integer pixel offsets. Empty glyphs
still consume their one-pixel margin. All uint32 products/sums wrap.

Packed rendering saves the live oversampling context, loads per-range bytes,
computes real scales/reciprocals/shifts, and handles packed nonempty rectangles
through actual glyph metrics, pixel bounds, shape/flatten/raster calls. Padding
mutates the input rectangles before drawing. The 28-byte packed-char output
contains texture coordinates, advance, shifted offsets and far offsets. Empty
rectangles may reuse the seven DWORDS of the most recent missing glyph. That
missing **index persists across ranges**, but the original still reads the
**current range's** packed-char array at that index. The recovered C preserves
this defect and the forward overlapping-copy order. The context's previous
oversample values are restored on a normal return.

Range orchestration zeros each initial char rectangle in reverse uint16 store
order, allocates total_count*24, ignores the original font-init return just as
the binary does, then gathers, packs, renders and releases the temporary rects.
The single-range API builds the original 24-byte range descriptor. Scaled
metrics choose height scaling for ordered-positive sizes and EM scaling for
the sign-flipped float word otherwise, then write all three output floats in
order with live pointer reloads. The public offset API retains the original
negative TTC index behavior (index -1 can read the collection's count).

## Validation

`make -C rewrite test-font-bake` passes **1,777 full-return scenarios / 5,331
executions**, plus **72 filter-entry checkpoints / 216 executions**, comparing
original x86, ordinary C and installed C. All SFNT, cmap, metrics, shape, heap,
flattening, sorting and raster helpers execute; none are stubbed. Independent
oracles check baked row layout, failure results, metadata, packed coordinates
and every visible/padding byte. Complete touched data/heap pages, caller words,
stack balance, preserved registers and x87 control are compared. Coverage
includes six x87 controls, both incoming stack alignments, zero/empty glyphs,
missing glyphs across ranges, boundaries, allocation failures, flags, output
overlap, live caller aliases and sampled real Fira ASCII pipelines.

The higher prefilter entry and packed renderer still call original horizontal
`0x833292` and vertical `0x83357c` filters for oversampling above one. Those
filters incorrectly DWORD-clear small original stack arrays, overwriting saved
state/return addresses. Normal-return tests use oversampling one, or omitted
rectangles that bypass filters. The 72 additional tests run actual oversampling
2/3/4 paths up to the **real filter entry**, then compare the complete pre-entry
data/heap state and all five filter arguments. They stop before the broken
filter executes; they are not counted as normal-return equivalence. The
filters are not silently replaced with corrected library code.

`make -C rewrite test-font-bake-qemu` launches a diagnostic X via real CMD/PS2
in original/recovered guests. An independently generated report checks font
count/offset/init, positive-height and negative-EM scaled metrics, two bake
entries, supplied-buffer prefilter dispatch, direct packed rendering, multi/
single-range orchestration, all emitted metadata and full pixel-buffer hashes.
All ten entry/body traces are required and complete source/disposable disks
remain unchanged. The diagnostic X is not counted as a rebuilt application.

The supplied-buffer fixture has width8 but six filled pixels; its real raster
driver copies 32 bytes per row, including 26 zero bytes. Other fixtures have
six-wide glyph output and copy 24 bytes per row. Hash expectations model each
actual declared scanline width independently. Baked/packed stride16 rows
overlap the 24-byte copy, matching the original overcopy behavior.

Focused installed atlas/render/flatten/scan/shape/outline/heap/context/metrics/
parse/init/X-loader, branding and inventory checks accompany complete kernel/
app installation bounds and production COLOR/input/cursor/RAM/TSS boot tests.
Production verifies all 517 loaded intervals and COLOR's 600 palette pixels,
close, task exit and CMD reuse. Unedited QEMU screenshots were visually
inspected; original logo retained. Evidence: `../evidence/2026-11-01-font-bake/`.

The original scan driver and its arbitrary-width/negative-width faults remain.
Malformed font-init failure paths in higher APIs, huge/wrapped writes, arbitrary
unmapped pointers and execution past the broken filters are outside this
validation. Type-2 interpretation and other font operations remain original.
Full `make verify` was not run for 517 intervals; its last complete recorded
run is 456 intervals. Five apps are rebuilt, 32 program paths remain unrecovered,
and the complete OS rewrite remains unfinished.
