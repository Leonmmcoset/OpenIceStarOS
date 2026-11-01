# 496-interval font rendering checkpoint

Eleven new C entries implement contour edges, raster dispatch, bitmap release
and glyph/codepoint allocation and drawing. Scope, original DWORD overcopy
behavior and negative-width limits are in [FONT_RENDER.md](../../recovery/FONT_RENDER.md).

Summaries record 2,568 original/ordinary/installed scenarios (7,704 executions),
independent geometry/pixel assertions, original/C QEMU reports and entry/body
traces. Real nested shape, heap, flatten, sorting and scan calls execute. The
diagnostic guest is not counted as a rebuilt application. All disks remain
unchanged by guest execution. Whole-disk installation bounds are verified.

Regression logs include flattening, scan/sorting, shape, outline, heap, context,
X loading, branding and inventory. Production checks verify COLOR's 600 pixels,
close/task exit/CMD reuse, cpuid input, cursor callbacks, RAM, timer/TSS and all
496 loaded intervals. PNGs are actual unedited QEMU captures and were visually
inspected; the original logo remains. Source and artifact hashes are in summary.json.

This is focused validation, not full `make verify` for 496 intervals. The last
full regression remains the recorded 456-interval run. The scan driver, atlas
packing, oversampling, Type-2 interpreter, drivers and many apps remain original.
INIT/kernel remain partial, five apps are rebuilt, and 32 program paths remain
unrecovered. The complete OS rewrite remains unfinished.
