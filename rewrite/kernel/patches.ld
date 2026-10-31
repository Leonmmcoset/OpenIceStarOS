OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_entry)
PROVIDE(kernel_handle_pointer_input = 0x801454);
PROVIDE(kernel_format_text = 0x81f88a);
PROVIDE(kernel_icon_png_8241d0 = 0x8241d0);
PROVIDE(kernel_icon_read_relative_826f38 = 0x826f38);
PROVIDE(kernel_icon_read_absolute_827798 = 0x827798);
PROVIDE(kernel_syscall_target_8082c6 = 0x8082c6);
PROVIDE(kernel_syscall_target_808378 = 0x808378);
PROVIDE(kernel_syscall_target_81021c = 0x81021c);
PROVIDE(kernel_syscall_target_807a0a = 0x807a0a);
PROVIDE(kernel_startup_lookup_8263c0 = 0x8263c0);
PROVIDE(kernel_startup_jpeg_823120 = 0x823120);
PROVIDE(kernel_startup_png_82443e = 0x82443e);
PROVIDE(kernel_startup_activate_810520 = 0x810520);
PROVIDE(kernel_startup_logo_821624 = 0x821624);
PROVIDE(kernel_startup_panel_81d6c2 = kernel_rounded_outline_81d6c2);
PROVIDE(kernel_startup_fill_837545 = 0x837545);
PROVIDE(kernel_startup_outline_837589 = 0x837589);
PROVIDE(kernel_startup_text_8376d3 = 0x8376d3);
PROVIDE(kernel_startup_read_alloc_827454 = 0x827454);
PROVIDE(kernel_startup_read_into_82760c = 0x82760c);
PROVIDE(kernel_startup_load_image_80214c = 0x80214c);
PROVIDE(kernel_x_find_relative_825c8e = 0x825c8e);
PROVIDE(kernel_x_header_relative_827826 = 0x827826);
PROVIDE(kernel_x_read_relative_826ade = 0x826ade);
PROVIDE(kernel_x_find_path_8263c0 = 0x8263c0);
PROVIDE(kernel_x_header_path_8278f4 = 0x8278f4);
PROVIDE(kernel_x_read_path_82760c = 0x82760c);
PROVIDE(kernel_x_dialog_811a42 = 0x811a42);
PROVIDE(kernel_x_show_dialog_8082c6 = 0x8082c6);
PROVIDE(kernel_bitmap_lookup_relative_825c8e = 0x825c8e);
PROVIDE(kernel_bitmap_read_relative_827032 = 0x827032);
PROVIDE(kernel_bitmap_free_815978 = kernel_free_allocation);
PROVIDE(kernel_fat_read_sector_828b44 = 0x828b44);
PROVIDE(kernel_fat_write_sector_828c18 = 0x828c18);
PROVIDE(kernel_font_malloc_817590 = kernel_malloc);
PROVIDE(kernel_font_free_8175a4 = kernel_free);
SECTIONS
{
    .entry 0x800040 : { *kernel_entry.o(.text.kernel_entry) }
    ASSERT(kernel_entry == 0x800040 && ADDR(.entry) + SIZEOF(.entry) <= 0x800070, "kernel entry slot overflow")
    .bootstrap 0x800070 : { *kernel_entry.o(.text.kernel_bootstrap) }
    ASSERT(kernel_bootstrap == 0x800070 && ADDR(.bootstrap) + SIZEOF(.bootstrap) <= 0x800104, "bootstrap slot overflow")
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
    .x_load_relative 0x8020a6 : SUBALIGN(1) { *kernel_x_loader.o(.text.kernel_x_load_relative_8020a6) }
    ASSERT(kernel_x_load_relative_8020a6 == 0x8020a6 && ADDR(.x_load_relative) + SIZEOF(.x_load_relative) <= 0x80214c, "relative X loader overflow")
    .x_load_path 0x80214c : SUBALIGN(1) { *kernel_x_loader.o(.text.kernel_x_load_path_80214c) }
    ASSERT(kernel_x_load_path_80214c == 0x80214c && ADDR(.x_load_path) + SIZEOF(.x_load_path) <= 0x802282, "path X loader overflow")
    .x_launch 0x802282 : SUBALIGN(1) { *kernel_x_loader.o(.text.kernel_startup_launch_image) }
    ASSERT(kernel_startup_launch_image == 0x802282 && ADDR(.x_launch) + SIZEOF(.x_launch) <= 0x80252a, "X launch overflow")
    ASSERT(kernel_handle_pointer_input == 0x801454, "pointer input entry moved")
    ASSERT(ADDR(.pointer_input) + SIZEOF(.pointer_input) <= 0x801632, "pointer input slot overflow")
    .syscall_surface 0x81017c : { *kernel_syscall_surface.o(.text.kernel_syscall_target_81017c) }
    ASSERT(kernel_syscall_target_81017c == 0x81017c && ADDR(.syscall_surface) + SIZEOF(.syscall_surface) <= 0x81021c, "surface forwarder overflow")
    .syscall_object_set 0x81041a : { *kernel_syscall_services.o(.text.kernel_syscall_target_81041a) }
    ASSERT(kernel_syscall_target_81041a == 0x81041a && ADDR(.syscall_object_set) + SIZEOF(.syscall_object_set) <= 0x810448, "object setter overflow")
    .syscall_object_field12 0x810448 : { *kernel_syscall_services.o(.text.kernel_syscall_target_810448) }
    ASSERT(kernel_syscall_target_810448 == 0x810448 && ADDR(.syscall_object_field12) + SIZEOF(.syscall_object_field12) <= 0x810478, "object field12 overflow")
    .syscall_object_field4 0x810478 : { *kernel_syscall_services.o(.text.kernel_syscall_target_810478) }
    ASSERT(kernel_syscall_target_810478 == 0x810478 && ADDR(.syscall_object_field4) + SIZEOF(.syscall_object_field4) <= 0x8104a0, "object field4 overflow")
    .object_find_name 0x8104a0 : { *kernel_object_registry.o(.text.kernel_object_find_name) }
    ASSERT(kernel_object_find_name == 0x8104a0 && ADDR(.object_find_name) + SIZEOF(.object_find_name) <= 0x810520, "object name lookup overflow")
    .object_set_state 0x810520 : { *kernel_object_registry.o(.text.kernel_object_set_state) }
    ASSERT(kernel_object_set_state == 0x810520 && ADDR(.object_set_state) + SIZEOF(.object_set_state) <= 0x81054e, "object state assignment overflow")
    .object_registry_add 0x81054e : { *kernel_object_registry.o(.text.kernel_object_registry_add) }
    ASSERT(kernel_object_registry_add == 0x81054e && ADDR(.object_registry_add) + SIZEOF(.object_registry_add) <= 0x8105dc, "object registry insertion overflow")
    .object_registry_remove 0x8105dc : { *kernel_object_registry.o(.text.kernel_object_registry_remove) }
    ASSERT(kernel_object_registry_remove == 0x8105dc && ADDR(.object_registry_remove) + SIZEOF(.object_registry_remove) <= 0x81067a, "object registry removal overflow")
    .startup_ui 0x81067a : SUBALIGN(1) {
        *kernel_startup_ui.o(.text.kernel_startup_ui)
        *kernel_startup_ui.o(.text.relative_rectangle .rodata*)
    }
    ASSERT(kernel_startup_ui == 0x81067a && ADDR(.startup_ui) + SIZEOF(.startup_ui) <= 0x810d4a, "startup UI slot overflow")
    .startup_desktop 0x810d4a : SUBALIGN(1) {
        *kernel_startup_desktop.o(.text.kernel_startup_desktop)
        *kernel_startup_desktop.o(.text.copy_dimensions)
    }
    ASSERT(kernel_startup_desktop == 0x810d4a && ADDR(.startup_desktop) + SIZEOF(.startup_desktop) <= 0x810f38, "startup desktop slot overflow")
    .startup_apps 0x810f38 : SUBALIGN(1) { *kernel_startup_apps.o(.text.kernel_startup_applications) }
    ASSERT(kernel_startup_applications == 0x810f38 && ADDR(.startup_apps) + SIZEOF(.startup_apps) <= 0x810f78, "startup_apps slot overflow")
    .x_icon_pointer 0x812c0e : SUBALIGN(1) { *kernel_x_icon.o(.text.kernel_x_icon_pointer) }
    ASSERT(kernel_x_icon_pointer == 0x812c0e && ADDR(.x_icon_pointer) + SIZEOF(.x_icon_pointer) <= 0x812c3e, "x_icon_pointer overflow")
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
    .cursor_show 0x8130cc : SUBALIGN(1) {
        *kernel_cursor.o(.text.kernel_cursor_show)
    }
    ASSERT(kernel_cursor_show == 0x8130cc && ADDR(.cursor_show) + SIZEOF(.cursor_show) <= 0x81313e, "cursor show overflow")
    .cursor_hide 0x81313e : SUBALIGN(1) {
        *kernel_cursor.o(.text.kernel_cursor_hide)
    }
    ASSERT(kernel_cursor_hide == 0x81313e && ADDR(.cursor_hide) + SIZEOF(.cursor_hide) <= 0x8131b0, "cursor hide overflow")
    .cursor_select 0x8131b0 : SUBALIGN(1) {
        *kernel_cursor.o(.text.kernel_cursor_select)
        *kernel_cursor.o(.text.cursor_select_body)
        *kernel_cursor.o(.rodata.cursor_select_body)
    }
    ASSERT(kernel_cursor_select == 0x8131b0 && ADDR(.cursor_select) + SIZEOF(.cursor_select) <= 0x81325c, "cursor select overflow")
    .cursor_move 0x81325c : SUBALIGN(1) {
        *kernel_cursor.o(.text.kernel_cursor_move)
        *kernel_cursor.o(.text.cursor_move_body)
        *kernel_cursor.o(.rodata.cursor_move_body)
    }
    ASSERT(kernel_cursor_move == 0x81325c && ADDR(.cursor_move) + SIZEOF(.cursor_move) <= 0x813320, "cursor move overflow")
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
    .scan_acpi 0x815106 : { *kernel_firmware_scan.o(.text.kernel_scan_acpi) }
    ASSERT(kernel_scan_acpi == 0x815106 && ADDR(.scan_acpi) + SIZEOF(.scan_acpi) <= 0x8152cc, "ACPI scan overflow")
    .scan_pmid 0x8152cc : { *kernel_firmware_scan.o(.text.kernel_scan_pmid) }
    ASSERT(kernel_scan_pmid == 0x8152cc && ADDR(.scan_pmid) + SIZEOF(.scan_pmid) <= 0x815320, "PMID scan overflow")
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
    .surfaces_init 0x8175c0 : SUBALIGN(1) {
        *kernel_surface_lifecycle.o(.text.kernel_surfaces_init)
    }
    ASSERT(kernel_surfaces_init == 0x8175c0 && ADDR(.surfaces_init) + SIZEOF(.surfaces_init) <= 0x817626, "surface table initializer overflow")
    .surface_create 0x817626 : SUBALIGN(1) {
        *kernel_surface_lifecycle.o(.text.kernel_surface_create)
        *kernel_surface_lifecycle.o(.text.surface_create_body)
        *kernel_surface_lifecycle.o(.text.surface_create_free_failure)
    }
    ASSERT(kernel_surface_create == 0x817626 && ADDR(.surface_create) + SIZEOF(.surface_create) <= 0x8177c4, "surface allocation overflow")
    .surface_find_free 0x8177c4 : SUBALIGN(1) {
        *kernel_surface_lifecycle.o(.text.kernel_surface_find_free)
    }
    ASSERT(kernel_surface_find_free == 0x8177c4 && ADDR(.surface_find_free) + SIZEOF(.surface_find_free) <= 0x81783e, "surface free-slot search overflow")
    .surface_delete 0x81783e : SUBALIGN(1) {
        *kernel_surface_lifecycle.o(.text.kernel_surface_delete)
        *kernel_surface_lifecycle.o(.text.surface_delete_body)
    }
    ASSERT(kernel_surface_delete == 0x81783e && ADDR(.surface_delete) + SIZEOF(.surface_delete) <= 0x8178f0, "surface deletion overflow")
    .surface_order_remove 0x8178f0 : SUBALIGN(1) {
        *kernel_surface_order.o(.text.kernel_surface_order_remove)
        *kernel_surface_order.o(.text.surface_order_remove_body)
    }
    ASSERT(kernel_surface_order_remove == 0x8178f0 && ADDR(.surface_order_remove) + SIZEOF(.surface_order_remove) <= 0x8179d6, "surface order removal overflow")
    .surface_order_append 0x8179d6 : SUBALIGN(1) {
        *kernel_surface_order.o(.text.kernel_surface_order_append)
        *kernel_surface_order.o(.text.surface_order_append_body)
    }
    ASSERT(kernel_surface_order_append == 0x8179d6 && ADDR(.surface_order_append) + SIZEOF(.surface_order_append) <= 0x817a3c, "surface order append overflow")
    .surface_order_front 0x817a3c : SUBALIGN(1) {
        *kernel_surface_order.o(.text.kernel_surface_order_front)
        *kernel_surface_order.o(.text.surface_order_front_body)
    }
    ASSERT(kernel_surface_order_front == 0x817a3c && ADDR(.surface_order_front) + SIZEOF(.surface_order_front) <= 0x817b32, "surface order front overflow")
    .surface_order_move 0x817b32 : SUBALIGN(1) {
        *kernel_surface_order.o(.text.kernel_surface_order_move)
        *kernel_surface_order.o(.text.surface_order_move_body)
    }
    ASSERT(kernel_surface_order_move == 0x817b32 && ADDR(.surface_order_move) + SIZEOF(.surface_order_move) <= 0x817cd0, "surface order move overflow")
    .surface_composite 0x817cd0 : SUBALIGN(1) {
        *kernel_surface_composite.o(.text.kernel_surface_target_817cd0)
        *kernel_surface_composite.o(.text* .rodata*)
    }
    ASSERT(kernel_surface_target_817cd0 == 0x817cd0 && ADDR(.surface_composite) + SIZEOF(.surface_composite) <= 0x817dac, "surface clipping/list dispatcher overflow")
    .surface_draw8 0x817dac : SUBALIGN(1) {
        *kernel_surface_draw.o(.text.kernel_surface_draw8)
    }
    ASSERT(kernel_surface_draw8 == 0x817dac && ADDR(.surface_draw8) + SIZEOF(.surface_draw8) <= 0x818134, "8-bit layer rasterizer overflow")
    .surface_blend4 0x818134 : SUBALIGN(1) {
        *kernel_surface_alpha.o(.text.kernel_surface_blend4)
    }
    ASSERT(kernel_surface_blend4 == 0x818134 && ADDR(.surface_blend4) + SIZEOF(.surface_blend4) <= 0x81846a, "four-pixel blend overflow")
    .surface_draw32 0x81846a : SUBALIGN(1) {
        *kernel_surface_draw32.o(.text.kernel_surface_draw32)
        *kernel_surface_draw32.o(.text.surface_draw32_body)
    }
    ASSERT(kernel_surface_draw32 == 0x81846a && ADDR(.surface_draw32) + SIZEOF(.surface_draw32) <= 0x818d14, "32-bit output rasterizer overflow")
    .surface_regions 0x818d14 : SUBALIGN(1) {
        *kernel_surface_present.o(.text.kernel_surface_regions_test)
    }
    ASSERT(kernel_surface_regions_test == 0x818d14 && ADDR(.surface_regions) + SIZEOF(.surface_regions) <= 0x818d5e, "surface rectangle predicate overflow")
    .surface_present8 0x818d5e : SUBALIGN(1) {
        *kernel_surface_present.o(.text.kernel_surface_present8)
        *kernel_surface_present.o(.text.surface_present8_body)
    }
    ASSERT(kernel_surface_present8 == 0x818d5e && ADDR(.surface_present8) + SIZEOF(.surface_present8) <= 0x818f22, "8-bit framebuffer submission overflow")
    .surface_present32 0x818f22 : SUBALIGN(1) {
        *kernel_surface_present.o(.text.kernel_surface_present32)
        *kernel_surface_present.o(.text.surface_present32_body)
    }
    ASSERT(kernel_surface_present32 == 0x818f22 && ADDR(.surface_present32) + SIZEOF(.surface_present32) <= 0x81905e, "32-bit framebuffer submission overflow")
    .surface_refresh 0x81905e : SUBALIGN(1) {
        *kernel_surface_refresh.o(.text.kernel_syscall_target_81905e)
        *kernel_surface_refresh.o(.text* .rodata*)
    }
    ASSERT(kernel_syscall_target_81905e == 0x81905e && ADDR(.surface_refresh) + SIZEOF(.surface_refresh) <= 0x8191d8, "surface refresh selector overflow")
    .surface_move 0x8191d8 : SUBALIGN(1) {
        *kernel_surface_move.o(.text.kernel_surface_move_refresh)
        *kernel_surface_move.o(.text* .rodata*)
    }
    ASSERT(kernel_surface_move_refresh == 0x8191d8 && ADDR(.surface_move) + SIZEOF(.surface_move) <= 0x8194e4, "moved-layer damage selector overflow")
    .surface_scale 0x8194e4 : SUBALIGN(1) {
        *kernel_surface_scale.o(.text.kernel_surface_scale)
        *kernel_surface_scale.o(.text* .rodata*)
    }
    ASSERT(kernel_surface_scale == 0x8194e4 && ADDR(.surface_scale) + SIZEOF(.surface_scale) <= 0x819c40, "same-depth surface scaler overflow")
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
    .display_state 0x81acc4 : SUBALIGN(1) { *kernel_startup_state.o(.text.kernel_init_display_state) }
    ASSERT(kernel_init_display_state == 0x81acc4 && ADDR(.display_state) + SIZEOF(.display_state) <= 0x81ad6e, "display_state slot overflow")
    .display_dac 0x81ad6e : SUBALIGN(1) {
        *kernel_display_dac.o(.text.kernel_display_write_palette)
        *kernel_display_dac.o(.text.display_dac_body)
    }
    ASSERT(kernel_display_write_palette == 0x81ad6e && ADDR(.display_dac) + SIZEOF(.display_dac) <= 0x81ae14, "display DAC slot overflow")
    .display_palette 0x81ae14 : SUBALIGN(1) { *kernel_display_palette.o(.text.kernel_display_init_palette) }
    ASSERT(kernel_display_init_palette == 0x81ae14 && ADDR(.display_palette) + SIZEOF(.display_palette) <= 0x81afbc, "display_palette slot overflow")
    .display_colors 0x81afbc : SUBALIGN(1) { *kernel_display_palette.o(.text.kernel_display_select_depth) *kernel_display_palette.o(.rodata*) }
    ASSERT(kernel_display_select_depth == 0x81afbc && ADDR(.display_colors) + SIZEOF(.display_colors) <= 0x81b26e, "display_colors slot overflow")
    .object_table_state 0x82aa0f : SUBALIGN(1) { *kernel_startup_state.o(.text.kernel_init_object_table_state) }
    ASSERT(kernel_init_object_table_state == 0x82aa0f && ADDR(.object_table_state) + SIZEOF(.object_table_state) <= 0x82aa33, "object_table_state slot overflow")
    .timer_find_free 0x82aa33 : SUBALIGN(1) { *kernel_timer_objects.o(.text.kernel_timer_find_free) }
    ASSERT(kernel_timer_find_free == 0x82aa33 && ADDR(.timer_find_free) + SIZEOF(.timer_find_free) <= 0x82aaad, "timer_find_free layout overflow")
    .timer_create 0x82aaad : SUBALIGN(1) { *kernel_timer_objects.o(.text.kernel_timer_create) }
    ASSERT(kernel_timer_create == 0x82aaad && ADDR(.timer_create) + SIZEOF(.timer_create) <= 0x82abaf, "timer_create layout overflow")
    .timer_delete 0x82abaf : SUBALIGN(1) { *kernel_timer_objects.o(.text.kernel_timer_delete) }
    ASSERT(kernel_timer_delete == 0x82abaf && ADDR(.timer_delete) + SIZEOF(.timer_delete) <= 0x82ac21, "timer_delete layout overflow")
    .timer_rearm 0x82ac21 : SUBALIGN(1) { *kernel_timer_objects.o(.text.kernel_timer_rearm) }
    ASSERT(kernel_timer_rearm == 0x82ac21 && ADDR(.timer_rearm) + SIZEOF(.timer_rearm) <= 0x82ac53, "timer_rearm layout overflow")
    .factorial 0x81a2aa : SUBALIGN(1) { *kernel_math_series.o(.text.kernel_factorial_81a2aa) }
    ASSERT(kernel_factorial_81a2aa == 0x81a2aa && ADDR(.factorial) + SIZEOF(.factorial) <= 0x81a2dc, "factorial overflow")
    .sine 0x81a2dc : SUBALIGN(1) { *kernel_math_series.o(.text.kernel_sine_81a2dc) }
    ASSERT(kernel_sine_81a2dc == 0x81a2dc && ADDR(.sine) + SIZEOF(.sine) <= 0x81a366, "sine overflow")
    .cosine 0x81a366 : SUBALIGN(1) { *kernel_math_series.o(.text.kernel_cosine_81a366) }
    ASSERT(kernel_cosine_81a366 == 0x81a366 && ADDR(.cosine) + SIZEOF(.cosine) <= 0x81a3f6, "cosine overflow")
    .power 0x81a538 : SUBALIGN(1) { *kernel_math_series.o(.text.kernel_power_81a538) }
    ASSERT(kernel_power_81a538 == 0x81a538 && ADDR(.power) + SIZEOF(.power) <= 0x81a604, "power overflow")
    .acos 0x81a3f6 : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_acos_81a3f6) }
    ASSERT(kernel_acos_81a3f6 == 0x81a3f6 && ADDR(.acos) + SIZEOF(.acos) <= 0x81a4fc, "acos overflow")
    .round 0x81a4fc : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_round_81a4fc) }
    ASSERT(kernel_round_81a4fc == 0x81a4fc && ADDR(.round) + SIZEOF(.round) <= 0x81a538, "round overflow")
    .absolute 0x81a604 : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_absolute_81a604) }
    ASSERT(kernel_absolute_81a604 == 0x81a604 && ADDR(.absolute) + SIZEOF(.absolute) <= 0x81a634, "absolute overflow")
    .format_float 0x81a634 : SUBALIGN(1) { *kernel_math_format.o(.text.kernel_format_float_81a634) }
    ASSERT(kernel_format_float_81a634 == 0x81a634 && ADDR(.format_float) + SIZEOF(.format_float) <= 0x81a9ee, "format_float overflow")
    .floor 0x81a9ee : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_floor_81a9ee) }
    ASSERT(kernel_floor_81a9ee == 0x81a9ee && ADDR(.floor) + SIZEOF(.floor) <= 0x81aa52, "floor overflow")
    .ceil 0x81aa52 : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_ceil_81aa52) }
    ASSERT(kernel_ceil_81aa52 == 0x81aa52 && ADDR(.ceil) + SIZEOF(.ceil) <= 0x81aab8, "ceil overflow")
    .remainder 0x81aab8 : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_remainder_81aab8) }
    ASSERT(kernel_remainder_81aab8 == 0x81aab8 && ADDR(.remainder) + SIZEOF(.remainder) <= 0x81ab2a, "remainder overflow")
    .sqrt 0x81ab2a : SUBALIGN(1) { *kernel_math_extra.o(.text.kernel_sqrt_81ab2a) }
    ASSERT(kernel_sqrt_81ab2a == 0x81ab2a && ADDR(.sqrt) + SIZEOF(.sqrt) <= 0x81ab34, "sqrt overflow")
    .simd_add 0x81ab34 : SUBALIGN(1) { *kernel_simd.o(.text.kernel_simd_add_81ab34) }
    ASSERT(kernel_simd_add_81ab34 == 0x81ab34 && SIZEOF(.simd_add) == 0x24, "SIMD add layout mismatch")
    .simd_subtract 0x81ab58 : SUBALIGN(1) { *kernel_simd.o(.text.kernel_simd_subtract_81ab58) }
    ASSERT(kernel_simd_subtract_81ab58 == 0x81ab58 && SIZEOF(.simd_subtract) == 0x24, "SIMD subtract layout mismatch")
    .simd_multiply 0x81ab7c : SUBALIGN(1) { *kernel_simd.o(.text.kernel_simd_multiply_81ab7c) }
    ASSERT(kernel_simd_multiply_81ab7c == 0x81ab7c && SIZEOF(.simd_multiply) == 0x4a, "SIMD multiply layout mismatch")
    .simd_blend 0x81abc6 : SUBALIGN(1) { *kernel_simd.o(.text.kernel_simd_blend_81abc6) }
    ASSERT(kernel_simd_blend_81abc6 == 0x81abc6 && SIZEOF(.simd_blend) == 0xfe, "SIMD blend layout mismatch")
    .pixel_put8 0x81b26e : SUBALIGN(1) { *kernel_pixel_access.o(.text.kernel_pixel_put8_81b26e) }
    ASSERT(kernel_pixel_put8_81b26e == 0x81b26e && ADDR(.pixel_put8) + SIZEOF(.pixel_put8) <= 0x81b2f4, "pixel_put8 overflow")
    .pixel_put32 0x81b2f4 : SUBALIGN(1) { *kernel_pixel_access.o(.text.kernel_pixel_put32_81b2f4) }
    ASSERT(kernel_pixel_put32_81b2f4 == 0x81b2f4 && ADDR(.pixel_put32) + SIZEOF(.pixel_put32) <= 0x81b37c, "pixel_put32 overflow")
    .icon_put8 0x81b37c : SUBALIGN(1) { *kernel_icon_pixels.o(.text.kernel_icon_put8_81b37c) }
    ASSERT(kernel_icon_put8_81b37c == 0x81b37c && ADDR(.icon_put8) + SIZEOF(.icon_put8) <= 0x81b3c0, "icon_put8 overflow")
    .icon_put32 0x81b3c0 : SUBALIGN(1) { *kernel_icon_pixels.o(.text.kernel_icon_put32_81b3c0) }
    ASSERT(kernel_icon_put32_81b3c0 == 0x81b3c0 && ADDR(.icon_put32) + SIZEOF(.icon_put32) <= 0x81b406, "icon_put32 overflow")
    .pixel_get8 0x81b406 : SUBALIGN(1) { *kernel_pixel_access.o(.text.kernel_pixel_get8_81b406) }
    ASSERT(kernel_pixel_get8_81b406 == 0x81b406 && ADDR(.pixel_get8) + SIZEOF(.pixel_get8) <= 0x81b486, "pixel_get8 overflow")
    .pixel_get32 0x81b486 : SUBALIGN(1) { *kernel_pixel_access.o(.text.kernel_pixel_get32_81b486) }
    ASSERT(kernel_pixel_get32_81b486 == 0x81b486 && ADDR(.pixel_get32) + SIZEOF(.pixel_get32) <= 0x81b514, "pixel_get32 overflow")
    .pixel_sample 0x81b514 : SUBALIGN(1) { *kernel_pixel_sample.o(.text.kernel_pixel_sample_below_81b514) }
    ASSERT(kernel_pixel_sample_below_81b514 == 0x81b514 && ADDR(.pixel_sample) + SIZEOF(.pixel_sample) <= 0x81b714, "pixel_sample overflow")
    .raster_line8 0x81b734 : SUBALIGN(1) { *kernel_raster_line.o(.text.kernel_raster_line8_81b734) }
    ASSERT(kernel_raster_line8_81b734 == 0x81b734 && ADDR(.raster_line8) + SIZEOF(.raster_line8) <= 0x81b8dc, "raster_line8 overflow")
    .raster_line32 0x81b8dc : SUBALIGN(1) { *kernel_raster_line.o(.text.kernel_raster_line32_81b8dc) }
    ASSERT(kernel_raster_line32_81b8dc == 0x81b8dc && ADDR(.raster_line32) + SIZEOF(.raster_line32) <= 0x81ba8a, "raster_line32 overflow")
    .raster_fill8 0x81ba8a : SUBALIGN(1) { *kernel_raster_fill.o(.text.kernel_raster_fill8_81ba8a) }
    ASSERT(kernel_raster_fill8_81ba8a == 0x81ba8a && ADDR(.raster_fill8) + SIZEOF(.raster_fill8) <= 0x81bd26, "raster_fill8 overflow")
    .raster_fill32 0x81bd26 : SUBALIGN(1) { *kernel_raster_fill.o(.text.kernel_raster_fill32_81bd26) }
    ASSERT(kernel_raster_fill32_81bd26 == 0x81bd26 && ADDR(.raster_fill32) + SIZEOF(.raster_fill32) <= 0x81bf50, "raster_fill32 overflow")
    .raster_gradient32 0x81bf50 : SUBALIGN(1) { *kernel_raster_gradient.o(.text.kernel_raster_gradient32_81bf50) }
    ASSERT(kernel_raster_gradient32_81bf50 == 0x81bf50 && ADDR(.raster_gradient32) + SIZEOF(.raster_gradient32) <= 0x81c058, "raster_gradient32 overflow")
    .raster_outline8 0x81c058 : SUBALIGN(1) { *kernel_raster_outline.o(.text.kernel_raster_outline8_81c058) }
    ASSERT(kernel_raster_outline8_81c058 == 0x81c058 && ADDR(.raster_outline8) + SIZEOF(.raster_outline8) <= 0x81c1e4, "raster_outline8 overflow")
    .raster_outline32 0x81c1e4 : SUBALIGN(1) { *kernel_raster_outline.o(.text.kernel_raster_outline32_81c1e4) }
    ASSERT(kernel_raster_outline32_81c1e4 == 0x81c1e4 && ADDR(.raster_outline32) + SIZEOF(.raster_outline32) <= 0x81c396, "raster_outline32 overflow")
    .raster_circle8 0x81c396 : SUBALIGN(1) { *kernel_raster_circle.o(.text.kernel_raster_circle8_81c396) }
    ASSERT(kernel_raster_circle8_81c396 == 0x81c396 && ADDR(.raster_circle8) + SIZEOF(.raster_circle8) <= 0x81c6f2, "raster_circle8 overflow")
    .raster_circle32 0x81c6f2 : SUBALIGN(1) { *kernel_raster_circle.o(.text.kernel_raster_circle32_81c6f2) }
    ASSERT(kernel_raster_circle32_81c6f2 == 0x81c6f2 && ADDR(.raster_circle32) + SIZEOF(.raster_circle32) <= 0x81ca82, "raster_circle32 overflow")
    .raster_disc8 0x81ca82 : SUBALIGN(1) { *kernel_raster_disc.o(.text.kernel_raster_disc8_81ca82) }
    ASSERT(kernel_raster_disc8_81ca82 == 0x81ca82 && ADDR(.raster_disc8) + SIZEOF(.raster_disc8) <= 0x81cd50, "raster_disc8 overflow")
    .raster_disc32 0x81cd50 : SUBALIGN(1) { *kernel_raster_disc.o(.text.kernel_raster_disc32_81cd50) }
    ASSERT(kernel_raster_disc32_81cd50 == 0x81cd50 && ADDR(.raster_disc32) + SIZEOF(.raster_disc32) <= 0x81d032, "raster_disc32 overflow")
    .brush_square 0x81d032 : SUBALIGN(1) { *kernel_raster_brush.o(.text.kernel_brush_square_81d032) }
    ASSERT(kernel_brush_square_81d032 == 0x81d032 && ADDR(.brush_square) + SIZEOF(.brush_square) <= 0x81d070, "brush_square overflow")
    .brush_line 0x81d070 : SUBALIGN(1) { *kernel_raster_brush.o(.text.kernel_brush_line_81d070) }
    ASSERT(kernel_brush_line_81d070 == 0x81d070 && ADDR(.brush_line) + SIZEOF(.brush_line) <= 0x81d1fc, "brush_line overflow")
    .brush_circle 0x81d1fc : SUBALIGN(1) {
        *kernel_raster_brush.o(.text.kernel_brush_circle_81d1fc .text.kernel_brush_circle_body)
    }
    ASSERT(kernel_brush_circle_81d1fc == 0x81d1fc && ADDR(.brush_circle) + SIZEOF(.brush_circle) <= 0x81d4e6, "brush_circle overflow")
    .circle_quadrants 0x81d4e6 : SUBALIGN(1) { *kernel_raster_brush.o(.text.kernel_circle_quadrants_81d4e6) }
    ASSERT(kernel_circle_quadrants_81d4e6 == 0x81d4e6 && ADDR(.circle_quadrants) + SIZEOF(.circle_quadrants) <= 0x81d6c2, "circle_quadrants overflow")
    .rounded_outline 0x81d6c2 : SUBALIGN(1) { *kernel_raster_rounded.o(.text.kernel_rounded_outline_81d6c2) }
    ASSERT(kernel_rounded_outline_81d6c2 == 0x81d6c2 && ADDR(.rounded_outline) + SIZEOF(.rounded_outline) <= 0x81d8ca, "rounded_outline overflow")
    .rounded_fill 0x81d8ca : SUBALIGN(1) { *kernel_raster_rounded.o(.text.kernel_rounded_fill_81d8ca) }
    ASSERT(kernel_rounded_fill_81d8ca == 0x81d8ca && ADDR(.rounded_fill) + SIZEOF(.rounded_fill) <= 0x81da68, "rounded_fill overflow")
    .chamfer_fill 0x81da68 : SUBALIGN(1) { *kernel_raster_rounded.o(.text.kernel_chamfer_fill_81da68) }
    ASSERT(kernel_chamfer_fill_81da68 == 0x81da68 && ADDR(.chamfer_fill) + SIZEOF(.chamfer_fill) <= 0x81db5a, "chamfer_fill overflow")
    .text_glyph8 0x81db5a : SUBALIGN(1) { *kernel_text_glyph.o(.text.kernel_text_glyph8_81db5a) }
    ASSERT(kernel_text_glyph8_81db5a == 0x81db5a && ADDR(.text_glyph8) + SIZEOF(.text_glyph8) <= 0x81dcc0, "text_glyph8 overflow")
    .text_rotated8 0x81dcc0 : SUBALIGN(1) { *kernel_text_glyph.o(.text.kernel_text_rotated8_81dcc0) }
    ASSERT(kernel_text_rotated8_81dcc0 == 0x81dcc0 && ADDR(.text_rotated8) + SIZEOF(.text_rotated8) <= 0x81de2a, "text_rotated8 overflow")
    .text_glyph32 0x81de2a : SUBALIGN(1) { *kernel_text_glyph.o(.text.kernel_text_glyph32_81de2a) }
    ASSERT(kernel_text_glyph32_81de2a == 0x81de2a && ADDR(.text_glyph32) + SIZEOF(.text_glyph32) <= 0x81df96, "text_glyph32 overflow")
    .text_rotated32 0x81df96 : SUBALIGN(1) { *kernel_text_glyph.o(.text.kernel_text_rotated32_81df96) }
    ASSERT(kernel_text_rotated32_81df96 == 0x81df96 && ADDR(.text_rotated32) + SIZEOF(.text_rotated32) <= 0x81e106, "text_rotated32 overflow")
    .text_wide8 0x81e106 : SUBALIGN(1) { *kernel_text_wide.o(.text.kernel_text_wide8_81e106) }
    ASSERT(kernel_text_wide8_81e106 == 0x81e106 && ADDR(.text_wide8) + SIZEOF(.text_wide8) <= 0x81e2cc, "text_wide8 overflow")
    .text_wide8_mask 0x81e2cc : SUBALIGN(1) { *kernel_text_wide.o(.text.kernel_text_wide8_mask_81e2cc) }
    ASSERT(kernel_text_wide8_mask_81e2cc == 0x81e2cc && ADDR(.text_wide8_mask) + SIZEOF(.text_wide8_mask) <= 0x81e45a, "text_wide8_mask overflow")
    .text_wide32 0x81e45a : SUBALIGN(1) { *kernel_text_wide.o(.text.kernel_text_wide32_81e45a) }
    ASSERT(kernel_text_wide32_81e45a == 0x81e45a && ADDR(.text_wide32) + SIZEOF(.text_wide32) <= 0x81e624, "text_wide32 overflow")
    .text_wide32_mask 0x81e624 : SUBALIGN(1) { *kernel_text_wide.o(.text.kernel_text_wide32_mask_81e624) }
    ASSERT(kernel_text_wide32_mask_81e624 == 0x81e624 && ADDR(.text_wide32_mask) + SIZEOF(.text_wide32_mask) <= 0x81e7b2, "text_wide32_mask overflow")
    .text_run8 0x81e7b2 : SUBALIGN(1) { *kernel_text_run.o(.text.kernel_text_run8_81e7b2) }
    ASSERT(kernel_text_run8_81e7b2 == 0x81e7b2 && ADDR(.text_run8) + SIZEOF(.text_run8) <= 0x81e87c, "text_run8 overflow")
    .text_vertical8 0x81e87c : SUBALIGN(1) { *kernel_text_run.o(.text.kernel_text_vertical8_81e87c) }
    ASSERT(kernel_text_vertical8_81e87c == 0x81e87c && ADDR(.text_vertical8) + SIZEOF(.text_vertical8) <= 0x81e952, "text_vertical8 overflow")
    .text_run32 0x81e952 : SUBALIGN(1) { *kernel_text_run.o(.text.kernel_text_run32_81e952) }
    ASSERT(kernel_text_run32_81e952 == 0x81e952 && ADDR(.text_run32) + SIZEOF(.text_run32) <= 0x81ea1c, "text_run32 overflow")
    .text_vertical32 0x81ea1c : SUBALIGN(1) { *kernel_text_run.o(.text.kernel_text_vertical32_81ea1c) }
    ASSERT(kernel_text_vertical32_81ea1c == 0x81ea1c && ADDR(.text_vertical32) + SIZEOF(.text_vertical32) <= 0x81eaf2, "text_vertical32 overflow")
    .text_number8 0x81eaf2 : SUBALIGN(1) { *kernel_text_number.o(.text.kernel_text_number8_81eaf2) }
    ASSERT(kernel_text_number8_81eaf2 == 0x81eaf2 && ADDR(.text_number8) + SIZEOF(.text_number8) <= 0x81ec98, "text_number8 overflow")
    .text_number32 0x81ec98 : SUBALIGN(1) { *kernel_text_number.o(.text.kernel_text_number32_81ec98) }
    ASSERT(kernel_text_number32_81ec98 == 0x81ec98 && ADDR(.text_number32) + SIZEOF(.text_number32) <= 0x81ee3e, "text_number32 overflow")
    .number_digits 0x81ee3e : SUBALIGN(1) { *kernel_text_number.o(.text.kernel_number_digits_81ee3e) }
    ASSERT(kernel_number_digits_81ee3e == 0x81ee3e && ADDR(.number_digits) + SIZEOF(.number_digits) <= 0x81ee7e, "number_digits overflow")
    .text_format8 0x81ee7e : SUBALIGN(1) { *kernel_text_format.o(.text.kernel_text_format8_81ee7e) }
    ASSERT(kernel_text_format8_81ee7e == 0x81ee7e && ADDR(.text_format8) + SIZEOF(.text_format8) <= 0x81f384, "text_format8 overflow")
    .text_format32 0x81f384 : SUBALIGN(1) { *kernel_text_format.o(.text.kernel_text_format32_81f384) }
    ASSERT(kernel_text_format32_81f384 == 0x81f384 && ADDR(.text_format32) + SIZEOF(.text_format32) <= 0x81f88a, "text_format32 overflow")
    .text_format 0x81f88a : SUBALIGN(1) { *kernel_text_format.o(.text.kernel_text_format_81f88a) }
    ASSERT(kernel_text_format_81f88a == 0x81f88a && ADDR(.text_format) + SIZEOF(.text_format) <= 0x81fd90, "text_format overflow")
    .icon_quantize 0x81fd90 : SUBALIGN(1) { *kernel_icon_pixels.o(.text.kernel_icon_quantize_81fd90) }
    ASSERT(kernel_icon_quantize_81fd90 == 0x81fd90 && ADDR(.icon_quantize) + SIZEOF(.icon_quantize) <= 0x81fec2, "icon_quantize overflow")
    .bitmap_draw_file 0x81fec2 : SUBALIGN(1) { *kernel_bitmap_files.o(.text.kernel_bitmap_draw_file_81fec2) }
    ASSERT(kernel_bitmap_draw_file_81fec2 == 0x81fec2 && ADDR(.bitmap_draw_file) + SIZEOF(.bitmap_draw_file) <= 0x81ffc4, "bitmap_draw_file overflow")
    .bitmap_draw 0x81ffc4 : SUBALIGN(1) { *kernel_bitmap.o(.text.kernel_bitmap_draw_81ffc4) }
    ASSERT(kernel_bitmap_draw_81ffc4 == 0x81ffc4 && ADDR(.bitmap_draw) + SIZEOF(.bitmap_draw) <= 0x82040c, "bitmap_draw overflow")
    .bitmap_fit_file 0x82040c : SUBALIGN(1) { *kernel_bitmap_files.o(.text.kernel_bitmap_fit_file_82040c) }
    ASSERT(kernel_bitmap_fit_file_82040c == 0x82040c && ADDR(.bitmap_fit_file) + SIZEOF(.bitmap_fit_file) <= 0x8204ac, "bitmap_fit_file overflow")
    .bitmap_fit 0x8204ac : SUBALIGN(1) { *kernel_bitmap_fit.o(.text.kernel_bitmap_fit_8204ac) }
    ASSERT(kernel_bitmap_fit_8204ac == 0x8204ac && ADDR(.bitmap_fit) + SIZEOF(.bitmap_fit) <= 0x820d66, "bitmap_fit overflow")
    .bitmap_surface_fit 0x820d66 : SUBALIGN(1) { *kernel_bitmap_surface_fit.o(.text.kernel_bitmap_surface_fit_820d66) }
    ASSERT(kernel_bitmap_surface_fit_820d66 == 0x820d66 && ADDR(.bitmap_surface_fit) + SIZEOF(.bitmap_surface_fit) <= 0x821446, "bitmap_surface_fit overflow")
    .bitmap_key 0x821446 : SUBALIGN(1) { *kernel_bitmap_key.o(.text.kernel_bitmap_key_draw_821446) }
    ASSERT(kernel_bitmap_key_draw_821446 == 0x821446 && ADDR(.bitmap_key) + SIZEOF(.bitmap_key) <= 0x821624, "bitmap_key overflow")
    .clock_draw_time 0x82ac53 : SUBALIGN(1) { *kernel_clock_display.o(.text.kernel_draw_clock_time) }
    ASSERT(kernel_draw_clock_time == 0x82ac53 && ADDR(.clock_draw_time) + SIZEOF(.clock_draw_time) <= 0x82acdd, "clock_draw_time layout overflow")
    .clock_draw_date 0x82acdd : SUBALIGN(1) { *kernel_clock_display.o(.text.kernel_draw_clock_date) }
    ASSERT(kernel_draw_clock_date == 0x82acdd && ADDR(.clock_draw_date) + SIZEOF(.clock_draw_date) <= 0x82ad81, "clock_draw_date layout overflow")
    .cmos_seconds 0x82adab : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_seconds) }
    ASSERT(kernel_cmos_seconds == 0x82adab && ADDR(.cmos_seconds) + SIZEOF(.cmos_seconds) == 0x82adc5, "cmos_seconds layout overflow")
    .cmos_minutes 0x82adc5 : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_minutes) }
    ASSERT(kernel_cmos_minutes == 0x82adc5 && ADDR(.cmos_minutes) + SIZEOF(.cmos_minutes) == 0x82addf, "cmos_minutes layout overflow")
    .cmos_hours 0x82addf : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_hours) }
    ASSERT(kernel_cmos_hours == 0x82addf && ADDR(.cmos_hours) + SIZEOF(.cmos_hours) == 0x82adf9, "cmos_hours layout overflow")
    .cmos_weekday 0x82adf9 : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_weekday) }
    ASSERT(kernel_cmos_weekday == 0x82adf9 && ADDR(.cmos_weekday) + SIZEOF(.cmos_weekday) == 0x82ae13, "cmos_weekday layout overflow")
    .cmos_day 0x82ae13 : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_day) }
    ASSERT(kernel_cmos_day == 0x82ae13 && ADDR(.cmos_day) + SIZEOF(.cmos_day) == 0x82ae2d, "cmos_day layout overflow")
    .cmos_month 0x82ae2d : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_month) }
    ASSERT(kernel_cmos_month == 0x82ae2d && ADDR(.cmos_month) + SIZEOF(.cmos_month) == 0x82ae47, "cmos_month layout overflow")
    .cmos_year 0x82ae47 : SUBALIGN(1) { *kernel_cmos_fields.o(.text.kernel_cmos_year) }
    ASSERT(kernel_cmos_year == 0x82ae47 && ADDR(.cmos_year) + SIZEOF(.cmos_year) == 0x82ae84, "cmos_year layout overflow")
    .icon_draw_id 0x82af08 : SUBALIGN(1) { *kernel_icon_directory.o(.text.kernel_icon_draw_id) }
    ASSERT(kernel_icon_draw_id == 0x82af08 && ADDR(.icon_draw_id) + SIZEOF(.icon_draw_id) <= 0x82af4c, "icon_draw_id overflow")
    .icon_draw_centered 0x82af4c : SUBALIGN(1) { *kernel_icon_directory.o(.text.kernel_icon_draw_centered) }
    ASSERT(kernel_icon_draw_centered == 0x82af4c && ADDR(.icon_draw_centered) + SIZEOF(.icon_draw_centered) <= 0x82b004, "icon_draw_centered overflow")
    .icon_draw_frame 0x82b004 : SUBALIGN(1) { *kernel_icon_directory.o(.text.kernel_icon_draw_frame) }
    ASSERT(kernel_icon_draw_frame == 0x82b004 && ADDR(.icon_draw_frame) + SIZEOF(.icon_draw_frame) <= 0x82b0a0, "icon_draw_frame overflow")
    .icon_draw_file 0x82b0a0 : SUBALIGN(1) { *kernel_icon_files.o(.text.kernel_icon_draw_file) }
    ASSERT(kernel_icon_draw_file == 0x82b0a0 && ADDR(.icon_draw_file) + SIZEOF(.icon_draw_file) <= 0x82b112, "icon_draw_file overflow")
    .icon_draw_file_auto 0x82b112 : SUBALIGN(1) { *kernel_icon_files.o(.text.kernel_icon_draw_file_auto) }
    ASSERT(kernel_icon_draw_file_auto == 0x82b112 && ADDR(.icon_draw_file_auto) + SIZEOF(.icon_draw_file_auto) <= 0x82b20c, "icon_draw_file_auto overflow")
    .icon_dib 0x82b20c : SUBALIGN(1) { *kernel_icon_dib.o(.text.kernel_icon_dib_82b20c) }
    ASSERT(kernel_icon_dib_82b20c == 0x82b20c && ADDR(.icon_dib) + SIZEOF(.icon_dib) <= 0x82b678, "icon_dib overflow")
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
    .string_upper 0x82a531 : { *kernel_string_extra.o(.text.kernel_string_upper_82a531) }
    ASSERT(kernel_string_upper_82a531 == 0x82a531 && ADDR(.string_upper) + SIZEOF(.string_upper) <= 0x82a57d, "string uppercase overflow")
    .string_find 0x82a57d : { *kernel_string_extra.o(.text.kernel_string_find_82a57d) }
    ASSERT(kernel_string_find_82a57d == 0x82a57d && ADDR(.string_find) + SIZEOF(.string_find) <= 0x82a609, "substring search overflow")
    .config_value 0x82a609 : { *kernel_string_extra.o(.text.kernel_config_value_82a609) }
    ASSERT(kernel_config_value_82a609 == 0x82a609 && ADDR(.config_value) + SIZEOF(.config_value) <= 0x82a70d, "configuration lookup overflow")
    .string_character 0x82a70d : { *kernel_string_extra.o(.text.kernel_string_character_82a70d) }
    ASSERT(kernel_string_character_82a70d == 0x82a70d && ADDR(.string_character) + SIZEOF(.string_character) <= 0x82a755, "character search overflow")
    .character_upper 0x82a755 : { *kernel_string_extra.o(.text.kernel_character_upper_82a755) }
    ASSERT(kernel_character_upper_82a755 == 0x82a755 && ADDR(.character_upper) + SIZEOF(.character_upper) <= 0x82a787, "character uppercase overflow")
    .unsigned_text 0x82a787 : { *kernel_string_extra.o(.text.kernel_unsigned_text_82a787) }
    ASSERT(kernel_unsigned_text_82a787 == 0x82a787 && ADDR(.unsigned_text) + SIZEOF(.unsigned_text) <= 0x82a8a3, "radix conversion overflow")
    .string_split 0x82a8a3 : { *kernel_string_extra.o(.text.kernel_string_split_82a8a3) }
    ASSERT(kernel_string_split_82a8a3 == 0x82a8a3 && ADDR(.string_split) + SIZEOF(.string_split) <= 0x82a9bb, "string split overflow")
    .random_range 0x82a9bb : { *kernel_syscall_services.o(.text.kernel_syscall_target_82a9bb) }
    ASSERT(kernel_syscall_target_82a9bb == 0x82a9bb && ADDR(.random_range) + SIZEOF(.random_range) <= 0x82aa0f, "random range overflow")
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
    .timestamp_low 0x83734a : { *kernel_timestamp.o(.text.kernel_timestamp_low) }
    ASSERT(kernel_timestamp_low == 0x83734a && SIZEOF(.timestamp_low) == 5, "low TSC layout changed")
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
    .font_get8 0x82b838 : { *kernel_font_parse.o(.text.kernel_font_get8_82b838) }
    ASSERT(kernel_font_get8_82b838 == 0x82b838 && SIZEOF(.font_get8) <= 54, "font_get8 overflow")
    .font_peek8 0x82b86e : { *kernel_font_parse.o(.text.kernel_font_peek8_82b86e) }
    ASSERT(kernel_font_peek8_82b86e == 0x82b86e && SIZEOF(.font_peek8) <= 52, "font_peek8 overflow")
    .font_seek 0x82b8a2 : { *kernel_font_parse.o(.text.kernel_font_seek_82b8a2) }
    ASSERT(kernel_font_seek_82b8a2 == 0x82b8a2 && SIZEOF(.font_seek) <= 60, "font_seek overflow")
    .font_skip 0x82b8de : { *kernel_font_parse.o(.text.kernel_font_skip_82b8de) }
    ASSERT(kernel_font_skip_82b8de == 0x82b8de && SIZEOF(.font_skip) <= 32, "font_skip overflow")
    .font_getn 0x82b8fe : { *kernel_font_parse.o(.text.kernel_font_getn_82b8fe) }
    ASSERT(kernel_font_getn_82b8fe == 0x82b8fe && SIZEOF(.font_getn) <= 72, "font_getn overflow")
    .font_buffer 0x82b946 : { *kernel_font_parse.o(.text.kernel_font_buffer_82b946) }
    ASSERT(kernel_font_buffer_82b946 == 0x82b946 && SIZEOF(.font_buffer) <= 28, "font_buffer overflow")
    .font_range 0x82b962 : { *kernel_font_parse.o(.text.kernel_font_range_82b962) }
    ASSERT(kernel_font_range_82b962 == 0x82b962 && SIZEOF(.font_range) <= 104, "font_range overflow")
    .font_cff_index 0x82b9ca : { *kernel_font_parse.o(.text.kernel_font_cff_index_82b9ca) }
    ASSERT(kernel_font_cff_index_82b9ca == 0x82b9ca && SIZEOF(.font_cff_index) <= 166, "font_cff_index overflow")
    .font_cff_int 0x82ba70 : { *kernel_font_parse.o(.text.kernel_font_cff_int_82ba70) }
    ASSERT(kernel_font_cff_int_82ba70 == 0x82ba70 && SIZEOF(.font_cff_int) <= 256, "font_cff_int overflow")
    .font_cff_skip 0x82bb70 : { *kernel_font_parse.o(.text.kernel_font_cff_skip_82bb70) }
    ASSERT(kernel_font_cff_skip_82bb70 == 0x82bb70 && SIZEOF(.font_cff_skip) <= 116, "font_cff_skip overflow")
    .font_cff_dict 0x82bbe4 : { *kernel_font_parse.o(.text.kernel_font_cff_dict_82bbe4) }
    ASSERT(kernel_font_cff_dict_82bbe4 == 0x82bbe4 && SIZEOF(.font_cff_dict) <= 228, "font_cff_dict overflow")
    .font_cff_ints 0x82bcc8 : { *kernel_font_parse.o(.text.kernel_font_cff_ints_82bcc8) }
    ASSERT(kernel_font_cff_ints_82bcc8 == 0x82bcc8 && SIZEOF(.font_cff_ints) <= 96, "font_cff_ints overflow")
    .font_cff_count 0x82bd28 : { *kernel_font_parse.o(.text.kernel_font_cff_count_82bd28) }
    ASSERT(kernel_font_cff_count_82bd28 == 0x82bd28 && SIZEOF(.font_cff_count) <= 46, "font_cff_count overflow")
    .font_cff_item 0x82bd56 : { *kernel_font_parse.o(.text.kernel_font_cff_item_82bd56) }
    ASSERT(kernel_font_cff_item_82bd56 == 0x82bd56 && SIZEOF(.font_cff_item) <= 190, "font_cff_item overflow")
    .font_u16 0x82be14 : { *kernel_font_parse.o(.text.kernel_font_u16_82be14) }
    ASSERT(kernel_font_u16_82be14 == 0x82be14 && SIZEOF(.font_u16) <= 32, "font_u16 overflow")
    .font_s16 0x82be34 : { *kernel_font_parse.o(.text.kernel_font_s16_82be34) }
    ASSERT(kernel_font_s16_82be34 == 0x82be34 && SIZEOF(.font_s16) <= 30, "font_s16 overflow")
    .font_u32 0x82be52 : { *kernel_font_parse.o(.text.kernel_font_u32_82be52) }
    ASSERT(kernel_font_u32_82be52 == 0x82be52 && SIZEOF(.font_u32) <= 52, "font_u32 overflow")
    .font_s32 0x82be86 : { *kernel_font_parse.o(.text.kernel_font_s32_82be86) }
    ASSERT(kernel_font_s32_82be86 == 0x82be86 && SIZEOF(.font_s32) <= 52, "font_s32 overflow")
    .font_signature 0x82beba : { *kernel_font_parse.o(.text.kernel_font_signature_82beba) }
    ASSERT(kernel_font_signature_82beba == 0x82beba && SIZEOF(.font_signature) <= 300, "font_signature overflow")
    .font_table 0x82bfe6 : { *kernel_font_parse.o(.text.kernel_font_table_82bfe6) }
    ASSERT(kernel_font_table_82bfe6 == 0x82bfe6 && SIZEOF(.font_table) <= 204, "font_table overflow")
    .font_offset 0x82c0b2 : { *kernel_font_parse.o(.text.kernel_font_offset_82c0b2) }
    ASSERT(kernel_font_offset_82c0b2 == 0x82c0b2 && SIZEOF(.font_offset) <= 228, "font_offset overflow")
    .font_count 0x82c196 : { *kernel_font_parse.o(.text.kernel_font_count_82c196) }
    ASSERT(kernel_font_count_82c196 == 0x82c196 && SIZEOF(.font_count) <= 156, "font_count overflow")
    .font_cff_subrs 0x82c232 : { *kernel_font_parse.o(.text.kernel_font_cff_subrs_82c232) }
    ASSERT(kernel_font_cff_subrs_82c232 == 0x82c232 && SIZEOF(.font_cff_subrs) <= 282, "font_cff_subrs overflow")
    .font_svg 0x82c34c : { *kernel_font_parse.o(.text.kernel_font_svg_82c34c) }
    ASSERT(kernel_font_svg_82c34c == 0x82c34c && SIZEOF(.font_svg) <= 112, "font_svg overflow")
    .font_hmetrics 0x82f958 : { *kernel_font_metrics.o(.text.kernel_font_hmetrics_82f958) }
    ASSERT(kernel_font_hmetrics_82f958 == 0x82f958 && SIZEOF(.font_hmetrics) <= 248, "font_hmetrics overflow")
    .font_vmetrics 0x83054a : { *kernel_font_metrics.o(.text.kernel_font_vmetrics_83054a) }
    ASSERT(kernel_font_vmetrics_83054a == 0x83054a && SIZEOF(.font_vmetrics) <= 118, "font_vmetrics overflow")
    .font_typo_metrics 0x8305c0 : { *kernel_font_metrics.o(.text.kernel_font_typo_metrics_8305c0) }
    ASSERT(kernel_font_typo_metrics_8305c0 == 0x8305c0 && SIZEOF(.font_typo_metrics) <= 170, "font_typo_metrics overflow")
    .font_bounds 0x83066a : { *kernel_font_metrics.o(.text.kernel_font_bounds_83066a) }
    ASSERT(kernel_font_bounds_83066a == 0x83066a && SIZEOF(.font_bounds) <= 130, "font_bounds overflow")
    .font_scale_height 0x8306ec : { *kernel_font_metrics.o(.text.kernel_font_scale_height_8306ec) }
    ASSERT(kernel_font_scale_height_8306ec == 0x8306ec && SIZEOF(.font_scale_height) <= 78, "font_scale_height overflow")
    .font_scale_em 0x83073a : { *kernel_font_metrics.o(.text.kernel_font_scale_em_83073a) }
    ASSERT(kernel_font_scale_em_83073a == 0x83073a && SIZEOF(.font_scale_em) <= 46, "font_scale_em overflow")
    .font_svg_record 0x83077c : { *kernel_font_metrics.o(.text.kernel_font_svg_record_83077c) }
    ASSERT(kernel_font_svg_record_83077c == 0x83077c && SIZEOF(.font_svg_record) <= 160, "font_svg_record overflow")
    .font_glyph_svg 0x83081c : { *kernel_font_metrics.o(.text.kernel_font_glyph_svg_83081c) }
    ASSERT(kernel_font_glyph_svg_83081c == 0x83081c && SIZEOF(.font_glyph_svg) <= 132, "font_glyph_svg overflow")
    .font_codepoint_svg 0x8308a0 : { *kernel_font_metrics.o(.text.kernel_font_codepoint_svg_8308a0) }
    ASSERT(kernel_font_codepoint_svg_8308a0 == 0x8308a0 && SIZEOF(.font_codepoint_svg) <= 50, "font_codepoint_svg overflow")
    .font_cmap 0x82ca7c : { *kernel_font_cmap.o(.text.kernel_font_glyph_index_82ca7c) }
    ASSERT(kernel_font_glyph_index_82ca7c == 0x82ca7c && SIZEOF(.font_cmap) <= 1122, "font cmap overflow")
    .font_init 0x82c3bc : { *kernel_font_init.o(.text.kernel_font_init_82c3bc) }
    ASSERT(kernel_font_init_82c3bc == 0x82c3bc && SIZEOF(.font_init) <= 1728, "font initializer overflow")
    PROVIDE(kernel_font_charstring_82e6f0 = 0x82e6f0);
    .font_vertex 0x82cf10 : { *kernel_font_glyph.o(.text.kernel_font_vertex_82cf10) }
    ASSERT(kernel_font_vertex_82cf10 == 0x82cf10 && SIZEOF(.font_vertex) <= 64, "font_vertex overflow")
    .font_glyph_offset 0x82cf50 : { *kernel_font_glyph.o(.text.kernel_font_glyph_offset_82cf50) }
    ASSERT(kernel_font_glyph_offset_82cf50 == 0x82cf50 && SIZEOF(.font_glyph_offset) <= 274, "font_glyph_offset overflow")
    .font_glyph_box 0x82d062 : { *kernel_font_glyph.o(.text.kernel_font_glyph_box_82d062) }
    ASSERT(kernel_font_glyph_box_82d062 == 0x82d062 && SIZEOF(.font_glyph_box) <= 250, "font_glyph_box overflow")
    .font_codepoint_box 0x82d15c : { *kernel_font_glyph.o(.text.kernel_font_codepoint_box_82d15c) }
    ASSERT(kernel_font_codepoint_box_82d15c == 0x82d15c && SIZEOF(.font_codepoint_box) <= 72, "font_codepoint_box overflow")
    .font_glyph_empty 0x82d1a4 : { *kernel_font_glyph.o(.text.kernel_font_glyph_empty_82d1a4) }
    ASSERT(kernel_font_glyph_empty_82d1a4 == 0x82d1a4 && SIZEOF(.font_glyph_empty) <= 166, "font_glyph_empty overflow")
    .font_cff_box 0x82f80a : { *kernel_font_glyph.o(.text.kernel_font_cff_box_82f80a) }
    ASSERT(kernel_font_cff_box_82f80a == 0x82f80a && SIZEOF(.font_cff_box) <= 256, "font_cff_box overflow")
    .font_kern_length 0x82fa50 : { *kernel_font_kern.o(.text.kernel_font_kern_length_82fa50) }
    ASSERT(kernel_font_kern_length_82fa50 == 0x82fa50 && SIZEOF(.font_kern_length) <= 122, "font_kern_length overflow")
    .font_kern_table 0x82faca : { *kernel_font_kern.o(.text.kernel_font_kern_table_82faca) }
    ASSERT(kernel_font_kern_table_82faca == 0x82faca && SIZEOF(.font_kern_table) <= 336, "font_kern_table overflow")
    .font_legacy_kern 0x82fc1a : { *kernel_font_kern.o(.text.kernel_font_legacy_kern_82fc1a) }
    ASSERT(kernel_font_legacy_kern_82fc1a == 0x82fc1a && SIZEOF(.font_legacy_kern) <= 270, "font_legacy_kern overflow")
    .font_coverage 0x82fd28 : { *kernel_font_kern.o(.text.kernel_font_coverage_82fd28) }
    ASSERT(kernel_font_coverage_82fd28 == 0x82fd28 && SIZEOF(.font_coverage) <= 406, "font_coverage overflow")
    .font_class 0x82febe : { *kernel_font_kern.o(.text.kernel_font_class_82febe) }
    ASSERT(kernel_font_class_82febe == 0x82febe && SIZEOF(.font_class) <= 358, "font_class overflow")
    .font_gpos_kern 0x830024 : { *kernel_font_kern.o(.text.kernel_font_gpos_kern_830024) }
    ASSERT(kernel_font_gpos_kern_830024 == 0x830024 && SIZEOF(.font_gpos_kern) <= 1054, "font_gpos_kern overflow")
    .font_glyph_kern 0x830442 : { *kernel_font_kern.o(.text.kernel_font_glyph_kern_830442) }
    ASSERT(kernel_font_glyph_kern_830442 == 0x830442 && SIZEOF(.font_glyph_kern) <= 102, "font_glyph_kern overflow")
    .font_codepoint_kern 0x8304a8 : { *kernel_font_kern.o(.text.kernel_font_codepoint_kern_8304a8) }
    ASSERT(kernel_font_codepoint_kern_8304a8 == 0x8304a8 && SIZEOF(.font_codepoint_kern) <= 104, "font_codepoint_kern overflow")
    .font_codepoint_metrics 0x830510 : { *kernel_font_kern.o(.text.kernel_font_codepoint_metrics_830510) }
    ASSERT(kernel_font_codepoint_metrics_830510 == 0x830510 && SIZEOF(.font_codepoint_metrics) <= 58, "font_codepoint_metrics overflow")
    .identity_paging 0x837448 : SUBALIGN(1) {
        *kernel_paging.o(.text.kernel_init_identity_paging)
        *kernel_paging.o(.text.build_identity_tables)
    }
    ASSERT(kernel_init_identity_paging == 0x837448 && ADDR(.identity_paging) + SIZEOF(.identity_paging) <= 0x8374b2, "identity paging slot overflow")
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
    .jpeg_info 0x83a70f : { *kernel_jpeg_decode.o(.text.kernel_jpeg_info_83a70f) }
    ASSERT(kernel_jpeg_info_83a70f == 0x83a70f && ADDR(.jpeg_info) + SIZEOF(.jpeg_info) <= 0x83a7a1, "jpeg info layout overflow")
    .jpeg_decode 0x83a7a1 : { *kernel_jpeg_decode.o(.text.kernel_jpeg_decode_83a7a1) }
    ASSERT(kernel_jpeg_decode_83a7a1 == 0x83a7a1 && ADDR(.jpeg_decode) + SIZEOF(.jpeg_decode) <= 0x83a835, "jpeg decode layout overflow")
    .jpeg_bits 0x83a835 : { *kernel_jpeg_bits.o(.text.kernel_jpeg_bits_83a835) }
    ASSERT(kernel_jpeg_bits_83a835 == 0x83a835 && ADDR(.jpeg_bits) + SIZEOF(.jpeg_bits) <= 0x83a919, "jpeg bit reader layout overflow")
    .jpeg_frame 0x83a919 : { *kernel_jpeg_frame.o(.text.kernel_jpeg_frame_83a919) }
    ASSERT(kernel_jpeg_frame_83a919 == 0x83a919 && ADDR(.jpeg_frame) + SIZEOF(.jpeg_frame) <= 0x83aae7, "jpeg frame layout overflow")
    .jpeg_tables 0x83aae7 : { *kernel_jpeg_tables.o(.text.kernel_jpeg_tables_83aae7) }
    ASSERT(kernel_jpeg_tables_83aae7 == 0x83aae7 && ADDR(.jpeg_tables) + SIZEOF(.jpeg_tables) <= 0x83ac7b, "jpeg quantization layout overflow")
    .jpeg_huffman 0x83ac7b : { *kernel_jpeg_huffman.o(.text.kernel_jpeg_huffman_83ac7b) }
    ASSERT(kernel_jpeg_huffman_83ac7b == 0x83ac7b && ADDR(.jpeg_huffman) + SIZEOF(.jpeg_huffman) <= 0x83af27, "jpeg Huffman layout overflow")
    .jpeg_scan 0x83af27 : { *kernel_jpeg_scan.o(.text.kernel_jpeg_scan_83af27) }
    ASSERT(kernel_jpeg_scan_83af27 == 0x83af27 && ADDR(.jpeg_scan) + SIZEOF(.jpeg_scan) <= 0x83b233, "jpeg scan layout overflow")
    .jpeg_prepare 0x83b233 : { *kernel_jpeg_scan.o(.text.kernel_jpeg_prepare_83b233) }
    ASSERT(kernel_jpeg_prepare_83b233 == 0x83b233 && ADDR(.jpeg_prepare) + SIZEOF(.jpeg_prepare) <= 0x83b351, "jpeg prepare layout overflow")
    .jpeg_symbol 0x83b351 : { *kernel_jpeg_scan.o(.text.kernel_jpeg_symbol_83b351) }
    ASSERT(kernel_jpeg_symbol_83b351 == 0x83b351 && ADDR(.jpeg_symbol) + SIZEOF(.jpeg_symbol) <= 0x83b41b, "jpeg symbol layout overflow")
    .jpeg_basis 0x83b41b : SUBALIGN(1) { *kernel_jpeg_basis.o(.text.kernel_jpeg_basis_83b41b .rodata*) }
    ASSERT(kernel_jpeg_basis_83b41b == 0x83b41b && ADDR(.jpeg_basis) + SIZEOF(.jpeg_basis) <= 0x83b653, "jpeg basis layout overflow")
    .jpeg_transform 0x83b653 : { *kernel_jpeg_transform.o(.text.kernel_jpeg_transform_83b653) }
    ASSERT(kernel_jpeg_transform_83b653 == 0x83b653 && ADDR(.jpeg_transform) + SIZEOF(.jpeg_transform) <= 0x83b73f, "jpeg transform layout overflow")
    .jpeg_magnitude 0x83b73f : { *kernel_jpeg_transform.o(.text.kernel_jpeg_magnitude_83b73f) }
    ASSERT(kernel_jpeg_magnitude_83b73f == 0x83b73f && ADDR(.jpeg_magnitude) + SIZEOF(.jpeg_magnitude) <= 0x83b7ab, "jpeg magnitude layout overflow")
    .jpeg_block 0x83b7ab : { *kernel_jpeg_block.o(.text.kernel_jpeg_block_83b7ab) }
    ASSERT(kernel_jpeg_block_83b7ab == 0x83b7ab && ADDR(.jpeg_block) + SIZEOF(.jpeg_block) <= 0x83b96d, "jpeg block layout overflow")
    .jpeg_expand 0x83b96d : { *kernel_jpeg_output.o(.text.kernel_jpeg_expand_83b96d) }
    ASSERT(kernel_jpeg_expand_83b96d == 0x83b96d && ADDR(.jpeg_expand) + SIZEOF(.jpeg_expand) <= 0x83ba4f, "jpeg expansion layout overflow")
    .jpeg_mcu 0x83ba4f : { *kernel_jpeg_decode.o(.text.kernel_jpeg_mcu_83ba4f) }
    ASSERT(kernel_jpeg_mcu_83ba4f == 0x83ba4f && ADDR(.jpeg_mcu) + SIZEOF(.jpeg_mcu) <= 0x83bc15, "jpeg MCU layout overflow")
    .jpeg_color 0x83bc15 : { *kernel_jpeg_output.o(.text.kernel_jpeg_color_83bc15) }
    ASSERT(kernel_jpeg_color_83bc15 == 0x83bc15 && ADDR(.jpeg_color) + SIZEOF(.jpeg_color) <= 0x83be59, "jpeg color layout overflow")
    .jpeg_image 0x83be59 : SUBALIGN(1) { *kernel_jpeg_decode.o(.text.kernel_jpeg_image_83be59 .rodata*) }
    ASSERT(kernel_jpeg_image_83be59 == 0x83be59 && ADDR(.jpeg_image) + SIZEOF(.jpeg_image) <= 0x83c02f, "jpeg image layout overflow")
    .x_lba_alloc 0x8252b4 : { *kernel_fat_block.o(.text.kernel_x_read_lba_alloc_8252b4) }
    ASSERT(kernel_x_read_lba_alloc_8252b4 == 0x8252b4 && ADDR(.x_lba_alloc) + SIZEOF(.x_lba_alloc) <= 0x82539e, "x_lba_alloc overflow")
    .x_lba_into 0x82539e : { *kernel_fat_block.o(.text.kernel_x_read_lba_into_82539e) }
    ASSERT(kernel_x_read_lba_into_82539e == 0x82539e && ADDR(.x_lba_into) + SIZEOF(.x_lba_into) <= 0x825476, "x_lba_into overflow")
    .fat_sectors_alloc 0x825476 : { *kernel_fat_block.o(.text.kernel_fat_read_sectors_alloc_825476) }
    ASSERT(kernel_fat_read_sectors_alloc_825476 == 0x825476 && ADDR(.fat_sectors_alloc) + SIZEOF(.fat_sectors_alloc) <= 0x8254ca, "fat_sectors_alloc overflow")
    .fat_read_sectors 0x8254ca : { *kernel_fat_block.o(.text.kernel_fat_read_sectors_8254ca) }
    ASSERT(kernel_fat_read_sectors_8254ca == 0x8254ca && ADDR(.fat_read_sectors) + SIZEOF(.fat_read_sectors) <= 0x825506, "fat_read_sectors overflow")
    .fat_read_partition 0x825506 : { *kernel_fat_block.o(.text.kernel_fat_read_partition_825506) }
    ASSERT(kernel_fat_read_partition_825506 == 0x825506 && ADDR(.fat_read_partition) + SIZEOF(.fat_read_partition) <= 0x8255ae, "fat_read_partition overflow")
    .fat_read_root_directory 0x8255ae : { *kernel_fat_block.o(.text.kernel_fat_read_root_directory_8255ae) }
    ASSERT(kernel_fat_read_root_directory_8255ae == 0x8255ae && ADDR(.fat_read_root_directory) + SIZEOF(.fat_read_root_directory) <= 0x8256a8, "fat_read_root_directory overflow")
    .fat_alloc_root_directory 0x8256a8 : { *kernel_fat_block.o(.text.kernel_fat_alloc_root_directory_8256a8) }
    ASSERT(kernel_fat_alloc_root_directory_8256a8 == 0x8256a8 && ADDR(.fat_alloc_root_directory) + SIZEOF(.fat_alloc_root_directory) <= 0x8257bc, "fat_alloc_root_directory overflow")
    .fat_alloc_directory 0x8257bc : { *kernel_fat_block.o(.text.kernel_fat_alloc_directory_8257bc) }
    ASSERT(kernel_fat_alloc_directory_8257bc == 0x8257bc && ADDR(.fat_alloc_directory) + SIZEOF(.fat_alloc_directory) <= 0x8258a6, "fat_alloc_directory overflow")
    .fat_root_name 0x8258a6 : { *kernel_fat_directory.o(.text.kernel_fat_root_name_8258a6) }
    ASSERT(kernel_fat_root_name_8258a6 == 0x8258a6 && ADDR(.fat_root_name) + SIZEOF(.fat_root_name) <= 0x825a7e, "fat_root_name overflow")
    .fat_directory_name 0x825a7e : { *kernel_fat_directory.o(.text.kernel_fat_directory_name_825a7e) }
    ASSERT(kernel_fat_directory_name_825a7e == 0x825a7e && ADDR(.fat_directory_name) + SIZEOF(.fat_directory_name) <= 0x825bcc, "fat_directory_name overflow")
    .fat_root_extension 0x825bcc : { *kernel_fat_directory.o(.text.kernel_fat_root_extension_825bcc) }
    ASSERT(kernel_fat_root_extension_825bcc == 0x825bcc && ADDR(.fat_root_extension) + SIZEOF(.fat_root_extension) <= 0x825c46, "fat_root_extension overflow")
    .fat_directory_extension 0x825c46 : { *kernel_fat_directory.o(.text.kernel_fat_directory_extension_825c46) }
    ASSERT(kernel_fat_directory_extension_825c46 == 0x825c46 && ADDR(.fat_directory_extension) + SIZEOF(.fat_directory_extension) <= 0x825c8e, "fat_directory_extension overflow")
    .fat_find_relative 0x825c8e : { *kernel_fat_directory.o(.text.kernel_x_find_relative_825c8e) }
    ASSERT(kernel_x_find_relative_825c8e == 0x825c8e && ADDR(.fat_find_relative) + SIZEOF(.fat_find_relative) <= 0x825d34, "fat_find_relative overflow")
    .fat_directory_find 0x825d34 : { *kernel_fat_directory.o(.text.kernel_fat_directory_find_825d34) }
    ASSERT(kernel_fat_directory_find_825d34 == 0x825d34 && ADDR(.fat_directory_find) + SIZEOF(.fat_directory_find) <= 0x825df4, "fat_directory_find overflow")
    .fat_root_find_directory 0x825df4 : { *kernel_fat_directory.o(.text.kernel_fat_root_find_directory_825df4) }
    ASSERT(kernel_fat_root_find_directory_825df4 == 0x825df4 && ADDR(.fat_root_find_directory) + SIZEOF(.fat_root_find_directory) <= 0x825e86, "fat_root_find_directory overflow")
    .fat_find_directory 0x825e86 : { *kernel_fat_directory.o(.text.kernel_fat_find_directory_825e86) }
    ASSERT(kernel_fat_find_directory_825e86 == 0x825e86 && ADDR(.fat_find_directory) + SIZEOF(.fat_find_directory) <= 0x825f48, "fat_find_directory overflow")
    .fat_find_directory_path 0x825f48 : { *kernel_fat_directory_path.o(.text.kernel_fat_find_directory_path_825f48) }
    ASSERT(kernel_fat_find_directory_path_825f48 == 0x825f48 && ADDR(.fat_find_directory_path) + SIZEOF(.fat_find_directory_path) <= 0x8263c0, "fat_find_directory_path overflow")
    .fat_root_size 0x826940 : { *kernel_fat_directory.o(.text.kernel_fat_root_size_826940) }
    ASSERT(kernel_fat_root_size_826940 == 0x826940 && ADDR(.fat_root_size) + SIZEOF(.fat_root_size) <= 0x82698c, "fat_root_size overflow")
    .fat_directory_size 0x82698c : { *kernel_fat_directory.o(.text.kernel_fat_directory_size_82698c) }
    ASSERT(kernel_fat_directory_size_82698c == 0x82698c && ADDR(.fat_directory_size) + SIZEOF(.fat_directory_size) <= 0x8269a6, "fat_directory_size overflow")
    .fat_root_attributes 0x8269a6 : { *kernel_fat_directory.o(.text.kernel_fat_root_attributes_8269a6) }
    ASSERT(kernel_fat_root_attributes_8269a6 == 0x8269a6 && ADDR(.fat_root_attributes) + SIZEOF(.fat_root_attributes) <= 0x8269f2, "fat_root_attributes overflow")
    .fat_directory_attributes 0x8269f2 : { *kernel_fat_directory.o(.text.kernel_fat_directory_attributes_8269f2) }
    ASSERT(kernel_fat_directory_attributes_8269f2 == 0x8269f2 && ADDR(.fat_directory_attributes) + SIZEOF(.fat_directory_attributes) <= 0x826a0e, "fat_directory_attributes overflow")
    .fat_root_date 0x826a0e : { *kernel_fat_directory.o(.text.kernel_fat_root_date_826a0e) }
    ASSERT(kernel_fat_root_date_826a0e == 0x826a0e && ADDR(.fat_root_date) + SIZEOF(.fat_root_date) <= 0x826a5a, "fat_root_date overflow")
    .fat_directory_date 0x826a5a : { *kernel_fat_directory.o(.text.kernel_fat_directory_date_826a5a) }
    ASSERT(kernel_fat_directory_date_826a5a == 0x826a5a && ADDR(.fat_directory_date) + SIZEOF(.fat_directory_date) <= 0x826a76, "fat_directory_date overflow")
    .fat_root_time 0x826a76 : { *kernel_fat_directory.o(.text.kernel_fat_root_time_826a76) }
    ASSERT(kernel_fat_root_time_826a76 == 0x826a76 && ADDR(.fat_root_time) + SIZEOF(.fat_root_time) <= 0x826ac2, "fat_root_time overflow")
    .fat_directory_time 0x826ac2 : { *kernel_fat_directory.o(.text.kernel_fat_directory_time_826ac2) }
    ASSERT(kernel_fat_directory_time_826ac2 == 0x826ac2 && ADDR(.fat_directory_time) + SIZEOF(.fat_directory_time) <= 0x826ade, "fat_directory_time overflow")
    .fat_find_path 0x8263c0 : { *kernel_fat_path.o(.text.kernel_x_find_path_8263c0) }
    ASSERT(kernel_x_find_path_8263c0 == 0x8263c0 && ADDR(.fat_find_path) + SIZEOF(.fat_find_path) <= 0x826940, "fat_find_path overflow")
    .fat_name83 0x827bba : { *kernel_fat_path.o(.text.kernel_fat_name83_827bba) }
    ASSERT(kernel_fat_name83_827bba == 0x827bba && ADDR(.fat_name83) + SIZEOF(.fat_name83) <= 0x827c72, "fat_name83 overflow")
    .fat_write_sectors 0x827c72 : { *kernel_fat_io.o(.text.kernel_fat_write_sectors_827c72) }
    ASSERT(kernel_fat_write_sectors_827c72 == 0x827c72 && ADDR(.fat_write_sectors) + SIZEOF(.fat_write_sectors) <= 0x827cc3, "fat_write_sectors overflow")
    .fat_flush_metadata 0x827cc4 : { *kernel_fat_metadata.o(.text.kernel_fat_flush_metadata_827cc4) }
    ASSERT(kernel_fat_flush_metadata_827cc4 == 0x827cc4 && ADDR(.fat_flush_metadata) + SIZEOF(.fat_flush_metadata) <= 0x827d49, "fat_flush_metadata overflow")
    .fat_release_root_chain 0x827d4a : { *kernel_fat_mutation.o(.text.kernel_fat_release_root_chain_827d4a) }
    ASSERT(kernel_fat_release_root_chain_827d4a == 0x827d4a && ADDR(.fat_release_root_chain) + SIZEOF(.fat_release_root_chain) <= 0x827e6c, "fat_release_root_chain overflow")
    .fat_release_record_chain 0x827e6c : { *kernel_fat_mutation.o(.text.kernel_fat_release_record_chain_827e6c) }
    ASSERT(kernel_fat_release_record_chain_827e6c == 0x827e6c && ADDR(.fat_release_record_chain) + SIZEOF(.fat_release_record_chain) <= 0x827f58, "fat_release_record_chain overflow")
    .fat_delete_root 0x827f58 : { *kernel_fat_mutation.o(.text.kernel_fat_delete_root_827f58) }
    ASSERT(kernel_fat_delete_root_827f58 == 0x827f58 && ADDR(.fat_delete_root) + SIZEOF(.fat_delete_root) <= 0x827fe2, "fat_delete_root overflow")
    .fat_delete_relative 0x827fe2 : { *kernel_fat_mutation.o(.text.kernel_fat_delete_relative_827fe2) }
    ASSERT(kernel_fat_delete_relative_827fe2 == 0x827fe2 && ADDR(.fat_delete_relative) + SIZEOF(.fat_delete_relative) <= 0x828018, "fat_delete_relative overflow")
    .fat_split_parent 0x828018 : { *kernel_fat_write.o(.text.kernel_fat_split_parent_828018) }
    ASSERT(kernel_fat_split_parent_828018 == 0x828018 && ADDR(.fat_split_parent) + SIZEOF(.fat_split_parent) <= 0x8280e4, "fat_split_parent overflow")
    .fat_write_path 0x8280e4 : { *kernel_fat_write.o(.text.kernel_fat_write_path_8280e4) }
    ASSERT(kernel_fat_write_path_8280e4 == 0x8280e4 && ADDR(.fat_write_path) + SIZEOF(.fat_write_path) <= 0x82867e, "fat_write_path overflow")
    .fat_write_root 0x82867e : { *kernel_fat_write.o(.text.kernel_fat_write_root_82867e) }
    ASSERT(kernel_fat_write_root_82867e == 0x82867e && ADDR(.fat_write_root) + SIZEOF(.fat_write_root) <= 0x828b35, "fat_write_root overflow")
    .fat_relative_read 0x826ade : { *kernel_fat_relative.o(.text.kernel_x_read_relative_826ade) }
    ASSERT(kernel_x_read_relative_826ade == 0x826ade && ADDR(.fat_relative_read) + SIZEOF(.fat_relative_read) <= 0x826c3a, "fat_relative_read overflow")
    .fat_relative_named 0x826c3a : { *kernel_fat_relative.o(.text.kernel_fat_read_relative_named_826c3a) }
    ASSERT(kernel_fat_read_relative_named_826c3a == 0x826c3a && ADDR(.fat_relative_named) + SIZEOF(.fat_relative_named) <= 0x826c6e, "fat_relative_named overflow")
    .fat_relative_cluster 0x826c6e : { *kernel_fat_relative.o(.text.kernel_fat_read_relative_cluster_826c6e) }
    ASSERT(kernel_fat_read_relative_cluster_826c6e == 0x826c6e && ADDR(.fat_relative_cluster) + SIZEOF(.fat_relative_cluster) <= 0x826dd0, "fat_relative_cluster overflow")
    .fat_relative_named_cluster 0x826dd0 : { *kernel_fat_relative.o(.text.kernel_fat_read_relative_named_cluster_826dd0) }
    ASSERT(kernel_fat_read_relative_named_cluster_826dd0 == 0x826dd0 && ADDR(.fat_relative_named_cluster) + SIZEOF(.fat_relative_named_cluster) <= 0x826f38, "fat_relative_named_cluster overflow")
    .fat_relative_alloc_named 0x826f38 : { *kernel_fat_relative.o(.text.kernel_icon_read_relative_826f38) }
    ASSERT(kernel_icon_read_relative_826f38 == 0x826f38 && ADDR(.fat_relative_alloc_named) + SIZEOF(.fat_relative_alloc_named) <= 0x827032, "fat_relative_alloc_named overflow")
    .fat_relative_alloc_index 0x827032 : { *kernel_fat_relative.o(.text.kernel_bitmap_read_relative_827032) }
    ASSERT(kernel_bitmap_read_relative_827032 == 0x827032 && ADDR(.fat_relative_alloc_index) + SIZEOF(.fat_relative_alloc_index) <= 0x82711e, "fat_relative_alloc_index overflow")
    .fat_allocate_index 0x82711e : { *kernel_fat_alloc.o(.text.kernel_fat_allocate_index_82711e) }
    ASSERT(kernel_fat_allocate_index_82711e == 0x82711e && ADDR(.fat_allocate_index) + SIZEOF(.fat_allocate_index) <= 0x8272f4, "fat_allocate_index overflow")
    .fat_allocate_clusters 0x8272f4 : { *kernel_fat_alloc.o(.text.kernel_fat_allocate_clusters_8272f4) }
    ASSERT(kernel_fat_allocate_clusters_8272f4 == 0x8272f4 && ADDR(.fat_allocate_clusters) + SIZEOF(.fat_allocate_clusters) <= 0x827454, "fat_allocate_clusters overflow")
    .fat_read_alloc 0x827454 : { *kernel_fat_files.o(.text.kernel_fat_read_alloc_827454) }
    ASSERT(kernel_fat_read_alloc_827454 == 0x827454 && ADDR(.fat_read_alloc) + SIZEOF(.fat_read_alloc) <= 0x82760c, "fat_read_alloc overflow")
    .fat_read_into 0x82760c : { *kernel_fat_files.o(.text.kernel_x_read_path_82760c) }
    ASSERT(kernel_x_read_path_82760c == 0x82760c && ADDR(.fat_read_into) + SIZEOF(.fat_read_into) <= 0x827798, "fat_read_into overflow")
    .fat_read_name_alloc 0x827798 : { *kernel_fat_files.o(.text.kernel_icon_read_absolute_827798) }
    ASSERT(kernel_icon_read_absolute_827798 == 0x827798 && ADDR(.fat_read_name_alloc) + SIZEOF(.fat_read_name_alloc) <= 0x8277dc, "fat_read_name_alloc overflow")
    .fat_read_name_into 0x8277dc : { *kernel_fat_files.o(.text.kernel_fat_read_named_8277dc) }
    ASSERT(kernel_fat_read_named_8277dc == 0x8277dc && ADDR(.fat_read_name_into) + SIZEOF(.fat_read_name_into) <= 0x827826, "fat_read_name_into overflow")
    .fat_header_relative 0x827826 : { *kernel_fat_files.o(.text.kernel_x_header_relative_827826) }
    ASSERT(kernel_x_header_relative_827826 == 0x827826 && ADDR(.fat_header_relative) + SIZEOF(.fat_header_relative) <= 0x8278f4, "fat_header_relative overflow")
    .fat_header_path 0x8278f4 : { *kernel_fat_files.o(.text.kernel_x_header_path_8278f4) }
    ASSERT(kernel_x_header_path_8278f4 == 0x8278f4 && ADDR(.fat_header_path) + SIZEOF(.fat_header_path) <= 0x82798c, "fat_header_path overflow")
    .fat_root_empty 0x82798c : { *kernel_fat_slots.o(.text.kernel_fat_root_empty_82798c) }
    ASSERT(kernel_fat_root_empty_82798c == 0x82798c && ADDR(.fat_root_empty) + SIZEOF(.fat_root_empty) <= 0x827a40, "fat_root_empty overflow")
    .fat_directory_empty 0x827a40 : { *kernel_fat_slots.o(.text.kernel_fat_directory_empty_827a40) }
    ASSERT(kernel_fat_directory_empty_827a40 == 0x827a40 && ADDR(.fat_directory_empty) + SIZEOF(.fat_directory_empty) <= 0x827ace, "fat_directory_empty overflow")
    .fat_find_free_cluster 0x827ace : { *kernel_fat_slots.o(.text.kernel_fat_find_free_cluster_827ace) }
    ASSERT(kernel_fat_find_free_cluster_827ace == 0x827ace && ADDR(.fat_find_free_cluster) + SIZEOF(.fat_find_free_cluster) <= 0x827b44, "fat_find_free_cluster overflow")
    .fat_find_free_after 0x827b44 : { *kernel_fat_slots.o(.text.kernel_fat_find_free_after_827b44) }
    ASSERT(kernel_fat_find_free_after_827b44 == 0x827b44 && ADDR(.fat_find_free_after) + SIZEOF(.fat_find_free_after) <= 0x827bba, "fat_find_free_after overflow")
    .font_context_bounds 0x82e05c : { *kernel_font_context.o(.text.kernel_font_context_bounds_82e05c) }
    ASSERT(kernel_font_context_bounds_82e05c == 0x82e05c && SIZEOF(.font_context_bounds) <= 132, "font_context_bounds overflow")
    .font_context_vertex 0x82e0e0 : { *kernel_font_context.o(.text.kernel_font_context_vertex_82e0e0) }
    ASSERT(kernel_font_context_vertex_82e0e0 == 0x82e0e0 && SIZEOF(.font_context_vertex) <= 246, "font_context_vertex overflow")
    .font_context_close 0x82e1d6 : { *kernel_font_context.o(.text.kernel_font_context_close_82e1d6) }
    ASSERT(kernel_font_context_close_82e1d6 == 0x82e1d6 && SIZEOF(.font_context_close) <= 166, "font_context_close overflow")
    .font_context_move 0x82e27c : { *kernel_font_context.o(.text.kernel_font_context_move_82e27c) }
    ASSERT(kernel_font_context_move_82e27c == 0x82e27c && SIZEOF(.font_context_move) <= 184, "font_context_move overflow")
    .font_context_line 0x82e334 : { *kernel_font_context.o(.text.kernel_font_context_line_82e334) }
    ASSERT(kernel_font_context_line_82e334 == 0x82e334 && SIZEOF(.font_context_line) <= 148, "font_context_line overflow")
    .font_context_curve 0x82e3c8 : { *kernel_font_context.o(.text.kernel_font_context_curve_82e3c8) }
    ASSERT(kernel_font_context_curve_82e3c8 == 0x82e3c8 && SIZEOF(.font_context_curve) <= 228, "font_context_curve overflow")
    .font_close_shape 0x82d24a : { *kernel_font_outline.o(.text.kernel_font_close_shape_82d24a) }
    ASSERT(kernel_font_close_shape_82d24a == 0x82d24a && SIZEOF(.font_close_shape) <= 316, "font_close_shape overflow")
    .font_subroutine 0x82e4ac : { *kernel_font_outline.o(.text.kernel_font_subroutine_82e4ac) }
    ASSERT(kernel_font_subroutine_82e4ac == 0x82e4ac && SIZEOF(.font_subroutine) <= 166, "font_subroutine overflow")
    .font_cid_subrs 0x82e552 : { *kernel_font_outline.o(.text.kernel_font_cid_subrs_82e552) }
    ASSERT(kernel_font_cid_subrs_82e552 == 0x82e552 && SIZEOF(.font_cid_subrs) <= 414, "font_cid_subrs overflow")
    .font_bitmap_box_subpixel 0x8308d2 : { *kernel_font_outline.o(.text.kernel_font_bitmap_box_subpixel_8308d2) }
    ASSERT(kernel_font_bitmap_box_subpixel_8308d2 == 0x8308d2 && SIZEOF(.font_bitmap_box_subpixel) <= 362, "font_bitmap_box_subpixel overflow")
    .font_bitmap_box 0x830a3c : { *kernel_font_outline.o(.text.kernel_font_bitmap_box_830a3c) }
    ASSERT(kernel_font_bitmap_box_830a3c == 0x830a3c && SIZEOF(.font_bitmap_box) <= 86, "font_bitmap_box overflow")
    .font_codepoint_bitmap_box_subpixel 0x830a92 : { *kernel_font_outline.o(.text.kernel_font_codepoint_bitmap_box_subpixel_830a92) }
    ASSERT(kernel_font_codepoint_bitmap_box_subpixel_830a92 == 0x830a92 && SIZEOF(.font_codepoint_bitmap_box_subpixel) <= 100, "font_codepoint_bitmap_box_subpixel overflow")
    .font_codepoint_bitmap_box 0x830af6 : { *kernel_font_outline.o(.text.kernel_font_codepoint_bitmap_box_830af6) }
    ASSERT(kernel_font_codepoint_bitmap_box_830af6 == 0x830af6 && SIZEOF(.font_codepoint_bitmap_box) <= 86, "font_codepoint_bitmap_box overflow")
    .font_heap_alloc 0x830b4c : { *kernel_font_heap.o(.text.kernel_font_heap_alloc_830b4c) }
    ASSERT(kernel_font_heap_alloc_830b4c == 0x830b4c && SIZEOF(.font_heap_alloc) <= 194, "font_heap_alloc overflow")
    .font_heap_free 0x830c0e : { *kernel_font_heap.o(.text.kernel_font_heap_free_830c0e) }
    ASSERT(kernel_font_heap_free_830c0e == 0x830c0e && SIZEOF(.font_heap_free) <= 26, "font_heap_free overflow")
    .font_heap_cleanup 0x830c28 : { *kernel_font_heap.o(.text.kernel_font_heap_cleanup_830c28) }
    ASSERT(kernel_font_heap_cleanup_830c28 == 0x830c28 && SIZEOF(.font_heap_cleanup) <= 50, "font_heap_cleanup overflow")
    .unexpected : { *(.text* .rodata* .data* .bss* COMMON) }
    ASSERT(SIZEOF(.unexpected) == 0, "unexpected allocated kernel data")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
