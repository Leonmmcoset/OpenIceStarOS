OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
    . = 0x400000;
    .multiboot : { KEEP(*(.multiboot)) }
    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    ASSERT(. < 0x6e0000, "firmware fixture overlaps its stack")
    .rebuilt 0x800000 : { *(.rebuilt) }
    .original 0x900000 : { *(.original) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
