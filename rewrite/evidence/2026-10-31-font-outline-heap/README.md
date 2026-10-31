# 472-interval outline and arena checkpoint

Ten new C functions are installed: quadratic closure, CFF biased subroutines,
CID local subroutines, four pixel-bound functions and three font object-arena
functions. The preceding six context helpers remain installed. Original/C
native comparisons cover 7,977 outline, 184 arena and 1,747 context scenarios
(9,908 scenarios / 29,724 executions for these 16 functions). All run real
nested helpers. Installed glyph/kerning, X-loader and FAT-block regressions
also pass. A final harness check uses distinct ordinary-C artifact names for
outline and arena suites; its fresh summaries and log are included separately
from the earlier combined installed-test log. Details are in `../../recovery/FONT_OUTLINE.md` and `FONT_HEAP.md`.

Three QEMU diagnostic suites execute all 16 entries through original and C
kernels, compare independent reports and check every byte of the disposable
33,554,944-byte disks and source images. The outline diagnostic includes
Type-2 local/global/CID subroutine calls and real Fira pixel bounds. The arena
diagnostic uses actual guest malloc/free. These diagnostic X files remain
test-only; no production application was newly classified as rebuilt.

`font_context_472_parallel_attempt.log` records one failed test setup during
four simultaneous QEMU runs: the **original** guest did not yield the expected
single CMD instance after automated clicking, before any font diagnostic ran.
The unchanged context suite passed on a serial retry; its successful original/C
reports are in `font_context_qemu_summary.json`. No implementation or expected
result was changed to make the retry pass.

Whole-disk kernel/application installers and inventory consistency pass.
Production boot verifies CLOCK animation, close/task exit/CMD reuse, PS/2
CMD/cpuid input, 128 MiB RAM, live heap headers, timer/TSS switches, cursor
callbacks and all 472 loaded patch intervals. The CLOCK and CMD PNGs were
captured from QEMU and visually inspected. The original logo is retained.

Source/image hashes and verification scope are in `summary.json`. Full
`make verify` was **not** run for 472 intervals: the completed 456-interval
regression is recorded separately in `../2026-10-31-font-456-regression/`.
The kernel and INIT remain partial. Five applications are rebuilt and 32
program paths still use original code. The Type-2 interpreter, complete
TrueType outline decoder, remaining rasterizer, drivers and many applications
still require C recovery. This checkpoint does not complete the OS rewrite.
