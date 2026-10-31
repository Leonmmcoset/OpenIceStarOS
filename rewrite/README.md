# OpenIceStarOS C rewrite

This directory is the incomplete C source recovery for OpenIceStarOS, integrated into copies of the
original IceStarOS VHD. The current build installs 390 declared kernel intervals and
five rebuilt applications. It still requires unrecovered original components;
the interval count is not a count of completely recovered operating-system
modules. The chronological notes below record each source/integration and
verification checkpoint.

The [binary recovery inventory](recovery/INVENTORY.md) lists every original
`.X`/`.DX` path, its current recovery status and boot-stage dependencies.
`make inventory` regenerates it; `make inventory-check` rejects stale records.

From the repository root, `make -C rewrite verify` builds and checks the C
images, and `make -C rewrite run-c-apps` starts the combined application image
in QEMU. Generated images and detailed test evidence are in `rewrite/build/`.

`boot/mbr.c` implements
the first BIOS boot stage; the small assembly entry establishes real-mode
segments and the stack and preserves the boot-drive byte at address 0x7c02,
which the menu accesses directly. The partition table is retained. `boot/menu.c`
implements the boot menu, selection, and loading of the next stage; its timer
polls BIOS ticks rather than installing the original RTC interrupt handler.

`make verify` builds a test disk from the original image and checks its partition
table, signature, and unchanged contents after sector 3. `make run` starts the
test disk in QEMU using a temporary snapshot. GCC with `-m16`, binutils, make,
and qemu-system-i386 are required.

The normal `OpenIceStarOS_c_mbr.vhd` test image replaces only the first two stages. The remainder of the
disk is copied from the original image, including the next boot stage, kernel,
drivers, applications, and resources.

`boot/fat16.c` is a freestanding FAT16 root-file reader. Verification builds it
for 32-bit x86 and uses a host sector reader to recover `INIT.X` from the
original VHD; its SHA-256 is checked against the independently extracted file.

`make run-hybrid` starts a second image whose third stage is now entirely
compiled from C and assembly sources, without including original boot bytes.
C selects VBE/VGA modes and prepares the display; assembly establishes the
GDT, enables A20 and protected mode, and initializes CPU/FPU/SSE state.
C code scans PCI configuration space into
the original low-memory table, loads the BPB,
FATs and root directory into the original memory layout, loads INIT through the
FAT16 reader and ATA PIO code, and prepares the INIT header and stack. Assembly
performs the segment switch and far jump. The menu loads eight sectors for this
hybrid stage.
The menu uses BIOS tick polling and the protected-mode entry uses flat
code/stack selectors, preserving the later startup contract rather than
instruction-for-instruction identity. All later programs in this image
still come from the original disk.
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
temporary disk snapshot, capture `build/boot_openicestaros.png`, and compare the
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
The pointer handler is replaced by an identical callback only inside these
polling tests, isolating its separate input and privileged-instruction behavior.
The twelve-interval image reached the desktop on 2026-10-27. QEMU reported
`EIP=0x8012d1, HLT=1`, confirming execution of the C-generated event loop,
and all twelve installed intervals matched the guest memory dump.

`kernel/pointer.c` recovers mouse input processing at `0x801454..0x801632`.
It drains the original byte queue and packet decoder, preserves wrapped DWORD
coordinate arithmetic and signed clipping, and retains the original press,
release, drag, and click timing rules. The event is dispatched once after
draining the queue when the cursor enables dispatch; the captured record is
cleared even if a callback changes global pointers. The two pointer differential
executables each compare 5,760 cases against original machine code, including
partial packets, all decoder phases, queue wrapping, coordinate and timer
boundaries, cursor enablement, and callback mutations. Only interrupt flag
helpers use identical test stubs; the original queue and decoder execute.
The thirteen-interval image reached the desktop on 2026-10-27, and every
compiled interval matched the guest memory dump.

`kernel/queue.c` recovers byte queue write/read/availability at `0x812e7e`,
`0x812f2c`, and `0x812f72`. Full queues shift the physical byte array rather
than adjusting ring indices, matching the original overflow behavior. Reads
retain the unchecked empty-queue count underflow. `kernel/pointer_decode.c`
recovers the four-phase PS/2 packet decoder at `0x812f96`, including ACK
synchronization, header validation, signed deltas, and inverted vertical motion.
Both queue test executables compare 369,099 queue cases and 1,179,648 decoder
cases against original leaf machine code. Cases cover all read/write indices
for six capacities, empty/full/overfull counts, byte truncation, all 256 packet
headers and input bytes, valid and invalid phases, and unaligned state storage.
The seventeen-interval image reached the desktop on 2026-10-27, with all
installed C intervals matching guest memory.

`kernel/pointer_init.c` recovers pointer initialization at `0x812c70` and
the PS/2 controller wait at `0x812e5e`. It preserves untouched cursor fields,
signed truncation of half-screen positions, byte-wise packet clearing, queue
capacity, click threshold, and the five original controller/device writes.
Both initialization test executables compare 4,704 initialization cases and
1,024 wait cases, including negative/odd/extreme screen dimensions, unaligned
structures, every status byte, delayed readiness, and pointer-global mutation
from port callbacks. Only port I/O is stubbed in those tests; boot uses the
real original x86 port helpers. The nineteen-interval image reached the desktop
on 2026-10-27 and passed all disk and loaded-memory checks.

`kernel/input_queue.c` recovers the separate keyboard byte queue at
`0x819d96..0x819e68` and ASCII uppercase conversion at `0x819e68..0x819e90`.
Keyboard overflow rejects the write without changing memory; unlike the mouse
queue it does not shift stored bytes. Reads retain the original unchecked
empty-queue underflow. Both input differential executables compare 369,099
queue cases and 262,144 full-width character cases against original machine
code, and the shared mouse-queue/decoder tests still pass. All twenty-three
installed intervals passed disk verification and QEMU loaded-memory checks;
the desktop was visually inspected on 2026-10-27.

`kernel/keyboard_init.c` recovers controller readiness, reading/discarding a
response byte, LED control, and keyboard initialization at
`0x819c40..0x819d96`. It preserves the captured queue pointer, the initial
physical read, the complete port transaction order, and signed-byte LED
arguments. Physical accesses still use the original GS-selector helpers.
Each differential executable covers 48 initialization, 4,096 LED, 1,024 wait,
and 256 discard cases, with all byte values, delayed readiness, unaligned
queues, and global-pointer mutation from I/O and physical-memory callbacks.
The twenty-seven-interval image passed the full verification suite and QEMU
loaded-memory checks and rendered the desktop on 2026-10-27.

`kernel/keyboard.c` recovers scancode translation, queue draining, modifier and
lock updates, and the read/translate/draw-character API at
`0x819e90..0x81a240`. It retains the original 52/66-pair search lengths,
left/right Shift selection, unconditional Caps conversion of mapped letters,
first-match lookup, full-width scancode comparison, and the original nonzero
guard for E0/E1 prefix state. Physical accesses retain their GS helper calls;
the five rendering arguments and original `%c` format address are preserved.
Each differential executable compares 28,704 translation, 60,096 modifier,
11,648 character-API, and 13,086 drain cases. The tests include every scancode
byte, 32-bit boundary inputs, all modifier combinations, every LED byte,
callback mutations, unaligned state, duplicate/NUL mappings, search limits,
and queue wrapping. Only physical accesses, LED I/O, and text rendering use
shared test callbacks; the real queue routines and ASCII conversion execute.
All thirty-one installed intervals passed full disk verification and QEMU
loaded-memory checks, and the desktop was visually inspected on 2026-10-27.

`python3 tools/boot_smoke.py --input-check` additionally moves the emulated
PS/2 pointer to the CMD icon and double-clicks it, types `cpuid`, and presses
Return. It reads CMD directly from FAT16, distinguishes relocated executable
instructions from cached file bytes, verifies the application's input buffer,
checks consumption of all ten make/break bytes, and checks command-buffer
clearing and return to the prompt. Mouse position is checked against guest
state, rather than inferred from injected motion. Evidence is written to
`build/input_smoke.json` and `build/input_{desktop,cmd,typed,cpuid}.png`.
The result text still requires visual inspection. On 2026-10-27 the test passed
and the final screenshot showed the CPU identification result and a new prompt.
This exercises the recovered keyboard/pointer path with the original desktop
and CMD application; those applications have not yet been rewritten.

`kernel/cpu.c` supplies the thirteen ISA primitives at `0x81a240..0x81a2aa`:
halt, interrupt enable/disable, three NOPs, EFLAGS read/write, byte/word/DWORD
port I/O, and the original far transfer to `0x40:0x9000`. These hardware
operations require inline assembly. Ordinary C functions implement the small
instruction boundaries where possible; naked port definitions preserve EDX
and the original partial-EAX write behavior. Manifest `exact` entries require
every compiled instruction byte to match the original, as well as enforcing
fixed addresses and sizes. Thus compiler changes cannot silently alter flags,
register preservation, or port widths. All forty-four installed intervals,
the full differential suite, and the interactive CMD smoke test passed on
2026-10-27; the CPU result screenshot was inspected. This is still a hybrid
image and not a complete C recovery of the OS.

`kernel/physical.c` recovers all six GS-selector physical byte/word/DWORD
read/write boundaries at `0x83735b..0x8373ee`. They temporarily select the
flat physical descriptor (selector 8), restore GS and EBX, and retain the
original flag and partial-EAX behavior. Naked inline assembly is necessary
for these segment-sensitive hardware operations. `kernel/cpu.c` also recovers
CR0 read/write at `0x83743c..0x837448`. All eight new entries use exact-byte
manifest verification and fixed-address/size linker assertions. The previous
physical-access linker aliases now resolve to the compiled definitions.
All fifty-two installed intervals passed full disk verification and the
interactive QEMU input smoke test on 2026-10-27; the CPU output and returned
CMD prompt were visually inspected. The adjacent RAM probe and paging setup
are still original code; recovery of these primitives does not recover them.

`kernel/service_forward.c` recovers the first twenty service-table forwarding
interfaces at `0x8374cf..0x837967`. The shared table pointer remains at
`0x1b0e4`; each interface copies its original argument list, calls the original
slot unconditionally, and returns the callback's raw DWORD result. Slots 6 and
12 sign-extend their final byte argument. Sibling-call optimization is disabled
so a callback writing its own argument stack cannot modify the outer caller's
arguments. Slot numbers are used as names until service meanings are established.
Each of `kernel_service_forward_test` and `kernel_service_forward_linked_test`
compares 10,112 cases with the original machine code, checking argument order,
byte truncation, return bits, callback counts, stack isolation, and mutations of
shared memory, the table pointer, and the active slot. All seventy-two installed
kernel intervals passed full disk verification and the interactive QEMU input
smoke test on 2026-10-27. The CPU identification output and fresh CMD prompt
were visually inspected. Later optional service slots remain original code.

`tools/xformat.py` builds type-`0x50` X applications from linked i386 ELF files
made with `ld --emit-relocs`. It preserves the observed 64-byte header contract,
records each `R_386_32` location, and leaves `R_386_PC32` references relative.
The kernel loader at `0x8023df..0x8024aa` adds the load base at those locations,
stores it in header field `0x1c`, then erases the relocation table, BSS, and
stack area. BSS therefore begins at the relocation-table file offset. The tool
rejects unsupported relocation types and external/absolute symbol references.
`tests/xformat.py` compares loaded code/data against three independent links,
including unaligned load bases, verifies the original SNAKE.X header/relocations,
and rejects malformed input. This format-tool checkpoint does not yet replace
an application in the disk image.

`apps/snake.c` recovers all seven SNAKE.X application routines: cell drawing,
food generation, reset, movement/collision, rendering/score formatting, keyboard
handling, and the complete application entry/event/timer loop. Its initialized
data, strings, BSS layout, 16-page stack request, and 2,852-byte X file size
match the original contract. Twelve used syscall boundaries in `apps/xapi.c`
are compiled from naked C definitions with the necessary interrupt assembly;
each exactly matches the original 14-byte stub. Unused SDK stubs are omitted.
`make build/snake.x` links only recovered sources and packages 96 compiler
relocations rather than copying any original application code.

`tests/snake.py` extracts the reference application from the original VHD,
validates its SHA-256, and executes both original and fully linked recovered
images at aligned and unaligned bases. The freestanding test compares 258,392
cases covering full-byte and full-width keyboard inputs, signed/wrapping
coordinates, all directions, food/collision paths, lengths 0 through selected
out-of-bounds values, score formatting, callback mutations, initialization
failure, and event/timer-loop execution. It compares the complete affected
state plus ordered syscall arguments, strings, and event-array observations.
Tests retain the original unchecked body-array overflow, draw-food-before-clear
reset order, and missing digits for negative scores. Interrupts use identical
test callbacks; actual OS loading and rendering require the separate QEMU
integration check. The complete verification suite passed on 2026-10-27.

`make run-c-snake` boots `build/OpenIceStarOS_c_snake.vhd`, which installs the
compiled Snake into its existing FAT16 cluster chain. `apps/snake_image.json`
records the original file hash and explicit short-name path. The installer
preserves both FAT copies, directory entries, allocation slack, and every byte
outside the replaced application. `tests/app_image.py` verifies those complete
disk invariants, multi-cluster replacement, and rejection of invalid inputs.

`make smoke-snake` uses QEMU snapshot mode, opens CMD through real PS/2 mouse
input, runs `cpuid`, changes to `C:\ICESTAR\GAMES`, and launches `snake.x`.
It compares the running relocated C instructions and strings with the linked
image, then verifies wall collision, R restart, Down-arrow movement, and an A
turn. The original keyboard translator maps E0 arrow scancodes to keypad
digits when Num Lock is enabled; the test disables that mode through the
emulated Num Lock key before testing the scan-only arrow path. It changes no
guest code or game state. Evidence and screenshots are written to
`build/snake_smoke.json` and `build/snake_*.png`. The game still depends on the
partially recovered kernel, original window system, desktop, and CMD.

The packager also accepts `--kind 0x0a` for the older shared-address-space
application format used by COLOR.X. The original kernel loader at
`0x80229a..0x8023ab` performs the same base relocation, header update, and
relocation/BSS/stack clearing as its `0x50` branch, then uses a different task
startup path. Both types are compared with independent ELF links at three
load bases. The original COLOR.X header, seven relocation entries, and
unrelocated palette-pointer address are checked directly. Installation now
requires preserving application type in addition to BSS size and stack pages.

`apps/color.c` recovers all three COLOR.X application routines: entry at
`0x40`, close callback at `0xe6`, and the 30-by-20 palette drawing loop at
`0xfe`. `apps/legacy_api.c` recovers its six reachable shared-service-table
wrappers. No original application instruction bytes are linked. The old
program's unused SDK functions are omitted. The compiled file retains type
`0x0a`, 14,516-byte file size, seven relocations, original GBK title at `0x3890`,
16-byte BSS at `0x3898`, and 16 stack pages. `make build/color.x` builds it.

`tests/color.py` executes original and recovered binaries at aligned and
unaligned bases with the real SDK forwarding code. Its 2,304 cases compare
ordered service arguments, complete BSS, window records, palette memory,
global table pointers, and close return bits. Cases include success/failure
handles, wrapped window-pool indices, all 600 drawing operations, callback
registration and dispatch, service-table switching, palette changes during
drawing, and callback mutations of the active surface and window record.
Only the OS services themselves use shared test callbacks. This checkpoint
builds and differentially verifies COLOR; guest installation follows separately.

`make run-c-apps` boots `build/OpenIceStarOS_c_apps.vhd` with recovered Snake,
Color, MOV, and StarScr installed from `apps/images.json`. The earlier Snake-only image is
retained as a separate checkpoint. The five installer tests restore every
replaced file and require the complete restored disk to match its source;
FAT entries, directories, allocation slack, and the VHD footer stay unchanged.

`make smoke-color` launches `color.x` through the original CMD using real PS/2
input. It verifies the relocated C instructions and initialized data, all seven
relocations, the registered compiled close callback, and one center pixel in
each of the 600 drawn rectangles against the live guest palette words. The
original app reads 600 consecutive words, including the mostly black later
entries; the rewrite preserves that behavior. The test moves the actual guest
pointer to the rendered close button, clicks it, checks removal from the window
pool and task termination, and runs `cpuid` again in CMD. It changes no guest
code or application state. Evidence is in `build/color_smoke.json` and
`build/color_{launched,closed,cmd_after_close}.png`. The full verification suite
and Color smoke test passed on 2026-10-27; all three screenshots were inspected.
These applications still run on the partially recovered kernel and original
desktop, window system, drivers, and CMD.

`apps/mov.c` recovers MOV.X's entry/animation loop at original offset `0x40`
and close handler at `0x262`. Its eight reachable SDK boundaries compile from
`legacy_api.c` and `legacy_image_api.c`; unused function sections are discarded.
`make build/mov.x` links only recovered C code and produces the original
type-`0x0a`, 14,932-byte file contract, 16 relocations, 16-byte frame-handle
BSS at `0x3a14`, 16 stack pages, and byte-identical GBK title and four resource
paths at `0x3990`. The four original JPEG files remain external resources.

`tests/mov.py` executes both X images, including their unmodified SDK wrappers,
at aligned and unaligned load bases. Its 40,960 differential cases compare
ordered service calls, selected service-table targets, strings and pointer
offsets, full BSS, window records, display/palette memory, global pointers,
timer state, and the close handler's returned bits. Cases cover signed and
wrapping window handles, negative/odd display dimensions, unsigned timer
boundaries and wraparound, multiple frame cycles, callbacks that change
frames/ticks/window records/service tables, and close dispatch during polling.
The original behavior of entering the animation loop after window-creation
failure is preserved. This checkpoint builds and differentially tests MOV;
installation and real guest execution are a separate integration step.

`make smoke-mov` launches the compiled MOV through CMD with real PS/2 input.
It verifies all 16 runtime relocations and the compiled close callback, reads
the four decoded image buffers from the guest, and compares every pixel of
each displayed 380-by-240 frame with those buffers. Image slot zero is valid
in the original allocator. All four distinct frames must appear before the
test clicks the close button and checks the window slot, four image slots,
and task are released. A subsequent `cpuid` command must return to a fresh CMD
prompt. The test changes no guest code or application state. Evidence and
screenshots are in `build/mov_smoke.json` and `build/mov_*.png`. The smoke test
passed on 2026-10-27; frame and post-close screenshots were inspected. JPEG
decoding, window composition, desktop, and CMD still use original code.

`apps/starscr.c` recovers STARSCR.X's entry at `0x40`, keyboard handler at
`0x160`, close handler at `0x180`, and four-phase drawing routine at `0x198`.
Its nine reachable SDK calls compile from the three `legacy_*_api.c` files;
four newly recovered wrappers are in `legacy_draw_api.c`. `make build/starscr.x`
links only C code, producing the original type-`0x0a`, 16,052-byte file size,
16-page stack, GBK title at `0x3dd0`, and 16-byte BSS at `0x3ddc`. The compiler
needs 21 relocations rather than the original 54; their count is generated
from ELF references, and the loader's memory contract is preserved.

`tests/starscr.py` executes original and compiled X binaries with their real
SDK wrappers at aligned and unaligned bases. Its 75,520 differential cases
compare ordered service calls, selected table targets, full BSS, window records,
display memory, callback registration, and defined return values. Cases cover
all four phases and out-of-range phases, negative/odd/extreme dimensions,
signed and wrapping window handles, twelve-frame loops, all byte-valued keys
and selected full-width keys, full-width press/release values, Esc-triggered
close dispatch, service-table switching, caller-argument mutations, and
callbacks changing record/surface/dimensions. Signed division truncates toward
zero and coordinate additions wrap. The original unchecked failure-path record
dereference is retained. This checkpoint builds and differentially tests the
screen saver; installation and real guest execution follow separately.
The full verification suite passed on 2026-10-27.

`make smoke-starscr` installs the compiled screen saver and launches it through
CMD using real PS/2 input. It verifies relocated instructions/data, all 21
relocations, both compiled callbacks, the full-screen geometry and window mode,
then compares 10,797 screen pixels in each of the four animation phases against
the guest palette. Probes omit a three-pixel band around circle boundaries,
whose edge rounding belongs to the unrecovered kernel rasterizer. A real Esc
key must remove the window from its pool and terminate its task. Evidence is
in `build/starscr_smoke.json` and `build/starscr_*.png`.

Post-exit desktop input is a known limitation, not a successful reuse test:
mouse movement remains queued and the pointer stays still.
`make smoke-starscr-reference` repeats the same test on the untouched original
VHD, in snapshot mode, and records the same limitation separately in
`build/starscr_reference_smoke.json` and `build/starscr_reference_*.png`.
No guest memory or code is modified by either smoke test. Animation, Esc, and
window/task cleanup passed on 2026-10-27; desktop reuse after this application
is not verified. The original close/scheduling implementation remains in use.

`kernel/runtime.c` recovers palette lookup (shared slot 237) at
`0x81b714..0x81b734` and tick waiting (slot 113) at
`0x82ad81..0x82adab`. Palette indexing remains unchecked, including unaligned
loads and 32-bit address arithmetic. Waiting retains the unsigned absolute
deadline comparison, its addition-wraparound early-return behavior, and the
last observed tick as its return value. Task yield at `0x81434a` still calls
the original scheduler. Both host-linked and fixed-address differential tests
pass 4,224 palette and 2,304 wait cases each. They compare memory, return
values, and ordered yield traces, including callbacks that advance, stall, or
reverse time and mutate shared globals. Only the task-yield boundary is
stubbed in these tests. The full verification suite passed on 2026-10-27;
the 74-interval image also passed the STARSCR animation, Esc, and task-cleanup
checks with every installed interval matching guest memory. The previously
documented post-STARSCR mouse-input limitation remains reproducible.
MOV also passed all four pixel-exact frames, PS/2 close, image/task cleanup,
and a subsequent CMD command on this image; its post-close screenshot was
visually inspected.

`kernel/tasks.c` recovers cooperative task yield (shared slot 108) at
`0x81434a..0x814536`. It preserves the 108-byte task records, captured task
table, flags/interrupt ordering, FPU save/initialize/restore branches,
callback-sensitive next links, scheduling-counter wraparound, and TSS selector
arithmetic. The compiled routine occupies 196 of the original 492 bytes.
`kernel/cpu.c` also recovers the five-byte far TSS jump at `0x828d2b` and
three-byte `fninit` boundary at `0x837347`; both match the original bytes.
On a real task switch the old C stack is saved by the processor, and the
routine restores its saved flags when that task resumes. Timer scheduling,
task creation/destruction, and the FPU save/restore implementations remain
original code.

`kernel_tasks_test` and `kernel_tasks_linked_test` each pass 165,888 original/C
differential cases. They compare the full task, service-table, and low-memory
regions plus ordered boundary traces across wrapped full-width indices,
aligned/unaligned tables, all state branches, counter boundaries, flags, and
64 callback-mutation combinations. Both binaries execute their real service
forwarders; only privileged flags, FPU reset, and far jump use common host
callbacks. Test-only trampolines never enter the disk image. The complete
verification suite passed on 2026-10-27. The resulting 77-interval image also
passed STARSCR's four pixel-probed phases, Esc exit, and window/task cleanup
in QEMU, with all installed intervals matching guest memory. Its first-phase
screenshot was inspected; the existing post-STARSCR mouse-input limitation
remains.
MOV on the same image passed all four pixel-exact frames, PS/2 close,
image/task cleanup, and a subsequent CMD `cpuid` command. Its post-close
screenshot was visually inspected. Logs are `build/verify_tasks.log`,
`build/starscr_tasks_smoke.log`, and `build/mov_tasks_smoke.log`.

`kernel/tasks.c` now also recovers the interrupt-driven task switch at
`0x814176..0x81434a` and timer tick entry at `0x813a5e..0x813a8c`.
The switch preserves the original captured table/index, FPU branches,
callback-sensitive next links, counter wrapping, and hardware TSS jump.
Unlike cooperative yield, this path neither saves/restores flags nor executes
CLI. The tick increments the IRQ counter, signals the shared event record,
then increments the tick and schedules only when the unsigned task high-water
mark exceeds one. Its C body needs 49 bytes, beyond the original 46-byte slot;
a five-byte C-generated tail-jump remains at the original entry, and the body
uses spare space in the timer-switch interval. That interval occupies 216 of
468 bytes, including both functions. Original IRQ register preservation,
PIC acknowledgement, and IRET remain outside these replacements.

Both task differential executables now test cooperative yield, timer switching,
and the complete tick entry, with 165,888 cases per path per executable
(995,328 comparisons altogether). Tick cases include IRQ/tick/high-water
boundaries, wraparound, unaligned event records, and event records aliasing
those globals or task fields. Memory and ordered callback traces match the
original machine code. The complete verification suite passed on 2026-10-27.

`make smoke-tasks` observes live timer preemption without changing guest code,
registers, or data. Paused TSS records must contain the real far-jump return
EIP, a saved stack pointing back into compiled C scheduling code, and the
expected far-pointer argument ABI. The tick must also advance. This passed on
the 79-interval image: task 0 was suspended inside `kernel_schedule_task`,
task 2 inside `kernel_yield_task`, and the two recorded samples advanced by
12 ticks. Evidence is `build/task_smoke.json`; the run also verified real PS/2
pointer input, CMD text entry, and prompt return.
The same image passed STARSCR's four phases, Esc and task cleanup, plus MOV's
four pixel-exact frames, close, image/task cleanup, and subsequent CMD `cpuid`.
Representative STARSCR and post-MOV screenshots were inspected. The existing
post-STARSCR mouse-input limitation remains; task creation/destruction and
FPU save/restore still use original implementations. Logs are
`build/verify_timer.log`, `build/tasks_preemption_smoke.log`,
`build/starscr_timer_smoke.log`, and `build/mov_timer_smoke.log`.

`kernel/task_create.c` recovers task subsystem initialization, free-slot search,
and nonpaged task creation at `0x814536..0x814a56`. Their compiled sizes are
156/208, 66/130, and 397/974 bytes. Initialization installs the original task
table and GDT, constructs the boot task, and loads its task register. Creation
preserves captured-table timing, signed owner-pointer handling, unchecked name
copying, linked-list writes, all DWORD TSS stores, descriptor arguments, and
high-water/hole accounting. `kernel/cpu.c` now provides the LGDT and LTR
boundaries at `0x8372e8..0x8372fe`; both match the original machine code exactly.
Descriptor construction, string copying, paged creation, and task deletion
still use original routines at this checkpoint.

Both construction differential executables pass 108 search, 3,840 creation,
and 3,840 full-initialization cases each (15,576 comparisons in total). The
tests execute the actual free-slot search and task/TSS construction, compare
all 319,488 mapped data bytes and ordered boundary traces, and cover unaligned
tables, failed creation, reused holes, the real 1,820-slot capacity, full-width
unsigned search boundaries, signed owner pointers, overlapping metadata/TSS
storage, unchecked long names, and 64 callback-mutation combinations.
Only flag/hardware, descriptor, and string boundaries are shared test callbacks.

The full verification suite passed on 2026-10-27. The 84-interval image passed
real TSS timer-preemption checks (12 ticks between samples), PS/2 and CMD input,
MOV's four pixel-exact frames, close, image/task cleanup and subsequent `cpuid`,
plus STARSCR's four phases, Esc and task cleanup. Every installed interval
matched guest memory. MOV's post-close screen and STARSCR's first phase were
visually inspected. The known post-STARSCR mouse limitation remains. Logs:
`build/verify_task_create.log`, `build/task_create_preemption_smoke.log`,
`build/mov_task_create_smoke.log`, and `build/starscr_task_create_smoke.log`.

`kernel/task_lifecycle.c` recovers application-task creation at
`0x814a56..0x814e54`, task deletion at `0x814e54..0x815002`, thread creation
at `0x815002..0x8150aa`, and the active-task query at `0x8150aa..0x815106`.
Their compiled sizes are 448/1,022, 139/430, 82/168, and 47/92 bytes.
The application constructor is used by the X program loader. Both it and the
ordinary task constructor set CR3 to the same `0x19000`; the earlier historical
description of this variant as "paged creation" did not establish separate
address spaces. Its differing TSS fields, rounded allocation extent with DWORD
wraparound, and allocator-before-flags ordering are retained.

Deletion preserves unchecked indices, link reloads after aliasing writes,
free-before-tail-update ordering, repeated-deletion behavior, and wrapping hole
accounting. Thread creation retains the original table snapshot across memory
allocation and nested task creation, including zero allocation results and the
original lack of rollback when task creation fails. No new safety guards are
added to these recovered routines. The three allocator routines, descriptor
emitter, and string copier remain original implementations at this checkpoint.

The lifecycle differential executables execute the real free-slot search and
both constructors. Each exercises 7,680 application creations, 2,304 deletions,
7,680 thread creations, and 7,436 active-state queries (50,200 comparisons
across host-linked C and fixed-address C). Every case compares all 319,488
mapped data bytes and ordered boundary traces. Cases cover allocation-size
rounding/wraparound, unaligned tables, failed creation, reused holes, signed
owner pointers, TSS/metadata aliasing, self-linked and wrapped task indices,
callback mutations at allocator/flags/string/descriptor/free boundaries,
thread stack-size wraparound, and negative/out-of-range active queries with
an invalid table pointer that must never be dereferenced.

The complete verification suite passed on 2026-10-27. The 88-interval image
passed QEMU's real TSS preemption checks with 12 ticks between samples, PS/2
and CMD input, MOV's four pixel-exact frames, close, image/task cleanup and
subsequent `cpuid`, and STARSCR's four phases, Esc and task cleanup. Every
installed interval matched guest memory. The post-MOV command screen and first
STARSCR phase were visually inspected. The existing post-STARSCR mouse-input
limitation remains; desktop reuse after that screen saver is not claimed.
Logs: `build/verify_task_lifecycle.log`, `build/tasks_lifecycle_smoke.log`,
`build/mov_lifecycle_smoke.log`, and `build/starscr_lifecycle_smoke.log`.
Representative JSON and image evidence is preserved under
`build/task_lifecycle_evidence/`.

`kernel/segments.c` recovers segment-descriptor emission at
`0x8135c6..0x81364c` in 84 of the original 134 bytes. It retains the granularity
threshold, unchecked base/access fields, and two WORD stores followed by four
BYTE stores. `kernel/strings.c` recovers seven routines at
`0x82a32b..0x82a531`: string copy (41/66 bytes), text-prefix copy (92/148),
bounded copy (55/92), length (22/36), equality (44/58), character append
(47/64), and string append (43/54). Task construction now uses these compiled
descriptor and string implementations in the disk image.

These APIs differ from libc: ordinary copy returns one byte beyond the copied
terminator, bounded copy returns the terminator address, equality returns
boolean 1/0, and character append uses a signed bound comparison. Text-prefix
copy retains the original rule that exactly one byte above `0x9f` removes the
last copied byte. Null handling, repeated volatile byte reads, unchecked
overlap, and DWORD address/bound wraparound remain unchanged.

Each helper differential executable compares original machine code against
host-linked or fixed-address C in 170,568 cases: 63,648 descriptor emissions,
1,716 copies, 31,680 text-prefix copies, 30,888 bounded copies, 330 length
queries, 1,386 equality queries, 38,016 character appends, and 2,904 string
appends (341,136 comparisons altogether). Every case compares return values
where defined and all 8,192 mapped data bytes. No helper is mocked. Tests cover
unaligned descriptors, full-width limits/access fields, ASCII and high bytes,
null/invalid pointer combinations, signed bounds, forward and same-pointer
overlap, finite backward overlap in bounded copies, and terminators directly
before an inaccessible guard page. Inputs that make the original unbounded
copy run forever are not used.

The complete verification suite passed on 2026-10-27. The 96-interval image
passed real TSS preemption checks with 12 ticks between samples, PS/2 and CMD
input, MOV's four pixel-exact frames, close, image/task cleanup and subsequent
`cpuid`, and STARSCR's four phases, Esc and task cleanup. All installed
intervals matched guest memory. MOV's post-close screen and STARSCR's first
phase were visually inspected. The existing post-STARSCR mouse-input
limitation remains. Allocation/free, memory probing/heap initialization, and
unrecovered kernel/driver/application routines still use original code.
Logs: `build/verify_helpers.log`, `build/tasks_helpers_smoke.log`,
`build/mov_helpers_smoke.log`, and `build/starscr_helpers_smoke.log`.
Representative JSON and image evidence is preserved in
`build/helpers_evidence/`.

`kernel/memory.c` recovers six heap services at `0x815464..0x815a90`:
ordinary allocation (230/358 bytes), explicit-owner allocation (225/350),
aligned allocation (309/592), free (95/146), allocation size (26/56), and
per-owner allocation total (59/78). Task constructors and deletion now use
these compiled allocation/free routines. The heap starts at 32 MiB and uses
16-byte next/previous/used/owner headers.

The recovered code preserves first-fit order, `(size + 0x40f)` rounding with
DWORD wraparound, the strict `> 0x40f` split threshold, store/reload ordering,
and the original incomplete previous-link maintenance. Aligned allocation
retains unsigned size division, signed address division, discarded prefixes,
and the unconditional write through the previous-header pointer. Null previous
pointers and invalid divisors are not silently repaired. Free performs the
original single previous/next merge and does not repair successor back-links.
Size queries ignore used state; owner totals count headers and require used
to equal exactly one.

Each memory differential executable passes 7,392 ordinary allocations, 7,392
explicit-owner allocations, 59,136 aligned allocations, 2,464 frees, 2,640 size
queries, and 528 owner totals (159,104 comparisons across host-linked and
fixed-address C). Every case compares return values, all 151,552 mapped data
bytes, and ordered flag/CLI callback traces. Layouts cover empty/occupied
tails, exact fits, split boundaries, unaligned and nonmonotonic links,
header aliasing, unsigned span/size wraparound, non-power-of-two alignment,
and callback changes to owners, memory limits, and heap state. Additional
forked comparisons verify SIGFPE for zero alignment and signed quotient
overflow, and SIGSEGV for a null previous-header pointer. Short trampolines
keep the tiny flag primitives from overlapping in the test fixture; a second
mapped arena captures actual 2 GiB-offset split headers from large unsigned
spans rather than excluding those cases.

The complete verification suite passed on 2026-10-27 with all 102 replacement
intervals. QEMU passed real TSS switching with 12 advancing timer ticks,
PS/2 and CMD input, MOV's four pixel-exact frames, close, image/task cleanup
and subsequent `cpuid`, and STARSCR's four phases, Esc and task cleanup.
All installed intervals matched guest memory. MOV's post-close CMD screen
and STARSCR's first phase were visually inspected. The original post-STARSCR
mouse-input stall remains; desktop reuse after that application is not
verified. Logs: `build/verify_memory.log`, `build/tasks_memory_smoke.log`,
`build/mov_memory_smoke.log`, and `build/starscr_memory_smoke.log`.
Representative JSON and image evidence is preserved in
`build/memory_evidence/`. Memory probing/heap initialization and other
unrecovered routines still use original code at this stage.

`kernel/memory_init.c` recovers the RAM-probe wrapper at
`0x815320..0x8153d4` (133/180 bytes) and heap initialization at
`0x8153d4..0x815464` (86/144). The wrapper tests EFLAGS.AC, conditionally
disables caches through CR0, scans pages from 4 MiB, and rereads CR0 before
clearing its cache-control bits. Heap initialization retains the original
store order, signed division of detected bytes by 4096, and untouched owner
field of the first heap header.

`kernel/memory_probe.c` rebuilds the segment-sensitive physical-page test
at `0x8373ee..0x83743c` with all 78 original bytes. It saves and restores
each tested page's final DWORD around complementary write patterns, using
DS=8 for physical memory. The original bounded-success exit is missing
`POP EDX` and unbalances the stack; this defect is documented and preserved.
Normal boot scans to `0xffffffff` and returns through the failed-write exit.
The two duplicate CR0 entry points at `0x8372fe..0x83730a` are also rebuilt
from the existing CPU primitives, with exact-byte checks.

Both memory-initialization differential executables pass 65,536 probe-wrapper
and 65,536 heap-initializer cases each (262,144 comparisons altogether).
Only flags, CR0, and the physical page-test boundary use shared callbacks;
the complete high-level routines execute. Every case compares all 8,192
mapped data bytes, defined results, and ordered callback traces. Inputs
cover AC availability, full-width detected sizes, signed page counts,
patterned owner fields, and all 32 callback mutation modes. The physical
page test itself is verified byte for byte, rather than run in user mode.
`make smoke-memory` adds read-only QEMU checks for the detected RAM limit,
page metadata, heap origin, and every header in the live next chain,
together with real TSS switching and PS/2/CMD input checks. Previous links
are recorded without imposing invariants the original allocator does not
maintain.

The complete verification suite passed on 2026-10-27 with all 107 replacement
intervals. In three QEMU boots the compiled probe detected 134,217,728 bytes
(32,768 pages), a 4,096-byte page size and a heap origin of 32 MiB; the live
next chain contained 59 valid headers, including 57 allocated headers. Real
TSS switching, PS/2 and CMD input, MOV's four exact animation frames, window
close and subsequent `cpuid`, and STARSCR's four phases, Esc and task cleanup
passed. All installed kernel intervals matched guest memory. MOV's post-close
CMD screen and STARSCR's first phase were visually inspected. The original
post-STARSCR mouse stall remains. Logs: `build/verify_memory_init.log`,
`build/memory_init_smoke.log`, `build/mov_memory_init_smoke.log`, and
`build/starscr_memory_init_smoke.log`; representative evidence is preserved
in `build/memory_init_evidence/`. Other unrecovered kernel, driver and
application routines still use original code.

`kernel/memory_ops.c` recovers seven memory services: the overlap-safe byte
move at `0x815a90` (70/114 bytes), byte fill at `0x815b02` (25/42), the
fixed 1,200-byte copy at `0x815b2c` (31/6,134), blocked DWORD copy at
`0x817322` (115/184), difference query at `0x817548` (45/72), and the
allocation/free wrappers at `0x817590` (5/20) and `0x8175a4` (5/28).
The large original copy unrolls 300 DWORD transfers; the C loop retains
forward DWORD access order, including propagation through overlapping and
unaligned ranges. The blocked copy takes a signed DWORD count, splits it
into 300-element blocks and a remainder, and performs no transfers for
negative counts. The difference query returns exactly zero or one, rather
than an ordering result, and does not access memory for nonpositive counts.
The byte move retains unsigned, wrapping overlap checks and returns the
original destination; fill truncates its value to the low byte.

The two memory-operation differential executables each pass 5,792 moves,
69,504 fills, 288 fixed-block copies, 6,592 blocked copies, 2,576 comparison
queries and 384 cases per allocation/free wrapper (171,040 comparisons
altogether). Every case compares all 16,384 mapped bytes, defined results,
and wrapper callback traces. Cases cover all four alignments, 18 valid
layouts, both overlap directions, identical pointers, exact block and
remainder boundaries, patterned/zero/high-byte data, low-byte truncation,
invalid pointers with zero/nonpositive counts, early comparison exit, and
source/destination boundaries adjoining inaccessible guard pages. Copy and
comparison helpers are never mocked; wrapper tests stub only their already
separately verified allocation/free boundaries.

The complete verification suite passed on 2026-10-27 with all 114 replacement
intervals. Three QEMU boots passed RAM/page/heap-chain checks (128 MiB and
59 or 60 live headers), real TSS switching, PS/2 and CMD input, MOV's four
pixel-exact frames, close and subsequent `cpuid`, and STARSCR's four phases,
Esc and task cleanup. All installed kernel intervals matched guest memory.
MOV's post-close CMD screen and STARSCR's first phase were visually inspected.
The original post-STARSCR mouse stall remains; desktop reuse after that
application is not verified. Logs: `build/verify_memory_ops.log`,
`build/memory_ops_smoke.log`, `build/mov_memory_ops_smoke.log`, and
`build/starscr_memory_ops_smoke.log`; representative evidence is preserved
in `build/memory_ops_evidence/`. Unrecovered routines continue to use the
original binary; this is not yet a complete OS build from source.

`kernel/memory_accel.c` rebuilds the SSE DWORD dispatcher at
`0x8173da..0x817484` (89/170 bytes). It uses an arithmetic block-count shift,
a signed remainder, a real call to the 16-byte SSE helper, and forward DWORD
tail transfers; negative counts perform no accesses. `kernel/memory_transfers.c`
rebuilds six REP copy/fill leaves at `0x817484..0x817548` and two SSE leaves
at `0x828e8f..0x828eeb`, with exact-byte assertions for all eight routines.
The REP leaves clear DF and update the caller's destination/count argument
slots (and source slot for copies). Word and DWORD counts are elements.
Byte/word fills retain truncation and the original arithmetic flag effects.
The explicit REP prefix before the word instructions preserves the original
`F3 66` order, which GAS otherwise reverses.

SSE copies retain unaligned, forward 16-byte access order, XMM register
clobbers, saved GPRs, and incoming DF. Their loops execute before decrementing
ECX: a zero block count is not a no-op. The 64-byte helper interleaves four
loads/stores rather than reading a whole block first, so overlapping ranges
can propagate data within each block. These behaviors are preserved.

Each accelerated-memory differential executable passes 139,152 normal cases:
15,936 dispatcher, 11,904 per REP copy, 12,528 per REP fill, and 24,960 per
SSE leaf. Host-linked and fixed-address tests execute the actual original
and rebuilt transfer helpers. A fixed-stack assembly harness compares all
16,384 data bytes, the leaf GPRs, defined arithmetic flags and DF, caller
argument slots, and all eight XMM registers. The dispatcher is checked for
memory, callee-saved GPRs, stack balance, DF and XMM state; caller-volatile
GPRs, arithmetic flags and argument slots are not C ABI outputs. Cases cover
both DF inputs, patterned/zero/high-byte buffers, word/DWORD unaligned
pointers, all 16 SSE alignments, 21 overlap offsets, signed dispatcher
boundaries, zero counts, and inaccessible source/destination guard pages.
Each executable also passes 96 forked fault cases, comparing SIGSEGV and
all shared buffer bytes written before the fault. These include zero-count
SSE execution and huge REP/SSE counts; the zero-count SSE cases must produce
actual writes before faulting. Normal comparisons total 278,304 across the
two executable variants, plus 192 fault comparisons.

The complete verification suite passed on 2026-10-27 with all 123 replacement
intervals. Three QEMU boots passed RAM/page/heap-chain checks (128 MiB and
59 live headers), real TSS switching, PS/2 and CMD input, MOV's four
pixel-exact frames, close and subsequent `cpuid`, and STARSCR's four phases,
Esc and task cleanup. All installed kernel intervals matched guest memory.
MOV's post-close CMD screen was visually inspected. The original
post-STARSCR mouse stall remains; desktop reuse after that application is
not verified. Logs: `build/verify_memory_accel.log`,
`build/memory_accel_smoke.log`, `build/mov_memory_accel_smoke.log`, and
`build/starscr_memory_accel_smoke.log`; representative evidence is preserved
in `build/memory_accel_evidence/`. Unrecovered routines continue to use the
original binary; this is not yet a complete OS build from source.

`kernel/graphics_primitives.c` recovers color blending at
`0x828f1b..0x829003` (95/232 bytes) and the 12-byte vertical-retrace wait at
`0x828f0f..0x828f1b`. Alpha is truncated to eight bits. At depth 32 each
B/G/R channel uses `(foreground * alpha + background * (256 - alpha)) / 256`,
with a zero top byte; other depths return the background only when the
truncated alpha is zero. The VGA leaf waits for bit 3 of port `0x3da` to be
set without first waiting for it to clear. It retains EAX's top 24 bits,
EDX, DF, stack behavior and all original instruction bytes.

Each graphics differential executable passes 16,790,528 original/C/oracle
color comparisons, including every byte-channel/byte-channel/alpha triple,
full-width alpha inputs and 13 display depths. Another 768 comparisons
execute the entire original and rebuilt VGA polling functions under ptrace;
only the privileged `IN` instruction receives deterministic port samples.
The test checks polling counts, final status, GPRs, stack, defined arithmetic
flags and both incoming DF states. Both host-linked and fixed-address
variants pass (33,581,056 color and 1,536 VGA comparisons altogether).

The complete verification suite passed on 2026-10-27 with all 125 replacement
intervals. Three QEMU boots passed RAM/page/heap-chain checks (128 MiB and
59 or 60 live headers), real TSS switching, PS/2 and CMD input, MOV's four
pixel-exact frames, close and subsequent `cpuid`, and STARSCR's four phases,
Esc and task cleanup. All installed kernel intervals matched guest memory.
MOV's post-close CMD screen was visually inspected. The original
post-STARSCR mouse stall remains; desktop reuse after that application is
not verified. Logs: `build/verify_graphics_primitives.log`,
`build/graphics_primitives_smoke.log`, `build/mov_graphics_primitives_smoke.log`,
and `build/starscr_graphics_primitives_smoke.log`; representative evidence
is preserved in `build/graphics_primitives_evidence/`. Unrecovered routines
continue to use the original binary; this is not yet a complete OS build
from source.

`kernel/service_optional.c/.h/.def` recovers the 25 optional service-table
wrappers at `0x837967..0x837f5f` (slots 20 through 44). The global table is
captured once; the selected slot is tested and, when nonzero, read again for
the callback. Slots 24, 27, 30, 31, 32, 34, 42, 43 and 44 retain the slot's
address in EAX when empty; the other optional wrappers return zero. Word
arguments in slots 34/42/43/44 and the last byte argument in slot 42 are
zero-extended. Callback results retain their full 32 bits. Real call
boundaries prevent callback writes from corrupting outer argument slots.

The host-linked optional-service test passes 275,456 original/C comparisons,
checking all 20,480 mapped data bytes, ordered callback traces, results and
the outer argument stack. It covers empty/nonempty slots, all four table
alignments, callback changes to the global table and current slot, full-width
return values, all 65,536 values of every narrowed word parameter and all
256 values of the byte parameter with and without nonzero upper bits.
Another 100 comparisons use ptrace hardware watchpoints to change the global
pointer after its read and a slot after its first read. They verify one global
load, two nonempty slot loads, changed callback selection, a real null-call
fault when a checked slot becomes zero, and no call when an empty slot becomes
nonzero. Only the external memory changes are supplied by the harness; the
complete original and rebuilt wrappers execute.

All 25 wrappers are now installed at their original entry points, bringing
the manifest to 150 replacement intervals. The slot-32 C function omits its
frame pointer to fit its 44-byte interval (42 compiled bytes); its real call
boundary and volatile rereads remain intact. Both host-linked and
fixed-address tests pass 275,456 ordinary and 100 watched comparisons each
(550,912 ordinary and 200 watched comparisons altogether). Logs:
`build/service_optional_build.log`, `build/service_optional_test.log` and
`build/service_optional_linked_test.log`.

The complete verification suite passed on 2026-10-27 with all 150 replacement
intervals. Three QEMU boots passed RAM/page/heap-chain checks (128 MiB and
59 live headers), real TSS switching, PS/2 and CMD input, MOV's four exact
animation frames, close and subsequent `cpuid`, and STARSCR's four phases,
Esc and task cleanup. All installed intervals matched guest memory. MOV's
post-close CMD screen was visually inspected. The original post-STARSCR
mouse stall remains; desktop reuse after that application is not verified.
Logs: `build/verify_service_optional.log`, `build/service_optional_smoke.log`,
`build/mov_service_optional_smoke.log` and `build/starscr_service_optional_smoke.log`;
representative evidence is in `build/service_optional_evidence/`. Other
unrecovered kernel, driver and application routines still use original code.

`kernel/cpuid.c/.h` recovers the EFLAGS.ID availability probe at
`0x82b678..0x82b6a0`, leaf-0 vendor query at `0x82b6a0..0x82b6e8`, and
leaf-1 feature query at `0x82b6e8..0x82b72e`. The availability probe restores
saved flags, including DF and ID; arithmetic flags are not a C ABI output
(the original prologue changes them before saving). Leaf 0 writes EAX,
EBX, EDX, ECX, then a zero byte, requiring 17 bytes. Leaf 1 writes EAX, EBX,
ECX, EDX, requiring 16 bytes. Ordered volatile unaligned stores retain this
layout. Observable return EAX values are the output address for leaf 0 and
output address + 12 for leaf 1.

The host-linked CPUID preview passes 8 availability, 720 leaf-output and
36 guard-page fault comparisons against the original. Ptrace supplies only
CPUID register outputs and immutable-ID hardware behavior; the complete
functions execute. Cases cover both initial ID/DF values, changeable and
immutable ID, full-width output words, all four byte alignments, page
boundaries, zero termination, callee-saved registers, stack balance, and all
8,192 shared output bytes, including writes before faults. The test also
checks the selected leaf and exactly one CPUID instruction per query. Log:
`build/cpuid_preview_test.log`.

All three routines are now installed at their original addresses, increasing
the manifest to 153 intervals. Both the host-linked and fixed-address tests
pass the same 8 availability, 720 leaf-output and 36 guard-page comparisons
(1,528 comparisons in total). Logs: `build/cpuid_build.log`,
`build/cpuid_test.log` and `build/cpuid_linked_test.log`.

The complete verification suite passed on 2026-10-27 with all 153 replacement
intervals. Three QEMU snapshot boots verified 128 MiB RAM and the live heap,
real TSS switching, PS/2 input and CMD, MOV's four exact animation frames,
close and subsequent `cpuid`, and STARSCR's four phases, Esc and task cleanup.
Every installed interval matched guest memory. MOV's post-close CMD screen
was visually inspected. The original post-STARSCR mouse stall remains;
desktop reuse after that application is not verified. Logs:
`build/verify_cpuid.log`, `build/cpuid_smoke.log`, `build/mov_cpuid_smoke.log`
and `build/starscr_cpuid_smoke.log`; evidence: `build/cpuid_evidence/`.

`kernel/fpu.c/.h` recovers FPU availability, enablement, task-context restore,
lazy CR0 marking and fault dispatch at `0x837f5f..0x8380ef`. These retain
CPUID EDX bit 0, the full-DWORD comparison after a word-sized FNSTSW, CR0
EM/TS/NE changes, the captured task table/index with unsigned 32-bit address
wrap, the second context-pointer read, and the post-FNINIT allocation-base
read. The original context operation is FXRSTOR, not FRSTOR. The lazy-mark
routine inlines the two CR0 instruction boundaries to fit its 24-byte slot.

`kernel/fpu_state.c` recovers eight hardware leaves at `0x83730a..0x837347`:
FNSAVE, FRSTOR, FSTENV, FLDENV, FXSAVE, FXRSTOR, XSAVE and XRSTOR. Their bytes
match the originals exactly. The original XSAVE/XRSTOR ABI uses EAX both as
the address and the low feature mask, with the incoming EDX as the high mask;
this behavior is preserved, including its limitations.

The host-linked preview passes 560 probe/enable, 504 lazy-task, 224 hardware
boundary and 1,512 hardware-watchpoint mutation comparisons against complete
original routines and an independent event oracle. Ptrace supplies only the
CPUID, CR0 and FPU instruction boundaries. Tests check event order, all
20,480 shared data bytes, callee-saved registers, stack balance and DF/ID.
The watchpoint pass runs with CONT and child-only breakpoints at hardware
instruction sites observed in the earlier single-step pass; Linux suppresses
data watchpoints during PTRACE_SINGLESTEP. It mutates captured table/index
globals, changes the context pointer between its two reads (including to
zero), and changes the allocation base at FNINIT.

Another 128 native comparisons execute original and rebuilt hardware leaves
against direct ISA operations, comparing 4,096 data bytes and the resulting
512-byte x87/SSE state. All eight leaves ran on this host; XSAVE/XRSTOR used
the original ABI with a zero effective feature mask. These native checks
skip explicitly if required host capabilities are absent. Log:
`build/fpu_preview_test.log`. Fixed-address installation and QEMU regression
remain pending for these 13 routines.

All 13 FPU routines are now installed at their original addresses, increasing
the manifest to 166 intervals. The five C control routines occupy 58/86,
45/72, 93/204, 10/24 and 13/14 bytes; all eight hardware leaves remain exact.
Both host-linked and fixed-address tests pass all 2,928 comparisons each,
including the 128 native x87/SSE state comparisons (5,856 in total). Logs:
`build/fpu_build.log`, `build/fpu_test.log`, `build/fpu_linked_test.log`.
Full-suite and QEMU verification of this image follow the installation step.

`python3 tests/fpu_qemu.py` builds an isolated multiboot fixture embedding both
the original and current kernels, then executes their FPU routines at CPL 0
under QEMU. It passes 96 real CR0 probe/enable/lazy cases, 576 task-context
and actual #NM cases, and 128 native hardware-leaf cases (800 total). These
checks use an independent state oracle, unsigned task-index wrap, unaligned
task entries, and complete 512-byte x87/SSE state comparisons. The fixture
enables OSFXSR/OSXSAVE and XCR0=3; XSAVE/XRSTOR retain the original pointer/mask
ABI and are exercised with a zero effective mask. This fixture does not boot
or modify the OS disk. Logs: `build/fpu_qemu_build.log`, `build/fpu_qemu.log`.

The complete `make verify` suite passed with all 166 intervals on 2026-10-27.
Three further OS snapshot boots passed the live 128 MiB memory/heap, real TSS
switching, PS/2/CMD, MOV frames/close/cleanup/CMD reuse, and STARSCR phases/
Esc/cleanup checks. All installed intervals matched guest RAM. MOV's post-close
CMD screenshot was visually inspected. STARSCR still has the known original
post-exit mouse stall; desktop reuse after STARSCR is not verified. Logs:
`build/verify_fpu.log`, `build/fpu_smoke.log`, `build/mov_fpu_smoke.log`,
`build/starscr_fpu_smoke.log`; screenshots/JSON: `build/fpu_evidence/`.

`kernel/interrupts.c/.h` recovers the interrupt-table initializer at
`0x813320..0x8135c6`: 265 ordered gate installations, a single captured boot
code selector, 11 gate overrides and 12 sparse callback stores. It preserves
vector 255 and all unused callback slots. `kernel/interrupt_hardware.c`
recovers the gate/LIDT boundary, boot-selector GS access and original PIC,
PIT and RTC programming at `0x8370d2..0x8371a0`; all five leaves compile to
identical original bytes (95, 18, 57, 13 and 23 bytes). The original gate
helper has unchecked 32-bit vector arithmetic and reloads IDTR after every
installation; RTC setup retains its original register value and mask changes.
The host preview passes 96 original/C/oracle scenarios (25,440 gate events)
across full-width selectors, memory patterns and callback mutations. It checks
the full 20,480-byte shared region and ordered arguments, including selector
mutation after capture and callback-table mutation during gate installation.
Log: `build/interrupts_preview_test.log`. Fixed-address installation and
privileged execution checks are the next step for these six functions.

These six functions are installed at their original addresses, bringing the
manifest to 172 intervals. The C initializer occupies 417 of 678 bytes; all
five hardware leaves remain byte exact. Both host-linked and fixed-address
tests pass 96 scenarios each (192 scenarios and 50,880 gate events total).
Logs: `build/interrupts_build.log`, `build/interrupts_test.log`,
`build/interrupts_linked_test.log`, `build/interrupts_images.log`. Full-suite,
dedicated CPL0 tests and OS QEMU regression follow this installation step.

`python3 tests/interrupts_qemu.py` builds an isolated multiboot fixture and
executes the original and rebuilt interrupt routines at CPL 0 under QEMU.
All 521 comparisons pass: 384 gate/LIDT/segment/wrapped-index cases, 16 GS
boot-selector cases, 24 complete IDT/callback initializations, one real
INT/IRET round trip through both implementations, and 96 PIC/PIT/RTC cases.
The fixture checks the complete 20,480-byte shared region against an
independent oracle, IDTR contents, register/segment preservation and device
configuration. Volatile RTC status C's low return byte is excluded from the
device result comparison; the routine's exact instruction bytes are checked
separately during image installation. The fixture keeps IF clear and does
not boot or modify the OS disk. Logs: `build/interrupts_qemu_build.log` and
`build/interrupts_qemu.log`.

`kernel/interrupt_entries.c/.h/.def` recovers all 14 interrupt-frame entry
points at `0x8371a0..0x8372e8`: two exception entries, the default PIC
acknowledgement entry, CLTS/#NM, timer, keyboard, IRQ5, RTC, IRQ9 through
IRQ12 and software interrupts 0x40/0x41. The symbolic assembly preserves the
CPU frame, original segment saves, callback addresses, EOI order, RTC status
read and STI placement. No CLD or exception error-code adjustment is added.
Only interrupt 0x41 substitutes the callback result for saved EAX. The #NM
entry executes only CLTS/IRET, as in the original, rather than invoking the
separate FPU callback. These functions must be entered with an interrupt
frame, never with an ordinary C call.

`python3 tests/interrupt_entries_exact.py` independently compiles these
sources and compares all 328 bytes with the SHA-256-verified original
kernel extracted directly from the VHD. All 14 match exactly. Installation
and privileged entry/return tests follow this source-recovery step.

The complete `make verify` suite passed with all 172 installed intervals
on 2026-10-27. Three OS snapshot boots verified live 128 MiB RAM/heap, real
TSS switching, PS/2/CMD, MOV's four exact frames/close/cleanup/CMD reuse, and
STARSCR's four phases/Esc/task cleanup. Every installed interval matched
guest RAM. The MOV post-close CMD screen was visually inspected. The known
original post-STARSCR mouse stall remains; desktop reuse after STARSCR is
not verified. Logs: `build/verify_interrupts.log`, `build/interrupts_smoke.log`,
`build/mov_interrupts_smoke.log`, `build/starscr_interrupts_smoke.log`;
screenshots/JSON: `build/interrupts_evidence/`. This is still a hybrid image
with unrecovered original kernel, driver and application code.

All 14 interrupt-frame entries are now installed at their original
addresses, bringing the manifest to 186 intervals. Linker assertions require
every entry and end address to match, and each manifest entry requires exact
original bytes. The installed-image check passes for all 328 bytes. The two
initializer differential variants and all 521 existing native interrupt
setup checks also pass. `make verify` now runs the entry byte-identity check.
Logs: `build/interrupt_entries_build.log`, `build/interrupt_entries_exact.log`,
`build/interrupt_entries_init_test.log`, `build/interrupt_entries_gate_qemu.log`.
Full-suite and OS regression for this image follow the installation step.

`python3 tests/interrupt_entries_qemu.py` executes all installed entry points
and their original counterparts at actual CPU privilege levels. All 1,056
original/rebuilt/oracle comparisons pass: 896 software INT/IRET cases across
interrupt and trap gates, 32 real #NM faults with CLTS and retry of the
faulting x87 instruction, and 128 CPL3 system calls through vectors 0x40 and
0x41 with a real TSS kernel-stack switch and IRET back to user mode.
Tests vary arithmetic flags, IF, DF, segment selectors, callback results and
callback register/segment clobbers. They check saved general registers,
stack balance, callback selection, segment/flag restoration and the special
0x41 EAX result. The original exception entry's unsaved FS behavior is retained.
RTC status C's volatile low byte is excluded only from callback-entry EAX
comparison; its upper bytes and the complete entry code remain checked.
The fixture runs without paging and does not boot or modify the VHD. Logs:
`build/interrupt_entries_qemu_build.log`, `build/interrupt_entries_qemu.log`.

`kernel/interrupt_callbacks.c/.h` recovers the five C IRQ callbacks at
`0x813a8c..0x813b70`: RTC clock sampling/event publication, keyboard byte
enqueue/event publication, IRQ5 DMA/DSP acknowledgement, unused IRQ9/10/11,
and mouse byte enqueue/event publication. Keyboard and mouse capture both
pointers before port I/O and ignore the queue writer's result; RTC reads
its event pointer after clock sampling. The preview differential test passes
30,720 original/C/oracle cases, covering every byte, three memory patterns,
eight dependency-mutation/rejection combinations, complete 12,288-byte shared
memory comparisons and ordered I/O/queue events. Log:
`build/interrupt_callbacks_preview.log`.

`kernel/cmos_clock.c` recovers the hardware leaf at `0x82ae84..0x82af08` as
132 byte-identical symbolic-assembly bytes. It retains GS save/selector/restore,
CMOS index order 0/2/4/6/7/8/9/0x32, and raw stores at `0x1b004..0x1b00b`.
`python3 tests/cmos_clock_exact.py` extracts the original directly from the
SHA-verified VHD before comparing. No UIP wait, BCD conversion, rollover
retry or index/NMI restoration is introduced. These six functions are not
installed by this preview step; fixed-address installation and privileged
execution checks follow separately.

The complete `make verify` suite passed for the 186-interval interrupt-entry
image on 2026-10-27. Three OS snapshot boots verified 128 MiB RAM and live
heap chains, real TSS preemption, PS/2/CMD, MOV's four exact frames/close/
cleanup/CMD reuse, and STARSCR's four phases/Esc/task cleanup. Every installed
interval matched guest RAM. MOV's post-close CMD screenshot was visually
inspected and shows `cpuid` output and a returned prompt. The known original
post-STARSCR mouse stall remains; desktop reuse after STARSCR is not verified.
Logs: `build/verify_interrupt_entries.log`, `build/interrupt_entries_smoke.log`,
`build/mov_interrupt_entries_smoke.log`, `build/starscr_interrupt_entries_smoke.log`;
screenshots/JSON: `build/interrupt_entries_evidence/`. This remains a hybrid
image containing original code outside the recovered intervals.

The five IRQ callbacks and CMOS sampler are now installed at their original
addresses, bringing the manifest to 192 intervals. The callbacks occupy
25/26, 56/64, 54/68, 1/6 and 56/64 bytes respectively; the 132-byte CMOS leaf
is required to remain byte exact. Both host-linked and fixed-address callback
variants pass 30,720 cases each (61,440 total). Installation rebuilt the C-init
and application VHDs. Logs: `build/interrupt_callbacks_build.log`,
`build/interrupt_callbacks_test.log`, `build/interrupt_callbacks_linked_test.log`,
`build/cmos_clock_exact.log`. Full-suite and OS regression follow separately.

`python3 tests/cmos_clock_qemu.py` passes 256 original/rebuilt/oracle cases
with real CPL0 CMOS IN/OUT. The clock is frozen while raw BCD/binary register
values vary; checks cover all eight sampled bytes, all surrounding shared
memory, a separate segment-base guard region, GS selectors 0/8/24/32,
EAX's century-byte result, other general registers, flags including DF,
and the final selected CMOS index. The GS=32 test uses a nonzero segment
base and verifies temporary selection of the flat GS=8 descriptor. The
fixture does not boot or modify the OS disk. Logs:
`build/cmos_clock_qemu_build.log`, `build/cmos_clock_qemu.log`.

`kernel/syscalls.c/.h` recovers both PUSHAD-frame dispatchers at
`0x813b70..0x814176`. Vector 0x40 selects 20 operations, including queue
request index 13 and task yield; vector 0x41 selects six result operations
and returns zero for unknown opcodes. The reconstruction retains the original
descending volatile DWORD reads, full-width results and the signed low-byte
arguments on 0x40 operations 6 and 11. Sibling calls are disabled so service
argument writes cannot corrupt the caller's saved PUSHAD registers. Numeric
callee names identify nine implementations still supplied by original code.

The standalone preview passes 12,340 original/C/oracle cases, including all
0x41 unknown operations 6..19, byte boundaries, argument/record mutations,
no-read paths with invalid pointers, and records against both guard-page
boundaries. Another 156 hardware-watchpoint executions verify each adjacent
argument pair is read high-to-low exactly once; poisoning a word immediately
after its read preserves the captured callback argument. All 144 undersized
record executions fault as expected, including the signed-byte routes whose
original code still performs DWORD loads. Log: `build/syscalls_preview.log`.
These dispatchers are not installed by this source-recovery step.

`python3 tests/syscalls_qemu.py` executes 3,456 original/C/oracle comparisons
of complete vector 0x40/0x41 chains, using actual INT, PUSHAD, service calls
and IRET at CPL0 and CPL3. A real TSS switches user calls to a kernel stack.
All valid routes and the unknown-opcode set run through both interrupt and
trap gates while IF, DF, arithmetic flags, selectors, arguments, service
returns and service writes to outgoing arguments vary. Checks cover callback
privilege/segments/IF, captured arguments, saved general registers, user EAX,
stack balance, restored flags/segments and full shared-memory comparisons.
Service implementations are observation stubs; this fixture validates the
dispatchers, not those remaining original services. The preview invokes the
newly compiled C dispatchers; `--installed` instead selects their fixed kernel
addresses. Logs: `build/syscalls_qemu_preview_build.log`,
`build/syscalls_qemu_preview.log`.

`kernel/syscall_services.c/.h` recovers three direct services at
`0x81041a..0x8104a0` and the ranged xorshift routine at
`0x82a9bb..0x82aa0f`. The object services retain the pointer table at
`0x6a0000`, wrapped indices, unaligned offsets 0x55f/12/4 and the setter's
raw EAX return. The random routine samples the low TSC once, performs logical
xorshifts 6/12/14, then applies the original unsigned wrapped-span remainder.
No checks or normalization are added: a zero span still raises #DE/SIGFPE.
`kernel/timestamp.c` recovers the five-byte RDTSC leaf at
`0x83734a..0x83734f`, preserving EDX and flags.

The standalone preview passes 12,420 original/C/oracle cases across patterned
memory, four index wrap aliases, four object alignments, boundary arguments,
seed/call-count mutations and full memory comparisons. Another 270 original/C
executions confirm zero-span SIGFPE. `python3 tests/timestamp_exact.py`
independently extracts the SHA-verified original kernel from the VHD and
confirms all five instruction bytes. Logs: `build/syscall_services_preview.log`,
`build/timestamp_exact_preview.log`. These five functions are not installed
by this source-recovery step.

The complete `make verify` suite passed for the 192-interval IRQ-callback
image on 2026-10-27. The first verification process was interrupted; its
partial log is retained separately and the fresh full run exited zero.
Three OS snapshot boots checked 128 MiB RAM/live heap chains, actual TSS
switching, PS/2/CMD, MOV frames/close/cleanup/CMD reuse and STARSCR phases/
Esc/task cleanup, with all installed intervals matching guest memory. The
post-MOV screenshot was visually checked and shows a second `cpuid` result
and a returned prompt. The original post-STARSCR mouse stall remains; desktop
reuse after STARSCR is not verified. Logs: `build/verify_interrupt_callbacks.log`,
`build/verify_interrupt_callbacks.exit` (0), `build/interrupt_callbacks_smoke.log`,
`build/mov_interrupt_callbacks_smoke.log`, `build/starscr_interrupt_callbacks_smoke.log`;
screenshots/JSON: `build/interrupt_callbacks_evidence/`. This remains a hybrid
image with unrecovered original kernel, driver and application dependencies.

Both system-call dispatchers are now installed at their original addresses,
bringing the manifest to 194 intervals. The C-init and application images
rebuilt successfully. Host-linked and fixed-address tests each pass 12,340
original/C/oracle cases, 156 ordered-read watchpoint executions and 144
expected guard-page faults. The installed dispatchers also pass all 3,456
complete CPL0/CPL3 INT/IRET comparisons. Logs:
`build/syscalls_install_build.log`, `build/syscalls_host_test.log`,
`build/syscalls_linked_test.log`, `build/syscalls_qemu_installed_build.log`
and `build/syscalls_qemu_installed.log`. The service bodies remain original
in this installation; the five recovered service/TSC functions follow in a
separate installation step.

The three object services, ranged xorshift and exact low-TSC primitive are now
installed at their original addresses, bringing the manifest to 199 intervals.
Their compiled sizes are 31/46, 23/48, 23/40, 51/84 and 5/5 bytes respectively.
C-init, Snake and application VHDs rebuilt. Host-linked and fixed-address
service tests each pass 12,420 original/C/oracle cases and 270 expected
zero-span SIGFPE executions; both dispatcher test variants still pass their
12,340 cases, 156 watchpoint executions and 144 expected guard-page faults.
The installed five-byte timestamp passes the independent byte-exact check.
Logs: `build/syscall_services_install_build.log`,
`build/syscall_services_host_test.log`, `build/syscall_services_linked_test.log`,
`build/syscalls_services_host_test.log`, `build/syscalls_services_linked_test.log`
and `build/timestamp_exact_installed.log`. Full-suite and OS regression follow
separately; original code outside recovered intervals is still required.

`python3 tests/syscalls_qemu.py --installed --services` passes another 3,456
complete original/rebuilt/oracle comparisons. This mode executes the actual
installed object setter/getters and random routine within CPL0/CPL3 INT/IRET
chains, retaining observer stubs for the other services. Object fields, index
wrap aliases, unaligned objects, raw return values, shared-memory changes and
one seed sample are checked; the TSC boundary uses a controlled identical
seed for both images. The non-service installed-chain mode also still passes
3,456 comparisons. Logs: `build/syscalls_services_dispatch_chain_build.log`,
`build/syscalls_qemu_services_build.log`, `build/syscalls_qemu_services.log`.

`kernel/syscall_surface.c/.h` recovers the surface-refresh forwarder at
`0x81017c..0x81021c`, used by INT 0x40 operation 16 and the service table.
It retains independent descending DWORD reads, repeated layer/table lookup,
wrapped indices, unaligned fields and the raw compositor return. The separate
outgoing argument area prevents a callee from overwriting its caller's input.
The standalone original/C/oracle fixture passes 18,433 cases plus 18 expected
guard-page SIGSEGV executions. Log: `build/syscall_surface_preview.log`.
This source checkpoint does not yet install the forwarder; the compositor
body at `0x81905e` remains an observation stub in this fixture.

The prior 199-interval image passed the complete `make verify` suite on
2026-10-27; `build/verify_syscall_services.exit` records exit status zero.
The surface forwarder is now installed at its original address, bringing
the manifest to 200 intervals. C-init, Snake and application VHDs rebuilt;
the host-linked and fixed-address forwarder tests each pass 18,433 cases
and 18 expected guard-page faults. Build/test logs:
`build/syscall_surface_install_build.log`, `build/syscall_surface_host_test.log`,
`build/syscall_surface_linked_test.log`. The lower compositor remains original
at this installation checkpoint; full-suite and OS regression follow.

`kernel/surface_refresh.c/.h` recovers refresh selection at
`0x81905e..0x8191d8`. The C body retains the zero-order early return,
unsigned layer-order comparison, signed active-index/style tests, fallback
layer 2, callback capture before interrupt masking, repeated volatile reads
and original rectangle arguments. The lower compositor at `0x817cd0` and
the framebuffer callback remain separate dependencies.

An assembly entry/return boundary restores EFLAGS only after C stack cleanup;
ordinary compiler-generated `ADD ESP` after `POPFL` would corrupt arithmetic
flags. The standalone fixture passes 24,576 original/C/oracle comparisons
covering all selection routes, index-wrap aliases, unaligned structures,
callback/table mutations, outgoing-argument writes and complete shared-memory
comparisons. Real userspace `POPFL` checks arithmetic flags and DF immediately
after each enabled-path return. Log: `build/surface_refresh_preview.log`.
This preview does not yet install the recovered selector into the disk.

The selector is now installed at its original address, bringing the manifest
to 201 intervals. Its compiled entry and C body occupy 259 of the available
378 bytes. C-init, Snake and combined application VHDs rebuilt successfully.
Both host-linked and fixed-address tests pass 24,576 original/C/oracle cases,
including real arithmetic-flag and DF restoration at the return boundary.
Logs: `build/surface_refresh_install_build.log`,
`build/surface_refresh_host_test.log`, `build/surface_refresh_linked_test.log`.
The lower compositor and framebuffer callback still use original code;
complete-suite and OS boot regression follow as a separate checkpoint.

`kernel/surface_composite.c/.h` recovers the layer-list dispatcher and signed
rectangle clipping at `0x817cd0..0x817dac`. It captures service-table slot 40
once, clips the caller's actual stack argument words, then dispatches from
the surface's unsigned order through the live list bound at `0x510000`.
The arithmetic retains 32-bit wraparound and signed comparisons; negative
dimensions are not normalized. The return is the final order counter,
including counter wraparound, rather than the drawing callback's result.
A small assembly entry exposes the real incoming words to the C helper.

The standalone preview passes 82,944 original/C/oracle cases covering all
clipping paths, caller-visible stores, unaligned objects, wrapped layer and
order indices, zero/multiple iterations, bound shrink/extension, callback
capture, incoming/outgoing argument mutations and full mapped-memory checks.
Log: `build/surface_composite_preview.log`. This source checkpoint does not
yet install the dispatcher; the per-layer drawing body remains original.

The 201-interval image passed the complete `make verify` suite on
2026-10-27 (`build/verify_surface_refresh.exit`: 0). Five sequential OS
snapshot boots checked the desktop, live heap/TSS/input paths and the
recovered Snake, Color, MOV and STARSCR applications. Every installed
interval matched guest memory. Fresh relevant screenshots were visually
checked: desktop `cpuid`, Snake after turning, Color's palette, another
`cpuid` after closing MOV, and STARSCR's second phase. Logs:
`build/surface_refresh_{system,snake,color,mov,starscr}_smoke.log`;
screenshots/JSON: `build/surface_refresh_evidence/`. The archive also contains
older unrelated artifacts; only each check's own outputs are evidence.
STARSCR still exhibits the original post-exit mouse stall even though its
phases, Esc exit and task cleanup pass; desktop reuse after STARSCR remains
unverified. This is still a hybrid image with original dependencies.

`python3 tests/syscalls_qemu.py --installed --services --surfaces` adds
2,048 native original/rebuilt/oracle surface-chain comparisons to the
existing 3,456 complete system-call cases. It executes the installed refresh
forwarder and selector, followed by the then-original clipping/list
dispatcher, inside actual CPL0/CPL3 interrupt/trap-gate INT/IRET chains.
Only drawing and framebuffer presentation are observer callbacks. Checks
include real CLI/POPFL, preserved DF and restored IF, signed clipping, live
list mutation, cached callbacks, isolated outgoing argument overwrites,
full shared-memory effects, registers, segments and flags. Log:
`build/syscalls_qemu_surfaces_services.log`. The preview, installed and
recovered-service modes each passed their 3,456 comparisons again after
adding the surface mode. Logs: `build/surface_chain_preview_regression.log`,
`build/surface_chain_installed_regression.log`,
`build/surface_chain_services_regression.log`;
`build/surface_chain_fixture_regression.exit`: 0.

The clipping/list dispatcher is now installed at `0x817cd0`, bringing the
manifest to 202 intervals. Its assembly entry and ordinary C body occupy
219 of 220 available bytes. C-init, Snake and combined application VHDs
rebuilt. The host-linked and fixed-address fixtures each pass 82,944
original/C/oracle cases; the selector and forwarder fixtures also retain
their 24,576 and 18,433 cases (plus 18 expected guard faults) in both modes.
The native recovered-service/surface-chain fixture passes its 3,456 base
and 2,048 focused comparisons with the recovered lower dispatcher too.
Logs: `build/surface_composite_install_build.log`,
`build/{surface_composite,surface_refresh,syscall_surface}_installed_regression.log`,
their `_linked` variants, `build/surface_composite_native_chain.log` and
`build/surface_composite_native_chain.exit` (0). The per-layer rasterizers
and framebuffer presentation bodies are still original at this checkpoint.

`kernel/surface_present.c/.h` recovers three more graphics routines:
the rectangle predicate at `0x818d14..0x818d5e`, 8-bit submission at
`0x818d5e..0x818f22`, and 32-bit submission at `0x818f22..0x81905e`.
The predicate deliberately retains the original two OR expressions rather
than substituting a conventional intersection test. Both submission bodies
capture their buffers and dimensions, clamp/swap the caller's actual x/y
words, retain wrapped endpoint arithmetic and signed comparisons, and
return the original early-exit value or final row counter. Eight-bit output
uses forward DWORD groups followed by a signed byte remainder, preserving
overlap effects; 32-bit output caches service-table slot 68 once and uses
isolated outgoing arguments for each row. Zero/negative dimensions retain
the original endpoint swapping behavior.

The standalone fixture passes 131,072 original/C/oracle predicate cases and
61,056 submission cases across both depths, four alignments, six buffer
alias arrangements, signed/overflow boundaries, caller-visible clipping
stores, cached-buffer/callback mutations, incoming/outgoing argument writes,
return values and complete mapped-memory comparisons. It explicitly skips
3,456 generated cases whose pixel ranges would exceed the fixture's mapped
pages; those cases are not counted as passing. Log:
`build/surface_present_preview.log`. This source checkpoint does not yet
install the three functions into the VHDs.

The three framebuffer functions are now installed at their original addresses,
bringing the manifest to 205 intervals. The predicate occupies 67/74 bytes,
8-bit submission 361/452 bytes, and 32-bit submission 264/316 bytes. All
three C-init, Snake and combined application VHDs rebuilt. Both host-linked
and installed-address fixtures pass the 131,072 predicate and 61,056
submission comparisons; 3,456 unsafe generated ranges are explicitly skipped.
The full-disk verifier confirms that only declared compiled kernel slots and
compiled boot stages changed. Logs: `build/surface_present_install_build.log`,
`build/surface_present_host_test.log`,
`build/surface_present_linked_test.log`, and
`build/surface_present_disk_verify.log`. These focused checks are for the
205-interval image; the earlier full-suite/OS evidence is not reused as a
regression result for this image.

`kernel/surface_draw.c/.h` recovers the 8-bit layer rasterizer at
`0x817dac..0x818134`. It preserves signed clipping, inclusive endpoints,
the unusual predicate call using screen dimensions, a signed transparency
key, and forward DWORD groups followed by the signed byte remainder. Live
layer-table/coordinate/key loads and cached buffers/root dimensions retain
their distinct original behavior when output aliases metadata.

The standalone original/C/independent-oracle fixture passes 536,000 cases
with two data patterns, four alignments, six pixel-buffer layouts, six
additional metadata-output layouts, wrapped table indices, signed/overflow
rectangles, eight layer geometries, four screen sizes and eight keys. It
checks raw return values and every byte in all five mapped shared pages.
It explicitly skips 4,672 generated unsafe ranges before invoking executable
code. Log: `build/surface_draw_preview.log`. This is a source checkpoint;
the 8-bit rasterizer is not yet installed into the VHDs here.

`kernel/surface_alpha.c/.h` recovers the four-pixel blending helper at
`0x818134..0x81846a`. Signed weights above 127 leave a pixel unchanged;
weights at or below zero copy the complete source DWORD; positive weights
up to 127 blend R/G/B in that order with denominator 128 and leave the high
byte unchanged. Weight words are reread for each channel, and arithmetic
retains 32-bit wraparound. The return preserves the original final EAX,
including a weight pointer, destination pointer, or full shifted blend
result depending on the last pixel's branch.

Its standalone original/C/bytewise-oracle test passes 15,616 cases covering
four alignments, signed boundary weights, mixed/uniform weights and 61
source/destination/weight layouts. Cases deliberately overlap weights and
pixels, including resulting arithmetic overflow and channel-order effects.
Raw returns and all bytes of three mapped shared pages match. Log:
`build/surface_alpha_preview.log`. The helper is not installed at this
source checkpoint.

The 8-bit layer rasterizer and four-pixel blending helper are now installed
at their original addresses, bringing the manifest to 207 intervals. Their
compiled sizes are 538/904 and 147/822 bytes respectively. C-init, Snake and
combined application VHDs rebuilt successfully. Host-linked and installed
fixtures each pass all 536,000 rasterizer cases (4,672 unsafe ranges explicitly
skipped) and 15,616 blending cases. The full-disk verifier confirms that only
declared compiled kernel slots and compiled boot stages changed. Logs:
`build/surface_draw_install_build.log`,
`build/surface_draw_{host,linked}_test.log`,
`build/surface_alpha_{host,linked}_test.log`, and
`build/surface_draw_disk_verify.log`. Full-suite and OS regression checks for
this newer image remain separate from the preceding 202-interval evidence.

`kernel/surface_draw32.c` recovers the main rasterizer at
`0x81846a..0x818d14`. It converts 8-bit source pixels through a cached
palette, copies opaque 32-bit rows through cached service slot 68, or blends
groups of four through slot 72 followed by the original three-entry inline
tail. Source/destination pitches, transparency key and buffers are captured,
while selected layer coordinates and the incoming layer word remain live.
The assembly entry exposes the actual incoming argument words to the C body
and isolates callback outgoing arguments. Signed clipping, wraparound,
unsupported-depth return values and the original screen-dimension predicate
call are preserved.

The standalone original/C/bytewise-oracle fixture passes 628,156 comparisons
and explicitly skips 5,444 generated unsafe ranges. It covers 8/32-bit and
unsupported source depths, signed/overflow rectangles, palette/key/alpha
behavior, overlapping pixel buffers, seven metadata-output aliases, cached
callback/global mutations, caller layer/rectangle mutation, outgoing argument
and weight overwrites, raw returns and all six shared memory pages. Log:
`build/surface_draw32_preview.log`. This source checkpoint precedes installation
of this rasterizer into the VHDs.

The 32-bit output rasterizer is now installed at `0x81846a`; its entry/body
occupy 1,158 of 2,218 available bytes. The manifest contains 208 intervals,
and C-init, Snake and combined application VHDs rebuilt. The installed-address
fixture repeats all 628,156 original/rebuilt/oracle comparisons with the same
5,444 explicit unsafe-range skips. The full-disk verifier passes. Logs:
`build/surface_draw32_install_build.log`,
`build/surface_draw32_linked_test.log`, and
`build/surface_draw32_disk_verify.log`.

`python3 tests/syscalls_qemu.py --installed --services --surfaces` also passes
3,456 complete native INT/IRET comparisons and 2,048 focused surface-chain
comparisons on this image. The surface-chain fixture uses observed drawing
callbacks; it does not by itself prove the real new rasterizer bodies run.
Log: `build/surface_draw32_native_chain.log`; exit marker:
`build/surface_draw32_native_chain.exit` (0). Whole-suite and desktop/application
regression checks are recorded separately when complete.

The 208-interval image was booted in five separate QEMU snapshots on
2026-10-27. Desktop, PS/2 pointer, CMD keyboard/queue/prompt, 128 MiB RAM,
59 live heap headers, advancing timer and suspended C scheduler/TSS stacks
passed. The rebuilt Snake passed movement, wall collision, restart and
arrow/letter turns; Color passed 600 palette-pixel probes, window close,
task exit and CMD reuse; MOV passed all four exact animation frames, 16
relocations, close/cleanup and CMD reuse. STARSCR passed 21 relocations,
four phases with 10,797 pixel probes each, Esc exit and task cleanup.
Its post-exit mouse probe still stalls, so desktop reuse after STARSCR is
not established; this matches the previously recorded original-image
limitation and is not treated as fixed by the graphics recovery.

Every boot compared all installed kernel slots with guest memory. Fresh
screenshots and JSON for each run were copied before the next run could
overwrite shared outputs, under
`build/surface_draw32_evidence/{system,snake,color,mov,starscr}/`.
`build/surface_draw32_evidence/results.json` records each image SHA-256,
exit status (all 0), and fresh artifact list. Logs:
`build/surface_draw32_{system,snake,color,mov,starscr}_smoke.log`.
Desktop/CPUID output, Snake turn, Color palette, MOV frame 2 and STARSCR
phase 2 screenshots were visually inspected. These are bounded integration
checks; unrecovered kernel, driver and application code is still supplied
by the original VHD.

The complete `make -j4 verify` regression for the 208-interval image also
completed successfully on 2026-10-27. Log:
`build/verify_surface_draw32.log`; exit marker:
`build/verify_surface_draw32.exit` (0). It repeats the host and installed-address
32-bit rasterizer's 628,156 comparisons each, the remaining kernel fixtures,
all four application differential tests, executable-format tests and full-disk
verification of both application images. The documentation-only correction
in `kernel/surface_refresh.c` was separately compiled with the production
flags and produced a byte-identical object; log:
`build/surface_draw32_comment_check.log`. This closes the graphics regression
checkpoint, not recovery of the whole OS. The unrecovered dependencies and
STARSCR post-exit mouse/timer limitation above remain.

`kernel/surface_move.c/.h` recovers moved-layer damage selection at
`0x8191d8..0x8194e4`. It composites the new rectangle, uses the original
rectangle predicate, redraws the exposed old strips and presents either
one combined rectangle or the two individual rectangles. The active lower
layer, displacement and presentation callback are cached, while the seven
incoming argument words stay live across callbacks. Signed decisions,
32-bit wrapped endpoints, exact comparison of the predicate return with
one, separate outgoing arguments and the final callback's raw return are
preserved.

The standalone original/C/independent-oracle fixture passes 51,200 cases,
covering all four movement quadrants, zero and signed-boundary dimensions,
32 curated/generated geometries, four unaligned/wrapped layer layouts,
five active-layer values and both style branches. Callback variants overwrite
outgoing arguments, modify the caller's live arguments, corrupt cached
global/table inputs and force predicate results of 0, 1, 2 or `0xffffffff`.
All events, caller words including a sentinel, raw returns and four mapped
memory pages match. Logs: `build/surface_move_preview_build.log` and
`build/surface_move_preview.log`. The C object occupies 586 bytes against
780 available; this source checkpoint does not yet install it in the VHDs.

The moved-layer selector is now installed at `0x8191d8`, using 586/780 bytes
and bringing the replacement manifest to 209 intervals. All three C-init,
Snake and combined-application VHDs rebuild. Both host-linked and
installed-address fixtures pass the same 51,200 cases; the full-disk kernel
verifier and both application-image verifiers pass without FAT, directory
or slack changes. Logs: `build/surface_move_install_build.log`,
`build/surface_move_{host,linked}_test.log`,
`build/surface_move_disk_verify.log`; exit marker:
`build/surface_move_install.exit` (0). These focused checks precede the
209-interval image's QEMU integration checks; the earlier full-suite result
is specifically for the 208-interval image.

The 209-interval images passed five fresh QEMU snapshot checks on 2026-10-27.
`python3 tools/boot_smoke.py --surface-move-check --task-check --memory-check`
checks the desktop, PS/2 pointer, relocated CMD, keyboard queue/prompt,
128 MiB RAM, 59 live heap headers, advancing timer and suspended recovered
C scheduler/TSS stacks. Its optional address-filtered execution trace is
enabled after startup and RAM/task probing, immediately before input events.
It observed five entries at `0x8191d8` and 67 C-body/return block execution
events, in addition to comparing all 209 installed intervals with guest
memory. `build/surface_move_execution.json` records image/trace SHA-256
digests, counts and executed block addresses. Enabling `exec,nochain` at
process startup first delayed boot beyond the fixed startup wait; that
tooling failure is preserved in
`build/surface_move_system_early_trace_failure.log`. The corrected check
starts tracing only after normal startup.

Separate boots repeat Snake movement, wall collision, restart and arrow/letter
turns; Color's 600 palette probes, close, task exit and CMD reuse; MOV's four
exact frames, 16 relocations, close/cleanup and CMD reuse; and STARSCR's
21 relocations, four phases with 10,797 pixel probes each, Esc exit and task
cleanup. STARSCR's post-exit mouse probe still stalls; desktop reuse after
that application is not verified. All five smoke processes exited 0; this
does not mean the known screen-saver limitation has been fixed.

Fresh screenshots and small evidence files were archived before subsequent
boots under `build/surface_move_evidence/{system,snake,color,mov,starscr}/`.
`build/surface_move_evidence/results.json` lists image hashes, exit statuses
and archived artifacts. All image hashes were rechecked against the current
builds, and the archived execution trace's hash was checked against its JSON.
Desktop/CPUID output, Snake turn, Color palette, MOV frame 2 and STARSCR phase 2
screenshots were visually inspected. Logs and exit markers:
`build/surface_move_{system,snake,color,mov,starscr}_smoke.{log,exit}`.
These are integration checkpoints for a partial recovery that still depends
on unrecovered components in the original image.

The complete `make -j4 verify` regression for the 209-interval image also
passed on 2026-10-27, with exit status 0 in approximately 754 seconds.
Log: `build/verify_surface_move.log`; exit marker:
`build/verify_surface_move.exit`; command/status/timing summary:
`build/verify_surface_move_summary.json`. This reruns all kernel fixtures,
including 51,200 moved-layer cases and 628,156 32-bit rasterizer cases in
each host-linked and installed-address mode. The four application
differential suites, executable-format tests and both entire-disk
application-image verifiers also pass. The five archived QEMU smoke image
hashes were rechecked after this regression and still match the built images.
This closes the 209-interval verification checkpoint; it does not remove
the remaining original-code dependencies or the STARSCR limitation above.

`kernel/surface_scale.c/.h` recovers the next interval, the same-depth scaler
at `0x8194e4..0x819c40`. It copies matching 8-bit and 32-bit surfaces using
the original integer threshold algorithm, including the single vertical
repeat adjustment, 32-bit high-byte masking, live caller words and descriptor
reloads, cached row bases, forward overlapping copies and raw return values.
Ratios are calculated before depth rejection; zero divisors and signed
quotient overflow retain the original x86 divide fault. The only assembly
is the incoming-storage adapter and the explicit hardware divide boundary.

The standalone original/C/byte-addressed-oracle fixture passes 50,252 cases,
including 536 metadata-alias and 204 caller-word-alias cases, four descriptor
alignments, wrapped layer indices, signed-boundary dimensions, all depth
branches and exact mapped-data/return comparisons. Each modeled access and
loop is bounded before either executable runs. It explicitly skips 1,732
unsafe generated scenarios and separately classifies 2,784 divide-fault
scenarios; 192 forked original/C executions confirm horizontal/vertical
divide-by-zero and quotient-overflow SIGFPE, including unsupported depths.
Logs: `build/surface_scale_preview_build.log`, `build/surface_scale_preview.log`;
exit marker: `build/surface_scale_preview.exit` (0). Production compilation
occupies 1,097 bytes against 1,884 available. This source checkpoint precedes
installation in the VHDs; the installed manifest remains at 209 intervals.

The scaler is now installed at its original `0x8194e4` entry, using
1,097/1,884 bytes and bringing the manifest to 210 intervals. C-init, Snake
and combined-application VHDs rebuild, and host-linked and installed-address
fixtures each pass all 50,252 data/return cases and 192 fault executions.
The entire-disk kernel check and both application-image checks pass: only
declared boot/kernel/application extents change. Logs and exit markers:
`build/surface_scale_install_build.{log,exit}` and
`build/surface_scale_{host,linked,disk,snake_image,apps_image}_test.{log,exit}`
(all final exits 0). This closes installation and focused verification;
the previous full-suite and QEMU results still describe the 209-interval
images until the new integration/regression runs finish.

`make smoke-surface-scale` builds a separate diagnostic-only image,
`build/OpenIceStarOS_scale_diagnostic.vhd`, by replacing only the existing
COLOR.X file extents with `tests/surface_scale_guest.c`. The production
application images retain the recovered Color. The real CMD loads this
type-0x0a X through the normal loader; 47 absolute relocations and the guest
instructions/data are verified before inspecting its report. The test borrows
two zero layer-table entries at the high-water mark under saved/disabled
interrupts, calls the original shared service slot 163, restores both entries
and the interrupt flags, then cooperatively yields. It does not modify the
high-water mark or allocate a visible test window.

The diagnostic passed in QEMU on 2026-10-27: six matching-depth calls cover
equal, smaller and larger targets for both 8-bit and 32-bit surfaces. An
independent Python model checks all 2,304 destination/guard bytes, all source
buffers, six raw returns, slot restoration and enabled-interrupt restoration.
The address-filtered trace records exactly six entries at `0x8194e4` and
1,044 C-body/return execution events. All 210 installed intervals also match
guest memory. This explicitly demonstrates test-app calls, not natural scaler
use by an original application. Evidence:
`build/surface_scale_smoke.json`, `build/surface_scale_execution.json`,
`build/surface_scale_guest_report.memory`, `build/surface_scale_guest.trace`;
log/exit/timing: `build/surface_scale_diagnostic_smoke.{log,exit}` and
`build/surface_scale_diagnostic_smoke_summary.json` (exit 0).

The production 210-interval images also passed five separate QEMU snapshot
boots on 2026-10-27. The system check verifies desktop input, relocated CMD,
keyboard queue/prompt, detected RAM, live heap headers, timer preemption and
suspended recovered C scheduler state. Snake movement/collision/restart/turns,
Color's 600 palette probes and close/task-exit/CMD reuse, MOV's four frames
and close/task-exit/CMD reuse, and STARSCR's four phases, 21 relocations,
Esc exit and task cleanup pass. STARSCR's subsequent mouse probe still stalls;
desktop reuse after that application remains unverified.

All five production smoke runs and the separate diagnostic run exit 0 and
compare all 210 kernel intervals with guest memory. Fresh artifacts are
archived under `build/surface_scale_evidence/{diagnostic,system,snake,color,mov,starscr}/`;
`results.json` records commands, durations, image digests, artifact lists and
artifact digests. Stale files from previous runs are excluded. The six image
digests and archived diagnostic trace digest were rechecked. Desktop CPUID,
Snake turn, Color palette, MOV frame 2 and STARSCR phase 2 screenshots were
visually inspected. Logs/exit markers:
`build/surface_scale_{system,snake,color,mov,starscr}_smoke.{log,exit}`.
These checks establish bounded integration of the current partial recovery;
they do not establish a fully recovered OS or fix the screen-saver limitation.

The 210-interval regression was completed in two segments on 2026-10-27.
The original `make -j4 verify` process was terminated with status 143 after
the host-linked 32-bit rasterizer passed, while the installed rasterizer
was starting; it has no successful whole-command exit marker. Its completed
prefix is retained in `build/verify_surface_scale.log`. A continuation reran
the installed rasterizer and every remaining recipe command, including both
move/scaler fixtures, fault cases, all application suites and both whole-disk
application verifiers. All 14 continuation commands exited 0 in 138.521 seconds.
Log/exit/timing: `build/verify_surface_scale_resume.log`,
`build/verify_surface_scale_resume.exit`, and
`build/verify_surface_scale_resume_summary.json`. Production sources and
images were unchanged between the segments, and the C-init, Snake and combined
application SHA-256 hashes match both the recorded QEMU evidence and the
before/after continuation digests. This is a completed segmented regression,
not a claim that the interrupted `make verify` process itself exited 0.

`kernel/surface_lifecycle.c/.h` recovers surface-table initialization,
free-slot search, creation and deletion at `0x8175c0`, `0x8177c4`,
`0x817626` and `0x81783e` respectively. The C code retains signed high-water,
capacity and free-count comparisons, wrapped indices/products, unaligned
descriptors, live caller storage, owner/depth reloads, descriptor aliases,
allocation-failure cleanup and callback ordering. Deletion passes the original
descriptor's +44 value unchanged to the still-original `0x8178f0` function,
then reloads the caller index and table for the final free/clear operations.
Creation failures deliberately leave interrupts disabled as in the original.
Small assembly boundaries expose incoming argument storage, capture the free
callback's failure EFLAGS before stack cleanup, and preserve flags at RET.

The standalone original/C/independent byte-memory oracle passes 175,692 cases
on 2026-10-27, comparing all 34,816 mapped bytes, raw returns, ordered external
calls, watched boundary state, outgoing argument mutations and successful
native POPFL returns. Native failure cases also compare arithmetic flags and
DF directly against original execution, including failed descriptor/pixel
allocations and negative slot results. Test CALL sites redirect the two/three
byte CLI/read-flags leaves without overwriting adjacent routines.
Log/exit/timing: `build/surface_lifecycle_preview_test.log`,
`build/surface_lifecycle_preview_test.exit` (0), and
`build/surface_lifecycle_preview_summary.json` (36.897 seconds).
Production compilation with the original four-byte i386 stack boundary fits
the four extents: 66/102, 344/414, 61/122 and 170/178 bytes in address order.
This source checkpoint precedes installation; the manifest remains at 210.

The four surface lifecycle routines are installed in their original entry
intervals, bringing the manifest to 214. Explicit linker assertions retain
all four boundaries; the 170-byte deletion implementation fits its original
178-byte slot using `-mpreferred-stack-boundary=2`, matching the original
four-byte i386 caller alignment. Existing 210 interval bytes match the prior
QEMU guest dump. Host-linked and installed-address fixtures each pass all
175,692 cases, including native success/failure flags. C-init, Snake and
combined-application images rebuild, and the complete kernel disk verifier
and both application-image verifiers exit 0. Evidence:
`build/surface_lifecycle_install_{build,host,linked,disk,snake_image,apps_image}.{log,exit}`,
`build/surface_lifecycle_install_summary.json`, and
`build/surface_lifecycle_install_images.json`. This records installation and
focused checks; the 210-interval regression/QEMU notes remain historical
until the new images are tested.

The 214-interval production application image passes
`make smoke-surface-lifecycle` on 2026-10-27 (exit 0; 29.559 seconds).
The filtered QEMU trace observes the recovered initialization entry once,
creation nine times, free-slot search four times and deletion once during
normal startup, CMD input and the recovered Color application's lifecycle.
The check also verifies all 214 installed intervals in guest memory,
128 MiB detected RAM, 59 live heap headers, timer/TSS preemption, PS/2
input, relocated CMD, 600 palette pixels, Color close/task exit and CMD reuse.
Desktop, CPUID, Color palette and post-close prompt screenshots were visually
inspected. Fresh artifacts are archived in
`build/surface_lifecycle_evidence/color/`, with image/trace/artifact digests
in `build/surface_lifecycle_evidence/results.json`. This is bounded
integration evidence for the partial recovery; the complete 214-interval
regression and the other application checks are recorded separately.

Separate 214-interval QEMU boots also pass Snake movement, collision, restart
and arrow/letter turns (34.309 seconds); MOV's 16 relocations, four exact
frames, PS/2 close, cleanup and CMD reuse (32.410 seconds); and STARSCR's
21 relocations, four phases with 10,797 pixel probes each, Esc exit and task
cleanup (26.683 seconds). All three commands exit 0 and compare every
installed interval with guest memory. The screen saver's subsequent mouse
probe still stalls, as in the original-image baseline; desktop reuse after
STARSCR is unverified. Fresh evidence is archived under
`build/surface_lifecycle_evidence/{snake,mov,starscr}/`; `results.json` records
commands, durations, image and artifact hashes. All archived digests and
current image digests were rechecked; Snake turn, MOV frame 2 and STARSCR
phase 2 screenshots were visually inspected. Logs/exit markers:
`build/surface_lifecycle_{snake,mov,starscr}_smoke.{log,exit}`.

`kernel/surface_order.c/.h` recovers removal, append, front promotion and
positioned movement at `0x8178f0`, `0x8179d6`, `0x817a3c` and `0x817b32`.
The C retains the original one-based order list, live count/caller/descriptor
reloads, wrapped indexing, unsigned gates and signed move-loop conditions.
Aliased metadata can make both movement directions run in one invocation;
equal-position movement instead returns before the flags-write callback and
exposes the original comparison flags. Small assembly adapters preserve that
exception, incoming argument storage, raw EAX and flags at RET.

The standalone original/C/independent bounded byte-memory oracle passes
152,591 cases on 2026-10-27 in 27.857 seconds, comparing all 29,696 mapped
data bytes, raw returns and ordered flag callbacks. It covers twelve alias
layouts, four descriptor alignments, live target/count mutations and wrapped
table/list indices; 86,116 cases compare native EFLAGS/DF and 268 execute both
move directions. All eleven required branch bits are exercised. The oracle
rejects 35,761 unsafe generated scenarios before either executable runs.
Production compilation uses `-fwrapv-pointer` and the original four-byte
stack boundary; the four implementations occupy 156/230, 98/102, 166/246
and 303/414 bytes. Log/exit/timing:
`build/surface_order_preview_test.log`, `build/surface_order_preview_test.exit`
(0), and `build/surface_order_preview_summary.json`. This source checkpoint
precedes installation; the manifest still declares 214 intervals.

`kernel/cursor.c/.h` recovers cursor show, hide, shape selection and movement
at `0x8130cc`, `0x81313e`, `0x8131b0` and `0x81325c`. Showing appends layer 1
and refreshes from layer 1; hiding removes it and refreshes from layer 2.
Geometry is reloaded after the order callback. Shape/movement routines retain
the descriptor captured before hiding, select the original six pixel-source
globals, preserve unsupported shape values and reload live incoming words
after each potentially aliased store. Only the incoming-storage adapters use
assembly.

The standalone original/C/independent byte-memory fixture passes 86,400 cases
on 2026-10-27 in 11.518 seconds with no unsafe generated scenarios. It compares
all 25,600 mapped data bytes, raw returns, callback order/arguments and watched
state, including nine alias layouts, four descriptor alignments, five position
words, all ten supported shapes and four unsupported values, sixteen callback
mutation modes, captured-pointer semantics and outgoing-argument isolation.
The production object fits the four original slots, including its jump tables:
75/114, 75/114, 136/172 and 148/196 bytes. Log/exit/timing:
`build/cursor_preview_test.log`, `build/cursor_preview_test.exit` (0), and
`build/cursor_preview_summary.json`. This is another source checkpoint;
the installed manifest remains at 214 until integration is recorded below.

The complete 214-interval `make -j4 verify` regression exits 0 on 2026-10-27
in 869.337 seconds. Both variants of every differential fixture, boot/INIT
checks, exact interrupt/timestamp layouts, application suites and both final
whole-disk application verifiers pass in one uninterrupted command.
All three production image SHA-256 values are unchanged before/after the
suite and match the archived QEMU evidence. Log/exit/timing and image hashes:
`build/verify_surface_lifecycle.log`, `build/verify_surface_lifecycle.exit`
(0), and `build/verify_surface_lifecycle_summary.json`.

The four surface order routines are installed at their original entry
addresses, bringing the manifest to 218 intervals. Linker assertions retain
all four extents; their production sizes are 156/230, 98/102, 166/246 and
303/414 bytes. Host and installed-address fixtures each pass 152,591 cases,
including 86,116 native EFLAGS/DF comparisons and 268 cases traversing both
movement directions. All previous 214 interval bytes remain unchanged, and
the kernel file changes only within the four new slots. C-init, Snake and
combined-application image builds and all three disk verifiers exit 0.
Logs and timings: `build/surface_order_install_{build,host,linked,disk,snake_image,apps_image}.{log,exit}`,
`build/surface_order_install_summary.json`; image hashes and interval checks:
`build/surface_order_install_images.json`. The recorded hashes were rechecked
against all three current images before this installation checkpoint.

The 218-interval application image also passes the command exposed as
`make smoke-surface-order` (exit 0; 29.702 seconds). Filtered QEMU execution
records remove/append/front/move entries 9/14/3/4 times respectively and
observes compiled body blocks for every function. The boot also verifies all
218 installed intervals, RAM/heap structure, timer/TSS preemption, PS/2 input,
CMD/CPUID, the recovered Color palette and close/task-exit/CMD-reuse sequence.
Desktop, CPUID, palette and post-close screenshots were visually inspected.
Fresh artifacts, image/trace hashes and timing are archived in
`build/surface_order_evidence/color/` and
`build/surface_order_evidence/results.json`; all 19 artifact hashes and the
current application-image hash were rechecked. This is bounded integration
evidence, separate from a complete regression of the new image.

Separate 218-interval QEMU application boots pass Snake's movement, collision,
restart and turns (33.502 seconds), MOV's 16 relocations and four exact frames
plus close/cleanup/CMD reuse (32.235 seconds), and STARSCR's 21 relocations,
four phases with 10,797 pixel probes each, Esc exit and task cleanup
(26.121 seconds). Each command exits 0 and verifies every installed interval
in guest memory. STARSCR's post-exit mouse probe still stalls, matching the
previous original-image baseline; desktop reuse remains unverified.
Fresh artifacts and image/artifact hashes are archived under
`build/surface_order_evidence/{snake,mov,starscr}/` and `results.json`.
All recorded hashes were rechecked, and Snake turn, MOV frame 2 and STARSCR
phase 2 screenshots were visually inspected. Logs/exit/timing:
`build/surface_order_{snake,mov,starscr}_smoke.{log,exit}` and corresponding
`_summary.json` files.

The four cursor helpers are now installed at `0x8130cc`, `0x81313e`,
`0x8131b0` and `0x81325c`, bringing the manifest to 222 intervals.
Their production extents, including both shape jump tables, occupy
75/114, 75/114, 136/172 and 148/196 bytes. Explicit linker assertions
retain each entry and boundary. Host and installed-address fixtures each
pass all 86,400 original/C/oracle cases; all prior 218 interval bytes remain
unchanged, and the kernel file changes only within the four cursor slots.
C-init, Snake and combined-application builds and all three complete disk
verifiers exit 0. Logs/exit/timing:
`build/cursor_install_{build,host,linked,disk,snake_image,apps_image}.{log,exit}`,
`build/cursor_install_summary.json`; interval checks and image hashes:
`build/cursor_install_images.json`. An initial application-verifier invocation
used an unsupported test option; its failed log is retained separately and
the documented successful checks use `tools/app_image.py verify` directly.

The 222-interval application image passes `make smoke-cursor` on 2026-10-27
(exit 0; 38.812 seconds). The first trace missed positioned movement, and
an initial CMD-border probe failed because CMD uses style 2. The check now
launches the original DEVMGR.X, whose seventh creation argument is style 3,
and uses only PS/2 input to hover its top-left border and interior. It checks
shape 2 with the original eight-pixel hotspot, normal-shape restoration,
pixel-source pointers, visibility, window deletion and return to root in CMD.
Filtered execution observes show/hide/select/move entries 10/10/11/1 times
and body blocks for all four compiled routines. All 222 intervals match guest
memory; RAM/heap, timer/TSS preemption, keyboard input, CMD/CPUID and recovered
Color palette/close/task-exit/CMD-reuse checks also pass. Border, interior and
Color screenshots were visually inspected. Successful artifacts and hashes
are archived in `build/cursor_evidence/color/` and `results.json`; earlier
failed attempts remain in separately named subdirectories. The application
image SHA-256 remains
`1bef08e3b88d858e5bf8b7ee80e3ed9942e0d66a12f2ebe1d33a382ae2f57c79`.
This is focused integration evidence, not a complete 222-interval regression.

### Firmware scan source checkpoint (2026-10-27)

`kernel/firmware_scan.c/.h` recovers the startup routines at
`0x815106..0x8152cc` (ACPI-like discovery) and `0x8152cc..0x815320`
(PMID discovery). The source preserves the original bytewise scan endpoints,
seven-byte `RSD PTR` comparison, both reads of the root's first entry,
signed wrapped length calculation, linear four-byte candidate walk, signed
control byte, unused word read and exact I/O callback order. FACP fields
are captured before the callbacks. The ACPI scan continues after a match;
PMID stops at the first match and leaves its global unchanged on a miss.
These are recovered behaviors, not a standards-compliant ACPI replacement.

`make test-firmware-scan-preview` passes 320 synthetic scenarios, with 640
executions of original x86 code and standalone C against an independent
memory and I/O oracle. The multiboot QEMU fixture enables paging so address
zero, firmware ROM addresses and cross-1-MiB reads can use controlled RAM.
Cases cover every signed-byte value, signed/wrapped lengths, invalid and
unaligned signatures, inclusive/exclusive bounds, null and wrapped root
pointers, linear rather than indirect table traversal, multiple signatures,
first inner match, port addition overflow and callback mutation of captured
fields. Each execution checks all `0x110000` mapped bytes and callee-saved
registers. Only the embedded fixture copies have their I/O entries hooked;
the production kernel and disks are unchanged at this checkpoint.

Reproduction: `python3 tests/firmware_scan_qemu.py` (QEMU exit 33 means
success). Evidence: `build/firmware_scan_preview_build.log`,
`build/firmware_scan_preview_qemu.log`, `build/firmware_scan_preview_summary.json`.
The installed manifest still contains 222 intervals until integration below.

### Firmware scan installation (2026-10-27)

The two firmware routines are installed at their original entries, bringing
the kernel manifest to 224 intervals. The ACPI body uses 253 of 454 bytes;
PMID uses 50 of 84 bytes. Linker assertions enforce entry addresses and slot
bounds. The module uses `-fno-delete-null-pointer-checks` because the original
PMID scan intentionally starts at address zero. Both startup callers ignore
EAX, so the recovered entry points expose void interfaces.

`make test-firmware-scan` passes all 320 scenarios with 960 original,
standalone-C and installed-C executions. Full-disk verification passes for
the init image and both application images. All 222 previously installed
intervals remain byte-identical; all 521 changed kernel bytes are confined
to `0x815106..0x815320`. Image hashes and slot sizes are recorded in
`build/firmware_scan_install_images.json`. The application image SHA-256 is
`20d0c0320627eab6c329d2b9f4ab5c61a773f8f57195938e07cf46673ce23dfc`.

### Startup installation and production boot (2026-10-27)

Seven recovered intervals are now installed: kernel entry/bootstrap, display
and object-table startup state, the startup application caller, VGA palette
construction and software color tables. The manifest contains 231 intervals.
The entry calls the three recovered startup routines by their C names; identity
paging, startup UI and background/window setup remain explicit original-code
boundaries, as do the image launcher and hardware DAC writer.

All four installed differential suites pass (entry: 864 scenarios, startup
state: 6,897, startup applications: 480, palette/tables: 684). Complete-disk
reconstruction passes for the init, Snake and combined-application images.
Every previous 224 interval is byte-identical, and all 1,499 changed kernel
bytes lie within the seven new slots. Evidence:
`build/startup_install_images.json`, `build/startup_install_build.log`,
`build/startup_install_disk_verify.log` and the four
`build/*_installed_summary.json` files. The 231-interval kernel SHA-256 is
`0f28ad0eeafa9a97fe368cc84d2ca09afa1d1584c1e758c6c1df712af9cb6382`;
combined-application image SHA-256 is
`fd413dfa25246c1e6867ff0336e61c2aedb094728859d8dfb77af95912eb2b95`.

### Install four startup intervals (2026-10-28)

The production manifest now installs 235 intervals, including startup UI and
resource orchestration, desktop/background creation, VGA DAC writes and
identity paging. Their compiled sizes are respectively 813/1,744, 336/494,
161/166 and 102/106 bytes. Entry/bootstrap calls now use the recovered names.
All 231 previous intervals remain byte-identical. All 2,440 changed kernel
bytes fall inside the four new slots. Each rebuilt disk differs from its
231-interval predecessor only in the existing kernel file's extents; FAT,
directory entries, other file contents and slack are unchanged.

Eight focused installed suites pass: entry, startup state/application caller,
palette, DAC, paging, desktop and startup UI. The four new suites execute
59,328 scenarios / 177,984 original/C/fixed-address executions, including
privileged DAC and paging QEMU fixtures. Evidence:
`build/startup_235_build.log`, `build/startup_235_focused_summary.json`,
`build/startup_235_installation_summary.json` and the corresponding test logs.
The pre-installation manifest, ELF, kernel and disks are retained in
`build/startup_before_235/`.

Four sequential production snapshot boots pass with Color, Snake, MOV and
STARSCR. Each verifies detected RAM/heap, real task switches, desktop input,
all 30 ordered entry/bootstrap blocks, 28 subsystem targets and every one of
the 235 loaded slots. Color verifies 600 palette pixels and close/CMD reuse;
Snake verifies movement, collision and restart; MOV verifies four frames and
close/CMD reuse; STARSCR verifies four phases, Esc and task cleanup. The
previously documented original post-STARSCR mouse/timer stall remains;
desktop reuse after that application is not verified. Logs, screenshots,
traces, memory captures and artifact hashes are archived in
`build/startup_235_evidence/{color,snake,mov,starscr}/`.

The 235-interval kernel SHA-256 is
`958993ea2566c0f39508ac10532f7c7a64269d177e402fb5e2c3ec1b9aa3f121`;
the combined-application disk SHA-256 is
`8b6e65969f271a297243af2a8df04cf881d924ccfdfc65a674dc0871bdb7bb12`.
These are focused installation/boot checks. The complete 231-interval
regression above does not establish a full regression of the 235-interval
image. Image decoders, resource/file loaders and object creation remain
explicit original-code dependencies.

`make smoke-kernel-entry` passes a real QEMU snapshot boot. The new
`tools/entry_smoke.py` compares all 28 external CALL targets against the
original entry/bootstrap and verifies 30 ordered executed blocks through entry
into the event loop. All 231 compiled intervals match loaded guest memory.
The boot also verifies 128 MiB RAM, 59 heap headers, timer/TSS switching, PS/2
input, CMD/CPUID, 600 Color pixel probes, closing the Color window, task exit
and CMD reuse. Desktop, CPUID and Color screenshots were visually inspected.
This normal 32-bit display boot does not execute the 8-bit palette-construction
path; that path is covered by the differential fixtures above.

Evidence: `build/startup_boot_smoke.log`, `build/entry_execution.json`,
`build/entry_guest.trace`, `build/boot_openicestaros.png`, `build/openicestaros_input_cpuid.png`,
`build/color_launched.png` and `build/color_closed.png`. These are focused
installation and boot checks; the complete 224-interval regression recorded
above is not a full regression of the new 231-interval image.

### VGA DAC writer source checkpoint (2026-10-27)

`kernel/display_dac.c/.h` recovers `0x81ad6e..0x81ae14`. The C body saves
the flags-helper result, disables interrupts, selects DAC index port `0x3c8`,
then writes the three separately read RGB components shifted right by two to
`0x3c9`. The inclusive comparison is signed; both the index and RGB address
advance with 32-bit wrapping. A 30-byte assembly ABI wrapper preserves the
original incoming RGB argument-slot update and performs only LEAVE/RET after
the flags restore, avoiding flag damage from compiler-generated stack cleanup.
The complete compiled routine is 161/166 bytes.

`python3 tests/display_dac_qemu.py` passes 6,912 scenarios / 20,736 executions
of original x86, ordinary C and fixed-address C at CPL0. An independent oracle
checks every ordered helper/I/O event, full-width and signed index arguments,
unaligned RGB sources, address wrapping through zero, component-by-component
callback mutation, all 20 KiB of mapped source/side-effect memory, updated
incoming arguments, stack canaries/balance and callee-saved registers. Native
CLI/POPFL verifies IF, DF and arithmetic flags for injected flags-helper return
values. Across 324 hardware checks the fixture writes real emulated VGA DAC
ports and reads back all 768 components. INT_MAX-crossing ranges are checked
with a controlled four-triplet escape; their full enormous loops are not run.
Only CALL operands in fixture copies are hooked; the production CPU helpers
and images are unchanged at this source checkpoint.

QEMU exits 33 (success) after 21.922 seconds. Evidence:
`build/display_dac_preview_test.log`, `build/display_dac_preview_qemu.log`,
`build/display_dac_preview_summary.json`, `build/display_dac_calls.h` and
`build/display_dac_preview.elf`. Installation follows after the running
231-interval baseline regression, so that baseline is not modified mid-run.

### Identity paging source checkpoint (2026-10-27)

`kernel/paging.c/.h` recovers `0x837448..0x8374b2`. Two C loops construct
1,024 PDEs at `0x19000` and 1,048,576 PTEs at `0x100000`, with original
`0x107` flags and wrapped 32-bit physical addresses covering 4 GiB. Stores
explicitly use GS rather than assuming DS has base zero. A small ISA wrapper
temporarily sets GS=8, loads CR3=`0x19000`, preserves CR0 while enabling PG,
returns CR0 in EAX, restores GS and the original register contract, and leaves
the final OR's flags intact. The compiled routine occupies 102/106 bytes.

`python3 tests/paging_qemu.py` passes 192 scenarios / 576 original-x86,
ordinary-C and fixed-address-C executions at CPL0. Ninety-six scenarios run
the privileged routines unhooked, including transitions with paging initially
off and initially on using an independent fixture map. Another 96 suppress
only the two control-register stores in fixture copies, allowing byte-exact
checks of every emitted table entry without CPU-generated accessed/dirty
bits. Unhooked runs compare every entry excluding those two hardware bits.
Both paths check table boundary guards, DS=flat/nonflat, GS=null/flat/nonflat,
CR0 feature preservation, CR3, EAX, all six other general registers, all data
selectors, stack balance/canaries and defined final flags including IF/DF.
The fixture lives above 6 MiB so the 1..5 MiB output cannot overwrite it.

QEMU exits 33 (success). Evidence: `build/paging_preview_test.log`,
`build/paging_preview_qemu.log`, `build/paging_preview_summary.json`,
`build/paging_cr_patches.h` and `build/paging_preview.elf`. Production images
remain unchanged while the 231-interval baseline regression runs.

### Additional startup application boots (2026-10-27)

Three sequential snapshot boots of the 231-interval application image pass:
Snake (34.345 seconds), MOV (32.514 seconds) and STARSCR (26.703 seconds).
Each verifies the recovered startup's 30 ordered blocks, all 28 external call
targets and all 231 loaded intervals. Application checks cover Snake movement,
collision and restart; all four exact MOV frames, closure and CMD reuse; and
STARSCR's four phases, Esc exit and task cleanup. STARSCR's original post-exit
mouse/timer stall remains; desktop reuse after it is not verified.

Per-boot evidence and hashes are archived in
`build/startup_evidence/{color,snake,mov,starscr}/`; `results.json` records the
three additional runs. Logs: `build/startup_{snake,mov,starscr}_smoke.log`.

Evidence: `build/firmware_scan_install_build.log`,
`build/firmware_scan_install_test.log`, `build/firmware_scan_installed_qemu.log`,
`build/firmware_scan_installed_summary.json`. The installed QEMU fixture is
also included in `make verify`. This records focused installation checks;
production boot coverage and a full 224-interval regression follow separately.

### Firmware production boot coverage (2026-10-27)

`make smoke-firmware` boots the 224-interval application image and records
execution of the entry and body of both recovered firmware routines. Each
entry executes once. All 224 installed intervals match guest memory. The
same boot verifies 128 MiB RAM, 59 heap headers, timer/TSS switching, real
PS/2 input, CMD/CPUID, Color's 600 palette probes, window closure, task exit
and CMD reuse. The desktop, CPUID output and Color screenshot were visually
inspected. No production image changes or guest-memory writes are used.

Three further snapshot boots, using `--firmware-check` with `--snake-check`,
`--mov-check` and `--starscr-check`, pass in 33.539, 32.335 and 26.266 seconds.
They cover Snake movement/collision/restart, all four MOV animation frames,
and STARSCR's four phases/Esc exit/task cleanup. Both firmware entries and
all 224 installed intervals are verified on each boot. STARSCR retains the
previously documented original post-exit mouse/timer stall; desktop reuse
after that application is still not verified.

Logs, per-boot traces, screenshots and memory evidence are archived in
`build/firmware_evidence/{color,snake,mov,starscr}/`; `results.json` records
image and artifact hashes. Logs are also retained as
`build/firmware_{boot,snake,mov,starscr}_smoke.log` with exit files (all 0).
These focused production boots precede the complete regression below.

### Kernel entry source checkpoint (2026-10-27)

`kernel/entry.c/.h` recovers entry `0x800040..0x800070` and bootstrap
`0x800070..0x800104`. The C preserves the original 28 ordered subsystem
calls, including enabling interrupts before PIT/RTC/device initialization,
the two PIC-mask writes (`0x21 <- 0xd8`, `0xa1 <- 0xec`), and their NOP
delay. Six address-named subsystem bodies remain original machine code;
their addresses and interfaces are explicit in `kernel/entry_boundaries.def`.
Recovering their caller does not establish recovery of those bodies.

`python3 tests/kernel_entry.py` compiles a separate preview without changing
production images. It passes 864 scenarios, with 2,592 executions of original
x86, ordinary C and fixed-address C. Bootstrap-only and complete-entry cases
check 22/28 calls, PIC arguments, all `0x3000` shared-memory bytes, stack
balance/canaries and callee-saved registers. Cases combine three memory
patterns, 16 callback mutation modes and nine return-value seeds. Original
and candidate fixture copies have CALL operands redirected to shared boundary
callbacks; tiny CLI/STI/NOP callees are not overwritten. This isolates the
orchestration and does not test device initialization itself.

Preview entry/bootstrap bodies occupy 40/48 and 135/148 bytes; linker
assertions enforce both original entry addresses and slot bounds. Evidence:
`build/entry_preview_summary.json`, `build/entry_calls.h`,
`build/entry_preview.elf`. The production manifest remains at 224 intervals
at this source checkpoint while its existing full regression finishes.

### Additional startup state source checkpoint (2026-10-27)

`kernel/startup_state.c/.h` recovers display-record initialization at
`0x81acc4..0x81ad6e` and object-table counter initialization at
`0x82aa0f..0x82aa33`. Display setup captures the record pointer once, retains
the original sequence of boot-parameter reads and stores, preserves offsets
28/32, calls palette initialization only for depth 8, then re-reads the
captured record's depth before selecting display dispatch. The two palette
and dispatch subsystem bodies remain original code. Object-table setup
writes exactly `0x4000`, 0 and 0 to its three original globals.

`python3 tests/kernel_startup_state.py` passes 6,864 display cases and 33
counter cases: 20,691 original-x86, ordinary-C and fixed-address-C executions.
Display scenarios combine three memory patterns, 13 aligned/unaligned/aliasing
record addresses, 11 full-width depth values and 16 callback mutation modes.
Aliases include boot parameters, the pointer global, object-table globals
and a page boundary. The bytewise oracle models sequential reads/writes
independently and checks all `0x5000` mapped bytes. Tests also verify callback
order/arguments, live depth after palette mutation, stack canaries/balance
and callee-saved registers. Only fixture CALL operands are redirected.

The preview bodies use 127/170 and 31/36 bytes, with fixed-entry/size linker
assertions. Evidence: `build/startup_state_preview_summary.json`,
`build/startup_state_calls.h` and `build/startup_state_preview.elf`.
Production kernels and disks are unchanged at this source checkpoint.

### Startup application caller source checkpoint (2026-10-27)

`kernel/startup_apps.c/.h` recovers `0x810f38..0x810f78`: read the first
argument pair, invoke original image entry `0x802282`, wait 20 ticks, then
read and launch the second pair. Each pair retains the original second-then-
first volatile read order. Neither pair is cached across launch/wait callbacks.
The image-launching body remains unrecovered; its explicit boundary does not
claim recovery of executable loading or of the launched applications.

`python3 tests/kernel_startup_apps.py` passes 480 scenarios / 1,440 executions
of original x86, ordinary C and fixed-address C. Three memory patterns, ten
full-width parameter seeds and 16 callback mutation modes cover launch/wait
order, the 20-tick argument, live second parameters, unaligned callback writes,
all `0x3000` shared-memory bytes, stack canaries/balance and callee-saved
registers. Only fixture CALL operands are redirected. The compiled body uses
51/64 bytes, with a fixed-entry/size linker assertion.

Evidence: `build/startup_apps_preview_summary.json`,
`build/startup_apps_calls.h`, `build/startup_apps_preview.elf`. Production
images are unchanged at this source checkpoint.

### Display palette source checkpoint (2026-10-27)

`kernel/display_palette.c/.h` recovers VGA palette construction at
`0x81ae14..0x81afbc` and software color-table initialization at
`0x81afbc..0x81b26e`. Palette construction retains the initial 0..15 write
using the captured global pointer, then builds and writes all 216 RGB
triplets for indices 16..231. The hardware DAC writer at `0x81ad6e` remains
original code. The software table uses identity entries for depth 8. Depth
32 uses the original 16 colors and six levels `[0,48,101,153,206,255]`, which
deliberately differ from the VGA cube's 51-step components. It also writes
the original unusual `0x07f00000` value at index 232; indices 233..255 are
untouched. Unsupported depths leave the entire table unchanged.

`python3 tests/kernel_display_palette.py` passes 288 palette scenarios and
396 table scenarios / 2,052 original-x86, ordinary-C and fixed-address-C
executions. Cases cover three memory patterns, 12 aligned/unaligned/aliasing
table/source addresses, 11 full-width depths and eight palette callback
mutation modes. Tests check all generated RGB bytes, ordered DAC ranges,
captured pointers even when the pointer global aliases the output, mutable
temporary palette data, all `0x8000` shared-memory bytes, stack canaries and
callee-saved registers. Fixture CALL operands alone are redirected.

The preview uses 144/424 bytes for palette construction and 285/690 bytes
for the software table including its read-only constants. Entry/size linker
assertions cover both. Evidence: `build/display_palette_preview_summary.json`,
`build/display_palette_calls.h` and `build/display_palette_preview.elf`.
Production images remain unchanged at this source checkpoint.

### Complete 224-interval regression (2026-10-27)

The uninterrupted `make -j4 verify` baseline passes with exit 0 in 950.796
seconds. It covers both ordinary-C and fixed-address kernel tests, the
installed firmware QEMU fixture, timestamp checks, four application tests,
image-tool tests, and complete-disk reconstruction checks. Kernel, init,
Snake and combined-application image hashes are identical before and after
the run. The source checkpoints above do not change this baseline.

Evidence: `build/verify_firmware.log`, `build/verify_firmware.exit` and
`build/verify_firmware_summary.json`. Baseline kernel SHA-256:
`44ef53e627d5ddd6757e0a89012615de4e912b9b33ddee015676395d4d3baab3`.
The combined-application image SHA-256 remains
`20d0c0320627eab6c329d2b9f4ab5c61a773f8f57195938e07cf46673ce23dfc`.

### Desktop startup source checkpoint (2026-10-28)

`kernel/startup_desktop.c/.h` recovers `0x810d4a..0x810f38`. It creates the
background and desktop objects in their original order, handles each signed
creation result independently, looks up the configured image, selects exactly
uppercase JPG/PNG, applies live display dimensions and desktop color, activates
successful objects, and finally promotes layer 1. Display/color base pointers
are captured once; object slots and source fields are re-read for each store,
including when a preceding store aliases another slot. The object factory,
image lookup, JPEG/PNG renderers and object activation remain explicit original
machine-code boundaries. Their bodies are not recovered by this caller.

`python3 tests/kernel_startup_desktop.py` passes 15,360 scenarios / 46,080
original-x86, ordinary-C and fixed-address-C executions in 3.566 seconds.
The independent bytewise model covers two memory patterns, ten aligned,
unaligned and aliasing layouts, all four independent creation-success/failure
combinations, six full-width signed lookup results, eight extensions and four
callback-mutation modes. It checks ordered boundary calls, captured pointers,
live source/slot reads, all 48 KiB of fixture memory, stack canaries/balance
and callee-saved registers. Only fixture CALL operands are redirected.

The body and shared dimension-copy helper occupy 336/494 bytes, with linker
entry/size assertions. Evidence: `build/startup_desktop_preview_test.log`,
`build/startup_desktop_preview_summary.json`, `build/startup_desktop_calls.h`
and `build/startup_desktop_preview.elf`. Production images are unchanged at
this source checkpoint while the 231-interval full regression completes.

### Startup UI/resource caller source checkpoint (2026-10-28)

`kernel/startup_ui.c/.h` recovers `0x81067a..0x810d4a`. It preserves both
startup drawing branches, signed division toward zero when centering, all 69
progress rectangles, seven resource-lookup stages and 62 one-tick waits.
Successful stages retain the original allocation/read/image-load calls and
destination globals; failure leaves those destinations unchanged. Executable
names are re-read after lookup. Display/color pointers are captured once,
while dimensions and colors are read live in the original argument order.
The drawing routines and file/image loading callees are still original-code
boundaries; recovering their orchestration does not recover their bodies.

`python3 tests/kernel_startup_ui.py` passes 36,864 scenarios / 110,592
original-x86, ordinary-C and fixed-address-C executions in 5.239 seconds.
An independent sign/magnitude centering model and bytewise memory oracle cover
two patterns, eight aligned/unaligned/aliasing layouts, nine dimension values,
eight full-width boot flags, eight resource-success masks and four mutation
modes. Each case checks every ordered drawing/load/wait call, all 40 KiB of
fixture memory, writable metadata across callbacks, stack balance/canaries
and callee-saved registers. Fixture CALL operands alone are redirected.

The caller and relative-rectangle helper occupy 813/1,744 bytes, enforced by
linker entry/size assertions. Evidence: `build/startup_ui_preview_test.log`,
`build/startup_ui_preview_summary.json`, `build/startup_ui_calls.h` and
`build/startup_ui_preview.elf`. Production images remain unchanged here.

### Complete 231-interval regression (2026-10-28)

The uninterrupted `make -j4 verify` baseline passes with exit 0 in 952.084
seconds. It covers the ordinary-C and fixed-address kernel tests, installed
startup/entry/palette tests, the firmware QEMU fixture, exact timestamp test,
all four application suites, image-tool tests and complete-disk reconstruction.
All four production artifacts have identical SHA-256 hashes before and after
the run. The four preview source checkpoints awaiting installation are not
part of this 231-interval baseline.

Evidence: `build/verify_startup.log`, `build/verify_startup.exit` and
`build/verify_startup_summary.json`. Kernel SHA-256:
`0f28ad0eeafa9a97fe368cc84d2ca09afa1d1584c1e758c6c1df712af9cb6382`.
Combined-application disk SHA-256:
`fd413dfa25246c1e6867ff0336e61c2aedb094728859d8dfb77af95912eb2b95`.

### Four startup installations (2026-10-28)

Commit `fdf63f3` installs startup UI/resources, desktop startup, the VGA DAC
writer and identity paging at their original addresses, increasing the
production manifest from 231 to 235 intervals. All prior intervals remain
unchanged; the 2,440 changed kernel bytes are confined to the four new slots.
Complete-disk checks restrict disk changes to the existing kernel file.
Eight focused installed tests pass. Four independent snapshot boots pass
Color, Snake, MOV and STARSCR checks, with startup execution tracing, memory,
task switching, real PS/2 input and all 235 guest-memory intervals checked.
STARSCR's previously observed original post-exit mouse/timer stall persists;
desktop reuse after that application is not established.

Evidence: `build/startup_235_installation_summary.json`,
`build/startup_235_focused_summary.json` and
`build/startup_235_evidence/results.json`. The kernel SHA-256 is
`958993ea2566c0f39508ac10532f7c7a64269d177e402fb5e2c3ec1b9aa3f121`;
the combined-application disk SHA-256 is
`8b6e65969f271a297243af2a8df04cf881d924ccfdfc65a674dc0871bdb7bb12`.

### Object registry source checkpoint (2026-10-28)

`kernel/object_registry.c/.h` recovers name lookup, object state assignment,
registry insertion and removal at `0x8104a0..0x81067a`. Lookup skips signed
nonpositive object pointers, captures capacity once and accepts only comparison
result 1. Registry routines capture the table/capacity but read the count live,
including after writes that alias it. Removal preserves the original extra
read at the old count before decrementing. State writes retain unchecked
32-bit index arithmetic and the unaligned field at offset `0x127`.

`python3 tests/kernel_object_registry.py` passes 61,474 scenarios / 184,422
original-x86, ordinary-C and fixed-address-C executions in 13.445 seconds.
The bytewise oracle checks all 40 KiB of fixture memory, comparison calls and
mutations, stack balance/canaries and callee-saved registers. Cases include
signed limits, duplicate/absent entries, unaligned tables and fields, wrapped
indices, overlapping globals and count changes during table writes. The
fixture explicitly excludes 1,326 unmapped or unbounded alias inputs before
execution; it does not establish behavior for those inputs. Only the name
comparison CALL is redirected in fixture copies of the original/candidate.

Preview bodies occupy 71/128, 31/46, 88/142 and 104/158 bytes. Linker
assertions enforce original entries and slot bounds. Evidence:
`build/object_registry_preview_summary.json`, `build/object_registry_calls.h`
and `build/object_registry_preview.elf`. This source checkpoint does not
change the production disk images.

### Object registry installation (2026-10-28)

The four object registry helpers are now installed at their original entries,
increasing the production manifest from 235 to 239 intervals. Their C bodies
occupy 71/128, 31/46, 88/142 and 104/158 bytes. Every prior interval remains
unchanged; the 450 changed kernel bytes are confined to these four new slots.
Complete-disk reconstruction checks pass for the kernel/init, Snake and
combined-application images, and disk differences are restricted to the
existing kernel file. The installed differential fixture passes the same
61,474 scenarios / 184,422 executions as the source checkpoint.

Four independent QEMU snapshot boots pass Color (29.734 s), Snake (34.699 s),
MOV (32.537 s) and STARSCR (26.778 s) checks. Each verifies startup execution,
all 239 guest-memory intervals, allocator/task state and real PS/2 input.
Color verifies 600 palette probes and closure; Snake verifies movement,
game-over, restart and directional input; MOV verifies four decoded animation
frames, transitions and cleanup; STARSCR verifies four display phases and
task/window cleanup. Representative screenshots have been visually checked.
These traces establish startup execution, not explicit execution of every
registry helper. STARSCR's previously documented post-exit mouse/timer stall
remains, and desktop reuse after it is not established.

Evidence: `build/object_registry_installation_summary.json`,
`build/object_registry_installed_summary.json`,
`build/object_registry_install_build.log` and
`build/object_registry_evidence/results.json` with per-application evidence
subdirectories. Kernel SHA-256:
`36f706a4a4440a8d6d0fd854b2f6c03a7a7c49f056ef4d90264ac775e9ccc116`.
Combined-application disk SHA-256:
`07771e8e045f98372a6980abf4c46c902d37a7c0cbceb74ae9bd9ee7d3cdbe01`.

The full 239-interval regression is recorded separately when it completes.

### Timer object source checkpoint (2026-10-28)

`kernel/timer_objects.c/.h` recovers free-slot lookup, creation, deletion and
rearming at `0x82aa33..0x82ac53`. It preserves signed table/count comparisons,
32-bit wrapped indices and deadlines, repeated live slot-pointer reads and
the original order of flag/allocator calls. Creation returns -1 without
restoring flags when lookup fails or allocation returns zero. This unusual
behavior is preserved, not fixed; it has not been established as the cause
of the separate STARSCR post-exit stall. Deletion is a void API; its incidental
EAX value is outside this fixture's return-value contract.

`python3 tests/kernel_timer_objects.py` passes 178,032 scenarios / 534,096
original-x86, ordinary-C and fixed-address-C executions in 91.105 seconds.
An independent bytewise oracle compares all 96 KiB of fixture memory, exact
flag/allocator callback order and arguments, stack balance/canaries and
callee-saved registers. It covers signed bounds, capacity exhaustion, holes,
allocation failure, tick wrapping, wrapped indices, unaligned records,
records overlapping counters/table entries and callback mutations. Valid
cases include 38,720 creates without restore, 77,328 with restore and 17,808
successful table aliases. The fixture excludes 12,168 unmapped/unbounded alias
inputs before execution and makes no claim about those inputs.

Only flag/allocator CALL operands are redirected in fixture copies; the
internal free-slot call and timer memory operations execute directly. The
C bodies fit 61/122, 160/258, 87/114 and 31/50 bytes with fixed-address and
size assertions. Evidence: `build/timer_objects_preview_summary.json`,
`build/timer_objects_preview_test.log`, `build/timer_objects_calls.h` and
`build/timer_objects_preview.elf`. This checkpoint does not alter production
images while the 239-interval regression is running.

### Packed clock display source checkpoint (2026-10-28)

`kernel/clock_display.c/.h` recovers the time/date wrappers at
`0x82ac53..0x82ad81`. Each captures its packed CMOS global once, forwards the
five drawing arguments and sends six/eight individual nibbles to the original
variadic text renderer at `0x81f88a`. The original format pointers and nibble
order are retained, including values 10..15 rather than normalizing invalid
BCD. The wrappers have void interfaces; renderer EAX is incidental.

`python3 tests/kernel_clock_display.py` passes 139,520 scenarios / 418,560
original-x86, ordinary-C and fixed-address-C executions in 1.977 seconds.
Coverage includes every 16-bit value paired with its complement in the other
half, all 16 values at each of eight nibble positions, 4,096 deterministic
32-bit samples per wrapper, and varied drawing arguments. The oracle checks
all 4 KiB of fixture memory, format pointers, five fixed arguments, every
variadic digit, callback mutations, stack balance/canaries and callee-saved
registers. Only renderer CALL operands are redirected in fixture copies.

C bodies fit 87/138 and 102/164 bytes with original-address and size assertions.
Evidence: `build/clock_display_preview_summary.json`,
`build/clock_display_preview_test.log`, `build/clock_display_calls.h` and
`build/clock_display_preview.elf`. Production images remain at 239 intervals
until their ongoing full regression completes.

### Timer interrupt-flag QEMU checkpoint (2026-10-28)

`python3 tests/timer_flags_qemu.py` passes 48 scenarios / 144 original-x86,
ordinary-C and fixed-address-C executions at CPL0 in 0.086 seconds (QEMU
exit 33). The CLI/PUSHFL/POPFL helpers execute unmodified; only allocator
entrypoints are redirected. Cases cover initial IF=0/1, four delay boundaries,
new slots, hole reuse, full tables, negative high-water counts, allocation
failure and deletion of positive/zero/negative pointers. The fixture checks
that allocator/free callbacks observe IF=0, creation failures leave IF cleared,
and successful creation and every deletion restore the incoming IF.

PIC IRQs are masked: this verifies real interrupt-flag instructions and the
observed IF contract, not IRQ delivery or every arithmetic EFLAGS bit. The
independent memory/callback/ABI fixture remains the broader timer test.
Evidence: `build/timer_flags_preview_summary.json`,
`build/timer_flags_preview_test.log`, `build/timer_flags_preview_qemu.log`
and `build/timer_flags_qemu.elf`. Production images remain unchanged.

### CMOS field reader source checkpoint (2026-10-28)

`kernel/cmos_fields.c/.h` recovers the seven seconds/minutes/hours/weekday/
day/month/year hardware helpers at `0x82adab..0x82ae84`. Named assembly
instructions inside C preserve their hardware ABI, including all general
registers except the EAX result. Six readers occupy 26 bytes each, and the
century/year reader occupies 61 bytes. Every compiled body is byte-for-byte
identical to its original interval; there are no embedded opaque code arrays.
The code preserves the original BCD arithmetic, including invalid nibbles,
century-first sampling, index/NMI writes and absence of UIP/rollover checks.

`python3 tests/kernel_cmos_fields.py` passes 67,072 scenarios / 201,216
original-x86, ordinary-source and fixed-address-source executions in 0.031
seconds. It exhausts all 256 raw bytes for each simple reader and all 65,536
century/year byte pairs, checking full-width EAX, all other general registers,
stack balance and canaries against a decimal arithmetic oracle. Only fixture
copies replace OUT with NOPs and IN with deterministic byte loads; these tests
do not establish physical RTC timing or port behavior. The unmodified helper
instruction sequences are checked separately for exact equality.

Evidence: `build/cmos_fields_preview_test.log`,
`build/cmos_fields_preview_summary.json` and `build/cmos_fields_preview.elf`.
Production remains at 239 intervals pending the ongoing baseline regression.

### Complete 239-interval regression (2026-10-28)

The uninterrupted `make -j4 verify` passes with exit 0 in 1,007.966 seconds.
It checks the ordinary-source and fixed-address kernel fixtures, installed
startup/object-registry routines, hardware QEMU fixtures, all four application
suites, image-tool tests and complete-disk reconstruction. The kernel and all
three production disks retain identical SHA-256 hashes before and after the
run. The timer/clock/CMOS source checkpoints are not installed in this baseline.

Evidence: `build/verify_object_registry.log`,
`build/verify_object_registry.exit` and
`build/verify_object_registry_summary.json`. Kernel SHA-256:
`36f706a4a4440a8d6d0fd854b2f6c03a7a7c49f056ef4d90264ac775e9ccc116`.
Combined-application disk SHA-256:
`07771e8e045f98372a6980abf4c46c902d37a7c0cbceb74ae9bd9ee7d3cdbe01`.

### Timer, clock display and CMOS installation (2026-10-28)

The four timer object functions, two packed clock display wrappers and seven
CMOS field readers are installed at their original entries, increasing the
manifest from 239 to 252 intervals. The 802 changed kernel bytes are confined
to the six new C slots; all seven CMOS instruction bodies match the original
bytes exactly. Every previous interval remains unchanged. Complete-disk
reconstruction passes for all three production images, whose differences from
the 239-interval baseline are restricted to the existing kernel file.

All four installed fixtures pass: 178,032 timer scenarios / 534,096 executions
(90.909 s), 139,520 display scenarios / 418,560 executions (2.106 s), 48 real
CPL0 interrupt-flag scenarios / 144 executions (0.195 s including setup), and
67,072 CMOS scenarios / 201,216 executions (0.139 s including setup).
Production hashes remain unchanged throughout those tests.

Independent QEMU snapshot boots pass Color (29.770 s), Snake (34.536 s), MOV
(32.772 s) and STARSCR (26.399 s), including startup execution, all 252 loaded
intervals, allocator/task state and real PS/2 input. Palette/closure, game
movement/restart, animation frames/cleanup and screensaver phases/cleanup
checks pass, and representative screenshots have been visually checked.
These tests do not establish execution of every replacement function.
STARSCR's previously documented post-exit mouse/timer stall remains; desktop
reuse after its exit is not established.

Evidence: `build/timer_clock_252_installation_summary.json`,
`build/timer_clock_252_focused_summary.json`,
`build/timer_clock_252_build.log`, `build/timer_clock_252_disk_verify.log`,
the four installed fixture summaries and
`build/timer_clock_252_evidence/results.json` with per-application evidence.
Kernel SHA-256:
`1936688b6d6a640f4008068fb5aac01e12ab2fa4ab0185fcb4c7da636922c7ff`.
Combined-application disk SHA-256:
`aea1371afeb6a959fd786d630eb4c316e47e8f0aea373dc9e6fd8fa6e61c7b66`.
This installation has focused and boot coverage; a complete 252-interval
regression has not yet been run.

### ICO directory source checkpoint (2026-10-28)

`kernel/icon_directory.c/.h` recovers the surface-ID wrapper at
`0x82af08..0x82af4c`, centered selection at `0x82af4c..0x82b004` and frame
dispatch at `0x82b004..0x82b0a0`. Selection uses the largest width only,
retains the first tie and immediately selects a zero width as 256. It uses
that width for both unsigned centered coordinates, ignoring entry heights.
Frame validation preserves the original signed comparison, including negative
frames and wrapped 32-bit indexing. Two zero dimension bytes select the PNG
decoder; all other pairs select the DIB decoder. Both decoders remain original.
The four original ICO resources independently confirm these payload formats.

`python3 tests/kernel_icon_directory.py` passes 235,066 scenarios / 705,198
original-x86, ordinary-C and fixed-address-C executions in 11.867 seconds.
The bytewise oracle covers all 65,536 width/height pairs through both direct
and ID APIs, all 65,536 ordered width pairs in four-entry selection sequences,
36,864 centering cases, signed/wrapped frames and table IDs, unaligned records,
zero/high counts with immediate termination, callbacks that mutate shared
metadata, and every frame of all four original resource directories. It checks
decoder choice/arguments, all 16 KiB of fixture memory, callee-saved registers,
stack balance and canaries. The 32 generated unmapped frame entries are excluded
before execution and are not counted as passing. Decoder pixel output is
outside this dispatch fixture's scope; only decoder CALL operands are redirected,
while both internal frame calls execute normally.

Compiled bodies occupy 41/68, 99/184 and 97/156 bytes, with linker assertions
for entry addresses, sizes and unexpected sections. Evidence:
`build/icon_directory_preview_test.log`,
`build/icon_directory_preview_summary.json`, `build/icon_directory_calls.h`,
`build/icon_directory_resources.h` and `build/icon_directory_preview.elf`.
This source checkpoint leaves all four production artifact hashes unchanged
at the 252-interval installation.

### ICO directory installation (2026-10-28)

The three ICO directory routines are installed at their original entries,
raising the manifest from 252 to 255 intervals. All previous interval bytes
remain unchanged; the 391 changed kernel bytes lie entirely within the three
new slots. All three disks pass complete reconstruction and differ from the
252-interval baseline only inside the existing kernel file.

The installed differential fixture passes all 235,066 scenarios / 705,198
executions in 11.940 seconds. Four independent QEMU snapshot checks pass Color
(29.557 s), Snake (33.803 s), MOV (32.440 s) and STARSCR (26.598 s), including
startup execution, all 255 loaded intervals, heap/task state, PS/2 input and
the respective application checks. Representative screenshots have been
visually inspected. Kernel and disk hashes stay unchanged during testing.
Decoder pixel correctness is not established by the dispatch fixture, and
STARSCR's original post-exit mouse/timer stall remains as documented earlier.

Evidence: `build/icon_directory_installed_test.log`,
`build/icon_directory_installed_summary.json`,
`build/icon_directory_255_installation_summary.json`,
`build/icon_directory_255_build.log`,
`build/icon_directory_255_disk_verify.log`, and
`build/icon_directory_255_evidence/results.json` with per-application logs,
memory dumps and screenshots. Kernel SHA-256:
`698adeff575da09968e9d72d9fb7a4c023ab5ff1692f1bfbe1947b829f388b2c`.
Combined-application disk SHA-256:
`66c369d8cb656f8f8c7df8ba41672858993f9598f37cff690a5154458c3ebd90`.
This is focused and boot coverage; a complete 255-interval regression has not
been run. The latest complete regression remains the 239-interval checkpoint.

### File icon source checkpoint (2026-10-28)

`kernel/x_icon.c/.h` recovers the unchecked X-header icon pointer accessor
at `0x812c0e..0x812c3e`. A zero DWORD at header +40 returns zero; a nonzero
offset is added with 32-bit wrapping and without magic, bounds or null checks.
`kernel/icon_files.c/.h` recovers the file/frame wrapper at
`0x82b0a0..0x82b112` and suffix-selecting wrapper at `0x82b112..0x82b20c`.
Only the second and third path bytes select absolute versus relative loading.
The automatic API tests uppercase `.ICO` only at lengths greater than four,
and uppercase `.X` only at lengths greater than two, after loading the file.
Both wrappers draw even when their selected icon pointer is null, then free a
nonzero loaded file. The automatic API returns the selected pointer after
freeing its backing allocation. These original behaviors are retained.

`python3 tests/kernel_icon_files.py` passes 336,248 scenarios / 1,008,744
original-x86, ordinary-C and fixed-address-C executions in 10.414 seconds.
Coverage includes all low/high WORD icon offsets, unaligned headers, wrapped
pointer sums, all 65,536 path lookahead pairs through both wrappers, every
first path byte, every final two-byte pair and each possible ICO suffix byte,
short/case-sensitive names, callback mutations and forged wrapped lengths.
Eight original X headers include four real embedded ICO directories (CMD,
TASKMGR, PICVIEW and DESKTOP). Return values, ordered boundary calls, all 8 KiB
of fixture memory, callee-saved registers, stack balance and canaries agree.

The wrapper fixture substitutes filesystem, string-length, X-header, drawing
and free callbacks; the real X-header routine is tested separately without
substitution. It does not execute the null-buffer X-header access, filesystem
I/O or image decoding. Bodies fit their original slots at 20/48, 77/114 and
165/250 bytes. Evidence: `build/icon_files_preview_test.log`,
`build/icon_files_preview_summary.json`, `build/icon_files_calls.h`,
`build/icon_files_resources.h` and `build/icon_files_preview.elf`.
All four production hashes remain unchanged at the 255-interval installation.

### File icon installation (2026-10-28)

The X-header accessor and two file icon wrappers are installed at their original
addresses, raising the manifest from 255 to 258 intervals. All previous interval
bytes remain unchanged. The 392 changed kernel bytes are confined to the three
new slots, and complete reconstruction passes for all three production disks.
Their only differences from the preceding images lie in the existing kernel
file; no files, FAT chains or directory entries are added or resized.

The installed file-icon fixture passes 336,248 scenarios / 1,008,744 executions
in 11.094 seconds, including equality of the installed slot bodies and NOP
padding with separately linked source. The installed ICO-directory regression
also passes 235,066 scenarios / 705,198 executions in 12.316 seconds.

Four independent QEMU snapshot checks pass Color (29.598 s), Snake (33.745 s),
MOV (32.452 s) and STARSCR (26.514 s), with startup execution, all 258 loaded
intervals, heap/task state, PS/2 input and the respective application checks.
Representative screenshots have been visually inspected. The four production
hashes remain unchanged during validation. These boot checks do not establish
natural execution of every new function; the entry trace only covers startup.
The file fixture substitutes filesystem and decoder boundaries. STARSCR's
documented original post-exit mouse/timer stall remains.

Evidence: `build/icon_files_installed_test.log`,
`build/icon_files_installed_summary.json`,
`build/icon_directory_258_installed_test.log`,
`build/icon_files_258_installation_summary.json`,
`build/icon_files_258_build.log`, `build/icon_files_258_disk_verify.log`,
`build/icon_files_258_validation.log` and
`build/icon_files_258_evidence/results.json` with per-application artifacts.
Kernel SHA-256:
`73ecc65b14b9b36222493ef37a5a5d4140295ed9f4cfb9b8ba877dfac0751b45`.
Combined-application disk SHA-256:
`3cad3adf5f9a9dae1cdafdb767e3f3510b0171ebc21e3cc46e749c03727bb0bf`.
This is focused and boot coverage. The latest complete regression remains the
239-interval checkpoint; a complete 258-interval regression has not been run.

### DIB icon decoder source checkpoint (2026-10-28)

`kernel/icon_dib.c/.h` recovers the renderer at `0x82b20c..0x82b678`.
It retains packed bottom-up pixel rows, a mask stride selected by halved image
height rather than width, and the original extra per-pixel mask-byte advance
in the 32-to-8-bit path. Only 8- and 32-bit source and destination depths draw.
The destination depth is a full DWORD; 32-to-32-bit output clears alpha, while
8-to-32-bit palette output preserves it. Quantizer coordinates are local and
drawing coordinates include the caller's offset with 32-bit wrapping. Callback
changes to pixel, palette and mask memory remain visible; captured dimensions
and depth branches remain fixed. No general BMP layout correction, validation
or clipping is added.

`python3 tests/kernel_icon_dib.py` passes 79,233 scenarios / 237,699 original-x86,
ordinary-C and fixed-address-C executions in 13.155 seconds. Coverage includes
all source WORD depths, high destination-depth bits, four conversion paths,
every mask byte and individual mask positions, unaligned and rectangular
images, height 47/48, odd doubled heights, palette indices and alpha bytes,
coordinate wrapping, and callback mutations. Seventy-one real non-PNG frames
cover four standalone ICO files and four X-file embedded directories, including
the unsupported 4-bit frame. Ordered helper events, all 32 KiB of fixture
memory, callee-saved registers, stack balance and canaries agree.

The compiled body occupies 468 of its 1,132-byte original slot. Only the put8,
put32 and quantizer CALL boundaries are substituted by this fixture; their
actual implementations are not recovered or verified here. Patterned mapped
memory beyond resource payloads models original out-of-payload reads and does
not establish safety for arbitrary invalid pointers. Evidence:
`build/icon_dib_preview_test.log`, `build/icon_dib_preview_summary.json`,
`build/icon_dib_preview.elf`, `build/icon_dib_calls.h` and
`build/icon_dib_resources.h`. This source checkpoint leaves production at
258 installed intervals with unchanged kernel and disk images.

### DIB icon decoder installation (2026-10-28)

The DIB renderer is installed at its original `0x82b20c` entry, increasing the
manifest from 258 to 259 intervals. All preceding intervals retain identical
bytes. The 1,117 changed kernel bytes are confined to the new 1,132-byte slot;
its 468-byte compiled body is followed by NOP padding. Complete reconstruction
passes for all three production disks, whose only changes from the preceding
images lie in the existing kernel file. Installed differential checks pass
79,233 DIB scenarios / 237,699 executions (13.088 s), 235,066 ICO-directory
scenarios / 705,198 executions (11.902 s) and 336,248 file-icon scenarios /
1,008,744 executions (10.478 s).

Four independent QEMU snapshot checks pass Color (29.581 s), Snake (33.798 s),
MOV (32.512 s) and STARSCR (26.467 s), including startup execution, all 259
loaded intervals, heap/task state, PS/2 input and each application check.
Representative application screenshots have been visually inspected.

`make smoke-icon-dib` additionally traces normal production startup and input
without a diagnostic guest or decoder substitution. Both the new decoder
entry and compiled body execute; the trace contains two entry records and
2,794,600 body records. TB chaining means these are trace records rather than
exact invocation counts, and this boot does not cover every conversion path.
The image's complete kernel and the installed body/padding are checked before
recording the execution evidence. The desktop screenshot was visually checked.
The four production hashes remain unchanged across validation.

Evidence: `build/icon_dib_installed_test.log`,
`build/icon_dib_installed_summary.json`,
`build/icon_directory_259_installed_test.log`,
`build/icon_files_259_installed_test.log`,
`build/icon_dib_259_installation_summary.json`,
`build/icon_dib_259_build.log`, `build/icon_dib_259_disk_verify.log`,
`build/icon_dib_259_validation.log` and `build/icon_dib_259_evidence/` with
per-application results and the separate `natural/` trace archive.
Kernel SHA-256:
`a445b710ee5fdf289a7531256f885229583c3c529f8177d309d718613d9856c6`.
Combined-application disk SHA-256:
`73a2aae6e2b8c7d2f741e79bfc9391ceb24df52c2020b6b1a31fd3018fbc4e33`.
The pixel helpers and quantizer remain original code at this checkpoint, and
STARSCR's documented original post-exit mouse/timer stall remains. This is
focused and boot coverage; the latest complete regression is still the
239-interval checkpoint.

### Icon pixel helpers source checkpoint (2026-10-28)

`kernel/icon_pixels.c/.h` recovers direct 8-bit and 32-bit surface writes at
`0x81b37c..0x81b3c0` and `0x81b3c0..0x81b406`, plus the palette quantizer at
`0x81fd90..0x81fec2`. Pixel writes retain signed bounds, short-circuit field
access, unaligned data, 32-bit address wrapping, low-byte truncation for 8-bit
output and full DWORD stores for 32-bit output. The quantizer retains the
`{{3,1},{0,2}}` threshold matrix, full-width RGB inputs, wrapped multiplication
by 21, signed division toward zero by 256 and then 4, and low-byte palette
output. It does not clamp or normalize out-of-range inputs.

`python3 tests/kernel_icon_pixels.py` passes 68,682,058 scenarios / 206,046,174
original-x86, ordinary-C and fixed-address-C executions in 7.879 seconds.
All 16,777,216 RGB values are tested at all four dither positions; additional
DWORD, signed-boundary and pseudorandom cases bring quantizer coverage to
68,419,584 scenarios. Each writer passes 131,237 scenarios covering signed
dimensions/coordinates, wrapped effective addresses, four alignments, all
65,536 complementary color pairs, writes aliasing surface metadata, guarded
short-circuit access and the entire 4 KiB memory oracle. ABI checks include
callee-saved registers, stack balance and canaries. No CALL boundaries are
substituted. The standalone fixture does not yet test the complete decoder
and helper chain together.

Bodies fit their original slots at 55/68, 56/70 and 98/306 bytes. Evidence:
`build/icon_pixels_preview_test.log`, `build/icon_pixels_preview_summary.json`,
`build/icon_pixels_preview.elf` and `build/icon_pixels_preview.x`. This source
checkpoint leaves production at 259 installed intervals with unchanged images.

### Complete DIB rendering chain checkpoint (2026-10-28)

`python3 tests/kernel_icon_render.py` passes 9,288 scenarios / 27,864
original-x86, ordinary-C and fixed-address-C executions in 2.779 seconds.
The decoder calls the real pixel writers and quantizer with no CALL
substitutions. All four conversion paths, mask bytes, alignments, rectangular
dimensions, height 47/48, coordinate wrapping, clipping, signed surface bounds
and seventy-one real non-PNG resource frames agree with a bytewise oracle.
Output aliases into headers, palettes, pixels and masks retain live-read
behavior. The entire 64 KiB fixture memory, callee-saved registers, stack
balance and canaries agree across implementations. The oracle predicts
2,985,649 pixel writes; unsupported source/destination depths are rejected in
42 scenarios. These controlled mapped fixtures do not establish arbitrary
invalid-pointer safety or full operating-system equivalence.

Evidence: `build/icon_render_preview_test.log`,
`build/icon_render_preview_summary.json`, `build/icon_render_preview.elf`
and `build/icon_render_preview.x`. This source-test checkpoint leaves
production at 259 intervals; the three pixel helper slots are not yet installed.

### Icon pixel helper installation (2026-10-28)

The two pixel writers and ordered-dither quantizer are installed at their
original entries, increasing production from 259 to 262 declared intervals.
All previous intervals, including the DIB decoder, retain identical bytes.
The 432 changed kernel bytes are confined to the three new slots; compiled
bodies occupy 55/68, 56/70 and 98/306 bytes, with NOP padding. Complete disk
reconstruction passes for the INIT, Snake and combined-application images,
whose only changes from the 259-interval images lie in the existing kernel file.

Installed differential checks pass the 68,682,058 pixel/quantizer scenarios
(7.913 s), 9,288 complete-chain scenarios (2.789 s), 79,233 DIB scenarios
(13.229 s), 235,066 directory scenarios (11.847 s) and 336,248 file-icon
scenarios (10.370 s). The complete-chain fixture executes the real decoder,
pixel writers and quantizer without CALL substitution.

Independent QEMU snapshot runs pass Color (29.680 s), Snake (34.306 s),
MOV (32.502 s) and STARSCR (26.546 s), including startup execution, all
262 loaded intervals, heap/task state, PS/2 input and application checks.
Representative application screenshots were visually inspected.
STARSCR's documented original post-exit mouse/timer stall remains.

`make smoke-icon-render` additionally traces all four recovered rendering
slots during normal production startup/input, without a diagnostic guest or
CALL substitution. The DIB decoder has two entry and 1,506,073 body trace
records; the 32-bit writer has 1,506,017 entry and twelve body records.
The 8-bit writer and quantizer are unobserved in this 32-bit-display boot.
TB chaining makes these trace records rather than exact invocation counts.
The complete disk kernel and all four installed bodies/padding are checked
against the current production build. This boot does not cover every pixel
conversion path; differential fixtures provide separate coverage.

Evidence: `build/icon_pixels_262_build.log`,
`build/icon_pixels_262_disk_verify.log`,
`build/icon_pixels_262_installation_summary.json`,
`build/icon_pixels_262_validation.log` and `build/icon_pixels_262_evidence/`,
including per-application captures, `differential/` summaries and `natural/`
trace/check results. All four production hashes remain unchanged throughout
validation. Kernel SHA-256:
`7dc0b67fe0fc3b200e38af427615d1e8e0147dd6d5f11137a9dc3aa3615ae6ed`.
Combined-application disk SHA-256:
`05f488636184854f94714965059e8e9e9abcb92c171000b78a835f44133d2912`.
This checkpoint has focused and boot coverage; the latest complete regression
remains the 239-interval checkpoint.

### Surface-ID pixel API source checkpoint (2026-10-28)

`kernel/pixel_access.c/.h` recovers the 8/32-bit setters at `0x81b26e` and
`0x81b2f4`, and getters at `0x81b406` and `0x81b486`. They use the original
table at `0x500000`, wrapped DWORD ID indexing, signed width/height comparisons,
repeated volatile table/field reads and wrapped effective pixel addresses.
Setters reject negative coordinates. Getters retain the original absence of
negative-coordinate checks and return `0xffffffff` when bounds reject; valid
32-bit pixels may also contain that value. No ID validation or surface-depth
check is added.

`python3 tests/kernel_pixel_access.py` passes 557,312 scenarios / 1,671,936
original-x86, ordinary-C and fixed-address-C executions in 42.789 seconds.
Each setter passes 139,344 scenarios and each getter 139,312. Coverage includes
full DWORD dimensions and coordinates, high/wrapped IDs including table slots
outside the ordinary table page, four alignments, every complementary WORD
color pair, pseudorandom inputs and writes aliasing surface metadata or the
table. Guarded width rejection skips an unmapped height field; negative setter
coordinates skip later unmapped accesses. There are 11,957/11,929 accepted
negative-coordinate 8/32-bit reads. The complete 12 KiB mapped table/data
oracle, getter results, callee-saved registers, stack balance and canaries agree.
No CALL boundaries are substituted. Patterned mapped memory does not establish
arbitrary invalid-pointer safety.

Bodies fit their original slots at 89/134, 93/136, 85/128 and 87/142 bytes.
Evidence: `build/pixel_access_preview_test.log`,
`build/pixel_access_preview_summary.json`, `build/pixel_access_preview.elf`
and `build/pixel_access_preview.x`. This source checkpoint leaves production
at 262 installed intervals; these four additional slots are not yet installed.

### Pixel service routing checkpoint (2026-10-28)

`tests/kernel_pixel_access.py` now also runs a complete service-chain fixture.
The original image and a preview based on the current 262-interval production
kernel execute 4,608 scenarios / 9,216 chains in 0.589 seconds. Each chain runs
the real service initializer at `0x8005aa`, selects 8/32-bit setter/getter
targets, then calls the real point-write and point-read forwarders at
`0x8374cf` and `0x837939`. The preview overlays only the four newly compiled
pixel API slots; no CALL is redirected. Cases cover signed dimensions,
coordinates, wrapped IDs and alignments. All 12 KiB of table/data memory,
getter results, ABI state and the separate 4 KiB dispatch table agree with
the oracle. Both modes execute writes, accepted negative-coordinate reads
and rejected reads. The standalone 557,312-case fixture also passes again
(42.547 s).

Evidence: `build/pixel_access_chain_preview_summary.json`,
`build/pixel_access_chain_candidate.x`,
`build/kernel_pixel_access_chain_preview_test` and the combined
`build/pixel_access_preview_test.log`. Production remains at 262 intervals.

### Complete 262-interval regression (2026-10-28)

`make -j4 verify` passes in 1,147.859 seconds (exit 0) against production
commit `15f576e43bb6aec20815e1d323a5c3b664c81ae1`. This runs the complete current
boot, INIT, kernel, device-state, graphics, startup/resource and application
verification recipe, including the installed pixel helpers and full DIB chain.
The kernel, three production disks and all snapshotted kernel/build inputs
retain their hashes throughout the run. The four additional surface-ID pixel
APIs are still a separate source/preview checkpoint and are not part of this
262-interval production regression.

Evidence: `build/full_regression_262.log` and
`build/full_regression_262_summary.json`, which records the source hashes,
production hashes, command, revision, elapsed time and exit status. This
supersedes the 239-interval checkpoint as the latest complete regression;
the separate application/natural-boot captures above provide the corresponding
262-interval runtime evidence.

### Surface-ID pixel API installation (2026-10-28)

The four surface-ID pixel APIs are installed at their original addresses,
increasing production from 262 to 266 intervals. All 262 preceding intervals
retain identical bytes. The 500 changed kernel bytes lie only in the four
new slots; bodies occupy 89/134, 93/136, 85/128 and 87/142 bytes, followed by
NOP padding. Complete reconstruction passes for all three production disks,
whose changes from the 262-interval images are confined to the existing kernel
file. FATs, directories, file slack, boot stages and applications retain their
previous bytes.

Installed checks pass 557,312 pixel API scenarios / 1,671,936 executions
(42.850 s), 4,608 complete service scenarios / 9,216 chains (0.567 s),
68,682,058 pixel/quantizer scenarios (7.987 s) and 9,288 complete DIB scenarios
(2.789 s). The installed startup table/state and service-forwarder regressions
also pass. No CALL is substituted in the complete pixel service or DIB chains.

Independent QEMU snapshot checks pass Color (29.652 s), Snake (34.362 s),
MOV (32.474 s) and STARSCR (26.631 s), including startup execution, all
266 loaded intervals, heap/task state, PS/2 input and the respective application
checks. Representative application screenshots were visually inspected.
The original STARSCR post-exit mouse/timer stall remains documented.

`make smoke-icon-render` now also verifies/traces the four surface-ID slots.
A separate normal startup/input run observes the new 32-bit setter (32,174
entry / eight body trace records) and getter (5,205 / four), plus the DIB
decoder (two / 1,474,138) and its direct 32-bit writer (1,474,072 / ten).
The desktop screenshot was visually checked. The 8-bit APIs and quantizer are
unobserved in this display mode; TB chaining prevents interpreting the records
as exact call counts. No diagnostic guest or CALL substitution is used.
The complete image kernel and eight compiled bodies/padding are checked, and
all four production hashes remain unchanged across validation.

Evidence: `build/pixel_access_266_build.log`,
`build/pixel_access_266_disk_verify.log`,
`build/pixel_access_266_installation_summary.json`,
`build/pixel_access_266_service_regression.log`,
`build/pixel_access_266_validation.log` and `build/pixel_access_266_evidence/`,
including independent application captures, `differential/` summaries and
`natural/` execution traces. Kernel SHA-256:
`800aef5719794aac703bd406e13335bf78b671e5902c5c7534cff77a674eff84`.
Combined-application disk SHA-256:
`0354da490ce7165ff81b0890cb25fc34f1a895504ad176ec440e2e2d4a9a54c6`.
The latest complete regression remains the successful 262-interval run;
266 has the focused and runtime coverage described here. The overall OS
recovery remains incomplete and continues to require original components.

### Layer sampling and DDA line source checkpoint (2026-10-28)

`kernel/pixel_sample.c/.h` recovers `0x81b514..0x81b714`: capture the subject's
order position minus one, traverse positive signed positions in descending
order, test open rectangle edges, read a DWORD pixel and skip an exact DWORD
color key. The sample ignores each layer's declared depth and the order-list
count, retains wrapped coordinates/table indices and repeated volatile reads,
and returns `0xffffffff` when no opaque candidate is found.

`kernel/raster_line.c/.h` recovers the 8/32-bit lines at
`0x81b734..0x81b8dc` and `0x81b8dc..0x81ba8a`. They retain the inclusive
major-axis count, ten-bit fixed-point coordinates, wrapped absolute differences
and shifts, signed minor-step division toward zero and arithmetic position
shifts. Bounds and surface depth are not checked. Table, width, pixel-base and
incoming ID/color words stay live across pixel stores; the remaining geometry
is captured before drawing. Negative signed counts draw nothing. The raw EAX
return is the completed count, or zero when no iteration runs.

`python3 tests/kernel_raster.py` passes 90,976 sampler scenarios / 272,928
original-x86, ordinary-C and fixed-address-C executions (16.195 s), plus
61,700 line scenarios / 185,100 executions (10.135 s). Sampler coverage
includes every rejection branch, full DWORD geometry, color-key/alpha values,
ignored depths, four alignments, wrapped IDs/order positions and guarded
short-circuit reads. No sampler scenarios are excluded. Line coverage includes
all octants, zero-length lines, fixed-point truncation/wrap, full DWORD pitch,
unclipped signed coordinates, 126 no-draw counts, 296 metadata/table writes
and 304 incoming-argument writes. Before execution, 2,114 generated line
fixtures are excluded for large loops or accesses outside controlled mappings.
This exclusion is a fixture limit, not clipping added to the recovered code.

The entire 32 KiB mapped memory, raw returns, six live caller words,
callee-saved registers, stack balance and canaries agree. A fixed mapped call
frame permits real stores into incoming arguments. The line oracle uses
widened arithmetic and shift/subtract division; no CALL is substituted.
These fixtures do not establish safety for arbitrary pointers or large loops.
Bodies occupy 358/512, 304/424 and 304/430 bytes. Evidence:
`build/raster_preview_test.log`, `build/raster_sample_preview_summary.json`,
`build/raster_line_preview_summary.json`, `build/raster_preview.elf` and
`build/raster_preview.x`. Production remains at 266 intervals at this source
checkpoint; none of the three new slots is installed yet.

### DDA service-chain checkpoint (2026-10-28)

`tests/kernel_raster_chain.c` executes the real service initializer at
`0x8005aa`, checks its 8/32-bit line target in slot one, and invokes the real
six-argument forwarder at `0x837501`. The untouched original kernel and a
266-interval production preview with the three raster slots overlaid pass
3,584 scenarios / 7,168 chains in 0.659 seconds. There are 608 excluded
large/unmapped executions across the two implementations. The complete
32 KiB fixture memory, raw return, caller words, ABI and separate 4 KiB
dispatch table agree; no CALL is substituted. The sampler and standalone
line fixtures also pass again (16.217 s and 10.119 s).

Evidence: `build/raster_preview_test.log`,
`build/raster_chain_preview_summary.json`, `build/raster_chain_candidate.x`
and `build/kernel_raster_chain_preview_test`. The production build still has
266 installed intervals at this checkpoint.

### Layer sampling and DDA installation (2026-10-28)

The sampler and both DDA line routines are installed at their original
addresses, increasing production from 266 to 269 intervals. All previous
266 intervals retain identical bytes. The 1,303 changed kernel bytes are
confined to the three new slots; all three production disks pass complete
reconstruction and differ from their predecessors only in the existing kernel
file. Bodies occupy 358/512, 304/424 and 304/430 bytes with NOP padding.

Installed differential checks pass 90,976 sampler scenarios (16.342 s),
61,700 line scenarios (10.158 s) and 3,584 complete line-service scenarios
(0.684 s). The surface-ID pixel regression passes 557,312 scenarios
(42.855 s) and 4,608 complete service scenarios (0.550 s); the complete DIB
chain passes 9,288 scenarios (2.821 s). Startup state/table and service
forwarder regressions also pass. The earlier source-checkpoint fixture
exclusions still apply; complete chains use actual calls with no substitutions.
The independent 68-million-case pixel-helper fixture was not rerun here.

Independent QEMU snapshot checks pass Color (29.596 s), Snake (33.975 s),
MOV (32.489 s) and STARSCR (26.489 s), including startup execution, all
269 loaded intervals, heap/tasks, PS/2 input and the respective application
checks. Representative application screenshots were visually inspected.
The original STARSCR post-exit mouse/timer stall remains documented.

A separate normal boot/input trace observes the new 32-bit line routine
(10 entry / 11 body records), alongside the DIB decoder (two / 1,496,477),
direct 32-bit writer (1,496,442 / ten), surface-ID setter (31,981 / nine)
and getter (5,202 / four). The eleven monitored compiled bodies and padding
match the disk kernel, and all installed intervals match guest memory.
The desktop and returned CPUID prompt were visually inspected. The sampler
and 8-bit line are unobserved in this boot; their differential evidence is
separate. TB chaining means trace records are not exact call counts.
No diagnostic guest or CALL substitution is used for this boot.

Evidence: `build/raster_269_installation_summary.json`,
`build/raster_269_build.log`, `build/raster_269_disk_verify.log`,
`build/raster_269_validation.log` and `build/raster_269_evidence/`, including
application captures and archived `natural/` and `differential/` artifacts
with SHA-256 manifests. The kernel and three disk hashes remain unchanged
through validation. Kernel SHA-256:
`874f262f8a3465366473e2c8fd248188d1c9cccae78eb8f8a633f0c77ee87d13`.
Combined-application disk SHA-256:
`d348e4d1b12174f6377db9c7c54d8887fcef66fef65485f069e98223276bbfb3`.
The latest complete `make verify` remains the successful 262-interval run;
269 has the focused and runtime coverage described above. Recovery of the
entire OS is still incomplete.

### Rectangle fill source and service-chain checkpoint (2026-10-28)

`kernel/raster_fill.c/.h` recovers the inclusive 8/32-bit fills at
`0x81ba8a..0x81bd26` and `0x81bd26..0x81bf50`. Each endpoint is independently
clipped to zero and then signed `(dimension - 1)`, retaining wrapped negative
coordinates for zero/negative dimensions. The 8-bit routine captures the
full-DWORD shifted/ORed color for its DWORD blocks, keeps the low-byte tail
color live, and reloads ID/table/width/pixels for each store. The 32-bit
routine captures pitch and the first-row pointer, writes that row with a live
color, then copies it through the actual dynamic service wrapper at `0x8378dd`.
It retains unsigned decrement/sentinel loops. The 8-bit raw return is the
final row; the 32-bit original returns EBP minus 40 (an expired temporary's
address), reproduced relative to the incoming frame using GCC's frame builtin.

`python3 tests/kernel_raster_fill.py` passes 44,446 direct scenarios /
133,338 original-x86, ordinary-C and fixed-address-C executions (11.282 s).
The independent bytewise oracle checks all 32 KiB of table/order/data memory,
six incoming words, raw EAX, callee-saved registers, stack balance/canaries
and the separate 4 KiB service table. Cases cover reversed endpoints, signed
clipping, wrapped IDs/dimensions/pitch, four alignments, high color bytes,
DWORD/tail lengths, the 300-DWORD copy boundary, metadata/table aliases,
incoming-word aliases and overlapping copy rows. Accepted cases include
9,256 negative clipped coordinates, 1,704 metadata/table stores and 1,892
incoming-word stores. Before execution, 4,304 generated scenarios are excluded
for large loops or accesses outside the controlled mappings; this is a fixture
limit, not additional clipping in the recovered functions.

The complete service fixture passes 13,760 scenarios / 27,520 original and
candidate chains (4.689 s), with 5,248 large/unmapped executions excluded.
It executes the real `0x8005aa` initializer, verifies display-selected fill
slot two and CPU-feature-selected scalar/SSE copy slot 17, then invokes the
real six-argument forwarder at `0x837545`, fill, copy wrapper and copy routine.
The ordinary C and fixed-address candidates also use the real copy services;
no CALL is substituted in either fixture. Both original and recovered
forwarders preserve the argument-frame offset checked by the raw EAX oracle.

Bodies fit at 489/668 and 387/554 bytes. Evidence:
`build/raster_fill_preview_test.log`,
`build/raster_fill_direct_preview_summary.json`,
`build/raster_fill_chain_preview_summary.json`,
`build/raster_fill_preview.elf` and `build/raster_fill_candidate.x`.
Production remains at 269 intervals at this source checkpoint; the preview
overlays only the two new fill slots on that production kernel.

### Rectangle fill installation (2026-10-28)

Both fill routines are installed at their original addresses, increasing
production from 269 to 271 intervals. All previous 269 intervals retain
identical bytes. The 1,167 changed kernel bytes are confined to the two new
slots; the three production disks pass complete reconstruction and differ
from their predecessors only inside the existing kernel file. The complete
kernel equals the independently linked source preview.

Installed checks pass all 44,446 direct fill scenarios / 133,338 executions
(11.329 s) and 13,760 complete service scenarios / 27,520 chains (4.788 s).
The preceding fixture limits still apply. Sampler (16.308 s), line (10.283 s)
and complete line-service (0.680 s) regressions pass against this kernel.
Fixed-address startup/table, service-forwarder, scalar-memory and accelerated
memory regressions also pass. Complete chains retain real CALL boundaries.

Independent QEMU snapshot checks pass Color (29.747 s), Snake (33.716 s),
MOV (32.511 s) and STARSCR (26.704 s), including startup execution, all
271 loaded intervals, heap/tasks, PS/2 input and the respective application
checks. Representative screenshots were visually inspected. STARSCR's
documented original post-exit mouse/timer stall is still present.

A separate normal boot/input trace observes the new 32-bit fill (7,414 entry /
130,773 body records), the 32-bit line (ten / 20), DIB decoder (two /
1,396,945), direct writer (1,396,882 / 13), surface-ID setter (31,822 / eight)
and getter (5,203 / four). All thirteen monitored compiled bodies/padding
match the disk kernel, and all 271 intervals match guest memory. The desktop
and returned CPUID prompt were visually inspected. The 8-bit fill, sampler
and 8-bit line remain unobserved in this boot. TB chaining prevents treating
trace records as exact call counts; no diagnostic guest or CALL substitution
is used.

Evidence: `build/raster_fill_271_installation_summary.json`,
`build/raster_fill_271_build.log`, `build/raster_fill_271_disk_verify.log`,
`build/raster_fill_271_validation.log`, `build/raster_fill_271_evidence/`
and the preserved `build/raster_fill_271_baseline/` and
`build/raster_fill_271_source_evidence/`. Natural and differential artifacts
include SHA-256 manifests. All four production hashes remain unchanged
through validation. Kernel SHA-256:
`3b2ea0c312ec23bc7204fdf05fc107d87d229181bec32e32e4ebaaefa66d1e02`.
Combined-application disk SHA-256:
`fc84f908009d471348d5477f80cddb9994eaac0575f68e178a94e1f00fa1e726`.
The latest complete `make verify` remains the successful 262-interval run;
271 has the focused/runtime coverage described here. The complete OS still
requires unrecovered original components.

### Rectangle outline source checkpoint (2026-10-28)

`kernel/raster_outline.c/.h` recovers the unclipped 8/32-bit borders at
`0x81c058..0x81c1e4` and `0x81c1e4..0x81c396`. Signed endpoint ordering,
inclusive edges, repeated corner/degenerate-edge stores, wrapped indexing and
live ID/color/table/width/pixel reads are retained. Horizontal edges execute
first; xmin is then recaptured from live incoming X arguments before vertical
edges, while xmax and Y bounds remain captured. Neither routine checks surface
depth or height. The raw EAX return is the completed vertical row value.

`python3 tests/kernel_raster_outline.py` passes 42,338 direct scenarios /
127,014 original-x86, ordinary-C and fixed-address-C executions (7.921 s).
The independent widened/bytewise oracle checks all 32 KiB of mapped memory,
incoming words, raw return, callee-saved state and stack/canaries. Coverage
includes four alignments, wrapped IDs/full-DWORD pitch, signed coordinates,
colors with high bytes, 623 metadata/table stores, 2,176 caller-word stores,
16 changed/recaptured left edges and 35,050 degenerate-edge scenarios.
There are 6,408 generated scenarios excluded before execution for large loops
or accesses outside controlled mappings; no clipping is added to the source.

The real initializer/mode-selection/slot-three forwarder fixture passes 7,764
scenarios / 15,528 original and candidate chains (1.861 s), with 3,672
large/unmapped executions excluded. It calls `0x8005aa` and `0x837589`, checks
the mode-selected target and complete dispatch table, and substitutes no CALL.
Both recovered bodies have no CALL and fit at 316/396 and 300/434 bytes.
Evidence: `build/raster_outline_preview_test.log`,
`build/raster_outline_direct_preview_summary.json`,
`build/raster_outline_chain_preview_summary.json`,
`build/raster_outline_preview.elf` and `build/raster_outline_candidate.x`.
This preview overlays only the two borders on the 271-interval production
kernel; the production images still contain 271 intervals at this checkpoint.

### Rectangle outline installation (2026-10-28)

The two border routines are installed at their original addresses, increasing
production from 271 to 273 intervals. All previous 271 intervals retain
identical bytes. The 791 changed kernel bytes lie only in the two new slots;
all three production disks pass complete reconstruction and differ from their
predecessors only in the existing kernel file. The complete installed kernel
equals the independently linked source preview.

Installed checks pass 42,338 direct border scenarios / 127,014 executions
(7.926 s) and 7,764 complete service scenarios / 15,528 chains (1.886 s),
with the documented large/unmapped fixture exclusions. Fill regressions pass
44,446 direct scenarios (11.297 s) and 27,520 complete chains (4.689 s).
Fixed-address startup/table and service-forwarder regressions also pass.
No CALL is substituted in the complete chains.

Independent QEMU snapshot checks pass Color (29.634 s), Snake (34.401 s),
MOV (32.434 s) and STARSCR (26.513 s), including startup execution, all
273 loaded intervals, heap/tasks, PS/2 input and the respective application
checks. Representative screenshots were visually inspected. STARSCR's
documented original post-exit mouse/timer stall remains present.

A separate normal boot/input trace observes the new 32-bit border (11 entry /
13 body records), plus 32-bit fill (7,394 / 130,505), line (ten / 20), DIB
decoder (two / 1,394,625), direct writer (1,394,597 / ten), surface-ID setter
(31,299 / eight) and getter (5,205 / four). All fifteen monitored compiled
bodies/padding match the disk kernel, and all 273 intervals match guest memory.
The desktop and returned CPUID prompt were visually inspected. The sampler
and 8-bit raster paths remain unobserved in this boot. Trace records are not
exact call counts because of TB chaining. No diagnostic guest is used.

Evidence: `build/raster_outline_273_installation_summary.json`,
`build/raster_outline_273_build.log`, `build/raster_outline_273_disk_verify.log`,
`build/raster_outline_273_validation.log`, `build/raster_outline_273_evidence/`,
`build/raster_outline_273_baseline/` and
`build/raster_outline_273_source_evidence/`. Natural and differential artifacts
have SHA-256 manifests. All four production hashes remain unchanged through
validation. Kernel SHA-256:
`85e8b7ea7d960a22922094befd3c6835022c1fb4c80ef0c8a989fbcc621526f2`.
Combined-application disk SHA-256:
`983a2662d8396d160a42ef8d827d641491a6af69f863e043a5fd12765dea6558`.
The latest complete `make verify` remains the successful 262-interval run;
273 has the focused/runtime coverage described above. The overall OS recovery
remains incomplete.

### Original binary inventory checkpoint (2026-10-28)

`tools/binary_inventory.py` traverses the original FAT16 filesystem and records
all 39 `.X`/`.DX` paths, original SHA-256 values, file sizes, raw header fields,
roles, source/output references and recovery status in `recovery/inventory.json`
and `recovery/INVENTORY.md`. There are 37 distinct binaries: CMD and TASKMGR each
appear in two paths. Four application paths are rebuilt, INIT and the kernel
are partially recovered, and 33 paths still use original programs. The four
drivers remain unrecovered. The third boot stage retains original bytes
through its hybrid assembly's `incbin` and is explicitly listed as partial.

Every inventory path/hash matches the independent extracted filesystem.
Generation is deterministic and `make inventory-check` passes. This check is
also included in `make verify` to detect stale status after future manifest
changes. Kernel slot bytes include padding/exact hardware boundaries and are
reported as physical coverage, not a semantic completion percentage. This
inventory checkpoint changes no production image.

### CLOCK source checkpoint (2026-10-28)

`apps/clock.c/.h/.ld` recovers CLOCK's entry, close, direct palette lookup,
face, individual hand and packed-BCD hands at original offsets `0x40`,
`0x18a`, `0x1a2`, `0x1c2`, `0x340` and `0x3fc`. The original integer hour
and minute divisions, x87 rounding/precision, double stores, palette reads,
callback registration and even the failed-create record dereference remain.
The complete 64-byte initialized-data block and 16-byte BSS stay at their
original offsets. The type-0x0a output remains 15,576 bytes with entry `0x40`
and 16 stack pages; its 24 relocations replace the original 26.

`apps/legacy_clock_api.c` retains the reachable text, thick-line/circle,
digit-count and x87 sine/cosine service boundaries. `apps/legacy_format.c`
recovers the SDK's nonstandard formatter at `0x3686`, including single-digit
widths 2..9 for d/x, signed character extension, Chinese byte pairs, unknown
conversions, CR/LF, zero padding and live unsigned vararg rereads. Numeric
conversion and drawing continue through the original service slots.

`python3 tests/clock.py` passes 55,576 original/C scenario pairs (1.815 s in
the source checkpoint). It compares complete service traces, BSS, window
records, palette memory, table/time/pool state, observable returns, x87
control words and stack/callee-saved contracts at aligned and unaligned load
bases. It exercises failed/successful creation, redraw/poll/close, all text
conversions, live incoming color/vararg changes, table/state changes and
separate outgoing arguments. Additional passes cover all four rounding modes
at x87 precision 24/53/64 and 512 packed values per combination. Trig modes
include controlled boundary returns and calls into the unmodified original
kernel's actual sine/cosine/power/factorial routines. Raster and window
services are controlled boundaries in this host fixture, not full guest runs.
A discovered compiler reuse of a face angle across an intervening service
call was removed to preserve rounding-mode changes.

Evidence: `build/clock_differential_summary.json`, `build/clock.original.x`,
`build/clock.kernel.x`, `build/clock.elf`, `build/clock.symbols.json` and
`build/clock.x`. At this source checkpoint the production manifest and image
still contain four rebuilt applications; CLOCK is not installed yet. The
complete OS recovery remains unfinished.

### CLOCK installation and real-guest validation (2026-10-28)

CLOCK is now the fifth application in `apps/images.json` and the combined
image. Restoring only its original file bytes reproduces the previous
four-application disk exactly: 14,223 disk bytes changed, all inside the
existing CLOCK extent. The kernel remains byte-identical at 273 intervals.
The inventory now records five rebuilt paths, two partial programs and 32
unrecovered paths. The original VHD is unchanged.

`tools/clock_smoke.py`, invoked by `make smoke-clock`, launches the real C X
file from CMD using PS/2 input without diagnostic guest code or memory writes.
The successful QEMU run also includes entry, heap and task checks. It verifies
all 24 relocations against guest code/data, loader-cleared BSS, the window
pool, surface, registered close callback, task and 200/100/400/380 geometry.
Three distinct complete frames at three packed times contain the rim, all
twelve labels, center and cleared background; representative screenshots were
visually inspected. The second hand moves. A PS/2 click at 587/112 closes the
window and removes the task. A subsequent real CMD `cpuid` returns to its
prompt; its screenshot was visually inspected. This verifies observed
rendering and motion, not a per-pixel oracle for every possible guest time.

Application regressions pass Snake (258,392 pairs), Color (2,304), MOV
(40,960), STARSCR (75,520) and CLOCK (55,576), along with four X-format tests,
five disk-installation tests, exact application-disk reconstruction and the
inventory check. The kernel/INIT disk also passes complete reconstruction.
The latest full `make verify` remains the successful 262-interval run; this
installation has the focused and guest checks described above.

Evidence: `build/clock_boot_validation.log`, `build/clock_smoke.json`,
`build/clock_installation_summary.json`, `build/clock_kernel_disk_verify.log`,
`build/clock_install_evidence/` and `build/clock_install_baseline/`.
CLOCK SHA-256:
`70a2e9ba6483f825909eab030364804863d2ecb2f60045bf561c1ef4ea72aedf`.
Combined image SHA-256:
`d5a2bc1764f6780ea08048ce221bc9f02721043100828b943863cf9c70225a1e`.
The full boot/kernel/application/driver C recovery remains unfinished.

### Kernel math series source checkpoint (2026-10-28)

`kernel/math_series.c/.h` recovers factorial `0x81a2aa..0x81a2dc`, sine
`0x81a2dc..0x81a366`, cosine `0x81a366..0x81a3f6` and repeated-product power
`0x81a538..0x81a604`. They retain ten-term series, binary64 intermediate
stores, extended division/addition, the original constant addresses and
x87 control-word behavior. Fractional powers retain the original repeated
loops (positive exponents effectively round up, negative exponents down in
ordinary finite cases); NaN exponents return the original -1.0. Quiet
comparisons avoid introducing the invalid exception for quiet NaNs. No
libm or transcendental instruction replaces a series. Code sizes are
49/50, 137/138, 111/144 and 182/204 bytes including no added helper slots.

`python3 tests/kernel_math_series.py` passes 118,512 scenarios / 344,016
executions (3.655 s) comparing original x86, ordinary C and fixed-address C.
The fixture compares all ten bytes of ST0, binary64 results, six sticky
exception flags, control words, return stack, incoming arguments, canaries
and callee-saved registers. Coverage includes three precision modes, four
rounding modes, four stack offsets (including unaligned), signed zeros,
denormals, infinities, signed NaN payloads, signaling NaNs, overflow,
factorials through 512, fractional powers and randomized binary64 inputs.
Infinite/huge exponents and enormous factorial loops are not executed; the
C source retains their original loops rather than adding a timeout/clamp.

There are 11,520 additional real service scenarios within the total:
original/candidate initializer -> actual shared slot 244/245 -> complete
series -> power/factorial, with full service-table comparison and no CALL
substitutions. Ordinary C runs are separate direct cases. The CLOCK fixture
now accepts `--kernel`; its 55,576 original/C application scenarios also pass
with the compiled four-function candidate kernel (1.954 s).

Evidence: `build/math_series_preview_summary.json`,
`build/math_series_preview_test.log`, `build/math_series_candidate.x`,
`build/math_series_preview.elf`, `build/clock_with_recovered_kernel_summary.json`
and `build/math_series_source_evidence/`. This source checkpoint overlays
four candidate slots without changing the 273-interval production manifest.

### Kernel math series installation (2026-10-28)

The four math functions are installed at their original addresses, increasing
production from 273 to 277 intervals. All previous 273 intervals retain
identical bytes. The 452 changed kernel bytes lie only in the four new slots;
all three production images pass complete reconstruction and differ from their
predecessors only in the existing kernel file. The entire installed kernel
equals the independently linked source preview.

Installed checks pass 118,512 math scenarios / 344,016 executions, including
11,520 complete service scenarios (3.642 s). CLOCK's 55,576 original/C pairs
also pass with the installed kernel (1.879 s); startup/table regressions and
inventory checks pass. These remain focused checks: the latest complete
`make verify` is still the successful 262-interval run.

A normal production CLOCK boot, using CMD and PS/2 input, observes all four
installed math entries and bodies without diagnostic guest code or CALL
substitutions. Entry/body TB records: factorial 1/4, sine 75/2252, cosine
75/3, power 3/11. These are trace records, not exact invocation counts.
The original CLOCK face, three changing frames, relocation/BSS/callback/task
contracts, close and subsequent CMD command all pass; the screenshots were
visually inspected. All 277 intervals match guest memory.

Separate normal boots verify Color, Snake, MOV and STARSCR behavior with
entry/heap/task checks and matching loaded intervals. MOV initially passed
its application checks but startup trace verification rejected one explicitly
cancelled QEMU block. `tools/entry_smoke.py` now removes an event only when
QEMU explicitly cancels both the same host TB and guest PC; real repeats and
out-of-order events remain errors. Four targeted parser tests pass and the
saved MOV trace revalidates all 30 ordered blocks and 28 call targets. A fresh
MOV run also passes the complete check. Representative screenshots were
visually inspected. STARSCR's original post-exit mouse/timer stall remains;
its desktop reuse is not claimed.

Evidence: `build/math_series_277_installation_summary.json`,
`build/math_series_277_build.log`, `build/math_series_277_disk_verify.log`,
`build/math_series_277_validation.log`, `build/math_series_277_app_boots.json`,
`build/math_series_277_evidence/`, `build/math_series_277_baseline/` and
`build/math_series_source_evidence/`. Per-run directories include SHA-256
manifests. Kernel SHA-256:
`47d9d71512973129b09adc18fce95727c265e8a84459d08510765a6abc1f35b6`.
Combined-application disk SHA-256:
`8c7a140dba6f9d90043fa1bddc1fbe6dff033fffe9682f3b5810d07d909d611e`.
The full boot/kernel/application/driver recovery remains incomplete.

### Remaining scalar math source checkpoint (2026-10-28)

`kernel/math_extra.c/.h` recovers acos `0x81a3f6`, round `0x81a4fc`, absolute
value `0x81a604`, floor `0x81a9ee`, ceil `0x81aa52`, remainder `0x81aab8` and
sqrt `0x81ab2a`. The 20-term acos expansion calls the recovered factorial and
power at their original addresses and preserves each binary64 intermediate
store. Its domain check uses quiet comparisons and the original constants;
out-of-domain inputs generate a real 0/0 invalid operation. The remaining
services retain the original int32 conversion, signed wrapping adjustments,
negative-zero behavior and zero-divisor result. The square root compiles to
the original x87 FSQRT with an equivalent frame epilogue and no double store
that would discard its extended return precision.

`python3 tests/kernel_math_extra.py` passes 323,424 scenarios / 927,264
executions, including 43,008 original/candidate complete service scenarios.
It compares all ten ST0 bytes, binary64 output, sticky exceptions, control
words, incoming words, stack/canaries and callee-saved registers. Coverage
includes all four rounding modes at three precisions, four stack alignments,
32-bit conversion boundaries and their adjacent binary64 encodings, signed
zeros/denormals/infinities/NaN payloads, signaling NaNs, random binary64
operands and in-domain acos values. Every service uses its real initialized
slot (246/247/249/250/251/252/253); complete service tables are compared and
no CALL is substituted. Code sizes are 252/262, 51/60, 37/48, 93/100, 95/102,
98/114 and 10/10 bytes. This source checkpoint leaves production at 277
intervals. Evidence: `build/math_extra_preview_summary.json`,
`build/math_extra_preview_test.log`, `build/math_extra_preview.elf` and
`build/math_extra_candidate.x`. Overall recovery remains unfinished.

### Float-formatting service source checkpoint (2026-10-28)

`kernel/math_format.c/.h` recovers slot 254 at `0x81a634..0x81a9ee` as C,
returning the end pointer without a NUL terminator. Its signed-64-bit FISTPLL
results are deliberately reduced to their low DWORD; its decimal digit
buffers, flag-dependent sign/padding order, live incoming width and precision,
unconditional '.', fractional digit reversal and trailing zeros follow the
original. For example, original precision-two output for 1.05 is `1.50`,
and zero padding precedes the sign (`00000-2.`). No ISO formatter substitutes
for these behaviors. Code fits at 802/954 bytes and calls the real recovered
power and round helpers with separate cdecl argument storage.

`python3 tests/kernel_math_format.py` passes 179,860 scenarios / 527,292
executions, including 12,288 complete original/candidate initialized-slot-254
chains with no CALL substitutions. It compares the entire 1 KiB output region,
end pointer, six incoming words, 128 bytes beyond incoming args, exception
flags, CW, empty x87 stack, callee-saved registers and canaries. Independent
literal byte/end/no-NUL oracles check the quirks above; an output alias onto
the incoming width verifies live parameter behavior. Coverage includes every
low-byte flag combination and random high flags, positive/negative/zero
precisions and widths, 32/64-bit conversion limits, signed special values,
random binary64 inputs and four alignments at all twelve precision/rounding
combinations. Tests exclude huge field widths/precisions from their bounded
output fixture; source adds no bound or clamp.

The independent preview links both the formatter and its recovered round
helper; the production power helper is unchanged. Evidence:
`build/math_format_preview_summary.json`, `build/math_format_preview_test.log`,
`build/math_format_preview.elf` and `build/math_format_candidate.x`.
At this source checkpoint production remains at 277 intervals; the seven
scalar services and formatter are not installed yet. Overall OS recovery
remains incomplete.

### Scalar math and formatter installation (2026-10-28)

The seven scalar functions and float formatter are installed at their original
addresses, increasing production from 277 to 285 declared intervals. All 277
previous intervals retain identical bytes. The 1,460 changed kernel bytes are
confined to the eight new slots, and the installed bodies match the independent
source previews. The INIT, Snake and combined-application disk images differ
from their predecessors only inside the existing kernel file. The complete
`make verify` now includes installed scalar-math and formatter differential
tests; a full 285-interval regression is running at this checkpoint.

Five normal QEMU boots pass CLOCK, Color, Snake, MOV and STARSCR application
checks, heap/task checks and matching loaded kernel intervals. CLOCK observes
the compiled factorial, sine, cosine, power, floor and ceil entries and bodies;
the other six math functions rely on their differential fixtures for execution
coverage. Trace records are not exact function-call counts. Startup tracing
also passes on the other four boots. Representative application screenshots
were visually inspected. STARSCR retains its previously documented original
post-exit mouse/timer stall; desktop reuse after it is not claimed.

Evidence: `build/math_extra_285_installation_summary.json`,
`build/math_extra_285_build.log`, `build/math_extra_285_app_boots.json`,
`build/math_extra_285_evidence/`, `build/math_extra_285_baseline/` and
`build/verify_285.log`. Kernel SHA-256:
`8ce8a12e8421a015a706481d91c9d882b21a2cf4e32bd03314d53cbec658bafb`.
Combined-application disk SHA-256:
`d682aab8254a4cdc9ce88c3699a57bcec53356b9fe0cf2d24913a0daa4ec7be1`.
The full boot/kernel/application/driver recovery remains incomplete.

### SIMD instruction boundaries source checkpoint (2026-10-28)

`kernel/simd.c/.h` recovers four 16-byte vector leaves at
`0x81ab34..0x81acc4`: byte add, subtract, multiply and four-pixel blend.
Named inline assembly inside naked C definitions preserves their all-GPR,
EFLAGS and XMM-output ABI and unaligned, read-before-store memory contract.
The 36/36/74/254 compiled bytes exactly match the original instructions;
no binary body is included. [SIMD arithmetic notes](recovery/SIMD.md) describe
the recovered lane formulas, alpha handling and original CPU requirements.
In particular, signed saturation after multiplication makes `255 * 255`
produce zero. Multiply requires SSE4.1; the other leaves require SSE2.

`python3 tests/kernel_simd.py` passes 426,368 direct scenarios, each executing
original, ordinary and fixed-address builds, plus 192 complete initialized
service-table scenarios: 1,279,488 machine executions in 1.305 seconds. An
independent byte oracle covers every input-byte pair, every ordinary weight
and complementary pixel value, random full-width weights, all 32-by-32-by-32
overlap offsets, every alignment and valid vectors against unmapped guard
pages. The fixture compares the complete output region, all GPRs, incoming
arguments/canaries, flags, all XMM registers, x87 state and MXCSR. Real service
cases run the original/candidate table initializers and the actual initialized
slot 18 without call substitutions. Evidence: `build/simd_preview_summary.json`,
`build/simd_preview_test.log`, `build/simd_preview.elf` and
`build/simd_candidate.x`. Production remains at 285 intervals at this source
checkpoint; full OS recovery remains incomplete.

### Circle outline source checkpoint (2026-10-28)

`kernel/raster_circle.c/.h` recovers 8-bit and 32-bit circle outlines at
`0x81c396..0x81c6f2` and `0x81c6f2..0x81ca82`. They preserve the eight-point
store order, asymmetric positive-side `center + offset - 1` coordinates,
side-specific signed bounds, wrapped addressing and original error recurrence.
The original can write negative coordinates or beyond a surface's other
bound; this recovery retains that behavior. Negative radii return immediately.
ID, center and color words remain live across stores, while the radius and
each iteration's eight coordinates are captured at the original points.
Compiled bodies fit their slots at 824/860 and 805/912 bytes.

`python3 tests/kernel_raster_circle.py` passes 33,042 direct scenarios / 99,126
executions against original x86, ordinary C and fixed-address C (6.093 s),
plus 3,104 actual original/candidate initialized-service/forwarder executions
(0.357 s). The independent octant model compares 32 KiB of mapped memory,
raw returns, six incoming words, canaries and callee-saved registers. Coverage
includes unaligned descriptors, wrapped IDs and geometry, signed dimension
extremes, zero/negative radii, all color bytes, caller/metadata aliases, 56,528
asymmetric coordinate stores and a descriptor whose height field is unmapped.
Straddling output stores change the live incoming color 28 times, and later
octants observe it. The bounded fixture excludes 5,040 direct scenarios with large/unmapped
accesses; it does not establish safety for all inputs. Source adds no clamp.
The service cases use the real mode-selected slot 4 and five-argument
forwarder, with no call substitutions.

Evidence: `build/raster_circle_direct_preview_summary.json`,
`build/raster_circle_chain_preview_summary.json`,
`build/raster_circle_preview_test.log`, `build/raster_circle_preview.elf` and
`build/raster_circle_candidate.x`. Production remains at 285 intervals at
this source checkpoint; complete OS recovery remains unfinished.

### Complete 285-interval regression (2026-10-28)

`make -C rewrite verify` completes successfully with exit code zero in
1,271.388 seconds. This replaces the 262-interval run as the latest complete
regression. The run includes all boot/INIT and kernel checks, installed scalar
math and float formatting, privileged QEMU fixtures, five application fixtures,
CLOCK against the installed kernel, X packaging, exact whole-disk application
installation and the binary inventory check. Kernel and all three production
disks retain their recorded pre-run SHA-256 values throughout verification.

Evidence: `build/verify_285_summary.json`, `build/verify_285.log` and
`build/full_regression_285_evidence/` (saved reports, manifests and hashes).
The separately tested SIMD and circle sources are not yet in the production
manifest at this checkpoint. Complete OS recovery remains unfinished.

### SIMD and circle outline installation (2026-10-28)

The four SIMD leaves and two C circle outlines are installed at their original
addresses, increasing production from 285 to 291 declared kernel intervals.
All 285 earlier slots remain byte-identical. Only 1,648 kernel bytes change,
inside the two circle slots; the 400 compiled SIMD bytes exactly reproduce
the previously retained original instructions. The installed circle bodies
match the independent source preview. All three production disks differ from
their 285-interval predecessors only within the existing kernel file, and
complete INIT/kernel/application disk reconstruction passes.

Installed checks pass the 33,042 circle scenarios / 99,126 executions plus
3,104 real service-chain executions, and 426,368 direct SIMD scenarios plus
192 initialized-service scenarios / 1,279,488 total SIMD executions. The
fixed-address startup/table initializer check and binary inventory also pass.
`make verify` now includes both new installed fixtures. The latest complete
regression remains the successful 285-interval run; this installation has the
focused and guest checks recorded here.

Five normal production QEMU boots pass CLOCK, Color, Snake, MOV and STARSCR
checks, heap/task checks and all 291 loaded kernel intervals. CLOCK's math
trace and the other four startup traces pass. Representative screenshots
were visually inspected. These boots do not establish execution of every new
SIMD or circle body; their separate differential fixtures execute those
bodies at their actual fixed addresses. STARSCR retains its original post-exit
mouse/timer stall, and reuse of the desktop after it is not claimed.

Evidence: `build/graphics_291_final_summary.json`,
`build/graphics_291_installation_summary.json`,
`build/graphics_291_validation_summary.json`, `build/graphics_291_app_boots.json`,
`build/graphics_291_evidence/` and `build/graphics_291_baseline/`.
Kernel SHA-256:
`44d816931c9eb139ab07971cf3b6e364958d0ca30c16d0f684eac650de694908`.
Combined-application disk SHA-256:
`aae4dd7a1b60f0d8018b80e10320f3b59cde12ddc0f0893f1ebe039691de41e3`.
Complete boot/kernel/application/driver source recovery remains unfinished.

### Filled-circle source checkpoint (2026-10-28)

`kernel/raster_disc.c/.h` recovers the 8-bit and 32-bit filled circles at
`0x81ca82..0x81cd50` and `0x81cd50..0x81d032`. The code preserves the
sixteen-digit decimal square-root refinement, binary64 intermediate stores,
x87 precision/rounding controls, signed comparisons, wrapped arithmetic,
repeated X clamps, asymmetric Y clipping and original pixel store order.
Incoming ID, center, radius and color words remain live at the original reads.
The compiled bodies occupy 618/718 and 621/738 available bytes.

`python3 tests/kernel_raster_disc.py` passes 15,456 direct scenarios / 46,368
original/ordinary-C/fixed-address-C executions (3.853 seconds), plus 320 real
mode-selected service scenarios / 640 executions (0.051 seconds). Comparisons
cover 32 KiB of mapped memory, raw returns, incoming argument mutations,
callee-saved registers, canaries, x87 control/exception/TOP state and preserved
incoming x87 data. The matrix includes 12,528 floating-point scenarios, 576
independent literal zero/one/negative-radius oracles, unaligned descriptors,
wrapped IDs, signed height extremes and 384 caller/metadata alias scenarios;
96 of those change the live color observed by subsequent stores.

The original can stall when a decimal increment is too small at 24-bit x87
precision, or when repeated right-edge clamps prevent X from progressing.
The C source retains both behaviors. Twelve isolated child-process probes
observe original/ordinary/fixed-address versions remaining active until their
100 ms SIGALRM watchdog, covering both pixel formats and both stall triggers.
The terminating matrix excludes 4,704 single-precision radius-greater-than-one
scenarios; the service matrix excludes the same class. These are bounded
fixtures, not a proof of termination or valid accesses for arbitrary inputs.
No original CALL target is replaced in these fixtures.

Evidence: `build/raster_disc_direct_preview_summary.json`,
`build/raster_disc_chain_preview_summary.json`, `build/raster_disc_preview_test.log`,
`build/raster_disc_preview.elf` and `build/raster_disc_candidate.x`.
Production still has 291 intervals at this source checkpoint. Complete OS
source recovery remains unfinished.

### Filled-circle production installation (2026-10-28)

The two decimal-root filled-circle bodies are installed at their original
addresses. Production now declares 293 kernel intervals. All 291 preceding
intervals are byte-identical to the saved baseline; 1,409 kernel bytes change
inside the two new slots only. The complete kernel matches its independent
source preview. All three rebuilt disks differ from the previous checkpoint
only in that kernel file; whole-disk INIT/kernel/application reconstruction
checks pass.

Installed filled-circle original/ordinary/fixed-address tests and actual
initialized service calls pass, including the isolated stall probes.
Installed circle, rectangle-border/fill, line/sampler, pixel-access and icon
pixel regressions also pass, together with startup/table initialization and
the updated binary inventory. `make verify` includes the installed disc test;
`make test-raster-disc` runs it separately. The latest complete regression
remains the 285-interval run; this checkpoint has the focused checks below.

Five normal production QEMU boots pass CLOCK, Color, Snake, MOV and STARSCR
checks, memory/task checks and all 293 loaded kernel interval comparisons.
CLOCK also passes its math trace; the other four pass ordered startup traces.
CLOCK and Color screenshots were visually inspected. The original STARSCR
post-exit input stall remains. These boots do not establish execution of every
filled-circle body; the separate fixed-address differential fixture does.

Evidence: `build/raster_disc_293_installation_summary.json`,
`build/raster_disc_293_validation_summary.json`,
`build/raster_disc_293_app_boots.json`, `build/raster_disc_293_evidence/` and
`build/raster_disc_291_baseline/`.
Kernel SHA-256:
`93dc8d69aa20820ce3b00dbf6c3f48bb5d4063053bb32644f32e443aced2c848`.
Combined-application disk SHA-256:
`fab48e34cc985e2cea1fb71e08bb97ae9915a28e63c5f0951b1fbf57926a9948`.
Complete boot/kernel/application/driver source recovery remains unfinished.

### Periodic-color rectangle source checkpoint (2026-10-28)

`kernel/raster_gradient.c/.h` recovers the unclipped 32-bit rectangle at
`0x81bf50..0x81c058`. Its color increments after every caller-selected number
of pixels. The counter persists across rows; color, increment and X extrema
restart from live incoming words on each row. Y extrema remain captured.
The period remains live after each store, so aliasing output can alter the
current iteration's increment decision. Arithmetic wraps like the original.
There is no clipping or added surface-height read. The C body uses 168 of
264 available bytes.

`python3 tests/kernel_raster_gradient.py` passes 23,956 original/ordinary-C/
fixed-address-C scenarios / 71,868 executions (4.611 seconds), plus 1,536
real initialized-table scenarios / 3,072 executions (0.257 seconds). The
independent widened-coordinate, byte-address oracle compares all 32 KiB of
mapped state, eight live incoming words, EAX, stack canaries and saved
registers. It observes 96,741 stores, including 260 metadata and 848 caller
stores, 11,378 row boundaries with a nonzero counter, 120 rows recapturing
mutated geometry/color/increment and 144 live period changes. Cases include
unaligned records, wrapped IDs, signed geometry and dimension extremes,
reversed/degenerate rectangles, overflowed colors, zero periods and periods
that do not divide row lengths. The bounded oracle excludes 3,824 large or
unmapped scenarios; original unchecked behavior remains in source.

The initialized-table fixture calls the real function at table offset
`0x208`, for six display depths and both CPU acceleration choices. It compares
all 320 table words and performs no CALL substitution. The shared raster ABI
fixture now supports eight arguments as well as six; the default six-argument
assembly bytes exactly match their predecessor, and the installed filled-circle
fixture passes again with the generalized fixture.

Evidence: `build/raster_gradient_direct_preview_summary.json`,
`build/raster_gradient_chain_preview_summary.json`,
`build/raster_gradient_preview_test.log` and
`build/raster_disc_shared_fixture_check.log`. Production remains at 293
intervals at this checkpoint. Complete OS source recovery remains unfinished.

### Periodic-color rectangle production installation (2026-10-28)

The recovered periodic-color rectangle is installed at `0x81bf50..0x81c058`,
increasing production to 294 declared intervals. All 293 preceding intervals
remain byte-identical. Only 256 kernel bytes change, inside this new slot,
and the kernel matches the independently built source preview. All three
production disks differ from their saved 293-interval predecessors only in
the kernel file. Complete disk reconstruction checks pass.

Installed gradient direct and real initialized-table fixtures pass, as do
filled circles and their stall probes, circle outlines, rectangle borders and
fills, line/sampler, pixel access, icon pixels, fixed-address startup/table
initialization and inventory checks. The default six-word ABI bridge remains
byte-identical after adding the separate eight-word mode. `make verify` now
runs the installed gradient fixture; `make test-raster-gradient` runs it alone.
The latest complete regression remains the successful 285-interval run;
this installation has the focused and guest checks recorded here.

Five normal production QEMU boots pass CLOCK, Color, Snake, MOV and STARSCR,
heap/task checks and all 294 loaded interval comparisons. CLOCK also passes
its math trace; Snake, MOV and STARSCR pass startup traces. Color's normal
production graphics trace observes both the new gradient entry and compiled
body (4 entry / 15 body records). The tracing report now includes the two
circle and two disc slots, which were unobserved in that boot; their separate
fixed-address differential fixtures cover execution. QEMU translation-block
records are not exact function-call counts. CLOCK and Color screenshots were
visually inspected. STARSCR retains its original post-exit input stall.

Evidence: `build/raster_gradient_294_final_summary.json`,
`build/raster_gradient_294_installation_summary.json`,
`build/raster_gradient_294_validation_summary.json`,
`build/raster_gradient_294_app_boots.json`, `build/raster_gradient_294_evidence/`
and `build/raster_gradient_293_baseline/`. Production hashes remain unchanged
during validation and guest boots.
Kernel SHA-256:
`6740e4d1a0096e7a4e0f3c1849da083ad639d44b03ef79835d18cc4a4387885c`.
Combined-application disk SHA-256:
`21099ca67908f80c7be06cfb82ba57548500527aed50354b0184be79d90017a9`.
Complete boot/kernel/application/driver source recovery remains unfinished.

### Palette and square-brush source checkpoint (2026-10-28)

`kernel/raster_brush.c/.h` recovers four routines: square brush at
`0x81d032..0x81d070`, square-stamped
DDA line at `0x81d070..0x81d1fc`, square-stamped circle at
`0x81d1fc..0x81d4e6`, and quadrant-selected circle pairs at
`0x81d4e6..0x81d6c2`. The combined fixture reuses the palette DWORD lookup
already recovered in `kernel/runtime.c` at `0x81b714..0x81b734`. The drawing
routines preserve wrapped indexing, ten-bit DDA positions,
signed division/truncation, asymmetric thick-circle octants, quadrant bit
order, duplicated points and the original separate service calls. All brush
math and control flow are C. A 15-byte ABI entry retains the original
zero-radius thick-circle return without changing incoming EAX, then jumps
to the C body for nonzero radii. The square uses the original six-word
outgoing frame so the underlying 32-bit fill's expired-frame EAX is also
preserved. GCC reserves EBX/ESI/EDI for this translation unit to achieve that
frame without extra save slots. Compiled slot usage is respectively
39/62, 316/396, 659/746 and 419/476 bytes.

`python3 tests/kernel_raster_brush.py` passes 46,904 original/ordinary-C/
fixed-address-C scenarios / 140,712 executions (7.844 seconds). A separate
fixture runs the real initializer, selects common slots 237..241 and executes
the entire fill/pixel/copy chains for 2,560 scenarios / 5,120 original/candidate
executions (0.356 seconds). Both display modes and both CPU copy selections
are covered. There are no substituted CALL targets. The independent widened
coordinate/byte-address models compare all 32 KiB of mapped state, eight live
incoming words, raw EAX, callee-saved registers and stack canaries.

The direct matrix has 64 palette, 17,788 square, 17,880 line, 4,572 thick-circle
and 6,600 quadrant-circle scenarios. It covers wrapped/unaligned palette reads,
ID aliases, signed dimensions, reversed and overflowed line geometry,
zero/negative square sizes and radii, all low quadrant masks and unused high
bits. It observes 12,722 metadata stores, 2,640 caller stores, 920 changed
incoming words, 576 zero-radius EAX cases, 612 packed-byte blocks and 32,232
real row copies. The bounded model excludes 4,736 unsafe or large generated
scenarios before execution; source retains unchecked original behavior.
An optional incoming-EAX seed was added to the raster ABI bridge; without
that option its assembly bytes are identical to the preceding version.

Evidence: `build/raster_brush_direct_preview_summary.json`,
`build/raster_brush_chain_preview_summary.json`,
`build/raster_brush_preview_test.log`, `build/raster_brush_preview.elf` and
`build/raster_brush_candidate.x`. Production remains at 294 intervals at this
source checkpoint. Complete OS source recovery remains unfinished.

### Square-brush production installation (2026-10-28)

The four square-brush and quadrant-circle routines are installed at their
original entries, increasing production from 294 to 298 declared intervals.
All preceding 294 intervals, including the already recovered palette lookup,
remain byte-identical. The palette fixture now reuses `kernel/runtime.c`;
no duplicate palette implementation is installed. Only 1,589 kernel bytes
change inside the four new slots. Production exactly matches the independent
source preview, and all three rebuilt disks change only in the kernel file.
Whole-disk INIT/kernel/application reconstruction checks pass.

Installed brush direct and initialized-service fixtures pass all 140,712
and 5,120 executions. Installed fill, pixel-access and periodic-color rectangle
fixtures also pass, as do fixed-address startup/table initialization, the
existing palette/tick-wait fixture (4,224 / 2,304 cases) and inventory checks.
`make verify` includes the installed brush test; `make test-raster-brush` runs
it separately. The latest complete regression remains the successful
285-interval run; this checkpoint has the focused and guest checks here.

Five ordinary production QEMU boots pass CLOCK, Color, Snake, MOV and
STARSCR checks, live heap/task checks and every one of the 298 loaded kernel
interval comparisons. CLOCK's graphics trace observes entry and compiled
body execution of all four new routines: square 5,647 / 5,646 records, line
15 / 1,382, thick circle 5 / 4,330 and quadrant circle 4 / 45. These QEMU
translation-block records are not exact function-call counts. The palette
lookup is unobserved in that run; its separate direct/service fixtures cover
execution. CLOCK verifies three complete changing frames, closure and CMD
reuse; its screenshot was visually inspected. The other four boots also pass
ordered startup traces. STARSCR retains its original post-exit input stall.

Evidence: `build/raster_brush_298_final_summary.json`,
`build/raster_brush_298_installation_summary.json`,
`build/raster_brush_298_validation_summary.json`,
`build/raster_brush_298_app_boots.json`, `build/raster_brush_298_evidence/`
and `build/raster_brush_294_baseline/`. Kernel and all three disk hashes remain
unchanged throughout validation and guest boots.
Kernel SHA-256:
`25aba3010273d3f684c869eabc75a32d3ff309f118f0821dab6f96d741cea3b4`.
Combined-application disk SHA-256:
`4a2079effd16dc6b5bd0fa6d0e5827315b8d4e86a9fdf74243d3148b4080f370`.
Complete boot/kernel/application/driver source recovery remains unfinished.

### Rounded/chamfered rectangle source checkpoint (2026-10-28)

`kernel/raster_rounded.c/.h` recovers three separate original routines:
rounded outline at `0x81d6c2..0x81d8ca`, rounded fill at
`0x81d8ca..0x81da68`, and chamfer fill at `0x81da68..0x81db5a`.
The outline snapshots its four centers before drawing, then re-reads live
geometry/ID/color between its four quadrant calls and four DDA edges.
The filled routines retain separate point/line/fill calls, wrapped products,
signed distance comparisons, live loop bounds and the original inclusive,
asymmetric edges. In particular, a zero radius still makes fill/edge calls;
large or negative radii are not normalized or clamped.

All geometry, loops and call orchestration are C. Empty compiler barriers
materialize individual coordinates and reserve a scratch word; they emit
no drawing or arithmetic instructions. Together with `-fno-defer-pop` and
four-byte stack alignment, this keeps the filled routines' outgoing frame
forty bytes below EBP. That preserves the 32-bit fill's incidental expired-
frame pointer in EAX. Compiled bodies use 456/520, 344/414 and 194/242 bytes.

`python3 tests/kernel_raster_rounded.py` passes 18,372 direct scenarios /
55,116 original/ordinary-C/fixed-address-C executions (3.156 seconds), plus
2,880 real initialized-table scenarios / 5,760 executions (0.429 seconds).
The composition model uses widened arithmetic and previously checked
byte-address leaf fill/copy models. It compares all 32 KiB of mapped state,
eight incoming words, raw EAX, saved registers and stack canaries. Actual
quadrant, line, fill, pixel and scalar/SSE copy routines execute, with no
substituted CALL targets. Observed coverage includes 21,347 metadata stores,
1,880 caller stores, 412 changed incoming words, 28 changes to live radius,
262,644 corner distance tests, 33,640 packed-byte blocks and 51,604 row copies.
The bounded model excludes 3,072 unsafe or large scenarios before execution;
source retains the original unchecked behavior.

The chain fixture executes real initializer/table entries 155/156 and the
direct chamfer entry for six depth values and both CPU copy choices. For
depths other than 8/32, the initializer retains pre-existing display slots;
the fixture seeds those with real 8-bit functions and checks that behavior.
It compares all 320 table words. Evidence:
`build/raster_rounded_direct_preview_summary.json`,
`build/raster_rounded_chain_preview_summary.json`,
`build/raster_rounded_preview_test.log`, `build/raster_rounded_preview.elf`
and `build/raster_rounded_candidate.x`. Production remains at 298 intervals
at this source checkpoint. Complete OS source recovery remains unfinished.

### Rounded/chamfered rectangle production installation (2026-10-28)

All three rectangle routines are installed at their original addresses,
increasing production from 298 to 301 declared intervals. Every preceding
interval is byte-identical. Only 1,135 kernel bytes change inside the three
new slots, and production exactly matches the independently built preview.
The startup panel boundary now links to the recovered rounded outline.
All three rebuilt disks change only in the kernel file. Whole-disk
INIT/kernel/application reconstruction checks pass.

Installed rounded direct and real-table fixtures pass all 55,116 and 5,760
executions. Brush, fill, pixel-access, periodic-color rectangles, DDA line/
sampler, border, circle, filled-circle and startup-UI fixtures also pass,
as do current fixed-address startup/table initialization, palette/tick-wait
and inventory checks. `make verify` includes the installed rounded test;
`make test-raster-rounded` runs it separately. The latest complete regression
remains the successful 285-interval run; this checkpoint has the focused
and guest checks described here.

Five ordinary production QEMU boots pass CLOCK, Color, Snake, MOV and
STARSCR behavior checks, memory/task checks and all 301 loaded interval
comparisons. CLOCK's graphics trace observes the new rounded outline entry
and compiled body (1 entry / 8 interior translation-block records). Rounded
fill and chamfer fill are unobserved in that boot; separate fixed-address
and initialized-table fixtures execute them. Translation-block records are
not exact call counts. CLOCK and Color screenshots were visually inspected;
CLOCK passes three complete changing frames, closure and CMD reuse.
STARSCR retains its original post-exit input stall.

Evidence: `build/raster_rounded_301_final_summary.json`,
`build/raster_rounded_301_installation_summary.json`,
`build/raster_rounded_301_validation_summary.json`,
`build/raster_rounded_301_app_boots.json`, `build/raster_rounded_301_evidence/`
and `build/raster_rounded_298_baseline/`. Production hashes remain unchanged
during validation and guest boots.
Kernel SHA-256:
`bb6518754712c8a3dad6463e0e819ff1194cdcf45fe7145e4fcac0249aae7cec`.
Combined-application disk SHA-256:
`9db17b2d944f29c63e00e7a3d5a89bed7a98108b3156c4d88cbb4bcc80fbe744`.
Complete boot/kernel/application/driver source recovery remains unfinished.

### 8x16 text glyph source checkpoint (2026-10-28)

`kernel/text_glyph.c/.h` recovers normal 8-bit glyphs at
`0x81db5a..0x81dcc0`, rotated 8-bit glyphs at `0x81dcc0..0x81de2a`, normal
32-bit glyphs at `0x81de2a..0x81df96`, and rotated 32-bit glyphs at
`0x81df96..0x81e106`. All four bodies are C, each using 227 bytes out of
358, 362, 364 and 368 available bytes respectively.

The original signed low character byte accepts only 0..127 (its additional
upper test against 132 is unreachable). The routines snapshot the descriptor,
character and font pointer. Each row snapshots its byte before drawing its
eight bits; later rows re-read potentially overwritten font data. Coordinates,
colors and descriptor fields remain live after stores. Only upper bounds are
checked; negative coordinates remain accepted. Rotated entries check the
ordinary `(x+column,y+row)` bounds but store at `(x+row,y-column)`, including
potentially negative destination rows. These behaviors are preserved.

`python3 tests/kernel_text_glyph.py` passes 70,532 direct scenarios / 211,596
original/ordinary-C/fixed-address-C executions (13.083 seconds). The independent
byte-address/mask-based model compares all 32 KiB of mapped state, a further
4 KiB of globals, six live incoming words, EAX, saved registers and stack
canaries. It covers every low character byte with unrelated high bits, both
orientations/depths, unaligned metadata/font/output, wrapped IDs/dimensions,
upper-only clipping, descriptor/table/caller/font aliases and short-circuit
unmapped-height/font guards. The bounded model excludes 5,772 unsafe scenarios
before execution; source retains the original unchecked addressing.

Coverage includes 1,106 metadata stores, 352 caller stores, 237 changed caller
words, 234,832 font stores and 19,027 row reads changed by earlier output.
Sixteen scenarios overwrite the global font pointer during drawing and
continue fetching the captured font; a further sixteen overwrite the caller's
character byte with a negative value while subsequent rows still fetch the
captured glyph, even when their pixels are clipped.

A separate real initializer/table fixture passes 24,576 scenarios / 49,152
executions (3.737 seconds), comparing all 320 table words for six depth values,
both CPU copy choices and table slots 6/12. Unsupported depths retain seeded
real 8-bit entries. No CALL targets are substituted. Evidence:
`build/text_glyph_direct_preview_summary.json`,
`build/text_glyph_chain_preview_summary.json`, `build/text_glyph_preview_test.log`,
`build/text_glyph_preview.elf` and `build/text_glyph_candidate.x`. Production
remains at 301 intervals at this source checkpoint. Complete OS source
recovery remains unfinished.

### 16x16 two-byte glyph source checkpoint (2026-10-28)

`kernel/text_wide.c/.h` recovers opaque 8-bit glyphs at
`0x81e106..0x81e2cc`, masked 8-bit glyphs at `0x81e2cc..0x81e45a`, opaque
32-bit glyphs at `0x81e45a..0x81e624` and masked 32-bit glyphs at
`0x81e624..0x81e7b2`. All drawing and control flow are C. Compiled bodies use
323/454, 321/398, 320/458 and 301/398 bytes.

Both input bytes are read before testing surface bounds. The accepted range
is 0xa0..0xfe for each byte, including the negative font indices allowed by
0xa0. The captured tile is `0x20200 + ((high-161)*94 + low-161)*32`.
Every visible pixel re-reads its font byte; this differs from the captured
8x16 rows. The transparent variants omit zero bits. Descriptor identity and
tile offset remain captured, while geometry/colors/descriptor fields remain
live after stores. Negative coordinates are not clipped.

`python3 tests/kernel_text_wide.py` passes 73,167 direct scenarios / 219,501
original/ordinary-C/fixed-address-C executions (29.122 seconds), plus 3,072
real initialized-table scenarios / 6,144 executions (0.892 seconds). The
independent row-mask/widened-address oracle compares all 32 KiB of mapped
state, the entire separately mapped 284 KiB font region, six caller words,
EAX, saved registers and stack canaries. It covers all 9,025 accepted byte
pairs in each of the four variants, every byte value in each position,
wrapped dimensions/IDs, unaligned metadata/input/output and aliases of
metadata, ID slots, caller words, encoded text and live font bits. Font
memory includes the accepted below-base tiles. No CALL targets are replaced.

Observed coverage includes 33,923 metadata stores, 336 caller stores, 192
encoded-byte stores, 27,456 font stores, 378 changed incoming words and
19,584 font-byte reads changed by previous pixel output. There are 3,544
accepted cases below the nominal font base. The bounded model excludes
2,585 unsafe scenarios before execution; unchecked behavior remains in C.
The table fixture compares all 320 words for six display depths, both CPU
choices and slots 7/14, including retention of seeded real 8-bit entries at
unsupported depths.

Evidence: `build/text_wide_direct_preview_summary.json`,
`build/text_wide_chain_preview_summary.json`, `build/text_wide_preview_test.log`,
`build/text_wide_preview.elf` and `build/text_wide_candidate.x`. This source
checkpoint was verified while the full 305-interval production regression
was running; the wide glyphs are not yet installed. Complete OS recovery
remains unfinished.

### Mixed text-run source checkpoint (2026-10-28)

`kernel/text_run.c/.h` recovers horizontal 8-bit text at
`0x81e7b2..0x81e87c`, vertical 8-bit text at `0x81e87c..0x81e952`, horizontal
32-bit text at `0x81e952..0x81ea1c`, and vertical 32-bit text at
`0x81ea1c..0x81eaf2`. All four C bodies use 155 bytes, within slots of
202, 214, 202 and 214 bytes. The combined preview also installs the four
preceding wide glyph sources, so the fixed-address composition executes
real recovered single/two-byte glyph bodies throughout.

Runs consume one byte for values below 0xa0 and two for all other values,
even when a child rejects an invalid character or a pair's second byte is
zero. The return is bytes consumed. The next byte is read before checking
index <=254, including at index 255 or 256. Vertical runs rotate only
single-byte characters and use ordinary wide glyphs at descending Y positions.
Outer ID/coordinates/colors/text remain live after every glyph call; each
child receives a separate argument snapshot. All original calls are retained.

`python3 tests/kernel_text_run.py` passes 29,167 direct scenarios / 87,501
original/ordinary-C/fixed-address-C executions (11.789 seconds), plus 1,488
real table scenarios / 2,976 executions (0.445 seconds). The composed oracle
compares 32 KiB of mapped state, the entire 284 KiB font region, six incoming
words, EAX, saved registers and stack canaries. It covers every leading byte,
threshold/invalid/truncated pairs, lengths 0..257, wrapped/negative geometry,
unaligned records/text/output, mutable future text and metadata/caller/font
aliases. It observes 482,008 ASCII and 236,467 wide calls, 33,358 text stores,
5,616 ASCII-font stores, 227,399 metadata stores, 592 caller stores and 226
changed incoming words. There are 328 returns of 255 and 120 returns of 256.

Twenty-four separate child-process guard probes verify SIGSEGV at the required
index-255/256 read for all four original/ordinary-C/fixed-address-C entries.
The bounded model excludes 7,209 unsafe direct scenarios and 48 unsafe table
scenarios before executing either implementation. Original unchecked behavior
remains in C. The table fixture compares all 320 words for six depths, both
CPU choices and slots 8/13. No CALL targets are substituted.

Evidence: `build/text_run_direct_preview_summary.json`,
`build/text_run_chain_preview_summary.json`, `build/text_run_preview_test.log`,
`build/text_run_preview.elf` and `build/text_run_candidate.x`. These eight
wide-glyph/run slots remain a source preview while the full 305-interval
production regression runs. Complete OS source recovery remains unfinished.

### 8x16 glyph installation checkpoint (2026-10-28)

The four `text_glyph.c` entries are installed in all three production disks,
bringing the kernel manifest to 305 intervals. All preceding 301 intervals
are byte-identical; only 1,405 kernel bytes change. Production matches the
verified source preview exactly, and each disk differs from the recorded
301 baseline only in its kernel. Installation evidence is recorded in
`build/text_glyph_305_installation_summary.json`.

The complete `make verify` passes in 1,319.823 seconds; kernel and three disk
SHA-256 values remain unchanged throughout. See `build/verify_305.log` and
`build/verify_305_summary.json`. A separate CLOCK QEMU boot passes rendering,
close/input, heap, task and all 305 loaded-interval checks. Normal execution
observes `text_glyph32` (269 entry / 17 body trace records) and
`text_rotated32` (21 / 685); 8-bit entries remain covered by the direct and
real initialized-table fixtures. CLOCK and desktop screenshots were visually
inspected. Boot evidence: `build/text_glyph_305_app_boots.json` and
`build/text_glyph_305_evidence/clock/`.

The installed kernel SHA-256 is
`ed9afd9def016a801e09727c3948922eba0a8af7d8f0f440b004df6043a5ee37`.
This remains a hybrid OS image; complete source recovery is unfinished.

### Wide glyph and mixed text installation checkpoint (2026-10-28)

The eight wide-glyph/text-run entries at `0x81e106..0x81eaf2` are installed
in all three production disks, bringing the manifest to 313 intervals.
All preceding 305 intervals remain byte-identical; only 2,424 kernel bytes
change, and production exactly matches the combined source preview. Each
disk differs from the recorded 305 baseline only in its kernel.

Focused checks pass for entire-disk scope, wide glyphs, mixed runs, ASCII
glyphs, rounded rectangles, startup UI, clock display, linked startup/runtime
and inventory. The shared direct fixture result arrays now honor their
configured incoming-word count; both wide/run installed suites pass again
with their original six-word configuration. This checkpoint uses the complete
305-interval regression plus these focused checks; a complete `make verify`
has not been run at 313.

CLOCK, Color, Snake, MOV and STARSCR pass QEMU startup, loaded-code, heap
and task checks. CLOCK traces execute new `text_wide32`, `text_wide32_mask`,
`text_run32` and `text_vertical32` entries and bodies. The other new entries
remain covered by fixed-address and initialized-table fixtures. CLOCK and
Color screenshots were visually inspected. Snake's first attempt failed
before launching CMD after the icon double-click; its screenshot shows the
desktop without CMD. A separate retry passes input, Snake and startup checks.
The failed attempt is retained in `build/text_run_313_evidence/snake_attempt1/`.
STARSCR retains the previously observed post-exit mouse-input stall.

Evidence: `build/text_run_313_final_summary.json`,
`build/text_run_313_validation_summary.json`, `build/text_run_313_app_boots.json`
and `build/text_run_313_evidence/`. The kernel SHA-256 is
`45e4db3790a4115c33d4fad81901e6abdf7526ea4df2fe0b8edf54a930b851fe`;
kernel and three disk hashes remain unchanged throughout testing and boots.
Complete bootloader/kernel/driver/application source recovery is unfinished.

### Unsigned radix drawing source checkpoint (2026-10-28)

`kernel/text_number.c/.h` recovers 8-bit number drawing at
`0x81eaf2..0x81ec98`, 32-bit number drawing at `0x81ec98..0x81ee3e`, and
digit count at `0x81ee3e..0x81ee7e`. These are pure C, using 147/422,
147/422 and 39/64 bytes. Drawing converts an unsigned value into a local
256-byte buffer and calls the real recovered mixed-text renderer. The
signed-byte comparison used to adjust digits is preserved for large bases,
including byte wrap, embedded zero and possible two-byte glyph sequences.
Zero prints/counts as one digit for every base. Nonzero base-zero division
faults and base-one nontermination are retained.

`python3 tests/kernel_text_number.py` passes 34,515 direct scenarios /
103,545 original/ordinary-C/fixed-address-C executions (22.085 seconds).
An independent increasing-power conversion model composes real glyph
semantics and compares 32 KiB of state, all 284 KiB of font memory, eight
incoming words (seven arguments plus a spare), EAX, registers and canaries.
Rendering snapshots its outer arguments before entering the text-run child;
190 changed caller words verify that later glyphs use that copied frame.
Coverage includes radices 2..40, every possible low output-byte remainder,
32-bit edge values and bases, unaligned metadata/output, wrapped coordinates,
metadata/ID/caller/font aliases, 1,252 wrapped digit bytes, six early-zero
strings and 732 wide-pair dispatches. The model excludes 331 unsafe cases,
including strings that would run into uninitialized trailing stack bytes;
the recovered C adds no such input guards.

Eighteen child-process probes reproduce nine SIGFPE base-zero faults and
nine base-one loops terminated by SIGALRM across all three implementations.
A separate real-initializer fixture passes 1,536 scenarios / 3,072 executions
(0.472 seconds), comparing all 320 service words for six depths, two CPU
choices and slots 9/242. It retains seeded 8-bit entries at unsupported
depths. Real fixed-address text/glyph callees execute without CALL patches.

Evidence: `build/text_number_direct_preview_summary.json`,
`build/text_number_chain_preview_summary.json`,
`build/text_number_preview_test.log`, `build/text_number_original.disasm`,
`build/text_number_preview.elf` and `build/text_number_candidate.x`.
Production remains at 313 intervals at this source checkpoint. Complete
OS recovery remains unfinished.

### Unsigned radix drawing installation checkpoint (2026-10-28)

All three number entries are installed in the production kernel and three
disks, bringing the manifest to 316 intervals. Every preceding 313 interval
remains byte-identical; only 883 kernel bytes change. Production equals the
source preview exactly. Whole-disk checks confirm that each disk differs
from the recorded 313 baseline only in its kernel.

Installed number fixtures, startup UI, clock display, linked startup/runtime
and inventory checks pass. A production CLOCK QEMU boot passes rendering,
close/input, heap, task and all 316 loaded-interval checks. Execution traces
observe `text_number32` (53 entry / 64 body records) and `number_digits`
(53 / 5); 8-bit drawing is covered by the direct and real initialized-table
fixtures. The CLOCK screenshot was visually inspected. Kernel and disk
hashes remain unchanged through validation and boot.

Evidence: `build/text_number_316_final_summary.json`,
`build/text_number_316_validation_summary.json`,
`build/text_number_316_app_boots.json` and `build/text_number_316_evidence/`.
The kernel SHA-256 is
`e50c9b5efcc24c7ec9b6d8e7b08df623096c65a749cc152e040038ff0fd0878f`.
The latest complete `make verify` checkpoint remains 305; subsequent 313
and 316 checkpoints use focused regression and recorded QEMU boots. Complete
OS source recovery remains unfinished.

### Variadic text formatting source checkpoint (2026-10-28)

`kernel/text_format.c/.h` recovers the 8-bit formatter at
`0x81ee7e..0x81f384`, 32-bit formatter at `0x81f384..0x81f88a`, and
service-dispatched formatter at `0x81f88a..0x81fd90`. Each pure C body is
944 bytes within its 1,286-byte original slot. They retain real glyph,
wide-glyph, run, radix and service-forwarder calls. The raw return is the
address of the terminating format byte, not a character count.

The original c/C, d/D, u/U, x/X and s/S behavior is preserved. Only one-digit
widths 2..9 followed by d/D/x/X are recognized. Unknown '%' forms consume
only the '%' byte, so `%%` draws two percent signs. CR resets X, while LF
adds 16 to Y without resetting X. X/Y, colors, ID and format pointer remain
live incoming words; varargs retain their initial cursor. Signed magnitudes
are captured before drawing a minus/padding; unsigned/hex values are read
again after padding, including when output changes the argument word.

`python3 tests/kernel_text_format.py` passes 7,513 scenarios / 22,539
original/ordinary-C/fixed-address-C executions (2.762 seconds). Each execution
loads a full original or candidate kernel at its real address and invokes
the real initializer. The composed oracle compares 32 KiB of state, all
284 KiB of font memory, eight incoming words, raw EAX, registers, canaries
and all 320 initialized service entries. It covers every literal/specifier/
width byte, signed/unsigned edge values, upper/lower case, malformed widths,
strings, CR/LF, unaligned/aliased metadata/input/output, caller/vararg/font
stores and wrapped geometry. Lengths through 300 verify that this outer
formatter has no 255-byte limit. The bounded oracle excludes 323 unsafe
scenarios before execution; C retains the original unchecked behavior.

Coverage includes 4,761 padding calls, 960 negative-number dispatches, 374
string dispatches, 3,409 unknown forms and 39 changed format-pointer words.
An included 192-scenario service composition covers six depths and both CPU
choices, retaining seeded 8-bit entries at unsupported depths. No CALL
targets are replaced. Evidence: `build/text_format_direct_preview_summary.json`,
`build/text_format_preview_test.log`, `build/text_format_original.disasm`,
`build/text_format_preview.elf` and `build/text_format_candidate.x`.
Production remains at 316 intervals at this source checkpoint. Complete
OS source recovery remains unfinished.

### Variadic text formatting installation checkpoint (2026-10-28)

All three formatters are installed in the production kernel and three
disks, bringing the manifest to 319 intervals. All preceding 316 intervals
remain byte-identical; only 3,726 kernel bytes change. Production equals the
independently linked source preview, and each disk differs from the recorded
316 baseline only in its kernel.

Installed formatter and radix fixtures, startup UI, clock display, linked
startup/runtime, inventory and whole-disk scope checks pass. CLOCK, Color,
Snake, MOV and STARSCR pass QEMU loaded-code, heap, task and application
checks. CLOCK and Color screenshots were visually inspected. The normal
CLOCK trace does not observe any of the three formatter entries: their
execution evidence comes from the composed original/native/fixed-address
fixtures, while the guest verifies all 319 installed byte ranges. The first
runner incorrectly required an observed formatter after the successful
CLOCK boot; the report records this observation limit and retains that
boot's artifacts without repeating it. STARSCR retains its known original
post-exit input stall.

Kernel and disk hashes remain unchanged throughout validation and boots.
Evidence: `build/text_format_319_final_summary.json`,
`build/text_format_319_validation_summary.json`,
`build/text_format_319_app_boots.json` and `build/text_format_319_evidence/`.
The kernel SHA-256 is
`c895124fba0f8051d6383f870f481353a92167c0af635409eb26114e6e700bd7`.
The latest complete `make verify` checkpoint remains 305; 313, 316 and 319
use focused regression and recorded guest boots. Five applications are
rebuilt, while INIT/kernel are partial and 32 original program paths remain
unrecovered. Complete OS source recovery is unfinished.

### BMP drawing source checkpoint (2026-10-28)

`kernel/bitmap.c/.h` recovers `0x81ffc4..0x82040c` in 847/1,096 bytes.
Drawing and control flow are C; an instruction-free EAX output binding
preserves the incoming register on negative-X rejection. The implementation
retains signed clipping, unsigned crop selection, captured file/header/palette
identities, live input and geometry between calls, and the required unused
global read. It calls the real pixel writers and ordered-dither quantizer.

This legacy renderer does not validate BMP signatures/compression or apply
standard row padding. Four-bit images use a byte-per-column source stride,
draw the low nibble first, reload addressing/data after that store and draw
the second pixel even at odd cropped widths. The 24-to-32-bit path reads a
full DWORD before masking, retaining its fourth-byte overread. The 24-to-8-bit
path passes local column/row coordinates to the quantizer.

`python3 tests/kernel_bitmap.py` passes 11,108 direct scenarios / 33,324
original/ordinary-C/fixed-address-C executions in 2.990 seconds. Its bounded
oracle compares 32 KiB of state, 4 KiB of globals, all six incoming words,
raw EAX, saved registers and canaries, excluding 2,716 unsafe scenarios
before execution. Coverage includes 54,380 stores, 224 bitmap stores, 593
metadata stores, 196 changed caller words, 20 changed bitmap arguments,
15 output-dependent nibble reloads and 1,153 odd-width pairs. Nine child
SIGSEGV probes confirm the four-byte fetch. No CALL targets are substituted.

A separate real-initializer composition passes 1,344 scenarios / 2,688
executions in 0.241 seconds, with 192 unsafe cases excluded. It compares
all 320 service words and executes slot 75 across six display depths and
two CPU choices; surface depth varies independently. The original VHD
contains no BMP resources, so these use generated/aliased image fixtures.
Evidence: `build/bitmap_direct_preview_summary.json`,
`build/bitmap_chain_preview_summary.json`, `build/bitmap_preview_test.log`,
`build/bitmap_original.disasm` and `build/bitmap_candidate.x`. Production
remains at 319 intervals at this source checkpoint. Complete OS recovery
is unfinished.

### BMP drawing installation checkpoint (2026-10-28)

The renderer is installed in the production kernel and all three disks,
bringing the manifest to 320 intervals. All preceding 319 intervals remain
byte-identical; only 1,051 kernel bytes change. Production matches the
independent source preview, and each disk differs from the recorded 319
baseline only in its kernel. `make test-bitmap` runs the installed direct
and initialized-service fixtures; both are included in `make verify`.

Installed BMP, quantizer, pixel access, startup UI, clock display, linked
startup/runtime, inventory and whole-disk checks pass. A production CLOCK
QEMU boot passes rendering, close/input, memory, task and all 320 loaded
interval checks; its screenshot was visually inspected. Normal startup
does not observe the BMP entry: execution of this body is established by
the direct and service composition fixtures. Kernel and disk hashes remain
unchanged throughout validation and boot.

Evidence: `build/bitmap_320_final_summary.json`,
`build/bitmap_320_validation_summary.json`, `build/bitmap_320_app_boots.json`
and `build/bitmap_320_evidence/`. The kernel SHA-256 is
`b4cf03fa96980fc96e45600a5c97bda9da262260e3b2b167f8c53d9ad5752afb`.
The latest complete regression remains 305; later checkpoints use focused
checks and recorded QEMU boots. Complete OS source recovery is unfinished.

### BMP file wrapper source checkpoint (2026-10-28)

`kernel/bitmap_files.c/.h` recovers file drawing at `0x81fec2..0x81ffc4`
and the file-to-fit wrapper at `0x82040c..0x8204ac`, in 220/258 and 134/160
bytes. Logic is C, with the same instruction-free incoming-EAX binding on
negative-X rejection. Initial signed coordinate clipping, wrapped surface
lookup, C..F uppercase absolute-drive detection, unchecked path lookahead,
live caller arguments and captured loaded-buffer identity are retained.
Absolute resolution requires a signed result greater than zero; relative
lookup accepts zero. Null buffers stop without drawing/freeing. Successful
loads draw before freeing and leave the free callee's raw EAX as the result.

`python3 tests/kernel_bitmap_files.py` passes 30,306 scenarios / 90,918
original/ordinary-C/fixed-address-C executions in 6.810 seconds. It compares
callback arguments/order, a complete copied 32-byte directory record, all
32 KiB of state, six incoming words, EAX, registers/canaries, 4 KiB of
globals and all 320 initialized service words. Coverage includes every byte
at all three drive-prefix positions, signed/wrapped coordinate and lookup
extremes, null/opaque buffers, 85,824 caller-word mutations and 35,196
directory-record reads. All six display depths and two CPU choices execute
through the real initializer, with fixed-address dispatch via slots 74/140.
Twelve child SIGSEGV probes preserve the unchecked lookahead; rejected first
bytes also execute safely at the end of a mapped page.

The fixture explicitly detours seven filesystem/draw/free helper entries.
For 96 composition scenarios it restores the BMP renderer entry inside the
recording callback and executes the actual original/recovered renderer and
pixel/quantizer bodies, producing 288 real-renderer executions across four
conversion modes. Filesystem/free and scaled drawing remain test boundaries;
this does not establish real filesystem integration. Evidence:
`build/bitmap_files_preview_summary.json`, `build/bitmap_files_preview_test.log`,
`build/bitmap_draw_file_original.disasm`, `build/bitmap_fit_file_original.disasm`
and `build/bitmap_files_candidate.x`. Production remains at 320 intervals
at this source checkpoint. Complete OS source recovery is unfinished.

### BMP file wrapper installation checkpoint (2026-10-28)

Both file wrappers are installed in the production kernel and all three
disks, bringing the manifest to 322 intervals. All preceding 320 intervals
remain byte-identical; only 382 kernel bytes change. Production exactly
matches the independent source preview, and each disk differs from the
recorded 320 baseline only in its kernel. `make test-bitmap-files` and
`make verify` include the installed wrapper fixture.

Installed wrapper/BMP fixtures, startup UI, clock display, linked startup/
runtime, inventory and whole-disk checks pass. A production CLOCK QEMU boot
passes rendering, close/input, memory, task and all 322 loaded-interval
checks. Its screenshot was visually inspected. Normal CLOCK execution
does not observe either new wrapper entry; their execution evidence comes
from the explicit helper-boundary fixtures, including real BMP composition.
Real filesystem integration of these wrappers remains unverified. Kernel
and disk hashes remain unchanged throughout validation and boot.

Evidence: `build/bitmap_files_322_final_summary.json`,
`build/bitmap_files_322_validation_summary.json`,
`build/bitmap_files_322_app_boots.json` and `build/bitmap_files_322_evidence/`.
The kernel SHA-256 is
`9f381907fd6914382db24870e54da771c76eb589a65d2eac9c643eeed090e3a0`.
The latest complete regression remains 305; later checkpoints use focused
checks and recorded guest boots. INIT/kernel and boot stage 3 remain
partial, five applications are rebuilt and 32 original program paths
remain unrecovered. Complete OS source recovery is unfinished.

### BMP fitting source checkpoint (2026-10-28)

`kernel/bitmap_fit.c` recovers `0x8204ac..0x820d66` as a 1,188-byte C body
in its original 2,234-byte interval. It accepts only 24-bit source data and
8-/32-bit targets. Percent steps divide the wrapped destination-times-100
by the unsigned dimension difference, using signed division. It retains
live dimensions, ID/bitmap arguments, source-coordinate clamps, horizontal
skip/repeat counters and asymmetric vertical repetition. In the 8-bit
repeat branch, source addressing stays captured while components are read
again; dither X is the preceding destination column. The 32-bit branch
recalculates the source on every repeated store and reads a full DWORD.
No signature/compression validation or standard BMP row padding is added.

`python3 tests/kernel_bitmap_fit.py` passes 26,940 scenarios / 80,820
original/ordinary-C/fixed-address-C executions in 10.416 seconds. The
bytewise oracle uses widened wrapped address arithmetic and bitwise signed
division, comparing 32 KiB of state, six caller words, raw EAX, registers/
canaries, 4 KiB of globals and all 320 service words. It invokes the real
initializer across six display depths/two CPU choices and dispatches the
original/fixed C via slot 76, with real pixel/quantizer callees throughout.
No helpers are substituted. The bounded oracle excludes 2,383 scenarios
before execution, including 411 that exceed its iteration budget; C retains
the original unchecked behavior.

Coverage includes all 0..8 source/target dimension combinations, signed/
wrapped geometry, six target depths/eight source depths, unaligned and
aliased metadata/caller/bitmap storage, 273,380 stores, 618 changed caller
words and 17 changed bitmap arguments. It exercises 30,312 horizontal skips,
87,213 repeats, 8,646 vertical skips, 23,588 vertical holds and 43,964 source
clamps. There are 1,087 duplicate stores whose fresh source address would
differ from the required captured one, and 48,982 distinct dither phases.
Forked probes reproduce 15 source-read SIGSEGVs, three zero-ratio loops
terminated by SIGALRM, three successful three-byte reads in 8-bit mode,
and three required-unused-global SIGSEGVs. Evidence:
`build/bitmap_fit_preview_summary.json`, `build/bitmap_fit_preview_test.log`,
`build/bitmap_fit_original.disasm` and `build/bitmap_fit_candidate.x`.
Production remains at 322 intervals at this source checkpoint. Complete
OS source recovery is unfinished.

### BMP fitting installation checkpoint (2026-10-28)

The fitting body is installed in the production kernel and all three disks,
bringing the manifest to 323 intervals. All preceding 322 intervals remain
byte-identical; only 2,172 kernel bytes change. Production matches the source
preview exactly, and each disk differs from its recorded 322 baseline only
in the kernel. `make test-bitmap-fit` and `make verify` include the installed
fitting fixture.

Installed fitting, file-wrapper and BMP fixtures pass, together with startup
UI, clock display, linked startup/runtime, inventory and whole-disk checks.
The file-wrapper fixture now also restores and executes the actual fitting
entry: 48 scenarios / 144 executions cover equal 3x2 source/target dimensions
for 8-/32-bit output and both file routes. Its complete suite passes 30,354
scenarios / 91,062 executions (6.952 seconds), including the preceding 288
real drawing executions. Filesystem and free helpers remain explicit test
boundaries; real filesystem integration is still unverified. The dedicated
fitting fixture covers other geometry without helper substitutions.

A production CLOCK QEMU boot passes rendering, close/input, memory, task
and all 323 loaded-interval checks; the screenshot was visually inspected.
The normal boot does not observe the new fitting entry. Its execution
evidence comes from direct/service and file-wrapper composition fixtures.
Kernel and disk hashes remain unchanged throughout validation and boot.
Evidence: `build/bitmap_fit_323_final_summary.json`,
`build/bitmap_fit_323_validation_summary.json`, `build/bitmap_fit_323_app_boots.json`
and `build/bitmap_fit_323_evidence/`. The kernel SHA-256 is
`e23e03f3c40a025c6cccaadd8fac5e97e44aba123fab7b0809ffd06380ff3112`.
The latest complete regression remains 305; subsequent checkpoints use
focused checks and recorded guest boots. Full bootloader/kernel/driver/
application source recovery remains unfinished.

### Direct-surface BMP fitting source checkpoint (2026-10-28)

`kernel/bitmap_surface_fit.c` recovers `0x820d66..0x821446` in 980/1,760
bytes of C. This entry takes a surface pointer directly, captures its output
buffer once and writes pixels without the ID table or clipped pixel helpers.
Surface/bitmap arguments and geometry remain live; the captured buffer stays
fixed even when output changes the surface's buffer field. It preserves the
preceding fitting algorithm's signed wrapped percent steps, asymmetric
repetition, source clamps, 8-bit captured source/dither phase and DWORD
overread in 32-bit mode. The 8-bit path reads the buffer before rejecting
an unsupported source depth; the 32-bit path rejects it before that read.

`python3 tests/kernel_bitmap_surface_fit.py` passes 27,079 scenarios / 81,237
original/ordinary-C/fixed-address-C executions in 10.932 seconds. The shared
bytewise/bounded fitting oracle now supports direct surface records and
captured-buffer stores. It compares 32 KiB of state, six incoming words,
EAX, registers/canaries, 4 KiB of globals and all 320 service words through
the real initializer/slot 77 across six display depths and both CPU choices.
Real quantizer code executes without helper substitutions. The oracle
excludes 2,245 unsafe scenarios, including 398 exceeding its iteration budget.

Coverage includes 276,360 stores, 630 changed caller words, 20 changed bitmap
arguments, nine changed surface pointers, 4,681 stores after the buffer field
changes, 580 captured-source duplicates and 49,897 distinct dither phases.
Forks reproduce 15 source faults, three zero-step loops, three successful
three-byte reads, three required-global faults and 12 buffer-read-order
probes. Six executions succeed with the ID table mapping removed. The
preceding installed ID-based fitting suite passes again with its exact
26,940-scenario counts after sharing the fixture.

Evidence: `build/bitmap_surface_fit_preview_summary.json`,
`build/bitmap_surface_fit_preview_test.log`,
`build/bitmap_surface_fit_original.disasm`,
`build/bitmap_surface_fit_previous_regression.log` and
`build/bitmap_surface_fit_candidate.x`. Production remains at 323 intervals
at this source checkpoint. Complete OS source recovery is unfinished.

### Direct-surface BMP fitting installation checkpoint (2026-10-28)

The new body is installed in the production kernel and all three disks,
bringing the manifest to 324 intervals. All preceding 323 intervals remain
byte-identical; only 1,715 kernel bytes change. Production equals the source
preview, and each disk differs from its recorded 323 baseline only in the
kernel. `make test-bitmap-surface-fit` and `make verify` include the installed
direct-surface fixture.

Installed direct/ID-based fitting fixtures, startup UI, clock display, linked
startup/runtime, inventory and whole-disk checks pass. A production CLOCK
QEMU boot passes rendering, close/input, memory, task and all 324 loaded
interval checks; its screenshot was visually inspected. Normal CLOCK
execution does not observe this direct-surface entry; execution evidence
comes from the original/native/fixed-address and real initialized-service
fixtures. Kernel and disk hashes remain unchanged throughout validation
and boot.

Evidence: `build/bitmap_surface_fit_324_final_summary.json`,
`build/bitmap_surface_fit_324_validation_summary.json`,
`build/bitmap_surface_fit_324_app_boots.json` and `build/bitmap_surface_fit_324_evidence/`.
The kernel SHA-256 is
`99a9962c69ec5e0036350e17a9ed4b6dec31ace05d8d68b15a420bf97717ca5a`.
The latest complete regression remains 305; later checkpoints use focused
checks and recorded guest boots. Full OS source recovery remains unfinished.

### Color-key BMP drawing source checkpoint (2026-10-28)

`kernel/bitmap_key.c` recovers `0x821446..0x821624` as 372 bytes of C in
its original 478-byte interval. It draws 24-bit or indexed 8-bit source
pixels onto 32-bit surfaces, skipping full-width color-key matches. Source
rows advance forward while destination Y runs backward. Header dimensions,
file offsets and incoming arguments remain live across writes. The 24-bit
path keeps the original DWORD source overread and always renders the fixed
literal `t=` at `(10,10)` afterward, including for an empty image. Its text
colors come from the captured color-table pointer after image drawing.
The original unused diagnostic vararg is not read by that literal formatter;
C supplies zero when no image pixel has been read.

`python3 tests/kernel_bitmap_key.py` passes 10,634 scenarios / 31,902
original/ordinary-C/fixed-address-C executions in 3.520 seconds. The independent
bytewise oracle composes image drawing, clipping and both real text glyphs.
The real pixel, formatter, glyph and service-initializer bodies execute without
helper substitutions, including slot 78 dispatch across six display depths
and both CPU feature choices. Checks compare 32 KiB of state, six caller
words, raw EAX, registers/canaries, 4 KiB of globals and 320 service words.

Coverage includes 1,439 key matches, 59,534 image pixel calls, 111,616 text
stores, 29,824 stores into font memory, 32 subsequent changed font-byte reads,
96 changed text-color cases, 909 changed caller words, 167 changed keys and
193 changed bitmap arguments. The bounded oracle excludes 3,670 unsafe
scenarios before execution, including 260 exceeding its iteration budget.
Forks reproduce 12 DWORD-overread/empty-image diagnostic faults and six
safe early returns that avoid invalid image/color memory.

Evidence: `build/bitmap_key_preview_summary.json`,
`build/bitmap_key_preview_test.log`, `build/bitmap_key_original.disasm` and
`build/bitmap_key_candidate.x`. Production remains at 324 intervals at this
source checkpoint. Full OS source recovery remains unfinished.

### Color-key BMP drawing installation checkpoint (2026-10-28)

The new body is installed in the production kernel and all three disks,
bringing the manifest to 325 intervals. All preceding 324 intervals remain
byte-identical; only 451 kernel bytes change. Production equals the source
preview, and each disk differs from its recorded 324 baseline only in the
kernel. `make test-bitmap-key` and `make verify` include the installed fixture,
which passes with its original 10,634 scenarios / 31,902 executions.

A production CLOCK QEMU boot passes rendering, close/input, memory, task
and all 325 loaded-interval checks; its screenshot was visually inspected.
Normal CLOCK execution does not observe the new color-key entry. Execution
evidence for that entry comes from the original/native/fixed-address and
initialized-service fixtures, using real pixel/formatter/glyph bodies.
Kernel and disk hashes remain unchanged throughout validation and boot.

Evidence: `build/bitmap_key_325_final_summary.json`,
`build/bitmap_key_325_integrity_summary.json`, `build/bitmap_key_325_app_boots.json`
and `build/bitmap_key_325_evidence/`. The kernel SHA-256 is
`a2c98f3384e9c7c71162b43530dbecd667e4c1e8e830a0c4110ccdee704f1c76`.
A complete `make -j4 verify` regression is running at this checkpoint;
`build/bitmap_key_325_verify.log` records its progress. The latest completed
full regression remains 305 until that run finishes. Full OS source recovery
remains unfinished.

### Remaining string services source checkpoint (2026-10-28)

`kernel/string_extra.c` recovers seven entries at `0x82a531..0x82a9bb` in
703/1,162 bytes: in-place ASCII uppercase, substring search, configuration
value lookup, character search, signed-byte uppercase, unsigned radix text
and delimiter splitting. Character search advances the actual incoming
pointer word. Configuration lookup scans the fixed table at `0x62200`, can
match a name inside another name, and copies until a semicolon or 255 bytes,
including bytes after a NUL. Radix conversion retains signed width/character
comparisons, live output/radix/width words, unsigned division and the zero
value's empty result for nonpositive widths. Splitting zeroes delimiters,
then writes pointers to nonempty runs without appending a sentinel.
The original radix-1 and nonempty-input/empty-delimiter loops remain intact.

`python3 tests/kernel_string_extra.py` passes 10,913 scenarios / 32,739
original/ordinary-C/fixed-address-C executions in 9.244 seconds. Independent
bytewise models use bitwise division and bounded memory/iteration checks;
1,160 unsafe scenarios are excluded before execution. Tests execute real
`strlen` and initializer bodies and dispatch via services 186..192 across
six display depths and both CPU choices, without helper substitutions.
They compare 32 KiB of memory, 4 KiB of configuration memory, six caller
words, EAX, registers/canaries, 4 KiB of globals and all 320 service words.

Coverage includes all character bytes and full-width inputs, substring
overlaps, configuration name collisions/embedded NUL/254..256-byte limits,
16 radices and nine signed widths, binary strings through length seven,
and output aliasing with inputs, configuration memory and caller words.
There are 98,077 stores, 1,599 caller-memory stores, 6,000 configuration
stores, 1,609 changed caller words, 14,880 pointer increments, 52 empty-zero
results and 2,536 emitted tokens. Forks reproduce six unchecked-memory/
divide faults, six loops terminated by SIGALRM and three safe terminator
returns. Evidence: `build/string_extra_preview_summary.json`,
`build/string_extra_preview_test.log`, `build/text_convert_original.disasm`
and `build/string_extra_candidate.x`. Production remains at 325 intervals
at this source checkpoint. Full OS source recovery remains unfinished.

### JPEG bit reader installation checkpoint (2026-10-28)

`kernel/jpeg_bits.c` recovers `0x83a835..0x83a919` in 196/228 bytes.
It preserves signed reservoir-count comparisons, x86 masked shifts,
16-bit result truncation, live context/caller words, and the original
unconditional skip after an input `0xff`. EOF retains source-pointer
increments while discarding local reservoir/count changes.

`make test-jpeg-bits` passes 7,408 scenarios / 22,224 original/ordinary-C/
fixed-address-C executions and 38,496 sequential calls. The bytewise oracle
compares source/state/caller memory, EAX, preserved registers and canaries;
1,748 scenarios change caller words. Forked probes cover three unchecked
source faults and six safe EOF/reservoir returns. Real JPEG information and
decoding execute twice with the original 260-by-260 image, comparing every
pixel, output slack and all 64 KiB of workspace without helper substitutions.

The entry is installed in the production kernel and all three disks,
bringing the manifest to 326 intervals. Reversing only this new slot
reproduces the preceding 325 kernel and disk SHA-256 values: all previous
slots and all other disk bytes are unchanged; 195 kernel bytes change.
The production image equals the independently linked source preview.
The Makefile includes installed/preview targets and the installed fixture
in `make verify`; the recovery inventory has been regenerated.

A production QEMU CLOCK boot passes its three changing complete frames,
PS/2 close, task exit, CMD reuse, 128 MiB RAM/heap and real task-switch
checks. All 326 loaded kernel intervals match the disk. The saved CLOCK
screenshot was visually inspected. This boot establishes loaded bytes and
system behavior; direct bit-reader execution is established by the fixtures.
The full 326-interval `make verify` regression is still running at this
checkpoint; no complete-regression result is claimed yet.

Evidence: `build/jpeg_bits_installed_summary.json`,
`build/jpeg_bits_326_integrity_summary.json`, `build/jpeg_bits_326_boot.log`
and `build/jpeg_bits_326_evidence/`. Kernel SHA-256:
`02e1af5c1d57557047ceab75eb92955db055f040126b2f55b2815233f760c871`.
The JPEG decoder as a whole and full OS source recovery remain unfinished.

### JPEG frame parser source checkpoint (2026-10-28)

`kernel/jpeg_frame.c` recovers `0x83a919..0x83aae7` as 402 bytes of C
inside its original 462-byte slot. It parses big-endian height/width,
component IDs, raw sampling bytes, horizontal/vertical sampling nibbles
and quantization-table selectors. Signed comparisons update the maximum
sampling factors. It ignores segment length/precision, accepts more than
three components (overlapping its adjacent arrays), and keeps source,
context and destination fields live across stores. Truncated payloads
advance the cursor to the declared end; wrapped pointer checks and
unchecked source faults retain the original behavior.

`python3 tests/kernel_jpeg_frame.py` passes 2,297 scenarios / 6,891
original/ordinary-C/fixed-address-C executions, with no excluded scenarios.
The separate bytewise bounded oracle compares all 64 KiB of context,
8 KiB of source, six incoming words, EAX, preserved registers and canaries.
It covers 15 component counts through 255, 11 input lengths, all 256
sampling bytes, four alignments, signed initial maxima, 18 source aliases,
and truncated cursors overlapping incoming words. Coverage includes 9,924
parsed components, 537 truncations, 71,136 state stores, 368 changed source
headers and three changed caller-word scenarios.

Forked probes reproduce six unchecked/wrapped-source faults and three
safe truncated returns. Two real information/decode executions of the
original 260-by-260 JPEG compare the complete output/slack and 64 KiB
workspace. No helper bodies are substituted. This recovers only the frame
parser; other JPEG routines remain original. Evidence:
`build/jpeg_frame_preview_summary.json`,
`build/jpeg_frame_preview_test.log`, `build/jpeg_frame_original.disasm`
and `build/jpeg_frame_candidate.x`. Production remains at 326 intervals
at this source checkpoint. Full OS source recovery remains unfinished.

### JPEG quantization parser source checkpoint (2026-10-28)

`kernel/jpeg_tables.c` recovers `0x83aae7..0x83ac7b` in 342/404 bytes.
The low three flag bits select one of eight table offsets; any upper flag
bit selects a two-byte input stride. That path retains only the first
byte of each pair. Positive payload lengths always process a complete
64-entry table even when the declared payload is shorter. Lengths zero
and one retain wrapped arithmetic; truncation sets the cursor to the
declared end. Table three overwrites the maximum-table field. The byte
path preserves the original captured context for its post-store cursor
increment, while the two-byte path reloads the live context.

`python3 tests/kernel_jpeg_tables.py` passes 9,331 scenarios / 27,993
original/ordinary-C/fixed-address-C executions. Its bounded bytewise oracle
excludes 34 unsafe/excessive-loop scenarios. Coverage includes all flag
bytes, 17 declared lengths, eight input limits, signed initial maxima,
unaligned context/source aliases, 504,000 table entries, 2,893 truncations,
1,030,201 stores, 7,271 wide-table paths, 4,082 undersized declared tables,
48 changed source headers and three changed caller-word scenarios.
All context/source bytes, six incoming words, EAX and the preserved
register/canary contract are compared.

Forks reproduce 12 source/wrapped-pointer/short-table faults and six safe
truncated/empty returns. Two full real JPEG info/decode executions compare
all output pixels/slack and the 64 KiB workspace without helper substitutions.
Evidence: `build/jpeg_tables_preview_summary.json`,
`build/jpeg_tables_preview_test.log`, `build/jpeg_tables_original.disasm`
and `build/jpeg_tables_candidate.x`. Production remains at 326 intervals
at this source checkpoint. Remaining JPEG routines and complete OS source
recovery are unfinished.

### 326-interval complete verification recipe checkpoint (2026-10-28)

Every command in the 326-interval `make verify` recipe has now passed.
The main run was terminated with exit code 143 after reaching the first
CLOCK test, after all preceding native/installed kernel checks, QEMU
hardware checks, math tests, X-format tests and the first four application
tests passed. The remaining six commands were executed separately: both
55,576-scenario CLOCK variants, application-image unit tests, both entire
application-disk checks and inventory consistency all pass. This is
complete recipe coverage across two runs; the interrupted `make verify`
process itself did not return zero. The 326 kernel and all three disk
hashes are unchanged from the saved QEMU boot checkpoint.

Evidence: `build/jpeg_bits_326_verify_main.log`,
`build/jpeg_bits_326_verify_tail.log` and
`build/jpeg_bits_326_verify_tail_summary.json`. Complete OS source recovery
remains unfinished.

### JPEG frame/quantization installation checkpoint (2026-10-28)

Both new JPEG parser bodies are installed at their original addresses,
bringing production to 328 declared intervals. Their installed fixtures
pass with the source-checkpoint counts, and the preceding bit-reader
fixture also passes against this combined kernel. Real original/C JPEG
information and complete decoding agree across all three fixtures, with
no helper substitutions. Make targets `test-jpeg-frame` and
`test-jpeg-tables` and their preview variants are available; installed
checks are included in `make verify`.

All preceding 326 intervals and all other kernel bytes remain unchanged;
789 kernel bytes change. Each of the three rebuilt disks differs from
its saved baseline only in the kernel. Whole boot/app disk reconstruction,
inventory, linked startup/runtime and desktop checks pass. A production
CLOCK QEMU boot passes rendering, closing, CMD reuse, memory, task switching
and all 328 loaded-interval checks; its CLOCK screenshot was visually
inspected. Kernel/disk hashes stay unchanged during validation and boot.

Evidence: `build/jpeg_parsers_328_build.log`,
`build/jpeg_parsers_328_validation.log`,
`build/jpeg_parsers_328_final_summary.json`,
`build/jpeg_parsers_328_boot.log` and `build/jpeg_parsers_328_evidence/`.
Kernel SHA-256:
`b9d9a39c2ef64def9dbe3fc9be57e10a8d21f4c60634393a39c9c2da8a401e3b`.
The latest complete verification recipe coverage is 326; this checkpoint
uses focused checks and a recorded production boot. Other JPEG routines
and full OS source recovery remain unfinished.

### Seven string services installation checkpoint (2026-10-28)

The seven previously recovered string entries at `0x82a531..0x82a9bb`
are now linked into production, bringing the manifest to 335 intervals.
`make test-string-extra` passes the original 10,913 scenarios / 32,739
original/ordinary-C/fixed-address-C executions, including real `strlen`,
initializer and services 186..192 without helper substitutions. Its source
preview equals the production image. All three installed JPEG fixtures
pass again with their source-checkpoint counts and real JPEG composition.
Both string test targets and the installed `make verify` check are wired
into the build.

All preceding 328 intervals and all other kernel bytes are unchanged;
only 1,106 kernel bytes change. All three production disks differ from
their saved baseline only in the kernel. Whole boot/application disk
checks, inventory, linked startup/runtime/string helpers, startup
applications/UI and 55,576 CLOCK scenarios against the production kernel
pass. A QEMU CLOCK boot passes changing complete frames, PS/2 close, task
exit, CMD reuse, RAM/heap and real task-switch checks. All 335 loaded
intervals match production; the screenshot was visually inspected. Kernel
and disk hashes remain unchanged during validation and boot.

Evidence: `build/string_extra_335_build.log`,
`build/string_extra_335_validation.log`, `build/string_extra_335_boot.log`,
`build/string_extra_335_final_summary.json` and
`build/string_extra_335_evidence/`. The latter preserves boot artifacts and
all four installed fixture summaries. Kernel SHA-256:
`a9dbe4cf8882d98afeda0c8bc39178f8c8df3e13753a7bedc11f0f7f19af4124`.
The latest complete verification recipe coverage is 326, executed across
the interrupted main run and its successful tail. The 328/335 checkpoints
use focused regression, byte comparisons and recorded production boots.
The system still retains original boot/INIT/kernel/driver/application
components; complete OS source recovery remains unfinished.

### JPEG Huffman table source checkpoint (2026-10-28)

`kernel/jpeg_huffman.c` recovers `0x83ac7b..0x83af27` in 484/684 bytes.
It preserves full-nibble table selection, captured table identity, live
context/source fields, length expansion, wrapped 16-bit canonical codes,
and symbol copying. Code generation precedes symbol bounds checking. Its
inner length-run scan can write code words beyond the declared symbol
count; oversized code/length arrays can overlap and cause the original
very long length-counter wrap loop. Even an empty table reads its retained
first length byte. Declared DHT length has no upfront payload bounds check.

`python3 tests/kernel_jpeg_huffman.py` passes 5,277 scenarios / 15,831
original/ordinary-C/fixed-address-C executions. The bounded bytewise oracle
excludes 852 unsafe/excessive-loop cases before native execution. Checks
compare all 128 KiB of context, 8 KiB of source, six incoming words, raw
EAX, preserved registers and canaries. Coverage includes all flag bytes,
16 lengths, 12 input limits, multi-record payloads, unaligned source/table/
cursor aliases, equal retained length sentinels and caller-word overlaps.
It observes 569,337 length-byte stores, 571,265 code-word stores, 470,264
symbol stores, 1,928 code stores past declared counts and 323 empty tables.

Forks reproduce 12 source/wrapped-pointer/mandatory-length/sentinel faults,
three length-counter wrap loops terminated by SIGALRM, and three safe
truncated returns. Two real JPEG info/decode executions compare complete
pixels/slack and workspace without helper substitutions. Evidence:
`build/jpeg_huffman_preview_summary.json`,
`build/jpeg_huffman_preview_test.log`, `build/jpeg_huffman_original.disasm`
and `build/jpeg_huffman_candidate.x`. Production remains at 335 intervals
at this source checkpoint. Full JPEG and OS source recovery are unfinished.

### JPEG Huffman table installation checkpoint (2026-10-28)

The Huffman table parser is now installed at its original address, bringing
production to 336 declared intervals. `make test-jpeg-huffman` passes all
5,277 scenarios / 15,831 original/ordinary-C/fixed-address-C executions,
including the fault/loop probes and complete original JPEG decoding. Both
installed and preview targets are available; the installed check is included
in `make verify`.

The preceding 335 intervals and all other kernel bytes are unchanged;
665 kernel bytes change. Whole boot/application disk reconstruction and
inventory consistency pass. A production QEMU CLOCK boot passes rendering,
three changing complete frames, PS/2 close, task exit, CMD reuse, RAM/heap
checks and real task switching. All 336 loaded intervals match production;
the CLOCK screenshot was visually inspected.

Evidence: `build/jpeg_huffman_336_build.log`,
`build/jpeg_huffman_installed_summary.json`,
`build/jpeg_huffman_336_integrity_summary.json`,
`build/jpeg_huffman_336_boot.log` and `build/jpeg_huffman_336_evidence/`.
Kernel SHA-256:
`cfa3907c977e632a038af2737a90ed7a7088960c0e8447d2edc6a56dcbdee490`.
A full 336-interval `make verify` run is in progress in
`build/jpeg_huffman_336_verify.log`; the latest completed full recipe coverage
remains 326 at this checkpoint. Complete JPEG and OS recovery are unfinished.

### JPEG scan and symbol source checkpoint (2026-10-28)

`kernel/jpeg_scan.c` recovers the marker dispatcher at `0x83af27..0x83b233`
(738/780 bytes), scan preparation at `0x83b233..0x83b351` (241/286 bytes)
and Huffman symbol lookup at `0x83b351..0x83b41b` (144/202 bytes).
The dispatcher resets the original ten fields, invokes the original helper
boundaries, ignores their return values, reads restart intervals and SOS
selectors, and skips unknown segments by their raw length. It retains
unchecked trailing SOS-byte advancement and zero-length segment loops.
Preparation retains signed/live loop limits, overlapping component arrays,
first matching component IDs, untouched fields on missing IDs, wrapped MCU
dimensions, the 4,096-word fill, unchecked buffer-pointer writes and the
original raw EAX count. Symbol lookup captures the selected table, reads
one bit per length, scans length/code entries without consulting the count,
and returns the first matching byte or `UINT32_MAX` after sixteen lengths.

`python3 tests/kernel_jpeg_scan.py` passes 3,803 scenarios / 11,409
original/ordinary-C/fixed-address-C executions: 1,903 dispatcher, 488
preparation and 1,412 symbol scenarios. Tests compare all 128 KiB of context,
8 KiB of source, six incoming words, raw EAX and preserved registers/stack
canaries. Coverage includes every marker and symbol byte, four alignments,
truncated headers, 255-component scans, duplicate/missing IDs, signed counts,
source/context/caller aliases, every position of a complete 8-bit canonical
codebook, refill/stuffing/EOF and full-nibble table selectors. Forked probes
compare 39 fault, 12 alarm-terminated loop and 42 successful executions.
Fault probes compare termination, rather than claiming a memory oracle for
the faulting paths. No helper bodies are substituted.

Two complete real JPEG information/decode executions compare every output
pixel/slack byte and all 128 KiB of workspace. Evidence:
`build/jpeg_scan_preview_summary.json`, `build/jpeg_scan_preview_test.log`,
`build/jpeg_scan_original.disasm` and `build/jpeg_scan_candidate.x`.
Production remains at 336 intervals at this source checkpoint. The inverse
transform and remaining JPEG/OS routines are still unrecovered.

### JPEG cosine basis source checkpoint (2026-10-28)

`kernel/jpeg_basis.c` recovers `0x83b41b..0x83b653` in 331/568 bytes,
including its 32 cosine constants. It generates all 4,096 signed fixed-point
basis coefficients, retains the zero-frequency `cos(pi/4)` factor and
arithmetic truncation, and reloads the destination for every store. The
original raw EAX is the address of its final stack counter; the recovered
routine preserves that address using its frame pointer. It does not expose
a pointer to a C local whose lifetime has ended.

The expanded scan fixture now passes 3,827 scenarios / 11,481 executions,
including 24 basis cases. Sixteen destination offsets compare every basis
coefficient with an independent Python/libm cosine calculation. Guard-page
and incoming-word destinations compare original/C fault or success status;
the combined probes cover 60 fault, 12 alarm-terminated loop and 45 successful
executions. Complete real JPEG decoding still matches every output/slack
and workspace byte with no helper substitutions. Evidence is in the updated
`build/jpeg_scan_preview_summary.json` and `build/jpeg_basis_oracle.bin`.
Production remains at 336 intervals; transform accumulation and the remaining
JPEG/OS routines are still unrecovered.

### JPEG inverse accumulation and magnitude source checkpoint (2026-10-28)

`kernel/jpeg_transform.c` recovers inverse accumulation at
`0x83b653..0x83b73f` (141/236 bytes) and signed magnitude extension at
`0x83b73f..0x83b7ab` (61/108 bytes). Accumulation clears 64 outputs,
captures each nonzero coefficient, retains low-DWORD products/sums and
arithmetic right shifts by seventeen, and preserves live overlapping
source/output/basis accesses. Its raw EAX stack-counter address also matches.
Magnitude conversion keeps the reader's zero-extended WORD, live arguments,
zero-count fast path and x86-masked shifts for oversized counts.

The expanded scan fixture passes 5,083 scenarios / 15,249 executions,
including 580 transform and 676 magnitude scenarios. Single-frequency cases
compare with the independently generated cosine basis, including wrapped
extreme signed products. Dense/overlapping buffers and every input byte's
eight-bit sign extension also pass. Forked probes cover 72 faults, twelve
bounded loops and 105 successful executions; nineteen cases mutate incoming
words. Real JPEG information and complete decoding still compare every
pixel/slack and all workspace bytes with no helper substitutions. Evidence:
`build/jpeg_scan_preview_summary.json`, `build/jpeg_scan_preview_test.log`
and `build/jpeg_scan_original.disasm`. Production remains at 336 intervals;
block entropy decoding, output composition and remaining OS code are unfinished.

### JPEG entropy block source checkpoint (2026-10-28)

`kernel/jpeg_block.c` recovers `0x83b7ab..0x83b96d` in 350/450 bytes.
It captures the quantization table before symbol decoding, updates the live
DC predictor, and retains unchecked AC runs and caller-provided zigzag
indices. Missing DC symbols return without touching output; missing AC
symbols clear the remaining coefficients. Low-DWORD dequantization, zero
AC magnitudes and runs extending past coefficient 63 retain their original
behavior. Helper calls execute real original or recovered code.

The scan fixture now passes 5,859 scenarios / 17,577 executions, including
776 block scenarios. Independent cases check DC predictor multiplication
and one AC coefficient for every run nibble and magnitude size; additional
cases cover all 256 DC and AC symbol bytes and overlapping predictor,
quantization, bit-reader, Huffman, zigzag and caller destinations. The combined
forks compare 84 faults, twelve bounded loops and 165 successful executions.
Complete real JPEG information/decode still matches every pixel/slack and
workspace byte. Evidence: `build/jpeg_scan_preview_summary.json` and
`build/jpeg_scan_preview_test.log`. Production remains at 336 intervals;
MCU assembly, color conversion and the rest of the OS remain unfinished.

### JPEG component expansion and color source checkpoint (2026-10-28)

`kernel/jpeg_output.c` recovers nearest-neighbour component expansion at
`0x83b96d..0x83ba4f` (167/226 bytes) and YCbCr conversion at
`0x83bc15..0x83be59` (448/580 bytes). Expansion retains signed bounds and
division, wrapped addresses, live source/output overlap and mutations of
incoming destination/stride words. Color conversion retains its original
integer coefficients and saturation, edge clipping, output pitch and mode
selection. Four-byte output preserves the fourth byte; two-byte output
repeatedly reads the same component sample within a row, as the original does.

The fixture now uses eight incoming words to cover the seven-argument
expander and passes 6,655 scenarios / 19,965 executions. The new 468 expansion
and 328 color cases include independent nearest-neighbour/grayscale oracles,
signed dimensions, unaligned and overlapping buffers, unsupported modes,
extreme luma and caller destinations. All prior comparisons and complete
JPEG pixel/slack/workspace checks still pass. Forked probes compare 96
faults, twelve bounded loops and 225 successes; 763 cases change incoming
words. Evidence: `build/jpeg_scan_preview_summary.json` and
`build/jpeg_scan_preview_test.log`. Production remains at 336 intervals;
MCU assembly, public wrappers and remaining OS code are unfinished.

### 336-interval full verification checkpoint (2026-10-28)

The complete `make -j2 verify` invocation finishes with exit code zero at
336 intervals. Every prerequisite, native and installed comparison, QEMU
hardware check, application test, whole-disk reconstruction and inventory
check passes in one uninterrupted run. The kernel SHA-256 remains
`cfa3907c977e632a038af2737a90ed7a7088960c0e8447d2edc6a56dcbdee490`.
Evidence: `build/jpeg_huffman_336_verify.log` and the updated
`build/jpeg_huffman_336_integrity_summary.json`; the log is also preserved
in `build/jpeg_huffman_336_evidence/`. This advances complete recipe coverage
from 326 to 336 intervals. Full OS recovery is still unfinished.

### JPEG public entries and MCU traversal source checkpoint (2026-10-28)

`kernel/jpeg_decode.c` recovers information/decode entry points at
`0x83a70f..0x83a835`, MCU component assembly at `0x83ba4f..0x83bc15`
and image traversal at `0x83be59..0x83c02f`. Their linked sizes are
116/146, 112/148, 387/454 and 342/470 bytes, including traversal's zigzag
table. These retain signed/wrapped dimension and stride arithmetic,
ignored helper errors, the restart bit-count adjustment and predictor reset.
All seventeen routines in this JPEG decoder now have recovered sources;
integration is still pending at this checkpoint.

The expanded fixture passes 6,751 scenarios / 20,253 native executions,
including direct MCU, image traversal and information-entry comparisons.
It extracts all twelve original JPEG files directly from FAT16 and performs
108 complete information/decode executions: original, ordinary C and fixed
address C; four-byte output, two-byte output and flagged four-byte output
with thirteen extra row bytes. Dimensions range from 260-by-260 through
1920-by-1080. Every execution compares the full twelve-MiB output/slack
and 128-KiB workspace; no helper bodies are substituted. The preview still
uses original bodies for the four previously recovered JPEG routines;
the installed check will exercise their recovered versions together.

Malformed MCU paths that consume uninitialized local coefficients after an
early block error have not been established equivalent. Existing unchecked
memory, division and looping behavior is retained, with bounded fault probes
covering the tested cases. Evidence: `build/jpeg_scan_preview_summary.json`,
`build/jpeg_scan_preview_test.log`, `build/jpeg_corpus/` and
`build/jpeg_basis_oracle.bin`. Production remains at 336 intervals here;
the complete operating system remains unfinished.

### Seventeen-routine JPEG installation checkpoint (2026-10-28)

All thirteen new entries are installed alongside the four previously
recovered JPEG routines, bringing production to 349 declared intervals.
The seventeen contiguous JPEG slots cover `0x83a70f..0x83c02f`; every
routine and its constant tables are compiled from recovered sources.
`make test-jpeg-scan` passes all 6,751 scenarios / 20,253 executions and
108 complete information/decode executions across all twelve disk JPEGs
against the combined production kernel. The four earlier installed JPEG
fixtures also pass with their original counts and complete decoder checks.
`test-jpeg-scan-preview` and the installed `make verify` check are available.

All preceding 336 intervals and every other kernel byte remain unchanged;
4,385 kernel bytes change. Each production disk differs from the saved
336-interval disk only in its kernel. Entire boot/application disk
reconstruction and inventory checks pass. A production QEMU CLOCK boot
passes three changing complete frames, PS/2 close, task exit, CMD reuse,
RAM/heap and real task-switch checks. All 349 loaded intervals match
production; the CLOCK screenshot was visually inspected. Kernel and disk
hashes remain unchanged during validation and boot.

Evidence: `build/jpeg_complete_349_build.log`,
`build/jpeg_complete_349_validation.log`, `build/jpeg_scan_installed_summary.json`,
`build/jpeg_complete_349_boot.log`, `build/jpeg_complete_349_final_summary.json`
and `build/jpeg_complete_349_evidence/`. Kernel SHA-256:
`4fe8ecf13e88a193f200cbec53506659e498660da529a6c29e5bc3d771c77ed5`.
The latest full recipe run passed at 336 intervals; this checkpoint uses
focused installed regression and a recorded production boot. Malformed MCU
paths using uninitialized coefficients remain outside established equivalence.
Original boot/INIT/kernel/driver/application components still remain; all
five rebuilt applications are unchanged. Complete OS recovery is unfinished.

### Third-stage VBE source checkpoint (2026-10-28)

`boot/stage3_video.c` recovers the original real-mode selector at stage
offsets `0x189..0x2c3`. Assembly boundaries issue INT 10h and access the
ES=0 controller/mode buffers and mode table. Selection retains the original
32-bit enumeration, requested-mode sentinel `0x105`, last 1024-by-768
fallback, truncated table offsets, unchecked mode-set status and VGA mode
13h fallback. This source checkpoint is not yet installed into the boot disk.

`make test-boot-video-preview` compares 168 scenarios / 336 original and
compiled executions in Unicorn, including 40,428 ordered BIOS requests.
It compares every boot-header byte and all 64 KiB of low memory except
private stack scratch. Cases cover controller/version failures, missing and
invalid modes, final-query and mode-set failures, nonzero counters, table
wrap and randomized mode lists. INT 10h uses a deterministic model; real
firmware execution requires a separate QEMU integration check. Install the
pinned dependency with `python3 -m pip install --target build/test-deps -r
tests/requirements.txt`. Evidence is `build/boot_video_summary.json`.
The production stage still includes original code at this checkpoint.

### Source-built third-stage installation (2026-10-28)

Both eight-sector third-stage images now compile entirely from recovered
C/assembly. `boot/stage3_hybrid.S` retains its historical filename but no
longer uses `.incbin`. It defines the original low-memory header layout,
eleven GDT descriptors, A20 gate, protected-mode transition, CPUID/FPU/SSE
initialization and INIT transfer. `boot/stage3_display.c` packs the original
framebuffer descriptor and clears 8/32-bit displays. Existing recovered PCI,
ATA/FAT16 and INIT-header implementations complete the stage. ELF/map files
are retained for inspection and the linker enforces the 4,096-byte budget.
The real-mode stack is explicitly initialized and protected-mode execution
uses flat code/stack selectors; the original used a stage-relative code
selector and expand-down stack. These are intentional integration changes.

`make test-boot-video` compares the preview and both production-linked
selectors with the original: 168 scenarios / 672 executions. `make
test-boot-stage3` compares complete startup through the pre-PCI boundary:
27 scenarios / 81 executions, including actual GDT/CR0/CR4/segment operations,
VBE/VGA fallback and framebuffer clearing, SSE present/absent, small/large
dimensions and an unaligned framebuffer. It compares boot fields, GDTR,
CR0/CR4, ordered BIOS/port calls and eight MiB of memory. Private stacks,
stage code/data and descriptor accessed bits are excluded. BIOS/CPUID use
deterministic models; zero-pixel and unsupported-depth hangs are untested.
Both checks are included in `make verify` and use the pinned Unicorn package
in `tests/requirements.txt` (install into `build/test-deps`).

The source-built production disk reached the desktop in QEMU and passed
PS/2 CMD opening, `cpuid`, prompt reuse, RAM/heap checks and actual task-switch
checks. All 349 installed kernel intervals matched guest memory. The command
result screenshot was visually inspected. Evidence: `build/stage3_source_build.log`,
`build/stage3_source_boot.log`, `build/boot_video_installed_summary.json`,
`build/boot_stage3_summary.json` and `build/openicestaros_input_cpuid.png`. Original
INIT/kernel/driver/application components still remain; this removes the
third-stage binary dependency and does not complete OS recovery.

### Third-stage real-firmware matrix (2026-10-28)

`make test-boot-stage3-qemu` runs the production disk with real QEMU BIOS
and CPU devices. GDB stops at the stage's pre-PCI boundary and inspects the
boot header, every GDT descriptor, CR0/CR4 and the complete cleared
framebuffer. Four configurations pass: standard VGA with the normal
32-bit selection; standard VGA with requested mode `0x105` (8-bit);
Cirrus with its 8-bit fallback; and Pentium without SSE. The framebuffers
are 3,145,728 or 786,432 bytes at 1024-by-768. Both SSE control bits are
enabled only when CPUID reports SSE. All runs use snapshots and leave the
production disk unchanged. This fixture sets the requested mode at the
real-mode entry; it does not test keyboard navigation of the menu.

`tools/boot_smoke.py` additionally accepts `--menu-choice 1|2` and
`--vga std|cirrus`. A real PS/2 selection of menu entry 2 reaches the 8-bit
desktop, opens CMD, executes `cpuid` and returns to the prompt; all 349
loaded intervals match. A separate Cirrus boot also passes the same desktop,
PS/2 and CMD checks. Both screenshots were inspected. Evidence:
`build/boot_stage3_qemu_summary.json`, `build/boot_stage3_qemu_*.gdb.log`
and `build/stage3_source_evidence/{std32,std8,cirrus}/`.

### Complete source-built-boot regression (2026-10-28)

The uninterrupted `make verify` finishes with exit code 0 at 349 installed
kernel intervals and five rebuilt applications. This includes both new
boot differential checks and every existing native/fixed-address comparison,
QEMU hardware fixture, JPEG corpus comparison, application check, entire-disk
reconstruction and recovery inventory check. The separate four-configuration
real-BIOS matrix also passes. A production CLOCK boot passes three changing
complete frames, PS/2 close, task exit, CMD reuse, heap/RAM and real TSS-switch
checks; every loaded kernel interval matches. The CLOCK frame was inspected.
The kernel and all five application binaries remain identical to the
pre-boot-change baseline; production disk hashes remain stable throughout
verification and snapshot boots. The original VHD also retains its recorded
SHA-256.

Evidence is preserved in `build/stage3_source_evidence/`, including
`stage3_full_verify.log`, `stage3_clock_boot.log`, the three desktop/input
captures and hardware-matrix summaries. `build/stage3_source_integrity_summary.json`
records the final hashes and outcomes. Third-stage SHA-256:
`52523573d544a5b14327a49117dc7cef1c09237bf2591980eff692e1b1f8fbd4`.
Kernel SHA-256 remains
`4fe8ecf13e88a193f200cbec53506659e498660da529a6c29e5bc3d771c77ed5`.
This completes the third-stage source replacement checkpoint. Complete
recovery of INIT, the kernel, drivers and remaining applications is unfinished.

### X executable loader source checkpoint (2026-10-28)

`kernel/x_loader.c` now reconstructs the two X file-loading entry points and
the application launch/relocation/task-creation entry at their original code
boundaries. It retains type `0x0a` and `0x50` loading, original allocation
sizes/alignment, BSS/relocation-table/stack clearing, live relocation-table
reads, selectors, duplicate-load dialogs and cursor transitions. The public
X header, relocation and startup contract is documented in `tools/xformat.py`.
The path loader's stack record is the 32-byte FAT directory entry copied by
the original filesystem lookup. The three C bodies are installed in the
fixed-address production kernel image.

`make test-x-loader` links ordinary and fixed-address C and compares both with
original x86 in Unicorn: 430 scenarios / 1,290 executions. The installed-mode
test also verifies the compiled production bytes at all three original entry
addresses.
Coverage includes both loader formats, allocation and read branches, shared
header mutation, relocation table/target aliasing, duplicate launch, CPU
registers/stack and every byte of the checked low-memory/arena regions. The
fixture extracts every supported original `.X` program and runs loader/launch
cases for each file at aligned and unaligned bases. Filesystem, memory
allocation, task and UI helpers are deterministic callbacks. Invalid and
unmapped relocation targets are not executed. Evidence:
`build/x_loader_installed_summary.json` and
`build/x_loader_installed_test.log`.

### X loader integrated boot and full regression (2026-10-28)

The C loader is linked into `openicestaros_kernel.x`, and the combined production image
installs that kernel alongside the five rebuilt applications. The complete
`make verify` run passes, including the installed loader differential test,
all existing kernel and application checks, whole-kernel disk comparison,
application extent comparison, and inventory validation. The inventory now
records 352 kernel replacement intervals; this count does not measure semantic
completion.

`python3 tools/boot_smoke.py build/OpenIceStarOS_c_apps.vhd --cursor-check
--memory-check --task-check --input-check` boots the combined image in QEMU
snapshot mode. It opens CMD using PS/2, executes `cpuid`, launches the original
`DEVMGR.X` through the C X loader, verifies its resizable window and cursor
behavior, closes it, and confirms memory, scheduling, and loaded C kernel
intervals. Evidence: `build/x_loader_full_verify.log`,
`build/x_loader_installed_test.log`, `build/cursor_smoke.json`, and
`build/cursor_*.png`. The applications and drivers not listed as rebuilt in
the recovery inventory remain original binaries.

### OpenIceStarOS build names (2026-10-28)

The C boot menu, generated VHD names, candidate kernel name and source include
guards now use OpenIceStarOS. Original disk names, FAT paths and the extracted
`build/icestar.x` differential reference retain their compatibility names.
The root Makefile remains a byte-exact assembly reconstruction of the original
boot sectors, including its original menu; use `rewrite/Makefile` for the
OpenIceStarOS system.

The renamed combined image builds, and installed boot-video (168 scenarios,
672 executions), stage-three (27 scenarios, 81 executions) and X-loader
(430 scenarios, 1,290 executions) differential checks pass. Evidence:
`build/branding_names_validation.log`. This checkpoint covers build/source
names only: inherited CMD/kernel/taskbar/resource display strings still need
an explicit installation step. The previously interrupted renamed full-suite
run is not recorded as a pass.

### FAT16 file and program-header readers (2026-10-28)

Six recovered C functions now implement directory-record allocation/reading,
pathname read wrappers and relative/record program-header sector reads. The
production kernel contains 358 declared source intervals. Original/C/fixed-C
comparison passes 692 scenarios / 2,076 executions, including all 39 original
program paths with real FAT chains and disk-sector contents. Installed X-loader
regression, exact kernel/application disk verification, branding checks and
inventory checks pass. A fresh QEMU boot executes the new C readers while
loading the desktop, CMD and rebuilt CLOCK; all 358 slots match guest memory.
Read behavior, exceptional cases, observed/unobserved entries and validation
limits are recorded in `recovery/FAT_FILES.md`. The complete OS remains unfinished.

### Branding regression and retained logo (2026-10-28)

The complete renamed 352-interval `make verify` run finished with exit 0,
including both surface lifecycle variants, all remaining kernel/application
checks, exact disk verification and inventory validation. The original startup
logo is intentionally retained at the user's request; its experimental replacement
and generator were removed. See `branding/README.md` and
`evidence/2026-10-28-branding/full-352-verification.json` for the exception and
precise regression scope. The later 358-interval image reproduces the separately
validated FAT-reader build byte for byte; only its six added kernel intervals
differ from the fully regressed image. This does not complete OS recovery.

### Relative FAT16 readers (2026-10-29)

Six more original-only filesystem bodies now execute from recovered C: whole-
chain reads by directory index/name, offset-cluster reads by index/name, and
allocating reads by index/name. The integrated kernel contains 364 declared
intervals. Combined original/ordinary-C/fixed-C FAT tests pass 1,926 scenarios /
5,778 executions; installed X-loader, icon/bitmap callers, disk, branding and
inventory checks pass. A normal QEMU boot verifies CLOCK and all installed
slots. A separate test-only X uses real production service slots and disk I/O
to execute all six new entries, validating ten calls and 328,000 data/guard
bytes, including the original differing past-EOF behaviors. Only the six new
kernel intervals differ from the 358-interval baseline. See
[`recovery/FAT_RELATIVE.md`](recovery/FAT_RELATIVE.md) and the committed evidence
for scope and commands. This is focused validation, not a full 364-interval
`make verify` run. Five applications are rebuilt; INIT/kernel and 32 original
program paths still require further recovery.

### Absolute FAT paths and short names (2026-10-29)

Absolute file lookup and FAT 8.3 name conversion now execute from recovered C
at `0x8263c0` and `0x827bba`. Installed original/C comparison passes 347 scenarios /
1,041 executions with real directory/FAT traversal and all 67 original files;
only allocation/free and hardware sectors are intercepted. The production
kernel contains 366 declared intervals. Normal QEMU boot observes 47 path-lookup
entries and 119 short-name entries, with desktop, CMD/cpuid, CLOCK, RAM, task
switching and all installed slots verified. A second production boot runs and
closes unchanged original DEVMGR.X. FAT-reader/X-loader/caller regressions,
exact disk checks, branding and inventory pass. Only the two new kernel slots
differ from the 364-interval baseline. See
[`recovery/FAT_PATH.md`](recovery/FAT_PATH.md) for preserved parser/name/lifetime
quirks and the committed evidence. This focused check does not constitute a
full 366-interval regression or complete OS recovery.

### FAT directory-index and bounded-cluster allocation (2026-10-29)

Two more original-only services now execute from C at `0x82711e` and
`0x8272f4`: allocation/read by arbitrary directory base and signed index, and
allocation/read of a requested cluster count. The kernel contains 368 declared
intervals. Fourteen-reader original/ordinary-C/fixed-C comparisons pass 2,410
scenarios / 7,230 executions, including all 39 program paths and preserved
zero-count, overflow, exact FAT-sentinel and live-state behavior. A separate
QEMU diagnostic checks eight real service entries, 16 calls and 524,800
data/guard bytes; both new C bodies execute. Ordinary production boot verifies
CMD, C CLOCK, RAM, TSS switching and all installed intervals. Path/X-loader,
icon/bitmap caller, disk, branding and inventory checks pass. Exactly 786 bytes
change within the two new kernel slots; the original logo and other disk
contents are unchanged. See [`recovery/FAT_ALLOC.md`](recovery/FAT_ALLOC.md)
for commands, evidence and limits. A full 368-interval `make verify` was not
run; five applications are rebuilt, INIT/kernel are partial and 32 program
paths remain unrecovered.

### FAT directory queries and relative lookup (2026-10-29)

Ten more original-only directory services now execute from recovered C:
root/supplied-directory filename, extension, size and attribute getters, plus
both relative file lookups. The kernel contains 378 declared intervals.
Original/ordinary-C/fixed-C comparisons pass 5,974 scenarios / 17,922 executions,
covering 12 on-disk directories and 101 occupied records, all attribute values,
case-sensitive lookup, status/pointer returns, bounded aliases and live-state
changes. The separate QEMU diagnostic checks eighteen real service addresses,
26 calls and 525,120 data/guard bytes, including all ten new entries. Ordinary
boot verifies CMD, C CLOCK, RAM/TSS switching and all installed intervals;
reader/path/X-loader/caller, disk, branding and inventory checks pass. Only
1,479 bytes within the ten new kernel slots change from the previous image.
See [`recovery/FAT_DIRECTORY.md`](recovery/FAT_DIRECTORY.md) and committed
evidence for commands and preserved behavior. No full 378-interval `make verify`
was run; the complete OS remains unfinished, with five apps rebuilt,
kernel/INIT partial and 32 program paths unrecovered.

### Directory-only lookup and FAT timestamps (2026-10-29)

Seven more original-only entries now execute from recovered C: root and
supplied-buffer directory-only lookups, absolute directory lookup, and four
raw date/time getters. The kernel contains 385 declared intervals. Installed
original/ordinary-C/fixed-C comparisons pass 7,187 directory scenarios /
21,561 executions and 758 path scenarios / 2,274 executions, with existing
reader/X-loader/caller, exact disk, branding and inventory checks passing.
A separate QEMU diagnostic verifies all seven new entries within 25 service
addresses, 37 calls and 526,144 checked data/guard bytes. Ordinary boot
exercises root/absolute directory lookup and verifies CMD/cpuid, C CLOCK,
RAM/TSS switching and all installed intervals. Only 1,630 bytes in the seven
new kernel slots change; the original startup logo is retained. See
[`recovery/FAT_DIRECTORY_PATH.md`](recovery/FAT_DIRECTORY_PATH.md) for preserved
parser/free-before-copy behavior, commands, evidence and screenshots. A full
385-interval `make verify` was not run. Five apps are rebuilt; INIT/kernel
remain partial and 32 program paths are unrecovered. The complete OS recovery
remains unfinished.

### FAT free-slot and cluster scans (2026-10-31)

Four original-only FAT helpers now execute from recovered C at `0x82798c`,
`0x827a40`, `0x827ace`, and `0x827b44`: root and supplied-directory unused or
deleted-slot scans, followed by first-free-cluster scans from cluster 2 or
after a supplied cluster. The original two-pass directory rule, live BPB
entry bound, signed half-FAT limit, wrapping arithmetic, and `-1` exhaustion
result are preserved. The installed differential suite passes 208 scenarios /
624 executions across original x86, ordinary C, and fixed-address C. See
[`recovery/FAT_SLOTS.md`](recovery/FAT_SLOTS.md). The kernel now has 390
declared intervals; this is a focused recovery step and the complete OS
rewrite remains unfinished.

### FAT contiguous-sector writer (2026-10-31)

The contiguous writer at `0x827c72` is now recovered in `kernel/fat_io.c`.
It preserves the original wrapped end calculation, unsigned loop condition,
live bytes-per-sector stride, one-sector ATA write sequence, ignored hardware
return value, and cdecl contract. `tests/kernel_fat_io.py` compares original
x86, ordinary C, and fixed-address C across 127 scenarios and 381 executions;
the installed run passes and the 50-byte body fits its 81-byte slot. See
[`recovery/FAT_IO.md`](recovery/FAT_IO.md). The kernel now has 390 declared
intervals; INIT, drivers, and most applications remain original or partial,
so the complete OS rewrite remains unfinished.
