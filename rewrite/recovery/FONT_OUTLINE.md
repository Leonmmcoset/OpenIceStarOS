# Quadratic closure, CFF subroutines and bitmap bounds

Seven more original font entries execute `kernel/font_outline.c` in the
472-interval kernel. Counts describe installed slots, not OS completion.

| Entry | Function | Compiled / slot bytes |
| --- | --- | --- |
| `0x82d24a` | quadratic contour closure | 220 / 316 |
| `0x82e4ac` | biased CFF subroutine lookup | 129 / 166 |
| `0x82e552` | CID local subroutine selection | 240 / 414 |
| `0x8308d2` | shifted glyph bitmap bounds | 248 / 362 |
| `0x830a3c` | glyph bitmap bounds | 48 / 86 |
| `0x830a92` | shifted codepoint bitmap bounds | 55 / 100 |
| `0x830af6` | codepoint bitmap bounds | 48 / 86 |

Quadratic closure emits a line, a quadratic curve, or two quadratic curves
according to the first/last off-curve flags. Midpoints use wrapping signed
32-bit sums followed by arithmetic right shifts. Vertices retain the original
14-byte stride, 16-bit coordinate truncation and untouched reserved fields.
The original caller's vertex-count slot is incremented before each vertex
call, even when vertices overlap arguments. Volatile coordinate parameters
retain later reads after an earlier vertex changes the argument area.

CFF INDEX lookup preserves the 107/1131/32768 biases at counts 1239/1240 and
33899/33900, signed validity tests after wrapping addition, and an empty buffer
for a biased index outside the count. Both the by-value buffer cursor and the
biased index are written back to their original argument slots. Aggregate
results use the original hidden pointer and `RET 4` convention. Explicit GCC
frame access is restricted to the original argument slots that must be written.

CID selection implements FDSelect formats 0 and 3 and retrieves the selected
FD dictionary's private local subroutine INDEX. Unsupported/missing selections
preserve the original unchecked item `-1` lookup; they are not normalized to a
new error policy. The CFF interpreter itself is still original code.

Bitmap bounds keep the original optional output order: floor(left),
floor(-top), ceil(right), ceil(-bottom), after scale and subpixel shifts.
The original scalar floor/ceil services and x87 precision/rounding control
remain observable, including their behavior for NaNs and large conversions.
Absent TrueType glyphs zero requested outputs without applying shifts. Live
volatile float/output arguments preserve changes caused by earlier outputs
overlapping later arguments; outputs can also overlap font/source data.

`make test-font-outline` compares original x86, ordinary C and installed C for
7,977 scenarios / 23,931 executions. It executes real nested helpers, compares
all touched data pages, argument bytes, preserved registers and x87 control,
and covers both 4- and 8-byte incoming stack alignment. Independent fixtures
specify packed quadratic vertices, CFF INDEX items and CID private subroutines.
All 5,565 Fira glyphs have pixel bounds checked against independent loca/glyf
bytes. Additional cases cover optional/overlapping outputs, caller-area
aliases, signed/wrapped coordinates and counts, truncated buffers, six x87
control words and interpreted CFF cubic geometry.

`make test-font-outline-qemu` executes all seven entries in original/C guests.
The same diagnostic X runs four closure branches, valid/missing subroutine
items, both FDSelect formats, actual Type-2 programs calling local/global/CID
subroutines, and real Fira glyph/codepoint bitmap bounds. Both guest reports
match independent expectations. Traces verify installed entries and bodies;
every byte of the temporary disks and source images is checked unchanged.
The diagnostic X is test-only and does not increase the rebuilt-app count.

See `../evidence/2026-10-31-font-outline-heap/` for installed regression and
QEMU/production CLOCK/RAM/TSS evidence. The complete full regression remains
the separately recorded 456-interval run; it is not a full run for 472 slots.
TrueType outline decoding, Type-2 interpretation and the complete rasterizer
still require recovery. Arbitrary malformed fonts and every alias arrangement
are not exhaustively established. INIT/kernel remain partial; five apps are
rebuilt and 32 program paths use original programs. The OS recovery remains
unfinished and the original logo is retained.
