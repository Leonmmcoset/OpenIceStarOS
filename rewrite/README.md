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

The normal test image replaces only the first two stages. The remainder of the
disk is copied from the original image, including the next boot stage, kernel,
drivers, applications, and resources.

`boot/fat16.c` is a freestanding FAT16 root-file reader. Verification builds it
for 32-bit x86 and uses a host sector reader to recover `INIT.X` from the
original VHD; its SHA-256 is checked against the independently extracted file.

`make run-hybrid` starts a second image that uses the original third stage's
video setup and descriptor tables. C code scans PCI configuration space into
the original low-memory table, loads the BPB,
FATs and root directory into the original memory layout, loads INIT through the
FAT16 reader and ATA PIO code, and prepares the INIT header and stack. Assembly
performs the segment switch and far jump. The menu loads eight sectors for this
hybrid stage.
This integration still depends on original boot code and all later programs;
booting it does not establish that the whole system has been rewritten.
The PCI scan retains the original unbounded table behavior, which can overwrite
other low memory if sufficiently many devices are present.

`init/entry.c` recovers the `INIT.X` entry control flow and its helper at
original addresses `0x70040` and `0x700a2`. `init/directory.c` recovers the
root directory and first-cluster file searches at `0x70c70` and `0x70bb0`.
`init/clusters.c` recovers the cluster-chain reads and primary ATA sector read
used by those searches. `make run-c-init` boots a third test image that loads
these C routines from sectors 13 through 16 at physical address `0x60000` and
redirects INIT's entry there. This image still loads the original INIT binary
at `0x70000`, as well as the patched kernel and original drivers and applications. It is
an integration test of the rewritten startup path, not a standalone rewrite.
The same image installs C startup-record initialization in `kernel/reset.c`
at the original `0x80049a` and the global-state initializer in `kernel/state.c`
at `0x800104`. Fields that the original leaves untouched retain their values
from `DAT.BIN`.
The verification suite executes the original x86 initializer and the C version
against identical patterned memory and compares the entire affected region.
The kernel function table initializer at `0x8004fa` is also recovered in
`kernel/api_table.c` and invoked from the original call at `0x800099`.

`kernel/service_table.c` recovers the service table initializer at `0x8005aa`.
Its display-specific entries distinguish 8-bit and 32-bit modes; bit 25 of the
CPU feature word selects the accelerated memory operations. Unsupported
display depths and the original unused slots preserve their previous values.
The differential test executes the original routine and the C routine for
six display depths, four feature words, and three memory patterns (72 cases).
This routine is installed at its original address in the boot image.

`kernel/patches.ld` links the recovered routines into their original
address intervals. Linker assertions check entry addresses and available space.
`kernel/patches.json` declares the replaceable intervals; `tools/kernel_image.py`
replaces those intervals and writes the equal-length kernel back along the
existing FAT16 cluster chain. It preserves the file header, all other kernel
bytes, directory entries, both FAT copies (including original unused-entry
discrepancies), allocation slack, and VHD footer. INIT no longer redirects
kernel calls into the four-sector C INIT block.

Verification constructs the expected entire disk byte for byte from the
original and the compiled boot stages and kernel intervals. Unexpected changes
anywhere on disk fail the check. In addition to host-linked C differential
tests, `kernel_linked_test` executes the actual fixed-address replacement
machine code at its original addresses. Both variants cover global state,
startup-record clearing, the API table, and all 72 service-table cases.

After `make verify`, run `python3 tools/boot_smoke.py` to start QEMU with a
temporary disk snapshot, capture `build/boot_c_init.png`, and compare the
guest's loaded kernel intervals with the compiled patches. Inspect the image
to confirm desktop rendering. This smoke test does not validate every driver
or application, and unrecovered code still comes from the original disk.
The fixed-address four-initializer image was booted on 2026-10-27: visual
inspection confirmed the desktop, and all four guest memory intervals matched.

`kernel/events.c` recovers six routines: queueing an API request at `0x80129e`,
dispatching the record's event slot at `0x801372`, dispatching a one-argument
API request at `0x8013aa`, and selecting event slots 1, 2, and 3 at
`0x8013f4`, `0x801414`, and `0x801434`. Record and API table addresses,
unchecked slot indexing, callback argument passing, and post-callback field
clearing retain the original behavior. Callbacks may alter the shared pointer
globals; clearing still targets the original captured record.
`kernel_events_test` and `kernel_events_linked_test` each compare 1,512 cases
against original machine code, using three memory patterns, six indices, four
arguments, and callbacks that modify record fields and shared pointer globals.
The event-enabled image also reached the desktop on 2026-10-27, with all ten
compiled intervals matching guest memory.

`kernel/poll.c` recovers the halt-and-poll loop at `0x8012ca` and one polling
iteration at `0x8012dc`. Each iteration preserves the original order: event
slots 1 and 2, the input queue and slot 3, pointer input, then an API request.
It observes later flags after earlier callbacks and clears the originally
captured record even when callbacks change the shared record pointer. The
loop uses inline `hlt` for the privileged CPU instruction.
The two polling differential executables each cover 384 cases: all 32 event
flag combinations, three memory patterns, and four callback mutation modes.
The unrecovered pointer handler is replaced by an identical callback only
inside these tests; the boot image continues to use its original implementation.
The twelve-interval image reached the desktop on 2026-10-27. QEMU reported
`EIP=0x8012d1, HLT=1`, confirming execution of the C-generated event loop,
and all twelve installed intervals matched the guest memory dump.
