# PNG parser, reconstruction and object lifetime

Twenty-three original kernel entries (0x8393a1..0x83a70f) now run
`kernel/png.c`, increasing the manifest from 517 to 540 intervals. This
includes the full PNG header/chunk parser and decode orchestration, five
scanline filters, Paeth selection, packed row compaction, format/size queries,
constructors, input ownership and destruction. The ten lower DEFLATE/zlib
entries remain original at this checkpoint. Five apps remain rebuilt; INIT
and kernel are partial. The original logo is unchanged.

| Entry | Address | Compiled / original slot bytes |
| --- | --- | --- |
| png_paeth | `0x8393a1` | 88 / 184 |
| png_scanline | `0x839459` | 327 / 900 |
| png_unfilter | `0x8397dd` | 157 / 182 |
| png_unpad | `0x839893` | 154 / 254 |
| png_postprocess | `0x839991` | 171 / 270 |
| png_format | `0x839a9f` | 262 / 330 |
| png_release_input | `0x839be9` | 55 / 58 |
| png_header | `0x839c23` | 531 / 746 |
| png_decode | `0x839f0d` | 1110 / 1468 |
| png_create | `0x83a4c9` | 104 / 176 |
| png_from_memory | `0x83a579` | 30 / 66 |
| png_destroy | `0x83a5bb` | 48 / 54 |
| png_error | `0x83a5f1` | 11 / 12 |
| png_error_line | `0x83a5fd` | 11 / 12 |
| png_width | `0x83a609` | 10 / 10 |
| png_height | `0x83a613` | 11 / 12 |
| png_bpp | `0x83a61f` | 23 / 42 |
| png_channels | `0x83a649` | 70 / 90 |
| png_depth | `0x83a6a3` | 11 / 12 |
| png_padded_bpp | `0x83a6af` | 30 / 60 |
| png_pixel_format | `0x83a6eb` | 11 / 12 |
| png_pixels | `0x83a6f7` | 11 / 12 |
| png_size | `0x83a703` | 11 / 12 |

The 52-byte object retains width/height (0/4), color/depth/format (8/12/16),
pixels/size (20/24), error/original source line (28/32), phase (36), input/
length (40/44), and one-byte input ownership (48). Header phase2 transitions
to1; successful decode transitions to0 and releases input. Existing errors
short-circuit. Allocator failures and each parser error retain their original
code, source line, partial object writes and cleanup behavior. All arithmetic
uses wrapping 32-bit operations, with original signed checks where present.

This is compatibility recovery, with the original limitations: no CRC or
Adler verification; no palette, gray16 or interlaced decoding; the unusual
low-depth gray-alpha format mappings remain. IHDR length is not checked,
width/height are not validated, unknown critical chunks fail but ancillary
chunks skip, and pre-header length only needs29 bytes. The original raw
buffer formula is `((width * (bpp * height + 7)) >> 3) + height`. Its stored/
backreference paths reject an exactly full output, though literal writes
permit it. Packed output's unused final bits retain prior heap bytes. The
padded-bpp accessor returns `bpp + (bpp & 7)`, not a conventional round-up.
No library replacement silently corrects these behaviors.

## Validation

`make -C rewrite test-png`: **3,746 scenarios / 11,238 executions** compare
original x86, ordinary C and the installed C intervals, with all nested heap,
DEFLATE, copy and PNG calls executing. Generated standards-based PNG fixtures
cover all five row filters and twelve original color/depth mappings, multiple
IDAT chunks, ancillary/critical chunks, independent pixel-byte expectations,
low-depth compaction and untouched trailing bits. Tests include buffer overlap,
allocation failures, constructor/destructor/ownership states, truncated and
mutated headers, invalid filters and all query values. Complete touched data/
heap pages, caller words, preserved registers, x87 state and stack balance
are compared. Original exact-capacity decoder failures are explicitly checked
as errors 4/0x208 or4/0x1da; they are not called successful PNG decodes.

`make -C rewrite test-png-qemu`: the same diagnostic X is launched through
real CMD/PS2 on original/recovered disposable disks. All23 entry addresses
and actual body instructions are required in QEMU traces. An independent
report checks decoded RGBA bytes, dimensions, format, state, queries, Paeth,
row filtering, packed bits, input release and destruction. Both complete
33,554,944-byte disposable disks match their initial contents. Diagnostic X
files are not counted as recovered applications.

Kernel/app installation validation checks entire-disk allowed changes.
Focused font-bake and X-loader regressions plus inventory validation accompany
this batch. Evidence is in `../evidence/2026-11-01-png/`. Full `make verify`
has not been rerun: the last complete recorded full regression is456 intervals.
Arbitrary unmapped pointers, adversarial stack aliases, wrapped enormous
loops and general malformed-input safety are outside the proven scope.
The complete operating-system rewrite remains unfinished.
