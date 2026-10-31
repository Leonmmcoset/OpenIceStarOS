# Font metrics, scaling and SVG document access

Nine original functions now execute C from `kernel/font_metrics.c`. The
kernel installs 439 intervals, including 77 exact instruction intervals.
These counts are not OS recovery percentages.

| Function | Compiled / slot bytes |
| --- | --- |
| `font_hmetrics` | 162 / 248 |
| `font_vmetrics` | 100 / 118 |
| `font_typo_metrics` | 127 / 170 |
| `font_bounds` | 103 / 130 |
| `font_scale_height` | 65 / 78 |
| `font_scale_em` | 37 / 46 |
| `font_svg_record` | 93 / 160 |
| `font_glyph_svg` | 80 / 132 |
| `font_codepoint_svg` | 31 / 50 |

## Recovered behavior

Horizontal metrics use the original signed 16-bit advance values and
signed glyph comparison with the hhea metric count. Glyphs past the last
full metric reuse its advance and read the trailing bearing array. Negative
glyphs and zero metric counts retain original unchecked pointer arithmetic.
Optional outputs are written in order, with live font pointers/table fields
reread for later outputs. Tests include outputs aliasing those fields.
Vertical metrics, OS/2 typo metrics and global head bounds preserve signed
values. OS/2 lookup returns zero and leaves outputs alone when absent.
The global bounds entry does not check its four output pointers.

Height scaling divides by signed ascent minus descent. EM scaling divides
by unsigned head units-per-EM. Floating results return through x87 ST(0),
including original zero divisors, infinities, NaNs and subnormal inputs.
No clamping or replacement error values are added.

SVG record selection searches inclusive glyph ranges in order, using the
original cached/list-relative offsets. The record helper reads/cache-resolves
the table even when the cache is zero; the document helper instead returns
zero before lookup when that cache is zero. It leaves the output untouched
on misses. The codepoint wrapper calls the still-original cmap lookup and
then the recovered glyph document helper. Output aliasing and live/cache
behavior remain observable.

## Validation and limits

`tests/kernel_font_metrics.py --installed` passes 1,098 scenarios / 3,294
executions across original x86, ordinary C and installed C without stubbing
any helper. Ordinary C calls original parser entries; installed C uses the
complete production kernel's recovered parsers. Comparisons include all
touched data pages, return values, preserved registers and unchanged caller
arguments. Void entries' volatile-register values and local stack scratch
are excluded. Scaling compares raw 80-bit x87 results and 32-bit conversions
under six control words: default, 24/53-bit precision and all rounding
modes. Inputs include finite/zero/negative heights, subnormals, infinities,
quiet/signaling NaNs and zero/reversed/extreme metric denominators.

Synthetic cases exercise optional/aliased outputs, signed metric/glyph
boundaries, OS/2 presence, SVG range endpoints and cache states. The real
Fira font supplies independent metric/bounds/scale expectations directly
from its SFNT table bytes, including both full and trailing hmtx records.

`tests/font_metrics_guest.c` loads/initializes the shipped Fira font through
real original kernel APIs, then calls all nine entries at their original
addresses. Host-parsed Fira table data independently specifies vertical,
typographic and horizontal metrics, bounds and scaling results. A specified
SVG list plus cmap format 0 checks glyph and codepoint document access,
range misses and untouched output on zero-cache return. Both original and
C QEMU guests match the expected report, execute every entry/body and leave
their entire 33,554,944-byte temporary disks unchanged. Source images are
checked unchanged; test-only diagnostics are not registered as rebuilt apps.

The expanded installed parser regression, contiguous FAT reader and X-loader
regressions pass. Kernel/app disk installers and inventory consistency pass.
Production CLOCK animation/close/task exit/CMD reuse, CMD/cpuid, 128 MiB RAM
and 59 heap headers, timer/TSS switches and all 439 installed intervals pass.
Evidence is in `../evidence/2026-10-31-font-metrics/`.

    make -C rewrite test-font-metrics test-font-metrics-qemu \
      test-font-parse test-fat-block test-x-loader inventory-check
    python3 rewrite/tools/boot_smoke.py rewrite/build/OpenIceStarOS_c_apps.vhd \
      --clock-check --memory-check --task-check

Full `make verify` was not run. Arbitrary invalid pointers/unbounded malformed
fonts, exhaustive SVG support and complete font rendering are not established.
The glyph cmap lookup, initializer, outlines/rasterizer and many other kernel
components still use original code at this step. INIT/kernel remain partial;
five apps are rebuilt and 32 program paths remain unrecovered. Complete OS
recovery is unfinished. The original logo is retained.
