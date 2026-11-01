# 481-interval font scan checkpoint

Five additional entries execute C: active-edge initialization, nonzero-winding
scanline filling, insertion sort, quicksort and the combined edge-sort entry.
Implementation scope and limitations are in
[FONT_SCAN.md](../../recovery/FONT_SCAN.md).

`font_scan_summary.json` records 2,959 original/ordinary/installed scenarios /
8,877 executions, with real nested arena/malloc/floor calls. Data and heap
pages, caller-area writes, stack balance, preserved registers and x87 control
are compared. Independent expectations use interval/pixel intersection lengths,
edge coordinates, stable insertion order and complete record permutations.

`font_scan_qemu_summary.json` records original/C guest reports for the active
edge structure, byte coverage, each sorting entry and 128 raster-driver bytes.
The rectangle has four vertical samples, fractional horizontal boundaries and
explicit row padding for the original DWORD copy contract. All five installed
bodies execute, together with the still-original raster driver. Execution
also includes normal font drawing through CMD. Complete disposable/source
disks remain unchanged; the diagnostic X is not a rebuilt application.

The regression log covers installed full-Fira shape decoding, arenas, outline
helpers, CFF contexts, X loading, branding residual checks and inventory. Shape
results continue to distinguish returned outlines from the original cyclic-heap
checkpoints; those original failures are not counted as successful returns.
The disk log verifies complete kernel/application installation change bounds.

Production boot checks include all 481 loaded intervals, COLOR's 600 palette
pixels, window close/task exit/CMD reuse, cpuid input, cursor callbacks, RAM and
timer/TSS switching. PNGs are unedited QEMU captures and were visually inspected.
The original logo is retained. Source/kernel/image hashes are in `summary.json`.

This is focused validation, not a complete `make verify` for 481 intervals.
The last complete full regression remains the recorded 456-interval run.
The raster driver, curve flattening, Type-2 interpreter, drivers and many apps
remain original. INIT/kernel remain partial; five apps are rebuilt and 32
program paths remain unrecovered. The complete OS rewrite remains unfinished.
