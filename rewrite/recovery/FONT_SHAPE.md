# TrueType outlines and glyph shape dispatch

Four additional original entries now execute `kernel/font_shape.c`. The kernel
installs 476 intervals; interval counts are not OS recovery percentages.

| Entry | Function | Compiled / slot bytes |
| --- | --- | --- |
| `0x82d386` | TrueType simple/compound outline decoder | 1846 / 3286 |
| `0x82f756` | CFF two-pass shape allocation | 122 / 180 |
| `0x82f90a` | TrueType/CFF shape dispatch | 46 / 78 |
| `0x82cede` | codepoint-to-shape dispatch | 31 / 50 |

The decoder reads either loca format, skips glyph instructions, expands repeated
point flags, and decodes signed 16-bit or compressed signed/unsigned byte deltas.
Coordinate accumulation wraps to the original 16-bit vertices. Simple contours
use the original allocation with input points stored after the output area;
the implementation preserves forward writes, implied midpoint vertices,
off-curve starts, quadratic controls and contour closure. Empty/missing glyphs
return zero and clear the output pointer. TrueType hint instructions are skipped,
as in the original, rather than interpreted.

Compound glyphs recurse through the shape dispatcher. Word/byte translations,
uniform scale, independent x/y scale and 2x2 transforms retain the original
F2Dot14 constants, float32 stores, x87 square-root call and truncation to signed
16-bit coordinates. The additional axis-length factors in the original transform
are retained, even when they differ from a conventional affine transform.
Point-matched arguments without the XY flag are not consumed or implemented,
matching the executable. Recursive component allocations, joining, freeing and
allocation-failure effects use the real kernel services. There is no new cycle
limit or error policy for malformed recursive glyph graphs.

CFF shape construction invokes the still-original Type-2 interpreter once for
the vertex count, allocates 14-byte vertices, and invokes it again for output.
It preserves the original empty/failure behavior, including its missing cleanup
and allocation-null checks. This recovers allocation/dispatch, not the Type-2
bytecode interpreter itself. Necessary helper addresses remain linked to the
installed C versions or, for unrecovered helpers, explicitly to original code.

## Original compound-copy defect

Both joins in `0x82dfd6` / `0x82e004` call `0x817510` with `vertex_count * 14`.
That entry executes `REP MOVSD`: its third argument counts DWORDs, not bytes.
The original therefore copies **four times the vertex byte length**, including
allocation slack and potentially following heap headers. The C decoder retains
the exact helper/count. It must not silently substitute a byte memcpy.

With a fresh 4 MiB test heap, Fira glyph 12 corrupts the header at `0x02000800`
and a later allocation reaches a cyclic list. This is present in the unmodified
original too. The emulator observes malloc entry, checks the real current list
for an unavoidable repeated node before a suitable first-fit block, and stops
before entering a proven infinite traversal. It does not edit registers,
repair the heap, synthesize allocations or treat the call as a successful
return. All touched data pages and original caller arguments are compared at
that shared entry checkpoint. Normal cases execute nested helpers to return.
These results describe this heap fixture, not a claim that every affected glyph
always hangs in every live guest heap layout.

## Validation

`make -C rewrite test-font-shape` compares original x86, ordinary C and installed
C across 6,917 scenarios / 20,751 emulator runs. Of these, 6,622 scenarios return
and 295 agree at a cyclic-heap checkpoint. All 5,565 shipped Fira glyphs are
covered: 4,638 simple, 884 compound, 43 empty. Additional cases cover both loca
formats, repeated/compressed flags, skipped instructions, all transform forms,
nested components, six x87 controls, allocation failures, output/source/heap
aliases, codepoint mapping and CFF cubic output/failures. Caller arguments,
preserved registers, stack balance and x87 control are checked on returns;
memory comparisons include heap headers, allocation slack and reserved bytes.
Both incoming stack alignments modulo eight are used. Synthetic closed-contour
geometry supplies independent expected move/line/quadratic vertices.

`make -C rewrite test-font-shape-qemu` loads the same diagnostic X through real
CMD/PS2 in the original and installed-C guests. It checks Fira outlines against
an independent table decoder, a compressed square, translation, recursive scale,
empty glyphs, CFF cubic output and invalid-opcode failure. Counts and signatures
include the vertex type and every relevant coordinate/control point. Execution
traces show all four installed entries/bodies ran. Complete disposable disks
and source images remain unchanged. The diagnostic X is not a rebuilt app.

Evidence is in `../evidence/2026-11-01-font-shape/`. The last complete full
regression was the separately recorded 456-interval run; focused checks do not
establish a full `make verify` for this kernel. Type-2 interpretation, the rest
of the rasterizer, drivers and many apps remain original. INIT/kernel remain
partial, five apps are rebuilt and 32 program paths remain unrecovered.
The full OS rewrite is unfinished. The original logo is retained.
