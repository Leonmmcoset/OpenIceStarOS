# Font atlas contexts, rectangle layout and drawing quads

Eleven entries now execute `kernel/font_atlas.c`; 507 kernel intervals are
installed. INIT/kernel remain partial. No application recovery status changes.

| Range (end exclusive) | Behavior | Compiled / slot bytes |
| --- | --- | --- |
| `0x832dbe..0x832f44` | baked character quad and advance | 366 / 390 |
| `0x832f44..0x832f78` | shelf context initialization | 52 / 52 |
| `0x832f78..0x8330f6` | signed/wrapping shelf rectangle packing | 246 / 382 |
| `0x8330f6..0x83323c` | two real heap allocations, context and pixel initialization | 262 / 326 |
| `0x83323c..0x833260` | release nodes then packer; stale descriptor retained | 31 / 36 |
| `0x833260..0x833284` | independent unsigned oversample limits | 36 / 36 |
| `0x833284..0x833292` | skip-missing flag | 14 / 14 |
| `0x8338c8..0x8338fa` | oversample shift with float32 spill and x87 return | 46 / 50 |
| `0x8338fa..0x833bbc` | real cmap/scaling/bounds across ranges | 476 / 706 |
| `0x8342c4..0x8342e8` | context-to-packer forwarding | 26 / 36 |
| `0x8346e2..0x83488c` | packed character quad and advance | 394 / 426 |

Rectangles are 24-byte records with coordinates at 0/4, dimensions at 12/16
and the packed flag at 20. The packer uses signed comparisons of wrapping
32-bit sums, moves to the prior bottom when width overflows, and clears all
remaining flags after the first height overflow. It retains the original
behavior for too-wide, zero and negative rectangles. Caller/context/record
aliases retain live reads and original store order.

Pack begin attempts both allocations before checking either result: a 20-byte
packer and **width-padding bytes**, not a multiplied node structure. Failure
releases each nonnull allocation without writing the descriptor. Success
initializes all 44 descriptor bytes in original order, uses width when stride
is zero, and calls the actual REP helper with width*height **DWORDS**. Tests
allocate and inspect the complete 4*width*height clear extent plus a sentinel.
Pack end frees nodes before rereading and freeing the packer; fields stay stale.

Gather selects line-height scaling for ordered-positive sizes and EM scaling
for the sign-flipped raw float word otherwise. It stores oversampling low
bytes in each range, handles contiguous and explicit codepoints through the
real cmap, and adds padding plus oversample-minus-one to real pixel bounds.
The first missing glyph may reserve space; later missing glyphs are omitted
across all ranges. Skip-missing suppresses even that first reservation. Empty
existing glyphs still retain padding. Unspecified rectangle fields stay intact.

Baked and packed quads preserve float32 reciprocal stores, double floor-call
arguments, x87 arithmetic and wrap before signed integer-to-float conversion.
Baked quads use the original half-pixel bias; packed quads support aligned
and unaligned coordinates. Texture coordinates and the final X advance use
live inputs, including sources overlapping output structures. The oversample
shift returns its original signed arithmetic and rounded float32 value in ST0.

## Validation

`make -C rewrite test-font-atlas` passes **2,684 scenarios / 8,052 executions**
against original x86, ordinary C and the installed C kernel. The row-layout,
quad/texture, missing-glyph dimensions, allocation/descriptor and pixel-clear
oracles are independent of the C implementation. Every touched data/heap
page, caller word, stack balance, preserved register and x87 control is
compared. Six x87 controls, both incoming stack alignments, raw32/raw80
float returns, NaNs/infinities, signed/wrapped dimensions, aliases, allocation
failures and sampled real Fira ranges are covered. No helper is stubbed.

`make -C rewrite test-font-atlas-qemu` launches a diagnostic X from real CMD
and PS/2 in original/recovered guests. It compares complete independent reports
for allocation/clear extent, oversample acceptance/rejection, missing glyphs,
row overflow, four quad modes and descriptor fields after release. Execution
traces cover all eleven entries and bodies; complete source/disposable disks
stay unchanged. This diagnostic is not counted as a rebuilt app.

Focused rendering, flatten, scan, shape, outline, heap, context, metrics,
X-loader, branding and inventory regressions and complete disk installation
checks accompany normal CLOCK/input/RAM/TSS boot checks. CLOCK face/labels,
three changing frames, PS/2 close, task exit and CMD reuse pass; screenshots
are actual QEMU captures, visually inspected. The original logo remains.
Evidence: `../evidence/2026-11-01-font-atlas/`.

These bounded tests do not validate malformed/unmapped pointers, unbounded
counts or enormous pixel writes. The original scan driver and broken
oversampling filters are still present. Glyph baking and packed atlas pixel
rendering are not claimed recovered by this step. Full `make verify` was not
run for 507 intervals; its last complete recorded run is 456 intervals.
Five apps are rebuilt, 32 program paths remain unrecovered and the complete
OS rewrite remains unfinished.
