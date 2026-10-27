OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_init_global_state)
PROVIDE(kernel_handle_pointer_input = 0x801454);
PROVIDE(kernel_format_text = 0x81f88a);
PROVIDE(kernel_syscall_target_8082c6 = 0x8082c6);
PROVIDE(kernel_syscall_target_808378 = 0x808378);
PROVIDE(kernel_syscall_target_81021c = 0x81021c);
PROVIDE(kernel_syscall_target_81017c = 0x81017c);
PROVIDE(kernel_syscall_target_81041a = 0x81041a);
PROVIDE(kernel_syscall_target_807a0a = 0x807a0a);
PROVIDE(kernel_syscall_target_810478 = 0x810478);
PROVIDE(kernel_syscall_target_810448 = 0x810448);
PROVIDE(kernel_syscall_target_82a9bb = 0x82a9bb);
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
    .interrupts_init 0x813320 : { *kernel_interrupts.o(.text.kernel_init_interrupts) }
    ASSERT(kernel_init_interrupts == 0x813320 && ADDR(.interrupts_init) + SIZEOF(.interrupts_init) <= 0x8135c6, "interrupts_init layout overflow")
    .segment_descriptor 0x8135c6 : { *kernel_segments.o(.text.kernel_set_segment_descriptor) }
    ASSERT(kernel_set_segment_descriptor == 0x8135c6 && ADDR(.segment_descriptor) + SIZEOF(.segment_descriptor) <= 0x81364c, "segment descriptor overflow")
    .timer_tick 0x813a5e : { *kernel_tasks.o(.text.kernel_timer_tick) }
    ASSERT(kernel_timer_tick == 0x813a5e && ADDR(.timer_tick) + SIZEOF(.timer_tick) <= 0x813a8c, "timer tick overflow")
    .rtc_callback 0x813a8c : { *kernel_interrupt_callbacks.o(.text.kernel_rtc_callback) }
    ASSERT(kernel_rtc_callback == 0x813a8c && ADDR(.rtc_callback) + SIZEOF(.rtc_callback) <= 0x813aa6, "RTC callback overflow")
    .keyboard_callback 0x813aa6 : { *kernel_interrupt_callbacks.o(.text.kernel_keyboard_callback) }
    ASSERT(kernel_keyboard_callback == 0x813aa6 && ADDR(.keyboard_callback) + SIZEOF(.keyboard_callback) <= 0x813ae6, "keyboard callback overflow")
    .audio_callback 0x813ae6 : { *kernel_interrupt_callbacks.o(.text.kernel_audio_callback) }
    ASSERT(kernel_audio_callback == 0x813ae6 && ADDR(.audio_callback) + SIZEOF(.audio_callback) <= 0x813b2a, "audio callback overflow")
    .unused_irq_callback 0x813b2a : { *kernel_interrupt_callbacks.o(.text.kernel_unused_irq_callback) }
    ASSERT(kernel_unused_irq_callback == 0x813b2a && ADDR(.unused_irq_callback) + SIZEOF(.unused_irq_callback) <= 0x813b30, "unused IRQ callback overflow")
    .mouse_callback 0x813b30 : { *kernel_interrupt_callbacks.o(.text.kernel_mouse_callback) }
    ASSERT(kernel_mouse_callback == 0x813b30 && ADDR(.mouse_callback) + SIZEOF(.mouse_callback) <= 0x813b70, "mouse callback overflow")
    .dispatch_syscall40 0x813b70 : { *kernel_syscalls.o(.text.kernel_dispatch_syscall40) }
    ASSERT(kernel_dispatch_syscall40 == 0x813b70 && ADDR(.dispatch_syscall40) + SIZEOF(.dispatch_syscall40) <= 0x81406c, "syscall 0x40 dispatcher overflow")
    .dispatch_syscall41 0x81406c : { *kernel_syscalls.o(.text.kernel_dispatch_syscall41) }
    ASSERT(kernel_dispatch_syscall41 == 0x81406c && ADDR(.dispatch_syscall41) + SIZEOF(.dispatch_syscall41) <= 0x814176, "syscall 0x41 dispatcher overflow")
    .schedule_task 0x814176 : SUBALIGN(1) {
        *kernel_tasks.o(.text.kernel_schedule_task)
        *kernel_tasks.o(.text.timer_tick_body)
    }
    ASSERT(kernel_schedule_task == 0x814176 && ADDR(.schedule_task) + SIZEOF(.schedule_task) <= 0x81434a, "timer task switch overflow")
    .yield_task 0x81434a : { *kernel_tasks.o(.text.kernel_yield_task) }
    ASSERT(kernel_yield_task == 0x81434a && ADDR(.yield_task) + SIZEOF(.yield_task) <= 0x814536, "task yield overflow")
    .tasks_init 0x814536 : { *kernel_task_create.o(.text.kernel_init_tasks) }
    ASSERT(kernel_init_tasks == 0x814536 && ADDR(.tasks_init) + SIZEOF(.tasks_init) <= 0x814606, "task init overflow")
    .task_find_free 0x814606 : { *kernel_task_create.o(.text.kernel_find_free_task) }
    ASSERT(kernel_find_free_task == 0x814606 && ADDR(.task_find_free) + SIZEOF(.task_find_free) <= 0x814688, "task free-slot search overflow")
    .task_create 0x814688 : { *kernel_task_create.o(.text.kernel_create_task) }
    ASSERT(kernel_create_task == 0x814688 && ADDR(.task_create) + SIZEOF(.task_create) <= 0x814a56, "task creation overflow")
    .task_create_app 0x814a56 : { *kernel_task_lifecycle.o(.text.kernel_create_application_task) }
    ASSERT(kernel_create_application_task == 0x814a56 && ADDR(.task_create_app) + SIZEOF(.task_create_app) <= 0x814e54, "application task creation overflow")
    .task_delete 0x814e54 : { *kernel_task_lifecycle.o(.text.kernel_delete_task) }
    ASSERT(kernel_delete_task == 0x814e54 && ADDR(.task_delete) + SIZEOF(.task_delete) <= 0x815002, "task deletion overflow")
    .task_create_thread 0x815002 : { *kernel_task_lifecycle.o(.text.kernel_create_thread) }
    ASSERT(kernel_create_thread == 0x815002 && ADDR(.task_create_thread) + SIZEOF(.task_create_thread) <= 0x8150aa, "thread creation overflow")
    .task_active 0x8150aa : { *kernel_task_lifecycle.o(.text.kernel_task_is_active) }
    ASSERT(kernel_task_is_active == 0x8150aa && ADDR(.task_active) + SIZEOF(.task_active) <= 0x815106, "task state query overflow")
    .memory_probe 0x815320 : { *kernel_memory_init.o(.text.kernel_probe_memory) }
    ASSERT(kernel_probe_memory == 0x815320 && ADDR(.memory_probe) + SIZEOF(.memory_probe) <= 0x8153d4, "memory probe overflow")
    .heap_init 0x8153d4 : { *kernel_memory_init.o(.text.kernel_init_heap) }
    ASSERT(kernel_init_heap == 0x8153d4 && ADDR(.heap_init) + SIZEOF(.heap_init) <= 0x815464, "heap initialization overflow")
    .allocate 0x815464 : { *kernel_memory.o(.text.kernel_allocate) }
    ASSERT(kernel_allocate == 0x815464 && ADDR(.allocate) + SIZEOF(.allocate) <= 0x8155ca, "allocation overflow")
    .allocate_owner 0x8155ca : { *kernel_memory.o(.text.kernel_allocate_for_task) }
    ASSERT(kernel_allocate_for_task == 0x8155ca && ADDR(.allocate_owner) + SIZEOF(.allocate_owner) <= 0x815728, "owner allocation overflow")
    .allocate_aligned 0x815728 : { *kernel_memory.o(.text.kernel_allocate_aligned) }
    ASSERT(kernel_allocate_aligned == 0x815728 && ADDR(.allocate_aligned) + SIZEOF(.allocate_aligned) <= 0x815978, "aligned allocation overflow")
    .free_allocation 0x815978 : { *kernel_memory.o(.text.kernel_free_allocation) }
    ASSERT(kernel_free_allocation == 0x815978 && ADDR(.free_allocation) + SIZEOF(.free_allocation) <= 0x815a0a, "allocation free overflow")
    .allocation_size 0x815a0a : { *kernel_memory.o(.text.kernel_allocation_size) }
    ASSERT(kernel_allocation_size == 0x815a0a && ADDR(.allocation_size) + SIZEOF(.allocation_size) <= 0x815a42, "allocation size overflow")
    .allocation_total 0x815a42 : { *kernel_memory.o(.text.kernel_task_allocation_total) }
    ASSERT(kernel_task_allocation_total == 0x815a42 && ADDR(.allocation_total) + SIZEOF(.allocation_total) <= 0x815a90, "allocation total overflow")
    .move_bytes 0x815a90 : { *kernel_memory_ops.o(.text.kernel_move_bytes) }
    ASSERT(kernel_move_bytes == 0x815a90 && ADDR(.move_bytes) + SIZEOF(.move_bytes) <= 0x815b02, "byte move overflow")
    .fill_bytes 0x815b02 : { *kernel_memory_ops.o(.text.kernel_fill_bytes) }
    ASSERT(kernel_fill_bytes == 0x815b02 && ADDR(.fill_bytes) + SIZEOF(.fill_bytes) <= 0x815b2c, "byte fill overflow")
    .copy_1200 0x815b2c : { *kernel_memory_ops.o(.text.kernel_copy_1200_bytes) }
    ASSERT(kernel_copy_1200_bytes == 0x815b2c && ADDR(.copy_1200) + SIZEOF(.copy_1200) <= 0x817322, "1200-byte copy overflow")
    .copy_dwords 0x817322 : { *kernel_memory_ops.o(.text.kernel_copy_dwords) }
    ASSERT(kernel_copy_dwords == 0x817322 && ADDR(.copy_dwords) + SIZEOF(.copy_dwords) <= 0x8173da, "DWORD copy overflow")
    .copy_dwords_sse 0x8173da : { *kernel_memory_accel.o(.text.kernel_copy_dwords_sse) }
    ASSERT(kernel_copy_dwords_sse == 0x8173da && ADDR(.copy_dwords_sse) + SIZEOF(.copy_dwords_sse) <= 0x817484, "SSE DWORD dispatcher overflow")
    .rep_copy_bytes 0x817484 : { *kernel_memory_transfers.o(.text.kernel_rep_copy_bytes) }
    ASSERT(kernel_rep_copy_bytes == 0x817484 && SIZEOF(.rep_copy_bytes) == 30, "REP byte copy layout changed")
    .rep_fill_bytes 0x8174a2 : { *kernel_memory_transfers.o(.text.kernel_rep_fill_bytes) }
    ASSERT(kernel_rep_fill_bytes == 0x8174a2 && SIZEOF(.rep_fill_bytes) == 38, "REP byte fill layout changed")
    .rep_copy_words 0x8174c8 : { *kernel_memory_transfers.o(.text.kernel_rep_copy_words) }
    ASSERT(kernel_rep_copy_words == 0x8174c8 && SIZEOF(.rep_copy_words) == 32, "REP word copy layout changed")
    .rep_fill_words 0x8174e8 : { *kernel_memory_transfers.o(.text.kernel_rep_fill_words) }
    ASSERT(kernel_rep_fill_words == 0x8174e8 && SIZEOF(.rep_fill_words) == 40, "REP word fill layout changed")
    .rep_copy_dwords 0x817510 : { *kernel_memory_transfers.o(.text.kernel_rep_copy_dwords) }
    ASSERT(kernel_rep_copy_dwords == 0x817510 && SIZEOF(.rep_copy_dwords) == 30, "REP DWORD copy layout changed")
    .rep_fill_dwords 0x81752e : { *kernel_memory_transfers.o(.text.kernel_rep_fill_dwords) }
    ASSERT(kernel_rep_fill_dwords == 0x81752e && SIZEOF(.rep_fill_dwords) == 26, "REP DWORD fill layout changed")
    .memory_differs 0x817548 : { *kernel_memory_ops.o(.text.kernel_memory_differs) }
    ASSERT(kernel_memory_differs == 0x817548 && ADDR(.memory_differs) + SIZEOF(.memory_differs) <= 0x817590, "memory comparison overflow")
    .malloc 0x817590 : { *kernel_memory_ops.o(.text.kernel_malloc) }
    ASSERT(kernel_malloc == 0x817590 && ADDR(.malloc) + SIZEOF(.malloc) <= 0x8175a4, "allocation wrapper overflow")
    .free 0x8175a4 : { *kernel_memory_ops.o(.text.kernel_free) }
    ASSERT(kernel_free == 0x8175a4 && ADDR(.free) + SIZEOF(.free) <= 0x8175c0, "free wrapper overflow")
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
    .cpuid_available 0x82b678 : { *kernel_cpuid.o(.text.kernel_cpuid_available) }
    ASSERT(kernel_cpuid_available == 0x82b678 && ADDR(.cpuid_available) + SIZEOF(.cpuid_available) <= 0x82b6a0, "cpuid_available overflow")
    .cpuid_vendor 0x82b6a0 : { *kernel_cpuid.o(.text.kernel_cpuid_vendor) }
    ASSERT(kernel_cpuid_vendor == 0x82b6a0 && ADDR(.cpuid_vendor) + SIZEOF(.cpuid_vendor) <= 0x82b6e8, "cpuid_vendor overflow")
    .cpuid_features 0x82b6e8 : { *kernel_cpuid.o(.text.kernel_cpuid_features) }
    ASSERT(kernel_cpuid_features == 0x82b6e8 && ADDR(.cpuid_features) + SIZEOF(.cpuid_features) <= 0x82b72e, "cpuid_features overflow")
    .palette_color 0x81b714 : { *kernel_runtime.o(.text.kernel_palette_color) }
    ASSERT(kernel_palette_color == 0x81b714 && ADDR(.palette_color) + SIZEOF(.palette_color) <= 0x81b734, "palette getter overflow")
    .far_task_jump 0x828d2b : { *kernel_cpu.o(.text.kernel_far_task_jump) }
    ASSERT(kernel_far_task_jump == 0x828d2b && SIZEOF(.far_task_jump) == 5, "far task jump layout changed")
    .copy_sse16 0x828e8f : { *kernel_memory_transfers.o(.text.kernel_copy_sse16_blocks) }
    ASSERT(kernel_copy_sse16_blocks == 0x828e8f && SIZEOF(.copy_sse16) == 34, "SSE 16-byte copy layout changed")
    .copy_sse64 0x828eb1 : { *kernel_memory_transfers.o(.text.kernel_copy_sse64_blocks) }
    ASSERT(kernel_copy_sse64_blocks == 0x828eb1 && SIZEOF(.copy_sse64) == 58, "SSE 64-byte copy layout changed")
    .wait_vertical_retrace 0x828f0f : { *kernel_graphics_primitives.o(.text.kernel_wait_vertical_retrace) }
    ASSERT(kernel_wait_vertical_retrace == 0x828f0f && SIZEOF(.wait_vertical_retrace) == 12, "VGA status polling layout changed")
    .blend_color 0x828f1b : { *kernel_graphics_primitives.o(.text.kernel_blend_color) }
    ASSERT(kernel_blend_color == 0x828f1b && ADDR(.blend_color) + SIZEOF(.blend_color) <= 0x829003, "color blending overflow")
    .string_copy 0x82a32b : { *kernel_strings.o(.text.kernel_copy_string) }
    ASSERT(kernel_copy_string == 0x82a32b && ADDR(.string_copy) + SIZEOF(.string_copy) <= 0x82a36d, "string copy overflow")
    .text_prefix 0x82a36d : { *kernel_strings.o(.text.kernel_copy_text_prefix) }
    ASSERT(kernel_copy_text_prefix == 0x82a36d && ADDR(.text_prefix) + SIZEOF(.text_prefix) <= 0x82a401, "text prefix copy overflow")
    .string_limit 0x82a401 : { *kernel_strings.o(.text.kernel_copy_string_limit) }
    ASSERT(kernel_copy_string_limit == 0x82a401 && ADDR(.string_limit) + SIZEOF(.string_limit) <= 0x82a45d, "bounded string copy overflow")
    .string_length 0x82a45d : { *kernel_strings.o(.text.kernel_string_length) }
    ASSERT(kernel_string_length == 0x82a45d && ADDR(.string_length) + SIZEOF(.string_length) <= 0x82a481, "string length overflow")
    .string_equal 0x82a481 : { *kernel_strings.o(.text.kernel_strings_equal) }
    ASSERT(kernel_strings_equal == 0x82a481 && ADDR(.string_equal) + SIZEOF(.string_equal) <= 0x82a4bb, "string equality overflow")
    .string_append_character 0x82a4bb : { *kernel_strings.o(.text.kernel_append_character) }
    ASSERT(kernel_append_character == 0x82a4bb && ADDR(.string_append_character) + SIZEOF(.string_append_character) <= 0x82a4fb, "character append overflow")
    .string_append 0x82a4fb : { *kernel_strings.o(.text.kernel_append_string) }
    ASSERT(kernel_append_string == 0x82a4fb && ADDR(.string_append) + SIZEOF(.string_append) <= 0x82a531, "string append overflow")
    .wait_ticks 0x82ad81 : { *kernel_runtime.o(.text.kernel_wait_ticks) }
    ASSERT(kernel_wait_ticks == 0x82ad81 && ADDR(.wait_ticks) + SIZEOF(.wait_ticks) <= 0x82adab, "tick wait overflow")
    .read_cmos_clock 0x82ae84 : { *kernel_cmos_clock.o(.text.kernel_read_cmos_clock) }
    ASSERT(kernel_read_cmos_clock == 0x82ae84 && ADDR(.read_cmos_clock) + SIZEOF(.read_cmos_clock) == 0x82af08, "CMOS clock layout mismatch")
    .interrupt_gate 0x8370d2 : { *kernel_interrupt_hardware.o(.text.kernel_install_interrupt_gate) }
    ASSERT(kernel_install_interrupt_gate == 0x8370d2 && ADDR(.interrupt_gate) + SIZEOF(.interrupt_gate) == 0x837131, "interrupt_gate layout mismatch")
    .boot_code_selector 0x837131 : { *kernel_interrupt_hardware.o(.text.kernel_boot_code_selector) }
    ASSERT(kernel_boot_code_selector == 0x837131 && ADDR(.boot_code_selector) + SIZEOF(.boot_code_selector) == 0x837143, "boot_code_selector layout mismatch")
    .pic_init 0x837143 : { *kernel_interrupt_hardware.o(.text.kernel_init_pic) }
    ASSERT(kernel_init_pic == 0x837143 && ADDR(.pic_init) + SIZEOF(.pic_init) == 0x83717c, "pic_init layout mismatch")
    .pit_init 0x83717c : { *kernel_interrupt_hardware.o(.text.kernel_init_pit) }
    ASSERT(kernel_init_pit == 0x83717c && ADDR(.pit_init) + SIZEOF(.pit_init) == 0x837189, "pit_init layout mismatch")
    .rtc_init 0x837189 : { *kernel_interrupt_hardware.o(.text.kernel_init_rtc) }
    ASSERT(kernel_init_rtc == 0x837189 && ADDR(.rtc_init) + SIZEOF(.rtc_init) == 0x8371a0, "rtc_init layout mismatch")
    .exception_entry 0x8371a0 : { *kernel_interrupt_entries.o(.text.kernel_exception_entry) }
    ASSERT(kernel_exception_entry == 0x8371a0 && ADDR(.exception_entry) + SIZEOF(.exception_entry) == 0x8371bc, "exception_entry layout mismatch")
    .spurious_entry 0x8371bc : { *kernel_interrupt_entries.o(.text.kernel_spurious_entry) }
    ASSERT(kernel_spurious_entry == 0x8371bc && ADDR(.spurious_entry) + SIZEOF(.spurious_entry) == 0x8371c6, "spurious_entry layout mismatch")
    .exception_fs_entry 0x8371c6 : { *kernel_interrupt_entries.o(.text.kernel_exception_fs_entry) }
    ASSERT(kernel_exception_fs_entry == 0x8371c6 && ADDR(.exception_fs_entry) + SIZEOF(.exception_fs_entry) == 0x8371e6, "exception_fs_entry layout mismatch")
    .device_unavailable_entry 0x8371e6 : { *kernel_interrupt_entries.o(.text.kernel_device_unavailable_entry) }
    ASSERT(kernel_device_unavailable_entry == 0x8371e6 && ADDR(.device_unavailable_entry) + SIZEOF(.device_unavailable_entry) == 0x8371e9, "device_unavailable_entry layout mismatch")
    .timer_entry 0x8371e9 : { *kernel_interrupt_entries.o(.text.kernel_timer_entry) }
    ASSERT(kernel_timer_entry == 0x8371e9 && ADDR(.timer_entry) + SIZEOF(.timer_entry) == 0x837209, "timer_entry layout mismatch")
    .keyboard_entry 0x837209 : { *kernel_interrupt_entries.o(.text.kernel_keyboard_entry) }
    ASSERT(kernel_keyboard_entry == 0x837209 && ADDR(.keyboard_entry) + SIZEOF(.keyboard_entry) == 0x837222, "keyboard_entry layout mismatch")
    .irq5_entry 0x837222 : { *kernel_interrupt_entries.o(.text.kernel_irq5_entry) }
    ASSERT(kernel_irq5_entry == 0x837222 && ADDR(.irq5_entry) + SIZEOF(.irq5_entry) == 0x83723b, "irq5_entry layout mismatch")
    .rtc_entry 0x83723b : { *kernel_interrupt_entries.o(.text.kernel_rtc_entry) }
    ASSERT(kernel_rtc_entry == 0x83723b && ADDR(.rtc_entry) + SIZEOF(.rtc_entry) == 0x83725a, "rtc_entry layout mismatch")
    .irq9_entry 0x83725a : { *kernel_interrupt_entries.o(.text.kernel_irq9_entry) }
    ASSERT(kernel_irq9_entry == 0x83725a && ADDR(.irq9_entry) + SIZEOF(.irq9_entry) == 0x837273, "irq9_entry layout mismatch")
    .irq10_entry 0x837273 : { *kernel_interrupt_entries.o(.text.kernel_irq10_entry) }
    ASSERT(kernel_irq10_entry == 0x837273 && ADDR(.irq10_entry) + SIZEOF(.irq10_entry) == 0x83728c, "irq10_entry layout mismatch")
    .irq11_entry 0x83728c : { *kernel_interrupt_entries.o(.text.kernel_irq11_entry) }
    ASSERT(kernel_irq11_entry == 0x83728c && ADDR(.irq11_entry) + SIZEOF(.irq11_entry) == 0x8372a5, "irq11_entry layout mismatch")
    .irq12_entry 0x8372a5 : { *kernel_interrupt_entries.o(.text.kernel_irq12_entry) }
    ASSERT(kernel_irq12_entry == 0x8372a5 && ADDR(.irq12_entry) + SIZEOF(.irq12_entry) == 0x8372be, "irq12_entry layout mismatch")
    .syscall40_entry 0x8372be : { *kernel_interrupt_entries.o(.text.kernel_syscall40_entry) }
    ASSERT(kernel_syscall40_entry == 0x8372be && ADDR(.syscall40_entry) + SIZEOF(.syscall40_entry) == 0x8372d1, "syscall40_entry layout mismatch")
    .syscall41_entry 0x8372d1 : { *kernel_interrupt_entries.o(.text.kernel_syscall41_entry) }
    ASSERT(kernel_syscall41_entry == 0x8372d1 && ADDR(.syscall41_entry) + SIZEOF(.syscall41_entry) == 0x8372e8, "syscall41_entry layout mismatch")
    .cpu_load_gdt 0x8372e8 : { *kernel_cpu.o(.text.kernel_load_gdt) }
    ASSERT(kernel_load_gdt == 0x8372e8 && ADDR(.cpu_load_gdt) + SIZEOF(.cpu_load_gdt) <= 0x8372f8, "LGDT boundary overflow")
    .cpu_load_tr 0x8372f8 : { *kernel_cpu.o(.text.kernel_load_task_register) }
    ASSERT(kernel_load_task_register == 0x8372f8 && ADDR(.cpu_load_tr) + SIZEOF(.cpu_load_tr) <= 0x8372fe, "LTR boundary overflow")
    .fpu_save_legacy 0x83730a : { *kernel_fpu_state.o(.text.kernel_fpu_save_legacy) }
    ASSERT(kernel_fpu_save_legacy == 0x83730a && SIZEOF(.fpu_save_legacy) == 7, "fpu_save_legacy layout overflow")
    .fpu_restore_legacy 0x837311 : { *kernel_fpu_state.o(.text.kernel_fpu_restore_legacy) }
    ASSERT(kernel_fpu_restore_legacy == 0x837311 && SIZEOF(.fpu_restore_legacy) == 7, "fpu_restore_legacy layout overflow")
    .fpu_save_environment 0x837318 : { *kernel_fpu_state.o(.text.kernel_fpu_save_environment) }
    ASSERT(kernel_fpu_save_environment == 0x837318 && SIZEOF(.fpu_save_environment) == 8, "fpu_save_environment layout overflow")
    .fpu_load_environment 0x837320 : { *kernel_fpu_state.o(.text.kernel_fpu_load_environment) }
    ASSERT(kernel_fpu_load_environment == 0x837320 && SIZEOF(.fpu_load_environment) == 7, "fpu_load_environment layout overflow")
    .fpu_save_extended 0x837327 : { *kernel_fpu_state.o(.text.kernel_fpu_save_extended) }
    ASSERT(kernel_fpu_save_extended == 0x837327 && SIZEOF(.fpu_save_extended) == 8, "fpu_save_extended layout overflow")
    .fpu_restore_extended 0x83732f : { *kernel_fpu_state.o(.text.kernel_fpu_restore_extended) }
    ASSERT(kernel_fpu_restore_extended == 0x83732f && SIZEOF(.fpu_restore_extended) == 8, "fpu_restore_extended layout overflow")
    .fpu_save_xstate 0x837337 : { *kernel_fpu_state.o(.text.kernel_fpu_save_xstate) }
    ASSERT(kernel_fpu_save_xstate == 0x837337 && SIZEOF(.fpu_save_xstate) == 8, "fpu_save_xstate layout overflow")
    .fpu_restore_xstate 0x83733f : { *kernel_fpu_state.o(.text.kernel_fpu_restore_xstate) }
    ASSERT(kernel_fpu_restore_xstate == 0x83733f && SIZEOF(.fpu_restore_xstate) == 8, "fpu_restore_xstate layout overflow")
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
    .page_probe 0x8373ee : { *kernel_memory_probe.o(.text.kernel_probe_pages) }
    ASSERT(kernel_probe_pages == 0x8373ee && SIZEOF(.page_probe) == 78, "physical page test layout changed")
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
    .optional_14 0x837967 : { *kernel_service_optional.o(.text.kernel_optional_14) }
    ASSERT(kernel_optional_14 == 0x837967 && ADDR(.optional_14) + SIZEOF(.optional_14) <= 0x83799b, "optional service 14 overflow")
    .optional_15 0x83799b : { *kernel_service_optional.o(.text.kernel_optional_15) }
    ASSERT(kernel_optional_15 == 0x83799b && ADDR(.optional_15) + SIZEOF(.optional_15) <= 0x8379dd, "optional service 15 overflow")
    .optional_16 0x8379dd : { *kernel_service_optional.o(.text.kernel_optional_16) }
    ASSERT(kernel_optional_16 == 0x8379dd && ADDR(.optional_16) + SIZEOF(.optional_16) <= 0x837a17, "optional service 16 overflow")
    .optional_17 0x837a17 : { *kernel_service_optional.o(.text.kernel_optional_17) }
    ASSERT(kernel_optional_17 == 0x837a17 && ADDR(.optional_17) + SIZEOF(.optional_17) <= 0x837a4b, "optional service 17 overflow")
    .optional_18 0x837a4b : { *kernel_service_optional.o(.text.kernel_optional_18) }
    ASSERT(kernel_optional_18 == 0x837a4b && ADDR(.optional_18) + SIZEOF(.optional_18) <= 0x837a77, "optional service 18 overflow")
    .optional_19 0x837a77 : { *kernel_service_optional.o(.text.kernel_optional_19) }
    ASSERT(kernel_optional_19 == 0x837a77 && ADDR(.optional_19) + SIZEOF(.optional_19) <= 0x837aab, "optional service 19 overflow")
    .optional_1a 0x837aab : { *kernel_service_optional.o(.text.kernel_optional_1a) }
    ASSERT(kernel_optional_1a == 0x837aab && ADDR(.optional_1a) + SIZEOF(.optional_1a) <= 0x837aed, "optional service 1a overflow")
    .optional_1b 0x837aed : { *kernel_service_optional.o(.text.kernel_optional_1b) }
    ASSERT(kernel_optional_1b == 0x837aed && ADDR(.optional_1b) + SIZEOF(.optional_1b) <= 0x837b1f, "optional service 1b overflow")
    .optional_1c 0x837b1f : { *kernel_service_optional.o(.text.kernel_optional_1c) }
    ASSERT(kernel_optional_1c == 0x837b1f && ADDR(.optional_1c) + SIZEOF(.optional_1c) <= 0x837b53, "optional service 1c overflow")
    .optional_1d 0x837b53 : { *kernel_service_optional.o(.text.kernel_optional_1d) }
    ASSERT(kernel_optional_1d == 0x837b53 && ADDR(.optional_1d) + SIZEOF(.optional_1d) <= 0x837b87, "optional service 1d overflow")
    .optional_1e 0x837b87 : { *kernel_service_optional.o(.text.kernel_optional_1e) }
    ASSERT(kernel_optional_1e == 0x837b87 && ADDR(.optional_1e) + SIZEOF(.optional_1e) <= 0x837bb3, "optional service 1e overflow")
    .optional_1f 0x837bb3 : { *kernel_service_optional.o(.text.kernel_optional_1f) }
    ASSERT(kernel_optional_1f == 0x837bb3 && ADDR(.optional_1f) + SIZEOF(.optional_1f) <= 0x837bdf, "optional service 1f overflow")
    .optional_20 0x837bdf : { *kernel_service_optional.o(.text.kernel_optional_20) }
    ASSERT(kernel_optional_20 == 0x837bdf && ADDR(.optional_20) + SIZEOF(.optional_20) <= 0x837c0b, "optional service 20 overflow")
    .optional_21 0x837c0b : { *kernel_service_optional.o(.text.kernel_optional_21) }
    ASSERT(kernel_optional_21 == 0x837c0b && ADDR(.optional_21) + SIZEOF(.optional_21) <= 0x837c51, "optional service 21 overflow")
    .optional_22 0x837c51 : { *kernel_service_optional.o(.text.kernel_optional_22) }
    ASSERT(kernel_optional_22 == 0x837c51 && ADDR(.optional_22) + SIZEOF(.optional_22) <= 0x837c9f, "optional service 22 overflow")
    .optional_23 0x837c9f : { *kernel_service_optional.o(.text.kernel_optional_23) }
    ASSERT(kernel_optional_23 == 0x837c9f && ADDR(.optional_23) + SIZEOF(.optional_23) <= 0x837ce5, "optional service 23 overflow")
    .optional_24 0x837ce5 : { *kernel_service_optional.o(.text.kernel_optional_24) }
    ASSERT(kernel_optional_24 == 0x837ce5 && ADDR(.optional_24) + SIZEOF(.optional_24) <= 0x837d25, "optional service 24 overflow")
    .optional_25 0x837d25 : { *kernel_service_optional.o(.text.kernel_optional_25) }
    ASSERT(kernel_optional_25 == 0x837d25 && ADDR(.optional_25) + SIZEOF(.optional_25) <= 0x837d5d, "optional service 25 overflow")
    .optional_26 0x837d5d : { *kernel_service_optional.o(.text.kernel_optional_26) }
    ASSERT(kernel_optional_26 == 0x837d5d && ADDR(.optional_26) + SIZEOF(.optional_26) <= 0x837d9d, "optional service 26 overflow")
    .optional_27 0x837d9d : { *kernel_service_optional.o(.text.kernel_optional_27) }
    ASSERT(kernel_optional_27 == 0x837d9d && ADDR(.optional_27) + SIZEOF(.optional_27) <= 0x837ddd, "optional service 27 overflow")
    .optional_28 0x837ddd : { *kernel_service_optional.o(.text.kernel_optional_28) }
    ASSERT(kernel_optional_28 == 0x837ddd && ADDR(.optional_28) + SIZEOF(.optional_28) <= 0x837e1d, "optional service 28 overflow")
    .optional_29 0x837e1d : { *kernel_service_optional.o(.text.kernel_optional_29) }
    ASSERT(kernel_optional_29 == 0x837e1d && ADDR(.optional_29) + SIZEOF(.optional_29) <= 0x837e63, "optional service 29 overflow")
    .optional_2a 0x837e63 : { *kernel_service_optional.o(.text.kernel_optional_2a) }
    ASSERT(kernel_optional_2a == 0x837e63 && ADDR(.optional_2a) + SIZEOF(.optional_2a) <= 0x837ecd, "optional service 2a overflow")
    .optional_2b 0x837ecd : { *kernel_service_optional.o(.text.kernel_optional_2b) }
    ASSERT(kernel_optional_2b == 0x837ecd && ADDR(.optional_2b) + SIZEOF(.optional_2b) <= 0x837f0d, "optional service 2b overflow")
    .optional_2c 0x837f0d : { *kernel_service_optional.o(.text.kernel_optional_2c) }
    ASSERT(kernel_optional_2c == 0x837f0d && ADDR(.optional_2c) + SIZEOF(.optional_2c) <= 0x837f5f, "optional service 2c overflow")
    .fpu_available 0x837f5f : { *kernel_fpu.o(.text.kernel_fpu_available) }
    ASSERT(kernel_fpu_available == 0x837f5f && ADDR(.fpu_available) + SIZEOF(.fpu_available) <= 0x837fb5, "fpu_available layout overflow")
    .fpu_enable 0x837fb5 : { *kernel_fpu.o(.text.kernel_fpu_enable) }
    ASSERT(kernel_fpu_enable == 0x837fb5 && ADDR(.fpu_enable) + SIZEOF(.fpu_enable) <= 0x837ffd, "fpu_enable layout overflow")
    .fpu_restore_task 0x837ffd : { *kernel_fpu.o(.text.kernel_fpu_restore_task) }
    ASSERT(kernel_fpu_restore_task == 0x837ffd && ADDR(.fpu_restore_task) + SIZEOF(.fpu_restore_task) <= 0x8380c9, "fpu_restore_task layout overflow")
    .fpu_mark_lazy 0x8380c9 : { *kernel_fpu.o(.text.kernel_fpu_mark_lazy) }
    ASSERT(kernel_fpu_mark_lazy == 0x8380c9 && ADDR(.fpu_mark_lazy) + SIZEOF(.fpu_mark_lazy) <= 0x8380e1, "fpu_mark_lazy layout overflow")
    .fpu_fault 0x8380e1 : { *kernel_fpu.o(.text.kernel_fpu_fault) }
    ASSERT(kernel_fpu_fault == 0x8380e1 && ADDR(.fpu_fault) + SIZEOF(.fpu_fault) <= 0x8380ef, "fpu_fault layout overflow")
    .unexpected : { *(.text* .rodata* .data* .bss* COMMON) }
    ASSERT(SIZEOF(.unexpected) == 0, "unexpected allocated kernel data")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
