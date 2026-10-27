OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(starscr_entry)
SECTIONS
{
    .text 0x40 : { *(.text.starscr_entry) *(.text*) }
    ASSERT(. <= 0x3dd0, "recovered STARSCR code exceeds initialized area")
    .rodata 0x3dd0 : { *(.rodata.starscr) *(.rodata*) }
    ASSERT(starscr_title == 0x3dd0, "STARSCR title layout changed")
    ASSERT(SIZEOF(.rodata) == 12, "STARSCR initialized data changed")
    .bss 0x3ddc (NOLOAD) : { *(.bss.starscr) }
    ASSERT(SIZEOF(.bss) == 16, "STARSCR memory contract changed")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
