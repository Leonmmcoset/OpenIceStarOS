OUTPUT_FORMAT(binary)
ENTRY(menu_entry)
SECTIONS
{
    . = 0;
    .header : { KEEP(*(.header)) }
    .text : { *(.start) *(.text*) *(.rodata*) }
    ASSERT(. <= 1536, "menu exceeds the three sectors loaded by the MBR")
    . = 1535;
    .pad : { BYTE(0) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.data*) *(.bss*) }
}
