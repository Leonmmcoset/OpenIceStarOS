# CFF Type-2 drawing context

Six functions at `0x82e05c..0x82e4ac` now execute `kernel/font_context.c`.
The kernel installs 462 intervals. These counts are not recovery percentages.

| Function | Compiled / slot bytes |
| --- | --- |
| `font_context_bounds` | 91 / 132 |
| `font_context_vertex` | 149 / 246 |
| `font_context_close` | 112 / 166 |
| `font_context_move` | 130 / 184 |
| `font_context_line` | 96 / 148 |
| `font_context_curve` | 192 / 228 |

## Original drawing semantics

Bounds mode initializes min/max on the first vertex and accumulates signed
32-bit endpoint coordinates. Cubic curves also include both control points;
this is a control-point envelope, not an exact mathematical curve-extrema box.
Output mode writes 14-byte vertices with endpoint, two control points and type,
retaining the reserved final byte. Type truncates to eight bits, coordinates to
16 bits and the vertex count wraps as a 32-bit value. The live output pointer
and count are reread between stores, including when outputs overlap context.

Contour closure emits a line to the start only when either start coordinate
differs from the current coordinate. Relative moves close the previous contour,
update current/start coordinates and emit a move vertex. Relative lines update
current coordinates and emit a line. Cubic curves accumulate three successive
pairs of deltas; every original float32 rounding boundary is retained before
integer coordinate conversion. x87 control is restored after truncation.

These helpers now run when the original Type-2 interpreter executes CFF paths.
The interpreter itself remains original code; this is not complete CFF outline
or font-renderer recovery.

## Validation and limits

`make -C rewrite test-font-context` passes 1,747 scenarios / 5,241 executions
across original x86, ordinary C and installed C without helper interception.
Comparisons include all touched data pages, preserved registers, caller
arguments and x87 control preservation under six precision/rounding settings.
Fixtures cover both modes, signed coordinate bounds, NaNs/infinities,
subnormals, relative control-point rounding, closure, wrapped counts and
output/context aliasing. One pointer/count overlap deliberately reproduces
the original unmapped write, including data writes before the fault.
Independent geometry expectations specify coordinates and packed vertices.

`make -C rewrite test-font-context-qemu` executes all six entries in original/C
kernels. The diagnostic checks complete context fields and all 56 bytes of a
move/line/cubic/closure vertex stream, bounds mode and an actual Type-2 cubic
program interpreted by the original bytecode engine. Original/C reports match
independent geometry, traces prove each installed entry/body ran, and complete
temporary disks and source images remain unchanged. The diagnostic X is not a
rebuilt production application.

Evidence is in `../evidence/2026-10-31-font-context/`. Complete regression
results for the preceding 456-interval kernel are recorded separately; they
do not establish a full run for the subsequent 462-interval kernel. The latter
has focused installed font/FAT/X-loader regressions, installation checks,
inventory consistency and production CLOCK/RAM/TSS boot validation.

The original Type-2 interpreter, TrueType outline decoder, rasterizer, drivers
and many applications remain. INIT/kernel are partial; five applications are
rebuilt and 32 program paths remain unrecovered. Arbitrary malformed fonts
and all outline/rasterization behaviors are not exhaustively established.
The complete OS recovery is unfinished. The original logo is retained.
