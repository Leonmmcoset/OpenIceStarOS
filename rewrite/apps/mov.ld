OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(mov_entry)
SECTIONS
{
    .text 0x40 : { *(.text.mov_entry) *(.text*) }
    ASSERT(. <= 0x3990, "recovered MOV code exceeds initialized area")
    .rodata 0x3990 : { *(.rodata.mov) *(.rodata*) }
    ASSERT(mov_strings == 0x3990, "MOV string layout changed")
    ASSERT(SIZEOF(.rodata) == 129, "MOV strings changed")
    .bss 0x3a14 (NOLOAD) : { *(.bss.mov) }
    ASSERT(SIZEOF(.bss) == 16, "MOV memory contract changed")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
