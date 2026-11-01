# 517-interval glyph baking and range rendering checkpoint

Ten entries execute C: contiguous atlas baking, prefilter dispatch, packed
rectangle rendering, multi/single range packing, scaled metrics and four
public font APIs. Scope and original defects are described in
[FONT_BAKE.md](../../recovery/FONT_BAKE.md).

Native summary records 1,777 full-return original/ordinary/installed scenarios
(5,331 executions), plus 72 filter-entry checkpoints (216 executions). These
additional cases stop BEFORE the original broken oversampling filter executes;
only actual pre-entry data/heap state and arguments are compared. Normal-return
coverage uses oversampling one or omitted rectangles. No helper is stubbed.
Independent expectations verify row boundaries, failure returns, complete pixel
bytes including original DWORD padding, metadata and scaled metrics.

QEMU reports verify every emitted metadata word and complete pixel-buffer
hashes in original/recovered guests. All ten entry/body traces execute and
whole source/disposable disks stay unchanged. Input uses real CMD/PS2. The
diagnostic X is not a rebuilt application. Installation bounds are checked.

Regression log includes installed atlas, render, flatten, scan, shape, outline,
heap, context, metrics, parse, init, X loader, branding and inventory checks.
Production boot verifies COLOR's 600 pixels, close/task exit/CMD reuse, input/
cursor/RAM/TSS and all 517 intervals. Unedited QEMU PNGs were visually inspected;
the original logo remains. Source/artifact hashes are recorded in summary.json.

This is focused validation, not full `make verify` for 517 intervals. The last
complete full regression remains the recorded 456-interval run. The scan driver,
oversampling filters and Type-2 interpreter remain original, and malformed/huge
input behavior beyond the stated cases is not established. Five apps are
rebuilt, INIT/kernel remain partial, 32 program paths remain unrecovered and
the complete OS rewrite remains unfinished.
