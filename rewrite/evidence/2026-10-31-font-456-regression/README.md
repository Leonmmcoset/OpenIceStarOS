# 456-interval complete regression checkpoint

`make -C rewrite verify` completed every recipe through the final application
whole-disk installers and inventory check on 2026-10-31, using the kernel from
commit `1865457`. The kernel contains 456 declared intervals. The full output
is in `make_verify.log`; image, kernel and input hashes are in `summary.json`.

The run covers boot stages, INIT/PCI, kernel original/C and installed-C tests,
font parser/metrics/cmap/glyph/initialization tests, FAT and X loading, memory,
tasks/interrupts/syscalls, surfaces, raster/text/bitmap/JPEG/math and all five
rebuilt application tests. Exact hardware entries and permitted whole-disk
changes are checked. QEMU production boot evidence for this same kernel is
recorded in `../2026-10-31-font-init/`.

New context/outline/arena source files were still uninstalled during this run.
This result does not certify later kernels containing those functions. INIT
and kernel remain partial; 32 program paths use original programs and five
applications are rebuilt. This is not completion of the whole OS recovery.
