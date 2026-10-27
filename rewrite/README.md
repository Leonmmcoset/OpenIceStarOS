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
compiled Snake into its existing FAT16 cluster chain. `apps/images.json`
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
