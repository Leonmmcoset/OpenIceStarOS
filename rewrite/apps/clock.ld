OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(clock_entry)
SECTIONS
{
    .text 0x40 : { *(.text.clock_entry) *(.text*) }
    .rodata : { *(EXCLUDE_FILE (*clock.o) .rodata*) *(.rodata.cst*) }
    ASSERT(. <= 0x3c30, "recovered CLOCK code exceeds initialized area")
    .clock_data 0x3c30 : { *(.rodata.clock) }
    ASSERT(SIZEOF(.clock_data) == 64, "CLOCK initialized data changed")
    .bss 0x3c70 (NOLOAD) : { *(.bss.clock) }
    ASSERT(SIZEOF(.bss) == 16, "CLOCK memory contract changed")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
