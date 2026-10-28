OUTPUT_FORMAT(elf32-i386)
ENTRY(stage_real_entry)
SECTIONS
{
    . = 0x1a800;
    .stage : { *(.stage) }
    .text : { *(.text*) *(.rodata*) }
    ASSERT(. <= 0x1b7ff, "source-built third stage exceeds eight sectors")
    . = 0x1b7ff;
    .pad : { BYTE(0) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) }
}
