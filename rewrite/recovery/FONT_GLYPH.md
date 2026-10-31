# Glyph bounds and pair positioning

Fifteen more original font entries now execute C from `kernel/font_glyph.c`
and `kernel/font_kern.c`. The kernel installs 455 intervals, including 77
exact instruction intervals. These counts are not OS completion percentages.

| Function | Compiled / slot bytes |
| --- | --- |
| `font_vertex` | 41 / 64 |
| `font_glyph_offset` | 151 / 274 |
| `font_glyph_box` | 187 / 250 |
| `font_codepoint_box` | 40 / 72 |
| `font_glyph_empty` | 85 / 166 |
| `font_cff_box` | 137 / 256 |
| `font_kern_length` | 73 / 122 |
| `font_kern_table` | 158 / 336 |
| `font_legacy_kern` | 160 / 270 |
| `font_coverage` | 208 / 406 |
| `font_class` | 218 / 358 |
| `font_gpos_kern` | 623 / 1054 |
| `font_glyph_kern` | 58 / 102 |
| `font_codepoint_kern` | 60 / 104 |
| `font_codepoint_metrics` | 37 / 58 |

## Behavior

TrueType lookup preserves signed glyph/count/loca-format comparisons,
16-bit scaled and 32-bit loca offsets, wrapped arithmetic, empty offsets
and reversed spans. The bounds entry writes optional outputs in the original
order and rereads the live font pointer, including when outputs alias it.
A missing TrueType glyph leaves outputs unchanged and returns zero.
The vertex writer changes only its type, endpoint and first control-point
fields, truncating coordinates as before.

CFF bounds initializes the original 12-word context and calls the still-original
Type-2 interpreter. It returns the vertex count on success and zero on failure,
writing zero bounds on failure. The general glyph-box wrapper returns one on
the CFF branch even when the interpreter fails. Empty-glyph detection preserves
this distinction. The codepoint box/metrics entries use the recovered cmap.
Neither the Type-2 interpreter nor complete outline decoding is recovered here.

Legacy kerning only accepts the first format-0 horizontal subtable with coverage
exactly 1; table output respects signed capacities, including negative return
values with no output writes. Pair searches retain unsigned composite keys and
signed 16-bit advances. Coverage and class lookup support formats 1 and 2,
including their distinct unsupported/missing return values.

GPOS takes precedence whenever its table exists, without fallback on a miss.
The original version gate is 1.0, lookup type 2, PairPos formats 1/2 and value
formats 4/0 (first-glyph xAdvance only). All lookups are scanned without Script,
Language or Feature filtering. Unsupported matching subtables terminate with
zero; uncovered ones are skipped. Pair sets use signed glyph comparisons;
class matrices preserve class-zero defaults and count bounds. This preserves
the original limited implementation, not all OpenType behavior.

## Validation and limits

`make -C rewrite test-font-glyph` passes 25,225 scenarios / 75,675 executions
across original x86, ordinary C and installed C without intercepted helpers.
Independent Fira loca/glyf bytes specify offsets, bounds and emptiness for all
5,565 glyphs. Metrics expectations come from hmtx. Synthetic fixtures specify
legacy/GPOS advances, empty/unsupported tables, signed boundaries, overlapping
outputs and CFF empty/error/rectangle programs. The rectangle's independently
specified bounds are (0,0,100,100) with five vertices including closure.
Comparisons include touched data pages, caller arguments and callee-saved
registers; volatile-register values of void functions and local stack scratch
are excluded.

`make -C rewrite test-font-glyph-qemu` runs every new entry in original/C guests.
The same diagnostic X loads Fira through real guest APIs and builds specified
CFF, legacy kern, PairPos pair-set and class-matrix fixtures. Both reports match
independent expectations. Traces establish each installed entry/body executed;
each full 33,554,944-byte disposable disk and both source images remain unchanged.
This diagnostic is not registered as a rebuilt application.

Installed parser/metrics/FAT-block/X-loader regressions, kernel/application
whole-disk installers and inventory consistency pass. Production QEMU boot
also verifies C CLOCK animation/close/task exit/CMD reuse, CMD/cpuid, 128 MiB
RAM, 59 live heap headers, timer/TSS switches and all 455 installed intervals.
Evidence is in
`../evidence/2026-10-31-font-glyph/`. Full `make verify` was not run for this
checkpoint. Arbitrary malformed fonts, all GPOS features and full font rendering
are not established. INIT/kernel remain partial; five apps are rebuilt and 32
program paths remain unrecovered. The complete OS is unfinished; the original
logo is retained.
