OUTPUT_FORMAT(binary)
ENTRY(_start)
SECTIONS
{
    . = 0x7c00;
    .text : { *(.start) *(.text*) *(.rodata*) }
    ASSERT(. <= 0x7dbe, "boot code exceeds partition table")
    . = 0x7dbe;
    .partition : { KEEP(*(.partition)) }
    . = 0x7dfe;
    .signature : { KEEP(*(.signature)) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) }
}
