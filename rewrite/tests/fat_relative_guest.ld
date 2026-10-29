OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(fat_guest_entry)
SECTIONS
{
    .text 0x40 : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    ASSERT(. <= 0x1000, "FAT diagnostic initialized payload overflow")
    .bss 0x1000 (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
