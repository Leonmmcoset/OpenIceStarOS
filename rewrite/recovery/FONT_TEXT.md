# Font name, Unicode and TrueType surface text recovery

Twenty original entries are now C implementations: eight name/collection APIs
and twelve glyph/text/encoding APIs. Installed intervals increase 550 to 570.
This batch does not include the preceding 33 PNG/DEFLATE entries. The original
logo is retained. INIT, kernel, drivers and most applications remain incomplete.

## Behavior

Name APIs match UTF-8 against UTF-16BE records, select adjacent family/subfamily
records, respect platform/encoding/language and macStyle bits, search SFNT/TTC
faces, expose name strings and provide the two public forwarding entries.
Prefix comparison mutates its UTF-16 pointer/length caller words; the original
unvalidated low-surrogate arithmetic and equality with negative lengths remain.

Text APIs rasterize real TrueType codepoints, draw opaque, upper-clipped or
alpha glyphs, translate the original legacy table, decode UTF-8, draw legacy
and UTF-8 strings, implement bounded multiline text, draw legacy clipped/alpha
strings and initialize the font context. The legacy clipped/alpha functions
are single-line renderers; they are not UTF-8 multiline variants. Public
surface services and real heap/glyph/raster code execute without helper mocks.

UTF-8 preserves its consumed-count partial writes, overlong rejection and
acceptance of three-byte surrogate scalars. Legacy lookup retains its original
AND-0x5e formula. Glyph positioning and spacing, signed comparisons, right/bottom
inclusive clips and multiline CR/LF handling remain. The multiline lower-limit
return leaks the current bitmap as in the original. Transparent legacy text
samples surface 2 when the target pixel equals its color key. Spacing fills
use the original inclusive rectangle service.

## Validation

`test-font-names`: 2,973 scenarios / 8,919 executions.
`test-font-text`: 2,830 scenarios / 8,490 executions.
Total: 5,803 scenarios / 17,409 executions
against original x86, ordinary C and installed C.

Independent checks cover UTF encoding, malformed leads/continuations/overlongs,
real shipped Fira name records, a two-face TTC collection, legacy indexes,
rectangle glyph coverage, exact color channels and output metadata. Other
cases compare every touched data/heap page, output aliases, caller storage,
callee-saved registers and x87 control. The guest diagnostic calls all 20
entries and requires decoded interior instructions in original/recovered QEMU.
Its Unicode/name/dimension/opaque/clipped/alpha pixel hashes are independently
calculated. Both whole disposable disks remain unchanged. The diagnostic is
not a rebuilt application.

Production COLOR boot verifies all 570 loaded intervals, 128 MiB RAM, heap,
real TSS/timer scheduling, PS/2/CMD/cpuid/cursor, palette pixels, close/task exit
and CMD reuse. Screenshots have been viewed. Whole kernel/app disk validation
and inventory checks pass; original VHD remains unchanged. Evidence is in
`../evidence/2026-11-01-font-text/`.

## Limits

Odd-sized UTF-16 inputs, arbitrary invalid pointers, unbounded malformed loops
and stack alias attacks are outside coverage. Original invalid legacy control
bytes before a first valid glyph and gap fill after a first failed bitmap
allocation may consume indeterminate locals; C initializes these locals to zero
and no equivalence is claimed for those undefined paths. The original raster
DWORD overcopy and unimplemented filters/Type-2/scan driver remain separate
limitations. Full `make verify` has not been rerun; the last full recorded run
is 456 intervals. Five apps are rebuilt and 32 program paths remain unrecovered.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| font_name_prefix | `0x8365b2` | 460 / 744 |
| font_name_equal | `0x83689a` | 44 / 62 |
| font_name_string | `0x8368d8` | 198 / 348 |
| font_name_pair | `0x836a34` | 482 / 652 |
| font_match | `0x836cc0` | 257 / 584 |
| font_find_name | `0x836f08` | 62 / 104 |
| font_find_name_api | `0x83700c` | 19 / 34 |
| font_name_equal_api | `0x83702e` | 22 / 42 |
| font_glyph_opaque | `0x829003` | 228 / 304 |
| font_glyph_clip | `0x829133` | 251 / 330 |
| font_glyph_alpha | `0x82927d` | 275 / 364 |
| font_legacy_code | `0x8293e9` | 120 / 156 |
| font_string_legacy | `0x829485` | 489 / 594 |
| font_utf8_cont | `0x8296d7` | 22 / 48 |
| font_utf8_decode | `0x829707` | 233 / 570 |
| font_string_utf8 | `0x829941` | 419 / 524 |
| font_string_lines | `0x829b4d` | 497 / 646 |
| font_string_legacy_clip | `0x829dd3` | 520 / 620 |
| font_string_legacy_alpha | `0x82a03f` | 541 / 646 |
| font_text_init | `0x82a2c5` | 48 / 102 |
