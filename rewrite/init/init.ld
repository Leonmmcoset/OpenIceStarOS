OUTPUT_FORMAT(binary)
ENTRY(init_entry)
SECTIONS
{
    . = 0x60000;
    .text : { *(.text.init_entry) *(.text*) *(.rodata*) }
    ASSERT(. <= 0x60800, "rewritten INIT entry exceeds four sectors")
    . = 0x607ff;
    .pad : { BYTE(0) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) }
}
