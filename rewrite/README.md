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
