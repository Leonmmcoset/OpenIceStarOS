# C rewrite

This directory is an incomplete C recovery integrated into copies of the
original VHD. The current build installs 222 declared kernel intervals and
four rebuilt applications. It still requires unrecovered original components;
the interval count is not a count of completely recovered operating-system
modules. The chronological notes below record each source/integration and
verification checkpoint.

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

`make run-c-snake` boots `build/IceStarOS_c_snake.vhd`, which installs the
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

`make run-c-apps` boots `build/IceStarOS_c_apps.vhd` with recovered Snake,
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
`build/IceStarOS_scale_diagnostic.vhd`, by replacing only the existing
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
