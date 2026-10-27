OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(color_entry)
SECTIONS
{
    .text 0x40 : { *(.text*) }
    ASSERT(. <= 0x3890, "recovered COLOR code exceeds initialized area")
    .rodata 0x3890 : { *(.rodata*) }
    ASSERT(color_title == 0x3890, "COLOR title layout changed")
    .bss 0x3898 (NOLOAD) : { *(.bss.color) }
    ASSERT(SIZEOF(.bss) == 16, "COLOR memory contract changed")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
