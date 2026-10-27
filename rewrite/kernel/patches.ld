OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_init_global_state)
SECTIONS
{
    .state 0x800104 : SUBALIGN(1) {
        *kernel_state.o(.text.kernel_init_global_state)
        *kernel_state.o(.text* .rodata*)
    }
    ASSERT(kernel_init_global_state == 0x800104, "state entry moved")
    ASSERT(ADDR(.state) + SIZEOF(.state) <= 0x80049a, "state slot overflow")
    .reset 0x80049a : SUBALIGN(1) {
        *kernel_reset.o(.text.kernel_init_startup_record)
        *kernel_reset.o(.text* .rodata*)
    }
    ASSERT(kernel_init_startup_record == 0x80049a, "reset entry moved")
    ASSERT(ADDR(.reset) + SIZEOF(.reset) <= 0x8004fa, "reset slot overflow")
    .api 0x8004fa : SUBALIGN(1) {
        *kernel_api_table.o(.text.kernel_init_api_table)
        *kernel_api_table.o(.text* .rodata*)
    }
    ASSERT(kernel_init_api_table == 0x8004fa, "API entry moved")
    ASSERT(ADDR(.api) + SIZEOF(.api) <= 0x8005aa, "API slot overflow")
    .services 0x8005aa : SUBALIGN(1) {
        *kernel_service_table.o(.text.kernel_init_service_table)
        *kernel_service_table.o(.text* .rodata*)
    }
    ASSERT(kernel_init_service_table == 0x8005aa, "service entry moved")
    ASSERT(ADDR(.services) + SIZEOF(.services) <= 0x80129e, "service slot overflow")
    .unexpected : { *(.text* .rodata* .data* .bss* COMMON) }
    ASSERT(SIZEOF(.unexpected) == 0, "unexpected allocated kernel data")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
