OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(snake_entry)
SECTIONS
{
    .text 0x40 : { *(.text*) }
    .rodata : { *(.rodata*) }
    ASSERT(. <= 0x92c, "recovered Snake code exceeds original initialized area")
    .snake_length 0x92c : { *(.snake_length) }
    .snake_strings 0x93c : { *(.snake_strings) }
    ASSERT(SIZEOF(.snake_length) == 4, "Snake length layout changed")
    ASSERT(SIZEOF(.snake_strings) == 67, "Snake strings layout changed")
    .bss 0x980 (NOLOAD) : { *(.bss.snake) }
    ASSERT(SIZEOF(.bss) == 0x380, "Snake BSS layout changed")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
