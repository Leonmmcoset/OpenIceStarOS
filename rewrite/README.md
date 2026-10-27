# C rewrite

This directory is an incomplete, independent C rewrite. `boot/mbr.c` implements
the first BIOS boot stage; the small assembly entry establishes real-mode
segments and the stack and preserves the boot-drive byte at address 0x7c02,
which the menu accesses directly. The partition table is retained. `boot/menu.c`
implements the boot menu, selection, and loading of the next stage; its timer
polls BIOS ticks rather than installing the original RTC interrupt handler.

`make verify` builds a test disk from the original image and checks its partition
table, signature, and unchanged contents after sector 3. `make run` starts the
test disk in QEMU using a temporary snapshot. GCC with `-m16`, binutils, make,
and qemu-system-i386 are required.

Only the first two stages are C code so far. The remainder of the disk is copied from
the original image, including the next boot stage, kernel, drivers, applications,
and resources. Successful boot is an integration test for this stage, not
evidence that the rest of the OS has been rewritten or its APIs recovered.

`boot/fat16.c` is a freestanding FAT16 root-file reader intended for the next
stage. It is not yet connected to the boot image. Verification builds it for
32-bit x86 and uses a host sector reader to recover `INIT.X` from the original
VHD; its SHA-256 is checked against the independently extracted file.
