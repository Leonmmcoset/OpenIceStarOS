# 476-interval font shape checkpoint

Four more entries execute C: TrueType simple/compound decoding, CFF two-pass
shape allocation, glyph dispatch and codepoint dispatch. Implementation and
limitations are described in [FONT_SHAPE.md](../../recovery/FONT_SHAPE.md).

`font_shape_summary.json` records 6,917 original/ordinary/installed scenarios
(20,751 runs), including all 5,565 Fira glyphs. There are 6,622 returning
scenarios and 295 cases where the original compound DWORD-copy defect leaves
a cyclic heap in the fresh-heap fixture. Those cases compare identical data
pages and caller arguments at a proven nonterminating malloc checkpoint; they
are not reported as successfully returned outlines. No original helper is
replaced by a test stub, and no invalid heap is repaired for the comparison.

`font_shape_qemu_summary.json` records matching original/C real guest reports
for independent Fira geometry, compressed points, translated/recursively scaled
components, empty glyphs, CFF cubic output and invalid opcode handling. All four
installed bodies execute; whole temporary/source disks remain unchanged.

The installed regression log covers previous font outline/context/heap/glyph
functions, X loading, FAT block, branding residual checks and inventory. The
disk log verifies the complete kernel/application installation change bounds.
Production boot checks cover 476 loaded intervals, CLOCK animation/close/task
exit/CMD reuse, cpuid input, cursor callbacks, RAM and timer/TSS task switching.
The two PNGs are unedited QEMU captures and were visually inspected; the
original logo is retained.

`summary.json` records exact source/kernel/image hashes. This is focused
validation, not a full `make verify` for 476 intervals. The last full regression
remains the separately recorded 456-interval checkpoint. INIT/kernel are still
partial; five apps are rebuilt and 32 program paths remain unrecovered.
CFF interpretation, the remaining rasterizer, drivers and many apps still use
original code. This does not complete the OS rewrite.
