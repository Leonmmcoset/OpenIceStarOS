OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_init_global_state)
PROVIDE(kernel_handle_pointer_input = 0x801454);
PROVIDE(kernel_read_flags = 0x81a24a);
PROVIDE(kernel_disable_interrupts = 0x81a242);
PROVIDE(kernel_write_flags = 0x81a24d);
PROVIDE(kernel_read_port_byte = 0x81a254);
PROVIDE(kernel_write_port_byte = 0x81a261);
PROVIDE(kernel_physical_write_byte = 0x83735b);
PROVIDE(kernel_physical_read_byte = 0x8373a8);
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
    .event_loop 0x8012ca : SUBALIGN(1) {
        *kernel_poll.o(.text.kernel_event_loop)
    }
    ASSERT(kernel_event_loop == 0x8012ca, "event loop entry moved")
    ASSERT(ADDR(.event_loop) + SIZEOF(.event_loop) <= 0x8012dc, "event loop slot overflow")
    .poll_events 0x8012dc : SUBALIGN(1) {
        *kernel_poll.o(.text.kernel_poll_events)
    }
    ASSERT(kernel_poll_events == 0x8012dc, "poll entry moved")
    ASSERT(ADDR(.poll_events) + SIZEOF(.poll_events) <= 0x801372, "poll slot overflow")
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
    .pointer_input 0x801454 : SUBALIGN(1) {
        *kernel_pointer.o(.text.kernel_handle_pointer_input)
    }
    ASSERT(kernel_handle_pointer_input == 0x801454, "pointer input entry moved")
    ASSERT(ADDR(.pointer_input) + SIZEOF(.pointer_input) <= 0x801632, "pointer input slot overflow")
    .pointer_init 0x812c70 : SUBALIGN(1) {
        *kernel_pointer_init.o(.text.kernel_init_pointer)
    }
    ASSERT(kernel_init_pointer == 0x812c70, "pointer init entry moved")
    ASSERT(ADDR(.pointer_init) + SIZEOF(.pointer_init) <= 0x812e5e, "pointer init slot overflow")
    .ps2_ready 0x812e5e : SUBALIGN(1) {
        *kernel_pointer_init.o(.text.kernel_wait_ps2_ready)
    }
    ASSERT(kernel_wait_ps2_ready == 0x812e5e, "PS2 ready entry moved")
    ASSERT(ADDR(.ps2_ready) + SIZEOF(.ps2_ready) <= 0x812e7e, "PS2 ready slot overflow")
    .queue_write 0x812e7e : SUBALIGN(1) {
        *kernel_queue.o(.text.kernel_queue_write)
    }
    ASSERT(kernel_queue_write == 0x812e7e, "queue write entry moved")
    ASSERT(ADDR(.queue_write) + SIZEOF(.queue_write) <= 0x812f2c, "queue write slot overflow")
    .queue_read 0x812f2c : SUBALIGN(1) {
        *kernel_queue.o(.text.kernel_queue_read)
    }
    ASSERT(kernel_queue_read == 0x812f2c, "queue read entry moved")
    ASSERT(ADDR(.queue_read) + SIZEOF(.queue_read) <= 0x812f72, "queue read slot overflow")
    .queue_available 0x812f72 : SUBALIGN(1) {
        *kernel_queue.o(.text.kernel_queue_available)
    }
    ASSERT(kernel_queue_available == 0x812f72, "queue available entry moved")
    ASSERT(ADDR(.queue_available) + SIZEOF(.queue_available) <= 0x812f96, "queue available slot overflow")
    .pointer_decode 0x812f96 : SUBALIGN(1) {
        *kernel_pointer_decode.o(.text.kernel_decode_pointer)
    }
    ASSERT(kernel_decode_pointer == 0x812f96, "pointer decoder entry moved")
    ASSERT(ADDR(.pointer_decode) + SIZEOF(.pointer_decode) <= 0x8130cc, "pointer decoder slot overflow")
    .keyboard_ready 0x819c40 : SUBALIGN(1) {
        *kernel_keyboard_init.o(.text.kernel_wait_keyboard_ready)
    }
    ASSERT(kernel_wait_keyboard_ready == 0x819c40, "keyboard ready entry moved")
    ASSERT(ADDR(.keyboard_ready) + SIZEOF(.keyboard_ready) <= 0x819c60, "keyboard ready slot overflow")
    .keyboard_discard 0x819c60 : SUBALIGN(1) {
        *kernel_keyboard_init.o(.text.kernel_discard_keyboard_byte)
    }
    ASSERT(kernel_discard_keyboard_byte == 0x819c60, "keyboard discard entry moved")
    ASSERT(ADDR(.keyboard_discard) + SIZEOF(.keyboard_discard) <= 0x819c7c, "keyboard discard slot overflow")
    .keyboard_leds 0x819c7c : SUBALIGN(1) {
        *kernel_keyboard_init.o(.text.kernel_set_keyboard_leds)
    }
    ASSERT(kernel_set_keyboard_leds == 0x819c7c, "keyboard LEDs entry moved")
    ASSERT(ADDR(.keyboard_leds) + SIZEOF(.keyboard_leds) <= 0x819cf8, "keyboard LEDs slot overflow")
    .keyboard_init 0x819cf8 : SUBALIGN(1) {
        *kernel_keyboard_init.o(.text.kernel_init_keyboard)
    }
    ASSERT(kernel_init_keyboard == 0x819cf8, "keyboard init entry moved")
    ASSERT(ADDR(.keyboard_init) + SIZEOF(.keyboard_init) <= 0x819d96, "keyboard init slot overflow")
    .input_write 0x819d96 : SUBALIGN(1) {
        *kernel_input_queue.o(.text.kernel_input_write)
    }
    ASSERT(kernel_input_write == 0x819d96, "input write entry moved")
    ASSERT(ADDR(.input_write) + SIZEOF(.input_write) <= 0x819dfe, "input write slot overflow")
    .input_read 0x819dfe : SUBALIGN(1) {
        *kernel_input_queue.o(.text.kernel_input_read)
    }
    ASSERT(kernel_input_read == 0x819dfe, "input read entry moved")
    ASSERT(ADDR(.input_read) + SIZEOF(.input_read) <= 0x819e44, "input read slot overflow")
    .input_available 0x819e44 : SUBALIGN(1) {
        *kernel_input_queue.o(.text.kernel_input_available)
    }
    ASSERT(kernel_input_available == 0x819e44, "input available entry moved")
    ASSERT(ADDR(.input_available) + SIZEOF(.input_available) <= 0x819e68, "input available slot overflow")
    .ascii_upper 0x819e68 : SUBALIGN(1) {
        *kernel_input_queue.o(.text.kernel_ascii_upper)
    }
    ASSERT(kernel_ascii_upper == 0x819e68, "ASCII upper entry moved")
    ASSERT(ADDR(.ascii_upper) + SIZEOF(.ascii_upper) <= 0x819e90, "ASCII upper slot overflow")
    .unexpected : { *(.text* .rodata* .data* .bss* COMMON) }
    ASSERT(SIZEOF(.unexpected) == 0, "unexpected allocated kernel data")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
