# Font edge construction and glyph bitmap rendering

Eleven original entries now execute `kernel/font_render.c`, increasing the
installed kernel to 496 intervals. The kernel and INIT remain partial.

| Original range (end exclusive) | Recovered behavior | Compiled / slot bytes |
| --- | --- | --- |
| `0x831884..0x831ba8` | orient, transform and sort contour edges | 500 / 804 |
| `0x83244c..0x832530` | flatten, rasterize and release an outline | 192 / 228 |
| `0x832530..0x832544` | bitmap release | 14 / 20 |
| `0x832544..0x832700` | allocate/render a subpixel glyph bitmap | 361 / 444 |
| `0x832700..0x832756` | allocate/render a glyph without subpixel shifts | 38 / 86 |
| `0x832756..0x832852` | render a subpixel glyph into supplied storage | 180 / 252 |
| `0x832852..0x8328a8` | render a glyph without shifts | 41 / 86 |
| `0x8328a8..0x83290c` | allocate/render a subpixel codepoint bitmap | 91 / 100 |
| `0x83298a..0x8329ec` | render a subpixel codepoint into supplied storage | 94 / 98 |
| `0x8329ec..0x832a42` | allocate/render a codepoint without shifts | 38 / 86 |
| `0x832a42..0x832a98` | render a codepoint without shifts | 41 / 86 |

Edge construction closes every contour from last point to first, omits horizontal
segments, preserves unordered float comparisons, chooses winding according to
the original inversion flag and applies scale/shift before the float32 stores.
It selects fifteen vertical samples for bitmap heights below eight, five
otherwise. The recovered quicksort/insertion pipeline sorts the complete edge
records, and the original scan driver consumes them and writes its sentinel.
Equal-Y order after quicksort is not assumed stable by the independent oracle.

The raster entry chooses the smaller scale with the original NaN comparison
behavior, divides the flatness by that scale, calls the recovered two-pass
flattener, builds edges and releases the temporary contour/point arrays. Bitmap
allocation first decodes the glyph, then applies the original zero-scale fallback:
the nonzero axis replaces the zero axis in the **caller's argument slots**. If
both axes are zero it frees the outline and returns zero without touching output
dimensions. Otherwise it computes subpixel bounds, writes optional dimensions
and offsets in order, allocates width*height, renders and releases the outline.
Supplied-buffer rendering retains its original distinction: it computes only
the required offsets and does not apply that scale fallback.

Float arguments are represented as raw cdecl words to preserve exact forwarding
and live aliases. Dimension/offset writes may change later pointers or scale
words; the C implementation rereads them. Address products and dimension
differences retain 32-bit wrapping behavior. No shape, allocator, flattener,
sorter, scanline filler or scan-driver boundary is stubbed in differential tests.

## Validation

`make -C rewrite test-font-render` passes **2,568 scenarios / 7,704 executions**
against original x86, ordinary C and installed C. Every touched data/heap page,
caller word, preserved register, stack balance and x87 control is compared.
Coverage includes six x87 controls, both stack alignments, transforms/inversion,
horizontal/degenerate/multiple contours, input/output aliases, signed contour
limits, the height-eight sampling threshold, scale fallback, null/overlapping
output pointers, empty glyphs, bounded negative-scale supplied-buffer rendering,
allocation failures and sampled real simple Fira glyph bitmap pipelines.

Independent checks compare oriented/transformed polygon-edge multisets and sorted
Y coordinates, rectangle coverage, all row padding and caller scale writeback.
`make -C rewrite test-font-render-qemu` calls all eight glyph/codepoint allocation
and supplied-buffer APIs through a diagnostic X launched by real CMD/PS2. Original
and recovered guests agree on independently expected dimensions, offsets and
pixel signatures. Traces cover all eleven installed entries and bodies and the
original scan driver. Entire diagnostic and source disks remain unchanged.

The scan driver at `0x830f24..0x831340` remains original. Its REP helpers copy
and clear **DWORD counts** even though width also denotes pixels. Consequently
allocated glyph rows use stride width but copy 4*width bytes; the guest fixture
checks its 24 visible bytes and 18 additional zero bytes, and supplied-buffer
checks include the 24 padding bytes per row. These tests preserve that behavior.
They do not establish arbitrary-width safety: sufficiently wide stack scanlines
overwrite original driver locals, and negative allocated widths can fault.
Negative-scale allocation coverage uses empty outlines; nonempty negative-scale
draws are exercised with a bounded supplied buffer. The malformed-input domain
is not claimed equivalent or safe beyond the documented cases.

The later oversampling filters also call DWORD clear on small original stack
arrays, overwriting saved state/return addresses. They are still original and
are not silently replaced with corrected library implementations.

Evidence: `../evidence/2026-11-01-font-render/`. Focused installed font, heap,
X-loader and branding regressions, whole-disk installation/inventory checks and
production boot checks are recorded there. Full `make verify` was not run for
496 intervals; the last complete full regression is the 456-interval run.
Five applications are rebuilt, 32 program paths remain unrecovered, and the
complete OS rewrite remains unfinished. The original logo is retained.
