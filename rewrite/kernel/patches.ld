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
    .queue_request 0x80129e : SUBALIGN(1) {
        *kernel_events.o(.text.kernel_queue_api_request)
    }
    ASSERT(kernel_queue_api_request == 0x80129e, "queue entry moved")
    ASSERT(ADDR(.queue_request) + SIZEOF(.queue_request) <= 0x8012ca, "queue slot overflow")
    .dispatch_event 0x801372 : SUBALIGN(1) {
        *kernel_events.o(.text.kernel_dispatch_event)
    }
    ASSERT(kernel_dispatch_event == 0x801372, "event entry moved")
    ASSERT(ADDR(.dispatch_event) + SIZEOF(.dispatch_event) <= 0x8013aa, "event slot overflow")
    .dispatch_request 0x8013aa : SUBALIGN(1) {
        *kernel_events.o(.text.kernel_dispatch_api_request)
    }
    ASSERT(kernel_dispatch_api_request == 0x8013aa, "request entry moved")
    ASSERT(ADDR(.dispatch_request) + SIZEOF(.dispatch_request) <= 0x8013f4, "request slot overflow")
    .dispatch_slot1 0x8013f4 : SUBALIGN(1) {
        *kernel_events.o(.text.kernel_dispatch_slot1)
    }
    ASSERT(kernel_dispatch_slot1 == 0x8013f4, "slot 1 entry moved")
    ASSERT(ADDR(.dispatch_slot1) + SIZEOF(.dispatch_slot1) <= 0x801414, "slot 1 overflow")
    .dispatch_slot2 0x801414 : SUBALIGN(1) {
        *kernel_events.o(.text.kernel_dispatch_slot2)
    }
    ASSERT(kernel_dispatch_slot2 == 0x801414, "slot 2 entry moved")
    ASSERT(ADDR(.dispatch_slot2) + SIZEOF(.dispatch_slot2) <= 0x801434, "slot 2 overflow")
    .dispatch_slot3 0x801434 : SUBALIGN(1) {
        *kernel_events.o(.text.kernel_dispatch_slot3)
    }
    ASSERT(kernel_dispatch_slot3 == 0x801434, "slot 3 entry moved")
    ASSERT(ADDR(.dispatch_slot3) + SIZEOF(.dispatch_slot3) <= 0x801454, "slot 3 overflow")
    .unexpected : { *(.text* .rodata* .data* .bss* COMMON) }
    ASSERT(SIZEOF(.unexpected) == 0, "unexpected allocated kernel data")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
