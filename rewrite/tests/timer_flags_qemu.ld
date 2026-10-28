OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS {
 . = 0x100000;
 .multiboot : { KEEP(*(.multiboot)) }
 .text : { *(.text.entry) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 .candidate 0x800000 : { KEEP(*(.candidate)) }
 .original 0x900000 : { KEEP(*(.original)) }
 /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
