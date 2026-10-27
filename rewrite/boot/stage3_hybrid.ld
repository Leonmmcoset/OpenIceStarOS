OUTPUT_FORMAT(binary)
ENTRY(stage_flat)
SECTIONS
{
    . = 0x1a800;
    .stage : { *(.stage) }
    .text : { *(.text*) *(.rodata*) }
    ASSERT(. <= 0x1b800, "hybrid third stage exceeds eight sectors")
    . = 0x1b7ff;
    .pad : { BYTE(0) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) }
}
