OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_init_global_state)
PROVIDE(kernel_handle_pointer_input = 0x801454);
PROVIDE(kernel_format_text = 0x81f88a);
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
    .yield_task 0x81434a : { *kernel_tasks.o(.text.kernel_yield_task) }
    ASSERT(kernel_yield_task == 0x81434a && ADDR(.yield_task) + SIZEOF(.yield_task) <= 0x814536, "task yield overflow")
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
    .keyboard_translate 0x819e90 : SUBALIGN(1) {
        *kernel_keyboard.o(.text.kernel_translate_scancode)
    }
    ASSERT(kernel_translate_scancode == 0x819e90, "keyboard translate entry moved")
    ASSERT(ADDR(.keyboard_translate) + SIZEOF(.keyboard_translate) <= 0x819f9c, "keyboard translate slot overflow")
    .keyboard_drain 0x819f9c : SUBALIGN(1) {
        *kernel_keyboard.o(.text.kernel_drain_keyboard)
    }
    ASSERT(kernel_drain_keyboard == 0x819f9c, "keyboard drain entry moved")
    ASSERT(ADDR(.keyboard_drain) + SIZEOF(.keyboard_drain) <= 0x819fde, "keyboard drain slot overflow")
    .keyboard_state 0x819fde : SUBALIGN(1) {
        *kernel_keyboard.o(.text.kernel_update_keyboard_state)
    }
    ASSERT(kernel_update_keyboard_state == 0x819fde, "keyboard state entry moved")
    ASSERT(ADDR(.keyboard_state) + SIZEOF(.keyboard_state) <= 0x81a1aa, "keyboard state slot overflow")
    .keyboard_character 0x81a1aa : SUBALIGN(1) {
        *kernel_keyboard.o(.text.kernel_read_draw_character)
    }
    ASSERT(kernel_read_draw_character == 0x81a1aa, "keyboard character entry moved")
    ASSERT(ADDR(.keyboard_character) + SIZEOF(.keyboard_character) <= 0x81a240, "keyboard character slot overflow")
    .cpu_halt 0x81a240 : { *kernel_cpu.o(.text.kernel_halt) }
    ASSERT(kernel_halt == 0x81a240 && SIZEOF(.cpu_halt) == 2, "halt layout changed")
    .cpu_cli 0x81a242 : { *kernel_cpu.o(.text.kernel_disable_interrupts) }
    ASSERT(kernel_disable_interrupts == 0x81a242 && SIZEOF(.cpu_cli) == 2, "CLI layout changed")
    .cpu_sti 0x81a244 : { *kernel_cpu.o(.text.kernel_enable_interrupts) }
    ASSERT(kernel_enable_interrupts == 0x81a244 && SIZEOF(.cpu_sti) == 2, "STI layout changed")
    .cpu_delay 0x81a246 : { *kernel_cpu.o(.text.kernel_delay_three_nops) }
    ASSERT(kernel_delay_three_nops == 0x81a246 && SIZEOF(.cpu_delay) == 4, "NOP layout changed")
    .cpu_flags_read 0x81a24a : { *kernel_cpu.o(.text.kernel_read_flags) }
    ASSERT(kernel_read_flags == 0x81a24a && SIZEOF(.cpu_flags_read) == 3, "read flags layout changed")
    .cpu_flags_write 0x81a24d : { *kernel_cpu.o(.text.kernel_write_flags) }
    ASSERT(kernel_write_flags == 0x81a24d && SIZEOF(.cpu_flags_write) == 7, "write flags layout changed")
    .port_read_byte 0x81a254 : { *kernel_cpu.o(.text.kernel_read_port_byte) }
    ASSERT(kernel_read_port_byte == 0x81a254 && SIZEOF(.port_read_byte) == 13, "IN byte layout changed")
    .port_write_byte 0x81a261 : { *kernel_cpu.o(.text.kernel_write_port_byte) }
    ASSERT(kernel_write_port_byte == 0x81a261 && SIZEOF(.port_write_byte) == 12, "OUT byte layout changed")
    .port_read_word 0x81a26d : { *kernel_cpu.o(.text.kernel_read_port_word) }
    ASSERT(kernel_read_port_word == 0x81a26d && SIZEOF(.port_read_word) == 14, "IN word layout changed")
    .port_write_word 0x81a27b : { *kernel_cpu.o(.text.kernel_write_port_word) }
    ASSERT(kernel_write_port_word == 0x81a27b && SIZEOF(.port_write_word) == 14, "OUT word layout changed")
    .port_read_dword 0x81a289 : { *kernel_cpu.o(.text.kernel_read_port_dword) }
    ASSERT(kernel_read_port_dword == 0x81a289 && SIZEOF(.port_read_dword) == 13, "IN DWORD layout changed")
    .port_write_dword 0x81a296 : { *kernel_cpu.o(.text.kernel_write_port_dword) }
    ASSERT(kernel_write_port_dword == 0x81a296 && SIZEOF(.port_write_dword) == 12, "OUT DWORD layout changed")
    .cpu_boot_jump 0x81a2a2 : { *kernel_cpu.o(.text.kernel_jump_boot_entry) }
    ASSERT(kernel_jump_boot_entry == 0x81a2a2 && SIZEOF(.cpu_boot_jump) == 8, "boot jump layout changed")
    .palette_color 0x81b714 : { *kernel_runtime.o(.text.kernel_palette_color) }
    ASSERT(kernel_palette_color == 0x81b714 && ADDR(.palette_color) + SIZEOF(.palette_color) <= 0x81b734, "palette getter overflow")
    .far_task_jump 0x828d2b : { *kernel_cpu.o(.text.kernel_far_task_jump) }
    ASSERT(kernel_far_task_jump == 0x828d2b && SIZEOF(.far_task_jump) == 5, "far task jump layout changed")
    .wait_ticks 0x82ad81 : { *kernel_runtime.o(.text.kernel_wait_ticks) }
    ASSERT(kernel_wait_ticks == 0x82ad81 && ADDR(.wait_ticks) + SIZEOF(.wait_ticks) <= 0x82adab, "tick wait overflow")
    .fpu_reset 0x837347 : { *kernel_cpu.o(.text.kernel_fpu_reset) }
    ASSERT(kernel_fpu_reset == 0x837347 && SIZEOF(.fpu_reset) == 3, "FPU initialization layout changed")
    .physical_write_byte 0x83735b : { *kernel_physical.o(.text.kernel_physical_write_byte) }
    ASSERT(kernel_physical_write_byte == 0x83735b && SIZEOF(.physical_write_byte) == 25, "physical byte write layout changed")
    .physical_write_word 0x837374 : { *kernel_physical.o(.text.kernel_physical_write_word) }
    ASSERT(kernel_physical_write_word == 0x837374 && SIZEOF(.physical_write_word) == 27, "physical word write layout changed")
    .physical_write_dword 0x83738f : { *kernel_physical.o(.text.kernel_physical_write_dword) }
    ASSERT(kernel_physical_write_dword == 0x83738f && SIZEOF(.physical_write_dword) == 25, "physical DWORD write layout changed")
    .physical_read_byte 0x8373a8 : { *kernel_physical.o(.text.kernel_physical_read_byte) }
    ASSERT(kernel_physical_read_byte == 0x8373a8 && SIZEOF(.physical_read_byte) == 23, "physical byte read layout changed")
    .physical_read_word 0x8373bf : { *kernel_physical.o(.text.kernel_physical_read_word) }
    ASSERT(kernel_physical_read_word == 0x8373bf && SIZEOF(.physical_read_word) == 24, "physical word read layout changed")
    .physical_read_dword 0x8373d7 : { *kernel_physical.o(.text.kernel_physical_read_dword) }
    ASSERT(kernel_physical_read_dword == 0x8373d7 && SIZEOF(.physical_read_dword) == 23, "physical DWORD read layout changed")
    .cpu_cr0_read 0x83743c : { *kernel_cpu.o(.text.kernel_read_cr0) }
    ASSERT(kernel_read_cr0 == 0x83743c && SIZEOF(.cpu_cr0_read) == 4, "read CR0 layout changed")
    .cpu_cr0_write 0x837440 : { *kernel_cpu.o(.text.kernel_write_cr0) }
    ASSERT(kernel_write_cr0 == 0x837440 && SIZEOF(.cpu_cr0_write) == 8, "write CR0 layout changed")
    .service_00 0x8374cf : { *kernel_service_forward.o(.text.kernel_service_00) }
    ASSERT(kernel_service_00 == 0x8374cf && ADDR(.service_00) + SIZEOF(.service_00) <= 0x837501, "service 00 overflow")
    .service_01 0x837501 : { *kernel_service_forward.o(.text.kernel_service_01) }
    ASSERT(kernel_service_01 == 0x837501 && ADDR(.service_01) + SIZEOF(.service_01) <= 0x837545, "service 01 overflow")
    .service_02 0x837545 : { *kernel_service_forward.o(.text.kernel_service_02) }
    ASSERT(kernel_service_02 == 0x837545 && ADDR(.service_02) + SIZEOF(.service_02) <= 0x837589, "service 02 overflow")
    .service_03 0x837589 : { *kernel_service_forward.o(.text.kernel_service_03) }
    ASSERT(kernel_service_03 == 0x837589 && ADDR(.service_03) + SIZEOF(.service_03) <= 0x8375cd, "service 03 overflow")
    .service_04 0x8375cd : { *kernel_service_forward.o(.text.kernel_service_04) }
    ASSERT(kernel_service_04 == 0x8375cd && ADDR(.service_04) + SIZEOF(.service_04) <= 0x837609, "service 04 overflow")
    .service_05 0x837609 : { *kernel_service_forward.o(.text.kernel_service_05) }
    ASSERT(kernel_service_05 == 0x837609 && ADDR(.service_05) + SIZEOF(.service_05) <= 0x837645, "service 05 overflow")
    .service_06 0x837645 : { *kernel_service_forward.o(.text.kernel_service_06) }
    ASSERT(kernel_service_06 == 0x837645 && ADDR(.service_06) + SIZEOF(.service_06) <= 0x83768f, "service 06 overflow")
    .service_07 0x83768f : { *kernel_service_forward.o(.text.kernel_service_07) }
    ASSERT(kernel_service_07 == 0x83768f && ADDR(.service_07) + SIZEOF(.service_07) <= 0x8376d3, "service 07 overflow")
    .service_08 0x8376d3 : { *kernel_service_forward.o(.text.kernel_service_08) }
    ASSERT(kernel_service_08 == 0x8376d3 && ADDR(.service_08) + SIZEOF(.service_08) <= 0x837717, "service 08 overflow")
    .service_09 0x837717 : { *kernel_service_forward.o(.text.kernel_service_09) }
    ASSERT(kernel_service_09 == 0x837717 && ADDR(.service_09) + SIZEOF(.service_09) <= 0x837761, "service 09 overflow")
    .service_0a 0x837761 : { *kernel_service_forward.o(.text.kernel_service_0a) }
    ASSERT(kernel_service_0a == 0x837761 && ADDR(.service_0a) + SIZEOF(.service_0a) <= 0x83779d, "service 0a overflow")
    .service_0b 0x83779d : { *kernel_service_forward.o(.text.kernel_service_0b) }
    ASSERT(kernel_service_0b == 0x83779d && ADDR(.service_0b) + SIZEOF(.service_0b) <= 0x8377d3, "service 0b overflow")
    .service_0c 0x8377d3 : { *kernel_service_forward.o(.text.kernel_service_0c) }
    ASSERT(kernel_service_0c == 0x8377d3 && ADDR(.service_0c) + SIZEOF(.service_0c) <= 0x83781d, "service 0c overflow")
    .service_0d 0x83781d : { *kernel_service_forward.o(.text.kernel_service_0d) }
    ASSERT(kernel_service_0d == 0x83781d && ADDR(.service_0d) + SIZEOF(.service_0d) <= 0x837861, "service 0d overflow")
    .service_0e 0x837861 : { *kernel_service_forward.o(.text.kernel_service_0e) }
    ASSERT(kernel_service_0e == 0x837861 && ADDR(.service_0e) + SIZEOF(.service_0e) <= 0x83789d, "service 0e overflow")
    .service_0f 0x83789d : { *kernel_service_forward.o(.text.kernel_service_0f) }
    ASSERT(kernel_service_0f == 0x83789d && ADDR(.service_0f) + SIZEOF(.service_0f) <= 0x8378bd, "service 0f overflow")
    .service_10 0x8378bd : { *kernel_service_forward.o(.text.kernel_service_10) }
    ASSERT(kernel_service_10 == 0x8378bd && ADDR(.service_10) + SIZEOF(.service_10) <= 0x8378dd, "service 10 overflow")
    .service_11 0x8378dd : { *kernel_service_forward.o(.text.kernel_service_11) }
    ASSERT(kernel_service_11 == 0x8378dd && ADDR(.service_11) + SIZEOF(.service_11) <= 0x83790b, "service 11 overflow")
    .service_12 0x83790b : { *kernel_service_forward.o(.text.kernel_service_12) }
    ASSERT(kernel_service_12 == 0x83790b && ADDR(.service_12) + SIZEOF(.service_12) <= 0x837939, "service 12 overflow")
    .service_13 0x837939 : { *kernel_service_forward.o(.text.kernel_service_13) }
    ASSERT(kernel_service_13 == 0x837939 && ADDR(.service_13) + SIZEOF(.service_13) <= 0x837967, "service 13 overflow")
    .unexpected : { *(.text* .rodata* .data* .bss* COMMON) }
    ASSERT(SIZEOF(.unexpected) == 0, "unexpected allocated kernel data")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
