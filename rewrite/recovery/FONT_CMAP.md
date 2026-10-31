# Character code to glyph lookup

The entry at `0x82ca7c..0x82cede` now runs `kernel/font_cmap.c`.
Its 644-byte C body fits the 1,122-byte original slot. The kernel installs
440 intervals; this is not a recovery percentage.

The dispatcher preserves formats 0, 4, 6, 12 and 13, and returns zero for
format 2 and other unsupported formats. Format 0 uses a signed comparison
and can read before the table or fault for negative character codes.
Format 4 retains the original search fields, signed bounds, wrapping delta
and indirect glyph array behavior: the indirect branch does not add idDelta.
Formats 12/13 use unsigned character ranges and signed group-count bounds.
No validation or normalization is added to the original ABI.

`make -C rewrite test-font-cmap` compares original x86, ordinary C and
installed C in 74,763 scenarios / 224,289 executions without helper stubs.
Independent table parsing supplies expected glyphs for both shipped Fira
charmaps, including all 65,536 BMP character codes in the selected map.
Synthetic fixtures cover supported/unsupported formats, signed bounds,
zero groups, wrapping offsets and original unmapped byte-read faults.
Comparisons check return values, touched data pages, caller arguments,
callee-saved registers and the specified fault addresses.

`make -C rewrite test-font-cmap-qemu` executes real font loading and
initialization, both the real Fira map and each supported synthetic format,
and unsupported format 2. Original/C reports match independent expectations;
execution traces confirm the installed body ran. Each entire disposable
33,554,944-byte disk remains unchanged, and both source images are unchanged.
The diagnostic X is test-only and is not registered as a recovered app.

Parser (5,371 scenarios), metrics (1,098), FAT block readers (882) and
X-loader (430) installed regressions pass. Kernel and application whole-disk
installation checks and inventory consistency pass. Evidence is in
`../evidence/2026-10-31-font-cmap/`. Full `make verify` was not run.
Arbitrary malformed fonts are not exhaustively validated. Font initialization,
outline interpretation and rasterization still contain original code at this
checkpoint. INIT/kernel remain partial, five applications are rebuilt and
32 program paths remain unrecovered. The complete OS is unfinished and the
original logo is retained.
