# Window creation and close recovery

Two original bodies now execute `kernel/window_lifecycle.c`, taking the kernel
from 605 to 607 installed intervals. Together with the preceding four
[image-control functions](IMAGE_CONTROLS.md), this batch adds six real kernel
functions. The five rebuilt applications and the program recovery counts do
not change.

Create allocates the original owner-tagged `0x5bb`-byte packed window object,
finds an object slot and creates a surface. Mode 3 allocates a screen-sized
backing buffer while retaining the requested logical dimensions. Style
selectors 0 through 3 read the live theme table: themes 1/2/3 use the original
6/28 client offset and 20-pixel title area; themes 4/5 use 8/32 and 24 pixels;
other themes use the full requested client area. Title copy, callback/control
fields, initial caption redraw, table publication and free/count updates keep
their original order. Unwritten payload bytes are not cleared.

Close saves the interrupt flags, calls the live close callback, clears a
matching drag state, activates the preceding surface when required, removes
and refreshes a visible surface, unregisters the object, deletes its children
and allocation, updates the cursor hit target and signals the completion word.
The callback address is at object `+0x5b7`; the completion pointer is at
`+0x55f`, with its completion word at pointer `+8`. Existing cdecl arguments,
fixed entry addresses, object/surface tables and packed offsets remain intact.

Original edge behavior is preserved rather than repaired. Create failures
return with interrupts disabled. Allocation failure still queries the slot;
slot failure frees the allocated object. Surface ownership is accessed before
the negative surface-ID check, including on the original invalid-ID path.
Close retains the original object pointer through callbacks and deletion,
then reads its completion pointer after deletion. The native boundary fixture
also changes that pointer during deletion to verify the read order. These
facts do not guarantee safety for arbitrary corrupt input or concurrent reuse.
The void close API does not promise an incidental EAX result.

`make -C rewrite test-window-lifecycle` passes **1,208 scenarios / 3,624
executions** against original x86, ordinary C and installed C: 715 create and
493 close cases. The suite executes the real owner heap and title copy, with
explicit surface/caption/activation/compositor/registry/delete/hit boundaries.
It compares ordered calls and independent packed-layout, geometry, counter
and flag expectations, plus touched memory, caller arguments, cdecl registers
and x87 controls. Cases include all theme classes, unsigned geometry wrapping,
mode 3, slot/allocation failures, reused payloads, overlapping titles, callback
argument-copy isolation, callback mutation and post-delete notification.

`make -C rewrite test-window-lifecycle-qemu` then runs actual original and
recovered kernels through CMD and emulated PS/2, without replacing those UI
boundaries. The temporary diagnostic X creates/closes all four style selectors,
checks a 1024x768 backing with 120x80 logical size, and installs a real callback
that verifies the object is still live and completion has not yet fired. Each
close must remove its object, call the callback once and signal completion.
The subsequent 31-service diagnostic exercises actual heap, captions,
surfaces, show/move/hide/restore/minimize/remove, dialogs and children. All 33
entry bodies execute; both 67-DWORD reports match independent expectations.
Both complete disposable disks, 33,554,944 bytes each, remain unchanged.
The diagnostic is not a rebuilt application and is not shipped in the image.

The following also pass on the 607-interval kernel:

- 816 image-control scenarios, 2,270 window-service scenarios and 430 X-loader
  scenarios against original/ordinary/installed C.
- Original/recovered image-control QEMU using real FAT, heap and PNG/BMP/JPEG/ICO
  decoding, independent pixels and unchanged disposable disks.
- Production CLOCK: all 607 installed slots match guest memory; 128 MiB RAM,
  59 live heap headers, timer/TSS switches, PS/2/CMD/cpuid, cursor, clock face
  and labels, three changing frames, close/task exit and CMD reuse pass.
- Kernel whole-disk allowed-range verification, five-application installation
  verification and inventory consistency.

Evidence and viewed [CLOCK](../evidence/2026-11-02-window-lifecycle/clock.png)
and [CMD](../evidence/2026-11-02-window-lifecycle/cmd_cpuid.png) screenshots are in
[2026-11-02-window-lifecycle](../evidence/2026-11-02-window-lifecycle/README.md).
The original VHD hash and original logo are retained. The last complete
`make verify` is the [601-interval run](../evidence/2026-11-01-kernel-601-regression/README.md);
607 has the focused validation above, not a new full-suite run. Other live theme
configurations, all display modes, arbitrary corrupt pointers and concurrent interleavings are
outside this evidence. INIT and the kernel remain partial; 32 program paths,
including all four drivers, still use original programs. The complete OS
rewrite is unfinished. Interval counts are not a completion percentage.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| window_create | `0x807a0a` | 986 / 1384 |
| window_close | `0x81021c` | 405 / 510 |
