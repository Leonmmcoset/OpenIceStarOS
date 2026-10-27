# IceStarOS reverse engineering

The input is a fixed VHD with an MBR and a FAT16 partition starting at LBA 17.
The first MBR stage reads sectors 2 to 4 using BIOS CHS addressing, then jumps
through a far pointer at offset 4 in the loaded second stage. The recovered
first stage is in `boot/mbr.asm`. The second stage is in `boot/menu.asm`;
it displays the boot menu, handles keyboard input and an RTC countdown,
and reads four sectors at CHS 0/0/6 into 1a80:0000 for the next stage.

Run `make verify` to assemble both stages, construct a copy of the disk with the
assembled sectors, and compare both stages and the complete image against
the input. Run `make run` to start that copy in QEMU with a temporary disk
snapshot. NASM, make, and qemu-system-i386 are required.

This is a partial recovery. The subsequent boot stage, kernel, drivers, and applications
in the rebuilt image still come from the original VHD; they have not been
decompiled or rebuilt. The image comparison verifies only the recovered stages
and the copying procedure, not source recovery of the remaining components.

The extracted filesystem contains 39 `.x` and `.dx` programs/drivers. Their
headers are not recognized by `file`; `init.x` and `icestar.x` have 64-byte
headers followed by x86 instructions. The precise format and loader contract
still need to be established before those programs can be reconstructed.

## Next stage observations

The boot menu loads four sectors starting at disk offset `0xa00` (LBA 5) to
`1a80:0000` and jumps through the far pointer at offset 4. The pointer targets
`1a80:007b`. The code at `0x7b` is 16-bit; it queries VESA video modes, builds a
GDT, enables A20 and protected mode, and jumps to 32-bit code at offset `0x2c3`.
The protected-mode code reads FAT16 metadata and searches for `INIT    X  `;
its ATA PIO sector reader begins at offset `0x4bd`, and its FAT file reader at
`0x5f2`. It loads `INIT.X` to physical address `0x70000`.

The stage's leading size field is `0x806`, while the menu requests only 2048
bytes (`0x800`). The last six bytes, at disk offset `0x1200`, therefore lie
outside that read. The original image still reaches the desktop in the tested
QEMU configuration, but this discrepancy needs investigation before changing
or claiming full recovery of the stage.

At offset `0x47d`, the stage sets `INIT.X` header fields at offsets `0x08`,
`0x14` and `0x18` to selectors `0x38`, `0x40` and `0x38`, respectively.
It sets the stack pointer to load address + file size (header offset `0x04`)
+ 4096 * stack pages (offset `0x0c`) + offset `0x2c` - 1, and far-jumps via
the offset and selector at header offset `0x10`. `INIT.X` starts at `0x70040`.
It clears `0x1b108`, loads `ICESTAR/DAT.BIN` to `0x1b000` and
`ICESTAR/ICESTAR.X` to `0x800000`, then calls the address at `0x800010` only
if both loads succeeded. It loops forever if either fails or the call returns.
