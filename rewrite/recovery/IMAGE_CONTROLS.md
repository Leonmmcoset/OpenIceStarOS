# Image-control lifecycle recovery

Four original functions now execute `kernel/image_controls.c`, taking the
kernel from 601 to 605 installed intervals. This recovers the create, release,
file-replacement and draw bodies behind the previously recovered image-slot,
fill and public release services. These are kernel entries, not four rebuilt
applications. Window creation/close and several format adapters remain original
at this checkpoint.

The packed control has an owner object at `+4`, child index at `+8`, mode at
`+12`, pixel buffer at `+28`, position and drawn extent at `+32..+44`, allocated
extent at `+48..+52`, depth at `+64`, path at `+84`, source file at `+0x153`
and file length at `+0x157`. Existing API addresses, surface/object/image tables,
owner-tagged heap and raw FAT records remain unchanged.

Create allocates a 351-byte object, selects a slot, appends the owner's child
ID, chooses control-sized storage for modes 0/1 or screen-sized storage for
modes greater than 1, then optionally reads and dispatches uppercase JPG, BMP,
PNG or ICO. File replacement frees the old source, copies the new path and
performs the same format routing. Draw dispatches real placeholder text for
empty/space-leading paths; otherwise it writes live 8/32bpp source pixels to
matching-depth owner surfaces. Loops reload metadata; coordinate/multiplication
arithmetic wraps as 32-bit x86.

Original failure behavior is preserved: create errors omit flag restoration;
some leak allocations or leave appended children. Negative modes can reuse
stale pixel fields because the allocator does not clear payloads. Release of
an empty slot also returns with interrupts disabled. Successful release does
not increment the free-slot count; it shifts one extra child-list word and
decrements the child count even if the ID was not found. File lookup failure
leaves the already-freed source pointer stale. These are compatibility facts,
not fixes or guarantees that malformed caller state is safe.

`make -C rewrite test-image-controls` passes **816 scenarios / 2,448 executions**
against original x86, ordinary C and installed C. It executes real owner-tagged
malloc/free, strcpy and pixel routines, with independent geometry/pixel, object
layout, flags and format-route expectations. Isolated flags/FAT/read/decoder
boundaries explicitly model their contracts; those models are not recovered
components. All touched data/heap pages, caller arguments, callee-saved registers
and x87 controls compare. Scenarios include failures, reused slots/payloads,
child compaction, stale paths, wrapped IDs, metadata writes and buffer overlap.

`test-image-controls-qemu` executes all four original/recovered bodies through
actual CMD/PS2, FAT reads, heap and four codecs on disposable disks. PNG/BMP/ICO
have independently generated constant-color pixel expectations; gray JPEG
allows one channel unit of decoder rounding. It checks decoded and drawn
pixels, owner fields, actual full-screen allocation, child compaction and the
real IF bit on the empty-release path. The diagnostic restores that bit after
observing the original early return. Both whole disposable disks remain
unchanged. The temporary diagnostic X and FAT fixture resources are not rebuilt
applications or shipped product additions.

The 2,270-scenario window-service and 430-scenario X-loader regressions pass.
All 31 window-service bodies rerun in original/recovered QEMU with matching
independent reports and unchanged whole disks. Production COLOR verifies all
605 slots, RAM/heap, TSS/timer, PS/2/CMD/cpuid, cursor, palette pixels and task
exit. Kernel whole-disk installation and inventory checks pass. Evidence and
viewed screenshots are in [2026-11-02-image-controls](../evidence/2026-11-02-image-controls/).

The last complete `make verify` remains the [601-interval checkpoint](../evidence/2026-11-01-kernel-601-regression/README.md);
this focused validation is not a new full-suite run. Unbounded malformed paths,
arbitrary corrupt pointers, all concurrent interleavings and every codec mode
are outside coverage. Five apps are rebuilt; INIT/kernel remain partial and
32 program paths, including four drivers, remain original. The complete OS
rewrite is unfinished. Original VHD and logo are retained.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| image_create | `0x8121d4` | 704 / 1032 |
| image_release | `0x8125dc` | 258 / 364 |
| image_file | `0x81282e` | 312 / 412 |
| image_draw | `0x8129ca` | 372 / 560 |
