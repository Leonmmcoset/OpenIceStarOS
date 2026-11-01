# Window, event and image-service recovery

31 additional original entries now execute `kernel/window_services.c`.
Installed intervals increase 570 to 601. Together with the preceding eight
font-name and twelve TrueType text entries, this requested batch recovers
**51 new original entries** from the 550-interval baseline. Prior PNG/inflate
work, test helpers, existing entries and diagnostic programs are not recounted.

The packed object ABI uses the existing `0x6a0000` table, surface table at
`0x500000`, one-based visibility list at `0x510000`, image table through
`0x1b124` and mailbox through `0x1b134`. API addresses, on-disk X binaries,
resource paths, original logo and raw formats remain compatible.

## Behavior

Mailbox publication scans all 256 bytes and waits until empty; copy-out uses
real forward strcpy then sets the caller flag. Launch wrappers publish the
path and create real tasks at the original two loader entries, retaining stack,
segment, priority and name arguments. Those loader task bodies remain original;
recovering these four services does not recover the complete launch pipeline.

Window services draw the nine inclusive frame rectangles and double border,
dispatch three caption implementations by theme style, perform strict signed
title/client/surface hits, find the topmost object's ID, search free slots,
release surfaces/owned icons/object records, set file/pointer icons, restore,
hide, show, remove, move, minimize and forward close. The original so-called
client-hit function samples a single byte even for 32bpp, before rectangle
checks, and performs no lower pointer bounds validation. This is preserved.

Four callback dispatchers retain copied arguments. Movement captures old x/y,
writes new surface/object coordinates, invokes the callback then reloads fields
for damage refresh. Redraw-all reads the live object count, routes captions and
then invokes per-object redraw callbacks. Dialog clicks use strict button
rectangles and set the positive choice word before closing. Image services
search free slots, fill live 8/32bpp width/height/color fields and forward release.
Arithmetic and pointer indexing wrap as 32-bit x86; invalid-pointer safety is
not added. C functions declared void do not promise incidental EAX contents.

## Validation

`make -C rewrite test-window-services` passes **2,270 scenarios / 6,810
executions** against original x86, ordinary C and installed C. Tests cover
signed/wrapped edge coordinates, color-key byte sampling, topmost visibility,
free counters/capacity boundaries, real strcpy and flag aliases, real 8/32bpp
frame/border pixels with an independent drawing oracle, all caption routes,
icon ownership, hidden/active/registered combinations, callback argument-copy
behavior, metadata/caller aliases and asynchronously cleared busy mailboxes.
All touched data pages, caller words, cdecl registers and x87 control compare.

For isolated state/routing scenarios, explicit boundaries record exact calls
to scheduling, icon I/O, legacy captions/close, free and compositor subsystems.
Some boundaries deliberately mutate object fields to check later reloads.
These boundary models are only test fixtures and are not restored code. The
real QEMU test below executes those subsystem calls without interception.

`test-window-services-qemu` starts one diagnostic X via real CMD/PS2 in
original and recovered disposable disks. It executes **all 31 installed
entries and decoded interior instructions**. The independent 51-DWORD report
checks task-created mailbox consumption, strict hits, frame/border/image pixel
hashes, callback args, real heap window/image allocation, show/move/hide/
restore/minimize/remove flags/order, image child-list release and close/dialog
object removal. Temporary static table fixtures are protected by interrupt
masking and restored. Other calls run actual scheduling, captions, heap,
compositing and lifecycle code. The input VHD and both whole disposable disks
remain byte-identical. This diagnostic is not a rebuilt application.

The existing font suites rerun against all 601 installed intervals: 2,973
name cases and 2,837 text cases. Additional text regressions execute legacy
double-byte drawing and colorkey fallback to the desktop background, including
an independent final pixel oracle. The three current suites total **8,080
scenarios / 24,240 executions**. The 430-case X-loader regression also passes.
Production COLOR verifies all 601 loaded slots, 128 MiB RAM, heap, TSS/timer,
PS/2/CMD/cpuid/cursor, 600 palette pixels, window close/task exit and CMD reuse.
Screenshots were viewed. Whole kernel/app disk validation and inventory checks
pass. Original VHD SHA-256 remains
`2d5a7eef4b18012045e25d8263fc4d67e1b3b860d964defa6fac11a57cf52599`.
Evidence: `../evidence/2026-11-01-window-services/`.

Full `make verify` was started for 601 intervals and its outcome will be
recorded separately. At this batch's initial commit its last completed
recorded run remains 456. This focused evidence is not a claim that the whole
OS is recovered: INIT/kernel are partial, five apps are rebuilt, four driver
paths and other apps remain original (32 unrecovered paths in total).
Arbitrary corrupt pointers, nonterminating loops, every possible asynchronous
interleaving and full desktop event-handler logic remain outside coverage.

| Entry | Address | Compiled / slot bytes |
| --- | --- | --- |
| ui_mailbox_put | `0x802e64` | 42 / 98 |
| ui_mailbox_get | `0x802ec6` | 32 / 44 |
| ui_launch_path | `0x802ef2` | 45 / 80 |
| ui_launch_relative | `0x802f42` | 45 / 94 |
| ui_frame | `0x807302` | 387 / 574 |
| ui_caption | `0x807540` | 139 / 238 |
| ui_title_hit | `0x80762e` | 146 / 162 |
| ui_client_hit | `0x8076d0` | 313 / 378 |
| ui_surface_hit | `0x80784a` | 156 / 168 |
| ui_find_hit | `0x8078f2` | 96 / 158 |
| ui_object_slot | `0x807990` | 61 / 122 |
| ui_delete | `0x807f72` | 118 / 140 |
| ui_icon_file | `0x807ffe` | 100 / 146 |
| ui_icon_pointer | `0x808090` | 58 / 90 |
| ui_restore | `0x8080ea` | 192 / 256 |
| ui_hide | `0x8081ea` | 154 / 220 |
| ui_show | `0x8082c6` | 143 / 178 |
| ui_remove | `0x808378` | 223 / 296 |
| ui_event4 | `0x809e24` | 64 / 84 |
| ui_event3 | `0x80ac4e` | 57 / 78 |
| ui_move | `0x80ac9c` | 340 / 436 |
| ui_event3b | `0x80ecba` | 57 / 78 |
| ui_event3c | `0x80ed08` | 57 / 78 |
| ui_redraw_all | `0x80fb12` | 135 / 234 |
| ui_minimize | `0x81002e` | 240 / 314 |
| ui_close_api | `0x810168` | 14 / 20 |
| ui_dialog_click | `0x811c46` | 369 / 512 |
| ui_double_border | `0x811e4c` | 91 / 122 |
| ui_image_slot | `0x81215a` | 65 / 122 |
| ui_image_fill | `0x812748` | 136 / 230 |
| ui_image_release_api | `0x812bfa` | 14 / 20 |
