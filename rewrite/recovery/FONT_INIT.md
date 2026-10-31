# Full SFNT font-info initialization entry

The original entry at `0x82c3bc..0x82ca7c` now executes `kernel/font_init.c`.
Its compiled 1,114-byte body fits the 1,728-byte slot. The kernel installs
456 intervals. This recovers the initializer entry, not the complete font
renderer or the complete kernel.

The C implementation initializes required and optional SFNT table offsets,
checks TrueType loca/glyf prerequisites and falls back to CFF when glyf is
absent. It follows the original CFF Name/Top DICT/String/Global Subr INDEX
sequence, Type-2 charstrings, private/local subroutines, CID FDArray and
FDSelect buffers. The original 0x20000000 CFF buffer bound is retained.
The glyph count comes from maxp or defaults to 65535. SVG cache starts at -1.
Unicode cmap selection accepts platform 0 and platform 3 encodings 1/10;
the last eligible record wins, without preferring a particular cmap format.
Missing required tables, unsupported charstring types and missing CID
FDSelect return zero after the same partial writes as the original.
Unrelated and not-yet-initialized fields remain caller-owned.

`make -C rewrite test-font-init` passes 300 scenarios / 900 executions for
original x86, ordinary C and installed C. No helpers are intercepted. Cases
cover required/optional/missing tables, directory order, nonzero SFNT offsets,
platform/encoding order, absent maxp, loca formats, output aliasing, CFF/CID,
private local subroutines and original partial failure states. The real Fira
font's table fields, glyph count and cmap are checked against independently
parsed SFNT bytes. Caller arguments, preserved registers and all touched data
pages are compared.

`make -C rewrite test-font-init-qemu` executes original/C initialization with
real Fira loading, specified non-CID and CID CFF tables, unsupported Type-1
charstrings and a missing required head table. Both reports match independent
expected fields and relative pointers, including partial output state.
Traces confirm execution of the installed initializer and CFF helper bodies.
Each complete 33,554,944-byte temporary disk and both source images remain
unchanged. The diagnostic X remains test-only, not a rebuilt application.

Kernel/app whole-disk installation checks and inventory consistency pass.
Production QEMU verifies CLOCK animation/close/task exit/CMD reuse, CMD/cpuid,
128 MiB RAM, 60 live heap headers, timer/TSS switching and all 456 installed
intervals. The captured CLOCK screenshot was visually inspected. Evidence
is in `../evidence/2026-10-31-font-init/`. Full `make verify` is recorded
separately when finished; it is not claimed by this focused checkpoint.

CFF Type-2 interpretation, TrueType outline decoding, rasterization and many
other kernel components still use original code. Arbitrary malformed fonts
and the complete OpenType specification are not exhaustively validated.
INIT/kernel remain partial; five applications are rebuilt and 32 paths remain
unrecovered. The complete OS recovery is unfinished. The original logo is retained.
