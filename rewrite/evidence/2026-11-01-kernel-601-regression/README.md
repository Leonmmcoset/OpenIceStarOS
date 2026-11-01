# 601-interval complete regression checkpoint

`make -C rewrite verify` completed with **exit code 0** on 2026-11-01,
running from 08:44:06 UTC through the final log write at 09:17:58 UTC.
The tested source commit is `7a76442844bcb5f4591c68d3ec92f47d93ae58b6`.
The complete output is in [make_verify.log](make_verify.log); input, kernel,
image, manifest and log hashes and tool versions are in [summary.json](summary.json).

All recipes ran through the final five-application whole-disk installers and
inventory check. Coverage includes boot models/installers, INIT/PCI, FAT and
X loading, all installed font/PNG/inflate/window-service suites, memory,
tasks, interrupts, syscalls, surfaces, raster/text/bitmap/JPEG, SIMD, x87 math,
X format and the five rebuilt applications. Hardware-entry byte checks and
QEMU display DAC, paging, timer flags and firmware scans also pass. Individual
tests retain their documented boundary models, unsafe-input exclusions and
coverage limits; passing this target does not extend those scopes.

The three suites for this requested batch pass 8,080 scenarios / 24,240
original-x86, ordinary-C and installed-C executions. The manifest grows from
550 to 601 intervals, with 51 distinct added original entries: eight font-name,
twelve TrueType text and 31 window/event/image services. Previous PNG/inflate
entries are not recounted.

Focused guest diagnostics are recorded separately: [20 font/text bodies at
570 intervals](../2026-11-01-font-text/font_text_qemu_summary.json) and [31
window-service bodies at 601 intervals](../2026-11-01-window-services/window_services_qemu_summary.json).
Those two opt-in targets are not rerun by `make verify`. Production 601-interval
boot, input/RAM/TSS/COLOR checks and viewed [CMD](../2026-11-01-window-services/cmd_cpuid.png)
and [COLOR](../2026-11-01-window-services/color.png) screenshots are in the
[window-service evidence](../2026-11-01-window-services/README.md).

The original VHD remains unchanged. The original logo is retained. INIT and
kernel remain partial; five apps are rebuilt, while 32 program paths,
including all four drivers, remain unrecovered. Of the kernel's 250,035-byte
payload, 97,792 bytes remain outside declared replacement slots; slot sizes
include padding and 77 exact hardware-instruction intervals. These byte and
interval counts are not OS completion percentages. **The complete OS rewrite
is unfinished.** The [456-interval checkpoint](../2026-10-31-font-456-regression/README.md)
is preserved as historical evidence.
