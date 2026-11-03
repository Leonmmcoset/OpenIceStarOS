# PNG surface, direct and file drawing recovery

Three actual function bodies now execute `kernel/png_draw.c`, taking the
installed kernel from 614 to 617 intervals. Together with the preceding
window-focus and window-geometry batches, this work adds ten functions from
the 607-interval baseline. Interval counts do not measure OS completion.

The surface-ID and direct-surface adapters construct the real PNG context,
query its initial error, decode, obtain width/height/bits-per-pixel/pixels,
walk the original `row != height` / `column != width` loops and destroy the
context. Source stride is unsigned bits-per-pixel shifted right by three.
The original decode return value is ignored. Initial-error early return has
no destroy; ordinary bad-header errors occur later, during decode, and the
zero-dimension path still destroys the context. No new error recovery is added.

Both adapters reread target depth for each pixel. In 32-bit mode, only a
nonzero source alpha byte draws, with RGB packed as `0xRRGGBB`. In 8-bit mode,
the original transparency check reads the entire source DWORD: nonzero RGB
with alpha zero still draws. Dithering uses source column/row, independent
of destination offset. Alternate PNG layouts retain the original byte
assumptions; this is behavior recovery, not a format-conversion correction.
Surface-ID drawing calls the live global video-mode pixel service, whereas
direct drawing selects the actual 8/32-bit pixel body by target depth. A
target/global-mode mismatch therefore has observable original behavior.

The file adapter recognizes only signed first characters C through F followed
by `:\` as absolute. Absolute lookup must return greater than zero; relative
lookup accepts index zero. It performs actual record/index reads and interprets
file length as signed. Only positive length and a nonzero read pointer reach
drawing followed by free; failed gates retain the original cleanup behavior.
FAT paths, X ABI, entry addresses and the original logo are unchanged.

`make -C rewrite test-png-draw` passes **344 scenarios / 1,032 executions**
against original x86, ordinary C and installed C: 112 surface-ID, 106 direct
and 126 file cases. PNG/DEFLATE, owner heap allocation/free, quantization and
pixel bodies actually execute. Only FAT lookup/read/length are explicit native
boundaries, checked by ordered paths, raw directory records and arguments.
Independent pixel oracles cover alpha versus DWORD transparency, dithering,
clipping, sentinels and target/global-mode mismatches. Other cases cover stored
and compressed streams, alternate source formats, short/corrupt headers,
cleanup, signed lengths, missing/zero reads, C/F versus B/G/lowercase/non-ASCII
paths, index zero, wrapped IDs/coordinates and destination-metadata aliasing.
Touched heap/data pages, caller arguments, cdecl registers and x87 controls
compare. Boundary output into a caller-local directory record is written to
its actual stack address; unrelated stack layout is not an observable oracle.

`test-png-draw-qemu` executes the same diagnostic through real CMD/PS2 on
original and recovered kernels. Two independently generated PNG resources
occupy otherwise unused root records/clusters in temporary disks. The guest
reads real FAT data, exercises all three adapters, compares whole off-screen
buffers in 8/32-bit mode at three offsets, checks absolute success and both
absolute/relative missing files, then displays a 96x64 PNG in a normal window.
PNG, DEFLATE, heap and pixels have no substitutes. The off-screen surface
temporarily borrows slot 4095; under CLI, the pixel service temporarily selects
the real original 8/32-bit entry to match the target mode. The table slot,
service pointer and caller flags are restored before normal window drawing.
Both independent **25-DWORD reports** match, all three entry/body traces are
present, and both complete **33,554,944-byte disposable disks** remain unchanged.
The diagnostic X and temporary fixtures are not rebuilt application entries.

On 617 intervals, neighboring regressions pass: PNG 3,746, DEFLATE 3,624,
image controls 816, geometry 440, lifecycle 1,208 and window services 2,270
scenarios. Production COLOR verifies every installed interval in guest memory,
128 MiB RAM and 59 live heap headers, timer/TSS switches, PS/2/CMD/cpuid,
cursor, 600 palette pixels, close/task exit and CMD reuse. Kernel allowed-range,
application whole-disk and inventory checks pass. Actual viewed
[PNG diagnostic](../evidence/2026-11-03-png-draw/png-diagnostic.png) and
[production COLOR](../evidence/2026-11-03-png-draw/color.png) captures, summaries,
logs and hashes are in [the evidence directory](../evidence/2026-11-03-png-draw/README.md).
Restoring only the three new slots to their original bytes reproduces the
previous 614-interval kernel and production-image hashes exactly; all other
kernel intervals and disk bytes are unchanged from that validated checkpoint.

The last complete `make verify` remains 601 intervals. These focused checks do
not cover every malformed PNG, display mode, concurrent state mutation or
all FAT devices. Relative successful reads are tested at the native dispatch
boundary; this QEMU fixture covers relative missing files. PNG fit/scaling
adapters at `0x82443e` and `0x824c70` remain original. INIT/kernel remain partial,
five applications are rebuilt, and 32 program paths including all four drivers
remain unrecovered. The complete OS rewrite is unfinished.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| png_surface_draw | `0x824014` | 368 / 444 |
| png_direct_draw | `0x8241d0` | 344 / 412 |
| png_file_draw | `0x82436c` | 154 / 210 |
