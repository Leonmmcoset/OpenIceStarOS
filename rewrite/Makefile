CC := gcc
LD := ld
OBJCOPY := objcopy
CFLAGS := -m16 -march=i386 -Os -ffreestanding -fno-pic -fno-pie \
          -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra
BUILD := build
IMAGE := ../IceStarOS_x86_32M.vhd
KERNEL_CFLAGS := -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie \
                 -fno-stack-protector -fno-asynchronous-unwind-tables \
                 -ffunction-sections -fdata-sections -Wall -Wextra -Werror
APP_CFLAGS := $(KERNEL_CFLAGS) -fno-optimize-sibling-calls
KERNEL_OBJECTS := $(BUILD)/kernel_entry.o $(BUILD)/kernel_state.o $(BUILD)/kernel_reset.o \
                  $(BUILD)/kernel_api_table.o $(BUILD)/kernel_service_table.o $(BUILD)/kernel_events.o $(BUILD)/kernel_poll.o $(BUILD)/kernel_pointer.o \
                  $(BUILD)/kernel_queue.o $(BUILD)/kernel_pointer_decode.o $(BUILD)/kernel_pointer_init.o \
                  $(BUILD)/kernel_input_queue.o $(BUILD)/kernel_keyboard_init.o $(BUILD)/kernel_keyboard.o $(BUILD)/kernel_cpu.o \
                  $(BUILD)/kernel_physical.o $(BUILD)/kernel_service_forward.o $(BUILD)/kernel_runtime.o $(BUILD)/kernel_tasks.o $(BUILD)/kernel_task_create.o $(BUILD)/kernel_task_lifecycle.o \
                  $(BUILD)/kernel_segments.o $(BUILD)/kernel_strings.o $(BUILD)/kernel_string_extra.o $(BUILD)/kernel_memory.o \
                  $(BUILD)/kernel_memory_init.o $(BUILD)/kernel_memory_probe.o $(BUILD)/kernel_memory_ops.o \
                  $(BUILD)/kernel_memory_accel.o $(BUILD)/kernel_memory_transfers.o \
                  $(BUILD)/kernel_graphics_primitives.o $(BUILD)/kernel_service_optional.o \
                  $(BUILD)/kernel_cpuid.o $(BUILD)/kernel_fpu.o $(BUILD)/kernel_fpu_state.o \
                  $(BUILD)/kernel_interrupts.o $(BUILD)/kernel_interrupt_hardware.o $(BUILD)/kernel_interrupt_entries.o \
                  $(BUILD)/kernel_interrupt_callbacks.o $(BUILD)/kernel_cmos_clock.o $(BUILD)/kernel_syscalls.o \
                  $(BUILD)/kernel_syscall_services.o $(BUILD)/kernel_timestamp.o $(BUILD)/kernel_syscall_surface.o \
                  $(BUILD)/kernel_surface_refresh.o $(BUILD)/kernel_surface_composite.o \
                  $(BUILD)/kernel_surface_present.o $(BUILD)/kernel_surface_draw.o \
                  $(BUILD)/kernel_surface_alpha.o $(BUILD)/kernel_surface_draw32.o \
                  $(BUILD)/kernel_surface_move.o $(BUILD)/kernel_surface_scale.o \
                  $(BUILD)/kernel_surface_lifecycle.o $(BUILD)/kernel_surface_order.o $(BUILD)/kernel_cursor.o \
                  $(BUILD)/kernel_firmware_scan.o \
                  $(BUILD)/kernel_startup_state.o $(BUILD)/kernel_startup_apps.o $(BUILD)/kernel_x_loader.o $(BUILD)/kernel_display_palette.o \
                  $(BUILD)/kernel_display_dac.o $(BUILD)/kernel_paging.o \
                  $(BUILD)/kernel_startup_desktop.o $(BUILD)/kernel_startup_ui.o \
                  $(BUILD)/kernel_object_registry.o \
                  $(BUILD)/kernel_timer_objects.o $(BUILD)/kernel_clock_display.o $(BUILD)/kernel_cmos_fields.o \
                  $(BUILD)/kernel_icon_directory.o $(BUILD)/kernel_icon_files.o $(BUILD)/kernel_x_icon.o $(BUILD)/kernel_icon_dib.o $(BUILD)/kernel_icon_pixels.o $(BUILD)/kernel_pixel_access.o $(BUILD)/kernel_bitmap.o $(BUILD)/kernel_bitmap_files.o $(BUILD)/kernel_bitmap_fit.o $(BUILD)/kernel_bitmap_surface_fit.o $(BUILD)/kernel_bitmap_key.o $(BUILD)/kernel_jpeg_bits.o $(BUILD)/kernel_jpeg_frame.o $(BUILD)/kernel_jpeg_tables.o $(BUILD)/kernel_jpeg_huffman.o \
                  $(BUILD)/kernel_pixel_sample.o $(BUILD)/kernel_raster_line.o $(BUILD)/kernel_raster_fill.o $(BUILD)/kernel_raster_outline.o $(BUILD)/kernel_raster_circle.o $(BUILD)/kernel_raster_disc.o $(BUILD)/kernel_raster_gradient.o $(BUILD)/kernel_raster_brush.o $(BUILD)/kernel_raster_rounded.o $(BUILD)/kernel_text_glyph.o $(BUILD)/kernel_text_wide.o $(BUILD)/kernel_text_run.o $(BUILD)/kernel_text_number.o $(BUILD)/kernel_text_format.o $(BUILD)/kernel_math_series.o $(BUILD)/kernel_math_extra.o $(BUILD)/kernel_math_format.o $(BUILD)/kernel_simd.o
KERNEL_PATCHES := $(BUILD)/kernel.entry.bin $(BUILD)/kernel.bootstrap.bin \
                  $(BUILD)/kernel.display_state.bin $(BUILD)/kernel.object_table_state.bin \
                  $(BUILD)/kernel.startup_apps.bin $(BUILD)/kernel.display_palette.bin $(BUILD)/kernel.display_colors.bin \
                  $(BUILD)/kernel.display_dac.bin $(BUILD)/kernel.identity_paging.bin \
                  $(BUILD)/kernel.startup_desktop.bin $(BUILD)/kernel.startup_ui.bin \
                  $(BUILD)/kernel.object_find_name.bin $(BUILD)/kernel.object_set_state.bin \
                  $(BUILD)/kernel.object_registry_add.bin $(BUILD)/kernel.object_registry_remove.bin \
                  $(BUILD)/kernel.timer_find_free.bin $(BUILD)/kernel.timer_create.bin $(BUILD)/kernel.timer_delete.bin $(BUILD)/kernel.timer_rearm.bin \
                  $(BUILD)/kernel.clock_draw_time.bin $(BUILD)/kernel.clock_draw_date.bin \
                  $(BUILD)/kernel.icon_draw_id.bin $(BUILD)/kernel.icon_draw_centered.bin $(BUILD)/kernel.icon_draw_frame.bin \
                  $(BUILD)/kernel.x_icon_pointer.bin $(BUILD)/kernel.icon_draw_file.bin $(BUILD)/kernel.icon_draw_file_auto.bin \
                  $(BUILD)/kernel.icon_dib.bin $(BUILD)/kernel.icon_put8.bin $(BUILD)/kernel.icon_put32.bin $(BUILD)/kernel.icon_quantize.bin $(BUILD)/kernel.bitmap_draw.bin $(BUILD)/kernel.bitmap_draw_file.bin $(BUILD)/kernel.bitmap_fit_file.bin $(BUILD)/kernel.bitmap_fit.bin $(BUILD)/kernel.bitmap_surface_fit.bin $(BUILD)/kernel.bitmap_key.bin $(BUILD)/kernel.jpeg_bits.bin $(BUILD)/kernel.jpeg_frame.bin $(BUILD)/kernel.jpeg_tables.bin $(BUILD)/kernel.jpeg_huffman.bin \
                  $(BUILD)/kernel.pixel_put8.bin $(BUILD)/kernel.pixel_put32.bin $(BUILD)/kernel.pixel_get8.bin $(BUILD)/kernel.pixel_get32.bin \
                  $(BUILD)/kernel.pixel_sample.bin $(BUILD)/kernel.raster_line8.bin $(BUILD)/kernel.raster_line32.bin \
                  $(BUILD)/kernel.brush_square.bin $(BUILD)/kernel.brush_line.bin $(BUILD)/kernel.brush_circle.bin $(BUILD)/kernel.circle_quadrants.bin \
                  $(BUILD)/kernel.rounded_outline.bin $(BUILD)/kernel.rounded_fill.bin $(BUILD)/kernel.chamfer_fill.bin \
                  $(BUILD)/kernel.text_glyph8.bin $(BUILD)/kernel.text_rotated8.bin $(BUILD)/kernel.text_glyph32.bin $(BUILD)/kernel.text_rotated32.bin \
                  $(BUILD)/kernel.text_wide8.bin $(BUILD)/kernel.text_wide8_mask.bin $(BUILD)/kernel.text_wide32.bin $(BUILD)/kernel.text_wide32_mask.bin $(BUILD)/kernel.text_run8.bin $(BUILD)/kernel.text_vertical8.bin $(BUILD)/kernel.text_run32.bin $(BUILD)/kernel.text_vertical32.bin \
                  $(BUILD)/kernel.text_number8.bin $(BUILD)/kernel.text_number32.bin $(BUILD)/kernel.number_digits.bin \
                  $(BUILD)/kernel.text_format8.bin $(BUILD)/kernel.text_format32.bin $(BUILD)/kernel.text_format.bin \
                  $(BUILD)/kernel.raster_fill8.bin $(BUILD)/kernel.raster_fill32.bin \
                  $(BUILD)/kernel.raster_gradient32.bin \
                  $(BUILD)/kernel.raster_outline8.bin $(BUILD)/kernel.raster_outline32.bin \
                  $(BUILD)/kernel.raster_circle8.bin $(BUILD)/kernel.raster_circle32.bin \
                  $(BUILD)/kernel.raster_disc8.bin $(BUILD)/kernel.raster_disc32.bin \
                  $(BUILD)/kernel.simd_add.bin $(BUILD)/kernel.simd_subtract.bin $(BUILD)/kernel.simd_multiply.bin $(BUILD)/kernel.simd_blend.bin \
                  $(BUILD)/kernel.acos.bin $(BUILD)/kernel.round.bin $(BUILD)/kernel.absolute.bin $(BUILD)/kernel.format_float.bin $(BUILD)/kernel.floor.bin $(BUILD)/kernel.ceil.bin $(BUILD)/kernel.remainder.bin $(BUILD)/kernel.sqrt.bin \
                  $(BUILD)/kernel.factorial.bin $(BUILD)/kernel.sine.bin $(BUILD)/kernel.cosine.bin $(BUILD)/kernel.power.bin \
                  $(BUILD)/kernel.cmos_seconds.bin $(BUILD)/kernel.cmos_minutes.bin $(BUILD)/kernel.cmos_hours.bin $(BUILD)/kernel.cmos_weekday.bin $(BUILD)/kernel.cmos_day.bin $(BUILD)/kernel.cmos_month.bin $(BUILD)/kernel.cmos_year.bin \
                  $(BUILD)/kernel.state.bin $(BUILD)/kernel.reset.bin \
                  $(BUILD)/kernel.api.bin $(BUILD)/kernel.services.bin \
                  $(BUILD)/kernel.queue_request.bin $(BUILD)/kernel.event_loop.bin \
                  $(BUILD)/kernel.poll_events.bin $(BUILD)/kernel.dispatch_event.bin \
                  $(BUILD)/kernel.dispatch_request.bin $(BUILD)/kernel.dispatch_slot1.bin \
                  $(BUILD)/kernel.dispatch_slot2.bin $(BUILD)/kernel.dispatch_slot3.bin $(BUILD)/kernel.pointer_input.bin \
                  $(BUILD)/kernel.queue_write.bin $(BUILD)/kernel.queue_read.bin \
                  $(BUILD)/kernel.queue_available.bin $(BUILD)/kernel.pointer_decode.bin \
                  $(BUILD)/kernel.x_load_relative.bin $(BUILD)/kernel.x_load_path.bin $(BUILD)/kernel.x_launch.bin \
                  $(BUILD)/kernel.pointer_init.bin $(BUILD)/kernel.ps2_ready.bin \
                  $(BUILD)/kernel.input_write.bin $(BUILD)/kernel.input_read.bin \
                  $(BUILD)/kernel.input_available.bin $(BUILD)/kernel.ascii_upper.bin \
                  $(BUILD)/kernel.keyboard_ready.bin $(BUILD)/kernel.keyboard_discard.bin \
                  $(BUILD)/kernel.keyboard_leds.bin $(BUILD)/kernel.keyboard_init.bin \
                  $(BUILD)/kernel.keyboard_translate.bin $(BUILD)/kernel.keyboard_drain.bin \
                  $(BUILD)/kernel.keyboard_state.bin $(BUILD)/kernel.keyboard_character.bin \
                  $(BUILD)/kernel.cpu_halt.bin $(BUILD)/kernel.cpu_cli.bin $(BUILD)/kernel.cpu_sti.bin \
                  $(BUILD)/kernel.cpu_delay.bin $(BUILD)/kernel.cpu_flags_read.bin $(BUILD)/kernel.cpu_flags_write.bin \
                  $(BUILD)/kernel.port_read_byte.bin $(BUILD)/kernel.port_write_byte.bin \
                  $(BUILD)/kernel.port_read_word.bin $(BUILD)/kernel.port_write_word.bin \
                  $(BUILD)/kernel.port_read_dword.bin $(BUILD)/kernel.port_write_dword.bin $(BUILD)/kernel.cpu_boot_jump.bin \
                  $(BUILD)/kernel.physical_write_byte.bin $(BUILD)/kernel.physical_write_word.bin \
                  $(BUILD)/kernel.physical_write_dword.bin $(BUILD)/kernel.physical_read_byte.bin \
                  $(BUILD)/kernel.physical_read_word.bin $(BUILD)/kernel.physical_read_dword.bin \
                  $(BUILD)/kernel.cpu_cr0_read.bin $(BUILD)/kernel.cpu_cr0_write.bin \
                  $(foreach slot,00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13,$(BUILD)/kernel.service_$(slot).bin) \
                  $(BUILD)/kernel.palette_color.bin $(BUILD)/kernel.wait_ticks.bin \
                  $(BUILD)/kernel.yield_task.bin $(BUILD)/kernel.far_task_jump.bin $(BUILD)/kernel.fpu_reset.bin \
                  $(BUILD)/kernel.schedule_task.bin $(BUILD)/kernel.timer_tick.bin \
                  $(BUILD)/kernel.tasks_init.bin $(BUILD)/kernel.task_find_free.bin $(BUILD)/kernel.task_create.bin \
                  $(BUILD)/kernel.cpu_load_gdt.bin $(BUILD)/kernel.cpu_load_tr.bin \
                  $(BUILD)/kernel.task_create_app.bin $(BUILD)/kernel.task_delete.bin $(BUILD)/kernel.task_create_thread.bin $(BUILD)/kernel.task_active.bin \
                  $(BUILD)/kernel.segment_descriptor.bin $(BUILD)/kernel.string_copy.bin $(BUILD)/kernel.text_prefix.bin \
                  $(BUILD)/kernel.string_limit.bin $(BUILD)/kernel.string_length.bin $(BUILD)/kernel.string_equal.bin \
                  $(BUILD)/kernel.string_append_character.bin $(BUILD)/kernel.string_append.bin \
                  $(BUILD)/kernel.string_upper.bin $(BUILD)/kernel.string_find.bin $(BUILD)/kernel.config_value.bin \
                  $(BUILD)/kernel.string_character.bin $(BUILD)/kernel.character_upper.bin $(BUILD)/kernel.unsigned_text.bin $(BUILD)/kernel.string_split.bin \
                  $(BUILD)/kernel.allocate.bin $(BUILD)/kernel.allocate_owner.bin $(BUILD)/kernel.allocate_aligned.bin \
                  $(BUILD)/kernel.free_allocation.bin $(BUILD)/kernel.allocation_size.bin $(BUILD)/kernel.allocation_total.bin \
                  $(BUILD)/kernel.memory_probe.bin $(BUILD)/kernel.heap_init.bin $(BUILD)/kernel.page_probe.bin \
                  $(BUILD)/kernel.move_bytes.bin $(BUILD)/kernel.fill_bytes.bin $(BUILD)/kernel.copy_1200.bin \
                  $(BUILD)/kernel.copy_dwords.bin $(BUILD)/kernel.memory_differs.bin \
                  $(BUILD)/kernel.malloc.bin $(BUILD)/kernel.free.bin \
                  $(BUILD)/kernel.copy_dwords_sse.bin $(BUILD)/kernel.copy_sse16.bin $(BUILD)/kernel.copy_sse64.bin \
                  $(BUILD)/kernel.rep_copy_bytes.bin $(BUILD)/kernel.rep_fill_bytes.bin \
                  $(BUILD)/kernel.rep_copy_words.bin $(BUILD)/kernel.rep_fill_words.bin \
                  $(BUILD)/kernel.rep_copy_dwords.bin $(BUILD)/kernel.rep_fill_dwords.bin \
                  $(BUILD)/kernel.wait_vertical_retrace.bin $(BUILD)/kernel.blend_color.bin \
                  $(foreach slot,14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b 2c,$(BUILD)/kernel.optional_$(slot).bin) \
                  $(BUILD)/kernel.cpuid_available.bin $(BUILD)/kernel.cpuid_vendor.bin $(BUILD)/kernel.cpuid_features.bin \
                  $(BUILD)/kernel.fpu_available.bin $(BUILD)/kernel.fpu_enable.bin $(BUILD)/kernel.fpu_restore_task.bin $(BUILD)/kernel.fpu_mark_lazy.bin $(BUILD)/kernel.fpu_fault.bin \
                  $(BUILD)/kernel.fpu_save_legacy.bin $(BUILD)/kernel.fpu_restore_legacy.bin $(BUILD)/kernel.fpu_save_environment.bin $(BUILD)/kernel.fpu_load_environment.bin $(BUILD)/kernel.fpu_save_extended.bin $(BUILD)/kernel.fpu_restore_extended.bin $(BUILD)/kernel.fpu_save_xstate.bin $(BUILD)/kernel.fpu_restore_xstate.bin \
                  $(BUILD)/kernel.interrupts_init.bin $(BUILD)/kernel.interrupt_gate.bin $(BUILD)/kernel.boot_code_selector.bin \
                  $(BUILD)/kernel.pic_init.bin $(BUILD)/kernel.pit_init.bin $(BUILD)/kernel.rtc_init.bin \
                  $(foreach entry,exception_entry spurious_entry exception_fs_entry device_unavailable_entry timer_entry keyboard_entry irq5_entry rtc_entry irq9_entry irq10_entry irq11_entry irq12_entry syscall40_entry syscall41_entry,$(BUILD)/kernel.$(entry).bin) \
                  $(foreach callback,rtc_callback keyboard_callback audio_callback unused_irq_callback mouse_callback,$(BUILD)/kernel.$(callback).bin) \
                  $(BUILD)/kernel.read_cmos_clock.bin $(BUILD)/kernel.dispatch_syscall40.bin $(BUILD)/kernel.dispatch_syscall41.bin \
                  $(BUILD)/kernel.syscall_object_set.bin $(BUILD)/kernel.syscall_object_field12.bin \
                  $(BUILD)/kernel.syscall_object_field4.bin $(BUILD)/kernel.random_range.bin $(BUILD)/kernel.timestamp_low.bin \
                  $(BUILD)/kernel.syscall_surface.bin $(BUILD)/kernel.surface_refresh.bin \
                  $(BUILD)/kernel.surface_composite.bin $(BUILD)/kernel.surface_regions.bin \
                  $(BUILD)/kernel.surface_present8.bin $(BUILD)/kernel.surface_present32.bin \
                  $(BUILD)/kernel.surface_draw8.bin $(BUILD)/kernel.surface_blend4.bin \
                  $(BUILD)/kernel.surface_draw32.bin $(BUILD)/kernel.surface_move.bin $(BUILD)/kernel.surface_scale.bin \
                  $(BUILD)/kernel.surfaces_init.bin $(BUILD)/kernel.surface_create.bin \
                  $(BUILD)/kernel.surface_find_free.bin $(BUILD)/kernel.surface_delete.bin \
                  $(BUILD)/kernel.surface_order_remove.bin $(BUILD)/kernel.surface_order_append.bin \
                  $(BUILD)/kernel.surface_order_front.bin $(BUILD)/kernel.surface_order_move.bin \
                  $(BUILD)/kernel.cursor_show.bin $(BUILD)/kernel.cursor_hide.bin \
                  $(BUILD)/kernel.cursor_select.bin $(BUILD)/kernel.cursor_move.bin \
                  $(BUILD)/kernel.scan_acpi.bin $(BUILD)/kernel.scan_pmid.bin

KERNEL_OBJECTS += $(foreach module,scan basis transform block output decode,$(BUILD)/kernel_jpeg_$(module).o)
KERNEL_PATCHES += $(foreach name,info decode scan prepare symbol basis transform magnitude block expand mcu color image,$(BUILD)/kernel.jpeg_$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_font_cmap.o
KERNEL_PATCHES += $(BUILD)/kernel.font_cmap.bin
KERNEL_OBJECTS += $(BUILD)/kernel_font_init.o
KERNEL_PATCHES += $(BUILD)/kernel.font_init.bin
KERNEL_OBJECTS += $(BUILD)/kernel_font_glyph.o $(BUILD)/kernel_font_kern.o
KERNEL_PATCHES += $(foreach name,font_vertex font_glyph_offset font_glyph_box font_codepoint_box font_glyph_empty font_cff_box font_kern_length font_kern_table font_legacy_kern font_coverage font_class font_gpos_kern font_glyph_kern font_codepoint_kern font_codepoint_metrics,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_font_metrics.o
KERNEL_PATCHES += $(foreach name,font_hmetrics font_vmetrics font_typo_metrics font_bounds font_scale_height font_scale_em font_svg_record font_glyph_svg font_codepoint_svg,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_font_parse.o
KERNEL_PATCHES += $(foreach name,font_cff_subrs font_svg font_get8 font_peek8 font_seek font_skip font_getn font_buffer font_range font_cff_index font_cff_int font_cff_skip font_cff_dict font_cff_ints font_cff_count font_cff_item font_u16 font_s16 font_u32 font_s32 font_signature font_table font_offset font_count,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_block.o
KERNEL_PATCHES += $(foreach name,x_lba_alloc x_lba_into fat_sectors_alloc fat_read_sectors fat_read_partition fat_read_root_directory fat_alloc_root_directory fat_alloc_directory,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_files.o
KERNEL_PATCHES += $(foreach name,fat_read_alloc fat_read_into fat_read_name_alloc fat_read_name_into fat_header_relative fat_header_path,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_relative.o
KERNEL_PATCHES += $(foreach name,fat_relative_read fat_relative_named fat_relative_cluster fat_relative_named_cluster fat_relative_alloc_named fat_relative_alloc_index,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_path.o
KERNEL_PATCHES += $(foreach name,fat_find_path fat_name83,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_io.o
KERNEL_PATCHES += $(BUILD)/kernel.fat_write_sectors.bin
KERNEL_OBJECTS += $(BUILD)/kernel_fat_metadata.o
KERNEL_PATCHES += $(BUILD)/kernel.fat_flush_metadata.bin
KERNEL_OBJECTS += $(BUILD)/kernel_fat_mutation.o
KERNEL_PATCHES += $(foreach name,fat_release_root_chain fat_release_record_chain fat_delete_root fat_delete_relative,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_write.o
KERNEL_PATCHES += $(foreach name,fat_split_parent fat_write_path fat_write_root,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_directory_path.o
KERNEL_PATCHES += $(BUILD)/kernel.fat_find_directory_path.bin
KERNEL_OBJECTS += $(BUILD)/kernel_fat_alloc.o
KERNEL_PATCHES += $(foreach name,fat_allocate_index fat_allocate_clusters,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_slots.o
KERNEL_PATCHES += $(foreach name,fat_root_empty fat_directory_empty fat_find_free_cluster fat_find_free_after,$(BUILD)/kernel.$(name).bin)
KERNEL_OBJECTS += $(BUILD)/kernel_fat_directory.o
KERNEL_PATCHES += $(foreach name,fat_root_name fat_directory_name fat_root_extension fat_directory_extension fat_find_relative fat_directory_find fat_root_size fat_directory_size fat_root_attributes fat_directory_attributes fat_root_date fat_directory_date fat_root_time fat_directory_time fat_root_find_directory fat_find_directory,$(BUILD)/kernel.$(name).bin)

.PHONY: all verify run run-hybrid run-c-init run-c-snake smoke-snake run-c-apps smoke-color smoke-mov smoke-starscr smoke-starscr-reference smoke-tasks smoke-memory test-x-loader
all: $(BUILD)/OpenIceStarOS_c_mbr.vhd

.PHONY: test-boot-video-preview
test-boot-video-preview:
	python3 tests/boot_video.py

.PHONY: test-boot-video test-boot-stage3 test-boot-stage3-qemu
test-boot-video: $(BUILD)/stage3_hybrid.bin $(BUILD)/stage3_c_init.bin
	python3 tests/boot_video.py --installed

test-boot-stage3: $(BUILD)/stage3_hybrid.bin $(BUILD)/stage3_c_init.bin
	python3 tests/boot_stage3.py

test-boot-stage3-qemu: $(BUILD)/OpenIceStarOS_c_init.vhd
	python3 tests/boot_stage3_qemu.py

test-x-loader: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_x_loader.py --installed

verify: test-x-loader

.PHONY: test-branding
test-branding: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tests/branding.py
	python3 tools/branding.py scan $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/branding_residuals.json

verify: test-branding

.PHONY: test-fat-files
test-fat-files: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_files.py --installed

verify: test-fat-files

.PHONY: test-fat-io
test-fat-io: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_io.py --installed

verify: test-fat-io

.PHONY: test-fat-metadata
test-fat-metadata: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_metadata.py --installed

verify: test-fat-metadata

.PHONY: test-fat-mutation
test-fat-mutation: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_mutation.py --installed

verify: test-fat-mutation

.PHONY: test-fat-write
test-fat-write: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_write.py --installed

verify: test-fat-write

.PHONY: test-fat-slots
test-fat-slots: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_slots.py --installed

verify: test-fat-slots

.PHONY: test-fat-path
test-fat-path: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_path.py --installed

verify: test-fat-path

.PHONY: test-fat-directory
test-fat-directory: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_directory.py --installed

verify: test-fat-directory

verify: test-boot-video test-boot-stage3

$(BUILD):
	mkdir -p $@

$(BUILD)/mbr_entry.o: boot/mbr_entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mbr.o: boot/mbr.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/mbr.bin: $(BUILD)/mbr_entry.o $(BUILD)/mbr.o boot/mbr.ld
	$(LD) -m elf_i386 -T boot/mbr.ld -o $@ $(BUILD)/mbr_entry.o $(BUILD)/mbr.o

$(BUILD)/menu_entry.o: boot/menu_entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/menu.o: boot/menu.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/menu_hybrid.o: boot/menu.c | $(BUILD)
	$(CC) $(CFLAGS) -DNEXT_STAGE_SECTORS=8 -c $< -o $@

$(BUILD)/menu.bin: $(BUILD)/menu_entry.o $(BUILD)/menu.o boot/menu.ld
	$(LD) -m elf_i386 -T boot/menu.ld -o $@ $(BUILD)/menu_entry.o $(BUILD)/menu.o

$(BUILD)/menu_hybrid.bin: $(BUILD)/menu_entry.o $(BUILD)/menu_hybrid.o boot/menu.ld
	$(LD) -m elf_i386 -T boot/menu.ld -o $@ $(BUILD)/menu_entry.o $(BUILD)/menu_hybrid.o

BRANDING := tools/branding.py branding/patches.json

$(BUILD)/OpenIceStarOS_c_mbr.vhd: $(IMAGE) $(BUILD)/mbr.bin $(BUILD)/menu.bin $(BRANDING)
	python3 tools/branding.py install $(IMAGE) $@
	dd if=$(BUILD)/mbr.bin of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD)/menu.bin of=$@ bs=512 seek=1 count=3 conv=notrunc status=none

$(BUILD)/fat16.o: boot/fat16.c boot/fat16.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/stage3_hybrid.o: boot/stage3_hybrid.S | $(BUILD)
	$(CC) -m32 -c $< -o $@

$(BUILD)/stage3_c_init.o: boot/stage3_hybrid.S | $(BUILD)
	$(CC) -m32 -DREWRITE_INIT -c $< -o $@

$(BUILD)/stage3_load.o: boot/stage3_load.c boot/fat16.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/stage3_load_c_init.o: boot/stage3_load.c boot/fat16.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DREWRITE_INIT -c $< -o $@

$(BUILD)/init_transfer.o: boot/init_transfer.c boot/init_transfer.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/pci.o: boot/pci.c boot/pci.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/stage3_video16.o: boot/stage3_video.c boot/stage3_video.h | $(BUILD)
	$(CC) $(CFLAGS) -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -Werror -c $< -o $@

$(BUILD)/stage3_video_io.o: boot/stage3_video_io.S | $(BUILD)
	$(CC) -m32 -c $< -o $@

$(BUILD)/stage3_display.o: boot/stage3_display.c | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

STAGE3_COMMON := $(BUILD)/stage3_video16.o $(BUILD)/stage3_video_io.o $(BUILD)/stage3_display.o $(BUILD)/fat16.o $(BUILD)/init_transfer.o $(BUILD)/pci.o

$(BUILD)/stage3_hybrid.elf: $(BUILD)/stage3_hybrid.o $(BUILD)/stage3_load.o $(STAGE3_COMMON) boot/stage3_hybrid.ld
	$(LD) -m elf_i386 -T boot/stage3_hybrid.ld -Map $(BUILD)/stage3_hybrid.map -o $@ $(BUILD)/stage3_hybrid.o $(BUILD)/stage3_load.o $(STAGE3_COMMON)

$(BUILD)/stage3_c_init.elf: $(BUILD)/stage3_c_init.o $(BUILD)/stage3_load_c_init.o $(STAGE3_COMMON) boot/stage3_hybrid.ld
	$(LD) -m elf_i386 -T boot/stage3_hybrid.ld -Map $(BUILD)/stage3_c_init.map -o $@ $(BUILD)/stage3_c_init.o $(BUILD)/stage3_load_c_init.o $(STAGE3_COMMON)

$(BUILD)/stage3_%.bin: $(BUILD)/stage3_%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/OpenIceStarOS_c_fat16.vhd: $(IMAGE) $(BUILD)/mbr.bin $(BUILD)/menu_hybrid.bin $(BUILD)/stage3_hybrid.bin $(BRANDING)
	python3 tools/branding.py install $(IMAGE) $@
	dd if=$(BUILD)/mbr.bin of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD)/menu_hybrid.bin of=$@ bs=512 seek=1 count=3 conv=notrunc status=none
	dd if=$(BUILD)/stage3_hybrid.bin of=$@ bs=512 seek=5 count=8 conv=notrunc status=none

$(BUILD)/fat16_image: tests/fat16_image.c boot/fat16.c boot/fat16.h | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iboot tests/fat16_image.c boot/fat16.c -o $@

$(BUILD)/init_entry.o: init/entry.c init/entry.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/init_directory.o: init/directory.c init/directory.h init/entry.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/init_clusters.o: init/clusters.c init/clusters.h init/entry.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/kernel_%.o: kernel/%.c Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_fat_files.o: kernel/fat_files.c kernel/fat_files.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_relative.o: kernel/fat_relative.c kernel/fat_relative.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_alloc.o: kernel/fat_alloc.c kernel/fat_alloc.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_slots.o: kernel/fat_slots.c kernel/fat_slots.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_directory.o: kernel/fat_directory.c kernel/fat_directory.h kernel/helpers.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_path.o: kernel/fat_path.c kernel/fat_path.h kernel/fat_files.h kernel/memory.h kernel/memory_accel.h kernel/string_extra.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_font_cmap.o: kernel/font_cmap.c kernel/font_cmap.h kernel/font_parse.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_font_init.o: kernel/font_init.c kernel/font_init.h kernel/font_parse.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_font_glyph.o: kernel/font_glyph.c kernel/font_glyph.h kernel/font_cmap.h kernel/font_parse.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_font_kern.o: kernel/font_kern.c kernel/font_kern.h kernel/font_cmap.h kernel/font_metrics.h kernel/font_parse.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_font_metrics.o: kernel/font_metrics.c kernel/font_metrics.h kernel/font_parse.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_font_parse.o: kernel/font_parse.c kernel/font_parse.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_block.o: kernel/fat_block.c kernel/fat_block.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_io.o: kernel/fat_io.c kernel/fat_io.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_metadata.o: kernel/fat_metadata.c kernel/fat_metadata.h kernel/fat_io.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_mutation.o: kernel/fat_mutation.c kernel/fat_mutation.h kernel/fat_metadata.h kernel/fat_directory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_write.o: kernel/fat_write.c kernel/fat_write.h kernel/fat_directory.h kernel/fat_path.h kernel/fat_slots.h kernel/fat_mutation.h kernel/fat_metadata.h kernel/fat_io.h kernel/fat_files.h kernel/memory.h kernel/helpers.h kernel/string_extra.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_fat_directory_path.o: kernel/fat_directory_path.c kernel/fat_path.h kernel/fat_files.h kernel/memory.h kernel/memory_accel.h kernel/string_extra.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_entry.o: kernel/entry.c kernel/entry.h kernel/entry_boundaries.def Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_startup_state.o: kernel/startup_state.c kernel/startup_state.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_startup_apps.o: kernel/startup_apps.c kernel/startup_apps.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_x_loader.o: kernel/x_loader.c kernel/x_loader.h kernel/memory.h kernel/memory_ops.h kernel/tasks.h kernel/cursor.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_display_palette.o: kernel/display_palette.c kernel/display_palette.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_display_dac.o: kernel/display_dac.c kernel/display_dac.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_paging.o: kernel/paging.c kernel/paging.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_startup_desktop.o: kernel/startup_desktop.c kernel/startup_desktop.h kernel/syscalls.h kernel/surface_order.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_startup_ui.o: kernel/startup_ui.c kernel/startup_ui.h kernel/runtime.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_object_registry.o: kernel/object_registry.c kernel/object_registry.h kernel/helpers.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_timer_objects.o: kernel/timer_objects.c kernel/timer_objects.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_clock_display.o: kernel/clock_display.c kernel/clock_display.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_cmos_fields.o: kernel/cmos_fields.c kernel/cmos_fields.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_icon_directory.o: kernel/icon_directory.c kernel/icon_directory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_icon_dib.o: kernel/icon_dib.c kernel/icon_dib.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_icon_pixels.o: kernel/icon_pixels.c kernel/icon_pixels.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_pixel_access.o: kernel/pixel_access.c kernel/pixel_access.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_pixel_sample.o: kernel/pixel_sample.c kernel/pixel_sample.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_raster_line.o: kernel/raster_line.c kernel/raster_line.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_raster_fill.o: kernel/raster_fill.c kernel/raster_fill.h kernel/service_forward.h kernel/service_forward.def Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_raster_outline.o: kernel/raster_outline.c kernel/raster_outline.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_math_series.o: kernel/math_series.c kernel/math_series.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fexcess-precision=fast -frounding-math -fno-math-errno -c $< -o $@

$(BUILD)/kernel_math_extra.o: kernel/math_extra.c kernel/math_extra.h kernel/math_series.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fexcess-precision=fast -frounding-math -fno-math-errno -c $< -o $@

$(BUILD)/kernel_math_format.o: kernel/math_format.c kernel/math_format.h kernel/math_series.h kernel/math_extra.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -fexcess-precision=fast -frounding-math -fno-math-errno -c $< -o $@

$(BUILD)/kernel_simd.o: kernel/simd.c kernel/simd.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

$(BUILD)/kernel_raster_circle.o: kernel/raster_circle.c kernel/raster_circle.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_raster_disc.o: kernel/raster_disc.c kernel/raster_disc.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -fexcess-precision=fast -frounding-math -fno-math-errno -c $< -o $@

$(BUILD)/kernel_raster_gradient.o: kernel/raster_gradient.c kernel/raster_gradient.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_raster_brush.o: kernel/raster_brush.c kernel/raster_brush.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -ffixed-ebx -ffixed-esi -ffixed-edi -c $< -o $@

$(BUILD)/kernel_raster_rounded.o: kernel/raster_rounded.c kernel/raster_rounded.h kernel/service_forward.h kernel/raster_brush.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -fno-defer-pop -c $< -o $@

$(BUILD)/kernel_text_glyph.o: kernel/text_glyph.c kernel/text_glyph.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_text_wide.o: kernel/text_wide.c kernel/text_wide.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_text_run.o: kernel/text_run.c kernel/text_run.h kernel/text_glyph.h kernel/text_wide.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_text_number.o: kernel/text_number.c kernel/text_number.h kernel/text_run.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_text_format.o: kernel/text_format.c kernel/text_format.h kernel/text_glyph.h kernel/text_wide.h kernel/text_run.h kernel/text_number.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_icon_files.o: kernel/icon_files.c kernel/icon_files.h kernel/icon_directory.h kernel/x_icon.h kernel/helpers.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_bitmap.o: kernel/bitmap.c kernel/bitmap.h kernel/pixel_access.h kernel/icon_pixels.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_bitmap_files.o: kernel/bitmap_files.c kernel/bitmap_files.h kernel/bitmap.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_bitmap_fit.o: kernel/bitmap_fit.c kernel/bitmap.h kernel/pixel_access.h kernel/icon_pixels.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_bitmap_surface_fit.o: kernel/bitmap_surface_fit.c kernel/bitmap.h kernel/icon_pixels.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_bitmap_key.o: kernel/bitmap_key.c kernel/bitmap.h kernel/pixel_access.h kernel/text_format.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_jpeg_bits.o: kernel/jpeg_bits.c kernel/jpeg_bits.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_jpeg_frame.o: kernel/jpeg_frame.c kernel/jpeg_frame.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_jpeg_tables.o: kernel/jpeg_tables.c kernel/jpeg_tables.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_jpeg_huffman.o: kernel/jpeg_huffman.c kernel/jpeg_huffman.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(foreach module,scan basis transform block output decode,$(BUILD)/kernel_jpeg_$(module).o): $(BUILD)/kernel_jpeg_%.o: kernel/jpeg_%.c kernel/jpeg_%.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_string_extra.o: kernel/string_extra.c kernel/string_extra.h kernel/helpers.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_x_icon.o: kernel/x_icon.c kernel/x_icon.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-optimize-sibling-calls -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_startup_apps.o: kernel/runtime.h
$(BUILD)/kernel_startup_state.o: kernel/display_palette.h
$(BUILD)/kernel_display_palette.o: kernel/display_dac.h

$(BUILD)/kernel_service_forward.o: kernel/service_forward.h kernel/service_forward.def
$(BUILD)/kernel_service_optional.o: kernel/service_optional.h kernel/service_optional.def
$(BUILD)/kernel_runtime.o: kernel/runtime.h
$(BUILD)/kernel_tasks.o: kernel/runtime.h kernel/service_forward.h kernel/service_forward.def
$(BUILD)/kernel_task_create.o: kernel/tasks.h kernel/helpers.h kernel/memory.h
$(BUILD)/kernel_task_lifecycle.o: kernel/tasks.h kernel/helpers.h kernel/memory.h
$(BUILD)/kernel_segments.o $(BUILD)/kernel_strings.o: kernel/helpers.h
$(BUILD)/kernel_memory.o: kernel/memory.h
$(BUILD)/kernel_memory_init.o $(BUILD)/kernel_memory_probe.o: kernel/memory_init.h
$(BUILD)/kernel_memory_ops.o: kernel/memory_ops.h kernel/memory.h
$(BUILD)/kernel_memory_accel.o $(BUILD)/kernel_memory_transfers.o: kernel/memory_accel.h
$(BUILD)/kernel_graphics_primitives.o: kernel/graphics_primitives.h
$(BUILD)/kernel_fpu.o: kernel/fpu.h kernel/cpuid.h
$(BUILD)/kernel_fpu_state.o: kernel/fpu.h
$(BUILD)/kernel_cpuid.o: kernel/cpuid.h
$(BUILD)/kernel_interrupts.o $(BUILD)/kernel_interrupt_hardware.o: kernel/interrupts.h
$(BUILD)/kernel_interrupt_entries.o: kernel/interrupt_entries.h kernel/interrupt_entries.def
$(BUILD)/kernel_interrupt_callbacks.o $(BUILD)/kernel_cmos_clock.o: kernel/interrupt_callbacks.h
$(BUILD)/kernel_syscalls.o: kernel/syscalls.h kernel/service_forward.h kernel/service_forward.def kernel/runtime.h
$(BUILD)/kernel_syscall_services.o $(BUILD)/kernel_timestamp.o: kernel/syscall_services.h kernel/syscalls.h
$(BUILD)/kernel_syscall_surface.o: kernel/syscall_surface.h kernel/syscalls.h
$(BUILD)/kernel_surface_refresh.o: kernel/surface_refresh.h
$(BUILD)/kernel_surface_composite.o: kernel/surface_composite.h
$(BUILD)/kernel_surface_present.o: kernel/surface_present.h
$(BUILD)/kernel_surface_draw.o: kernel/surface_draw.h kernel/surface_present.h
$(BUILD)/kernel_surface_alpha.o: kernel/surface_alpha.h
$(BUILD)/kernel_surface_draw32.o: kernel/surface_draw.h kernel/surface_present.h
$(BUILD)/kernel_surface_move.o: kernel/surface_move.h kernel/surface_composite.h kernel/surface_present.h
$(BUILD)/kernel_surface_scale.o: kernel/surface_scale.h

# Original i386 callers promise four-byte, not sixteen-byte, stack alignment.
# Use that ABI for this tightly sized allocation/deletion module.
$(BUILD)/kernel_surface_lifecycle.o: kernel/surface_lifecycle.c kernel/surface_lifecycle.h kernel/surface_refresh.h kernel/memory.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_surface_order.o: kernel/surface_order.c kernel/surface_order.h kernel/surface_refresh.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fwrapv-pointer -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_cursor.o: kernel/cursor.c kernel/cursor.h kernel/surface_order.h kernel/surface_refresh.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_firmware_scan.o: kernel/firmware_scan.c kernel/firmware_scan.h Makefile | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fno-delete-null-pointer-checks -mpreferred-stack-boundary=2 -c $< -o $@

$(BUILD)/kernel_patch.elf: $(KERNEL_OBJECTS) kernel/patches.ld
	$(LD) -m elf_i386 -T kernel/patches.ld -Map=$(BUILD)/kernel_patch.map -o $@ $(KERNEL_OBJECTS)

$(BUILD)/kernel.%.bin: $(BUILD)/kernel_patch.elf
	$(OBJCOPY) -O binary --only-section=.$* $< $@

$(BUILD)/openicestaros_kernel.x: $(IMAGE) $(KERNEL_PATCHES) kernel/patches.json tools/kernel_image.py $(BRANDING)
	python3 tools/kernel_image.py patch $(IMAGE) kernel/patches.json $(BUILD) $@

$(BUILD)/kernel_reset_test: tests/kernel_reset.c kernel/reset.c | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror $^ -o $@

$(BUILD)/kernel_state_test: tests/kernel_state.c kernel/state.c kernel/reset.c kernel/api_table.c kernel/service_table.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_linked_test: tests/kernel_state.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/state.c kernel/reset.c kernel/api_table.c kernel/service_table.c -o $@

$(BUILD)/kernel_events_test: tests/kernel_events.c kernel/events.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_events_linked_test: tests/kernel_events.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/events.c -o $@

$(BUILD)/kernel_poll_test: tests/kernel_poll.c kernel/poll.c kernel/events.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_poll_linked_test: tests/kernel_poll.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/poll.c kernel/events.c -o $@

$(BUILD)/kernel_pointer_test: tests/kernel_pointer.c kernel/pointer.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_pointer_linked_test: tests/kernel_pointer.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/pointer.c -o $@

$(BUILD)/kernel_queue_test: tests/kernel_queue.c kernel/queue.c kernel/pointer_decode.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_queue_linked_test: tests/kernel_queue.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/queue.c kernel/pointer_decode.c -o $@

$(BUILD)/kernel_input_test: tests/kernel_queue.c kernel/input_queue.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DQUEUE_INPUT_TEST -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_input_linked_test: tests/kernel_queue.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DQUEUE_INPUT_TEST -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/input_queue.c -o $@

$(BUILD)/kernel_pointer_init_test: tests/kernel_pointer_init.c kernel/pointer_init.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_keyboard_init_test: tests/kernel_keyboard_init.c kernel/keyboard_init.c | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start $^ -o $@

$(BUILD)/kernel_keyboard_init_linked_test: tests/kernel_keyboard_init.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/keyboard_init.c -o $@

$(BUILD)/kernel_keyboard_test: tests/kernel_keyboard.c kernel/keyboard.c kernel/input_queue.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -Wl,-e,_start tests/kernel_keyboard.c kernel/keyboard.c kernel/input_queue.c -o $@

$(BUILD)/kernel_keyboard_linked_test: tests/kernel_keyboard.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/keyboard.c kernel/input_queue.c -o $@

$(BUILD)/kernel_pointer_init_linked_test: tests/kernel_pointer_init.c $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -nostdlib -static -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DKERNEL_LINKED_TEST -Wl,-e,_start $< kernel/pointer_init.c -o $@

$(BUILD)/kernel_service_forward_test: tests/kernel_service_forward.c kernel/service_forward.c kernel/service_forward.h kernel/service_forward.def $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_service_forward.c kernel/service_forward.c -o $@

$(BUILD)/kernel_service_forward_linked_test: tests/kernel_service_forward.c kernel/service_forward.c kernel/service_forward.h kernel/service_forward.def $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_service_forward.c kernel/service_forward.c -o $@

$(BUILD)/kernel_service_optional_test: tests/kernel_service_optional.c kernel/service_optional.c kernel/service_optional.h kernel/service_optional.def $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_service_optional.c kernel/service_optional.c -o $@

$(BUILD)/kernel_service_optional_linked_test: tests/kernel_service_optional.c kernel/service_optional.c kernel/service_optional.h kernel/service_optional.def $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_service_optional.c kernel/service_optional.c -o $@

$(BUILD)/kernel_runtime_test: tests/kernel_runtime.c kernel/runtime.c kernel/runtime.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_runtime.c kernel/runtime.c -o $@

$(BUILD)/kernel_runtime_linked_test: tests/kernel_runtime.c kernel/runtime.c kernel/runtime.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_runtime.c kernel/runtime.c -o $@

$(BUILD)/kernel_tasks_test: tests/kernel_tasks.c kernel/tasks.c kernel/runtime.h kernel/service_forward.c kernel/service_forward.h kernel/service_forward.def $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_tasks.c kernel/tasks.c kernel/service_forward.c -o $@

$(BUILD)/kernel_tasks_linked_test: tests/kernel_tasks.c kernel/tasks.c kernel/runtime.h kernel/service_forward.c kernel/service_forward.h kernel/service_forward.def $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_tasks.c kernel/tasks.c kernel/service_forward.c -o $@

$(BUILD)/kernel_task_create_test: tests/kernel_task_create.c kernel/task_create.c kernel/tasks.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_task_create.c kernel/task_create.c -o $@

$(BUILD)/kernel_task_create_linked_test: tests/kernel_task_create.c kernel/task_create.c kernel/tasks.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_task_create.c kernel/task_create.c -o $@

$(BUILD)/kernel_task_lifecycle_test: tests/kernel_task_lifecycle.c kernel/task_lifecycle.c kernel/task_create.c kernel/tasks.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_task_lifecycle.c kernel/task_lifecycle.c kernel/task_create.c -o $@

$(BUILD)/kernel_task_lifecycle_linked_test: tests/kernel_task_lifecycle.c kernel/task_lifecycle.c kernel/task_create.c kernel/tasks.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_task_lifecycle.c kernel/task_lifecycle.c kernel/task_create.c -o $@

$(BUILD)/kernel_helpers_test: tests/kernel_helpers.c kernel/segments.c kernel/strings.c kernel/helpers.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_helpers.c kernel/segments.c kernel/strings.c -o $@

$(BUILD)/kernel_helpers_linked_test: tests/kernel_helpers.c kernel/segments.c kernel/strings.c kernel/helpers.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_helpers.c kernel/segments.c kernel/strings.c -o $@

$(BUILD)/kernel_memory_test: tests/kernel_memory.c kernel/memory.c kernel/memory.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_memory.c kernel/memory.c -o $@

$(BUILD)/kernel_memory_linked_test: tests/kernel_memory.c kernel/memory.c kernel/memory.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_memory.c kernel/memory.c -o $@

$(BUILD)/kernel_memory_init_test: tests/kernel_memory_init.c kernel/memory_init.c kernel/memory_init.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_memory_init.c kernel/memory_init.c -o $@

$(BUILD)/kernel_memory_init_linked_test: tests/kernel_memory_init.c kernel/memory_init.c kernel/memory_init.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_memory_init.c kernel/memory_init.c -o $@

$(BUILD)/kernel_memory_ops_test: tests/kernel_memory_ops.c kernel/memory_ops.c kernel/memory_ops.h kernel/memory.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_memory_ops.c kernel/memory_ops.c -o $@

$(BUILD)/kernel_memory_ops_linked_test: tests/kernel_memory_ops.c kernel/memory_ops.c kernel/memory_ops.h kernel/memory.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_memory_ops.c kernel/memory_ops.c -o $@

$(BUILD)/kernel_memory_accel_test: tests/kernel_memory_accel.c kernel/memory_accel.c kernel/memory_transfers.c kernel/memory_accel.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_memory_accel.c kernel/memory_accel.c kernel/memory_transfers.c -o $@

$(BUILD)/kernel_memory_accel_linked_test: tests/kernel_memory_accel.c kernel/memory_accel.c kernel/memory_transfers.c kernel/memory_accel.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_memory_accel.c kernel/memory_accel.c kernel/memory_transfers.c -o $@

$(BUILD)/kernel_graphics_primitives_test: tests/kernel_graphics_primitives.c kernel/graphics_primitives.c kernel/graphics_primitives.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_graphics_primitives.c kernel/graphics_primitives.c -o $@

$(BUILD)/kernel_graphics_primitives_linked_test: tests/kernel_graphics_primitives.c kernel/graphics_primitives.c kernel/graphics_primitives.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_graphics_primitives.c kernel/graphics_primitives.c -o $@

$(BUILD)/kernel_cpuid_test: tests/kernel_cpuid.c kernel/cpuid.c kernel/cpuid.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_cpuid.c kernel/cpuid.c -o $@

$(BUILD)/kernel_cpuid_linked_test: tests/kernel_cpuid.c kernel/cpuid.c kernel/cpuid.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_cpuid.c kernel/cpuid.c -o $@

$(BUILD)/kernel_fpu_test: tests/kernel_fpu.c kernel/fpu.c kernel/fpu_state.c kernel/cpuid.c kernel/cpu.c kernel/fpu.h kernel/cpuid.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_fpu.c kernel/fpu.c kernel/fpu_state.c kernel/cpuid.c kernel/cpu.c -o $@

$(BUILD)/kernel_fpu_linked_test: tests/kernel_fpu.c kernel/fpu.c kernel/fpu_state.c kernel/cpuid.c kernel/cpu.c kernel/fpu.h kernel/cpuid.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_fpu.c kernel/fpu.c kernel/fpu_state.c kernel/cpuid.c kernel/cpu.c -o $@

$(BUILD)/kernel_interrupts_test: tests/kernel_interrupts.c kernel/interrupts.c kernel/interrupts.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_interrupts.c kernel/interrupts.c -o $@

$(BUILD)/kernel_interrupts_linked_test: tests/kernel_interrupts.c kernel/interrupts.c kernel/interrupts.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_interrupts.c kernel/interrupts.c -o $@

$(BUILD)/kernel_interrupt_callbacks_test: tests/kernel_interrupt_callbacks.c kernel/interrupt_callbacks.c kernel/interrupt_callbacks.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_interrupt_callbacks.c kernel/interrupt_callbacks.c -o $@

$(BUILD)/kernel_interrupt_callbacks_linked_test: tests/kernel_interrupt_callbacks.c kernel/interrupt_callbacks.c kernel/interrupt_callbacks.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_interrupt_callbacks.c kernel/interrupt_callbacks.c -o $@

$(BUILD)/kernel_syscalls_test: tests/kernel_syscalls.c kernel/syscalls.c kernel/syscalls.h kernel/service_forward.h kernel/service_forward.def kernel/runtime.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_syscalls.c kernel/syscalls.c -o $@

$(BUILD)/kernel_syscalls_linked_test: tests/kernel_syscalls.c kernel/syscalls.c kernel/syscalls.h kernel/service_forward.h kernel/service_forward.def kernel/runtime.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_syscalls.c kernel/syscalls.c -o $@

$(BUILD)/kernel_syscall_services_test: tests/kernel_syscall_services.c kernel/syscall_services.c kernel/syscall_services.h kernel/syscalls.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_syscall_services.c kernel/syscall_services.c -o $@

$(BUILD)/kernel_syscall_services_linked_test: tests/kernel_syscall_services.c kernel/syscall_services.c kernel/syscall_services.h kernel/syscalls.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_syscall_services.c kernel/syscall_services.c -o $@

$(BUILD)/kernel_syscall_surface_test: tests/kernel_syscall_surface.c kernel/syscall_surface.c kernel/syscall_surface.h kernel/syscalls.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_syscall_surface.c kernel/syscall_surface.c -o $@

$(BUILD)/kernel_syscall_surface_linked_test: tests/kernel_syscall_surface.c kernel/syscall_surface.c kernel/syscall_surface.h kernel/syscalls.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_syscall_surface.c kernel/syscall_surface.c -o $@

$(BUILD)/kernel_surface_refresh_test: tests/kernel_surface_refresh.c kernel/surface_refresh.c kernel/surface_refresh.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_refresh.c kernel/surface_refresh.c -o $@

$(BUILD)/kernel_surface_refresh_linked_test: tests/kernel_surface_refresh.c kernel/surface_refresh.c kernel/surface_refresh.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_refresh.c kernel/surface_refresh.c -o $@

$(BUILD)/kernel_surface_composite_test: tests/kernel_surface_composite.c kernel/surface_composite.c kernel/surface_composite.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_composite.c kernel/surface_composite.c -o $@

$(BUILD)/kernel_surface_composite_linked_test: tests/kernel_surface_composite.c kernel/surface_composite.c kernel/surface_composite.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_composite.c kernel/surface_composite.c -o $@

$(BUILD)/kernel_surface_present_test: tests/kernel_surface_present.c kernel/surface_present.c kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_present.c kernel/surface_present.c -o $@

$(BUILD)/kernel_surface_present_linked_test: tests/kernel_surface_present.c kernel/surface_present.c kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_present.c kernel/surface_present.c -o $@

$(BUILD)/kernel_surface_draw_test: tests/kernel_surface_draw.c kernel/surface_draw.c kernel/surface_draw.h kernel/surface_present.c kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_draw.c kernel/surface_draw.c kernel/surface_present.c -o $@

$(BUILD)/kernel_surface_draw_linked_test: tests/kernel_surface_draw.c kernel/surface_draw.c kernel/surface_draw.h kernel/surface_present.c kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_draw.c kernel/surface_draw.c kernel/surface_present.c -o $@

$(BUILD)/kernel_surface_alpha_test: tests/kernel_surface_alpha.c kernel/surface_alpha.c kernel/surface_alpha.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_alpha.c kernel/surface_alpha.c -o $@

$(BUILD)/kernel_surface_alpha_linked_test: tests/kernel_surface_alpha.c kernel/surface_alpha.c kernel/surface_alpha.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_alpha.c kernel/surface_alpha.c -o $@

$(BUILD)/kernel_surface_draw32_test: tests/kernel_surface_draw32.c kernel/surface_draw32.c kernel/surface_draw.h kernel/surface_alpha.c kernel/surface_alpha.h kernel/surface_present.c kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_draw32.c kernel/surface_draw32.c kernel/surface_alpha.c kernel/surface_present.c -o $@

$(BUILD)/kernel_surface_draw32_linked_test: tests/kernel_surface_draw32.c kernel/surface_draw32.c kernel/surface_draw.h kernel/surface_alpha.c kernel/surface_alpha.h kernel/surface_present.c kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_draw32.c kernel/surface_draw32.c kernel/surface_alpha.c kernel/surface_present.c -o $@

$(BUILD)/kernel_surface_move_test: tests/kernel_surface_move.c kernel/surface_move.c kernel/surface_move.h kernel/surface_composite.h kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_move.c kernel/surface_move.c -o $@

$(BUILD)/kernel_surface_move_linked_test: tests/kernel_surface_move.c kernel/surface_move.c kernel/surface_move.h kernel/surface_composite.h kernel/surface_present.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_move.c kernel/surface_move.c -o $@

$(BUILD)/kernel_surface_scale_test: tests/kernel_surface_scale.c kernel/surface_scale.c kernel/surface_scale.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_scale.c kernel/surface_scale.c -o $@

$(BUILD)/kernel_surface_scale_linked_test: tests/kernel_surface_scale.c kernel/surface_scale.c kernel/surface_scale.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_scale.c kernel/surface_scale.c -o $@

$(BUILD)/kernel_surface_lifecycle_test: tests/kernel_surface_lifecycle.c kernel/surface_lifecycle.c kernel/surface_lifecycle.h kernel/surface_refresh.h kernel/memory.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_surface_lifecycle.c kernel/surface_lifecycle.c -o $@

$(BUILD)/kernel_surface_lifecycle_linked_test: tests/kernel_surface_lifecycle.c kernel/surface_lifecycle.c kernel/surface_lifecycle.h kernel/surface_refresh.h kernel/memory.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_lifecycle.c kernel/surface_lifecycle.c -o $@

$(BUILD)/kernel_surface_order_test: tests/kernel_surface_order.c kernel/surface_order.c kernel/surface_order.h kernel/surface_refresh.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fwrapv-pointer -nostdlib -static -Wl,-e,_start tests/kernel_surface_order.c kernel/surface_order.c -o $@

$(BUILD)/kernel_surface_order_linked_test: tests/kernel_surface_order.c kernel/surface_order.c kernel/surface_order.h kernel/surface_refresh.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -fwrapv-pointer -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_surface_order.c kernel/surface_order.c -o $@

$(BUILD)/kernel_cursor_test: tests/kernel_cursor.c kernel/cursor.c kernel/cursor.h kernel/surface_order.h kernel/surface_refresh.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -Wl,-e,_start tests/kernel_cursor.c kernel/cursor.c -o $@

$(BUILD)/kernel_cursor_linked_test: tests/kernel_cursor.c kernel/cursor.c kernel/cursor.h kernel/surface_order.h kernel/surface_refresh.h $(BUILD)/openicestaros_kernel.x | $(BUILD)
	$(CC) $(KERNEL_CFLAGS) -nostdlib -static -DKERNEL_LINKED_TEST -Wl,-e,_start tests/kernel_cursor.c kernel/cursor.c -o $@

$(BUILD)/init_c.bin: $(BUILD)/init_entry.o $(BUILD)/init_directory.o $(BUILD)/init_clusters.o init/init.ld
	$(LD) -m elf_i386 -T init/init.ld -o $@ $(BUILD)/init_entry.o $(BUILD)/init_directory.o $(BUILD)/init_clusters.o

$(BUILD)/OpenIceStarOS_c_init.vhd: $(BUILD)/OpenIceStarOS_c_fat16.vhd $(BUILD)/stage3_c_init.bin $(BUILD)/init_c.bin $(BUILD)/openicestaros_kernel.x tools/kernel_image.py
	python3 tools/kernel_image.py install $(BUILD)/OpenIceStarOS_c_fat16.vhd $(BUILD)/openicestaros_kernel.x $@
	dd if=$(BUILD)/stage3_c_init.bin of=$@ bs=512 seek=5 count=8 conv=notrunc status=none
	dd if=$(BUILD)/init_c.bin of=$@ bs=512 seek=13 count=4 conv=notrunc status=none

$(BUILD)/init_clusters_test: tests/init_clusters.c init/clusters.c init/directory.c init/clusters.h init/directory.h | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -DINIT_CLUSTERS_HOST_TEST -DINIT_DIRECTORY_HOST_TEST -Iinit tests/init_clusters.c init/clusters.c init/directory.c -o $@

$(BUILD)/init_directory_test: tests/init_directory.c init/directory.c init/directory.h | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -DINIT_DIRECTORY_HOST_TEST -Iinit tests/init_directory.c init/directory.c -o $@

$(BUILD)/init_entry_test: tests/init_entry.c init/entry.c init/entry.h | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinit tests/init_entry.c init/entry.c -o $@

$(BUILD)/init_transfer_test: tests/init_transfer.c boot/init_transfer.c boot/init_transfer.h | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iboot tests/init_transfer.c boot/init_transfer.c -o $@

$(BUILD)/pci_test: tests/pci.c boot/pci.c boot/pci.h | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -DPCI_HOST_TEST -Iboot tests/pci.c boot/pci.c -o $@

verify: $(BUILD)/OpenIceStarOS_c_mbr.vhd $(BUILD)/OpenIceStarOS_c_fat16.vhd $(BUILD)/OpenIceStarOS_c_init.vhd $(BUILD)/fat16.o $(BUILD)/fat16_image $(BUILD)/init_entry.o $(BUILD)/init_directory.o $(BUILD)/init_clusters.o $(BUILD)/init_directory_test $(BUILD)/init_clusters_test $(BUILD)/init_entry_test $(BUILD)/init_transfer_test $(BUILD)/pci_test $(BUILD)/kernel_reset_test $(BUILD)/kernel_state_test $(BUILD)/kernel_linked_test $(BUILD)/kernel_events_test $(BUILD)/kernel_events_linked_test $(BUILD)/kernel_poll_test $(BUILD)/kernel_poll_linked_test $(BUILD)/kernel_pointer_test $(BUILD)/kernel_pointer_linked_test $(BUILD)/kernel_queue_test $(BUILD)/kernel_queue_linked_test $(BUILD)/kernel_pointer_init_test $(BUILD)/kernel_pointer_init_linked_test
	test "$$(wc -c < $(BUILD)/mbr.bin)" -eq 512
	test "$$(wc -c < $(BUILD)/menu.bin)" -eq 1536
	cmp -i 446:446 -n 66 $(IMAGE) $(BUILD)/mbr.bin
	python3 tools/branding.py verify-boot $(IMAGE) $(BUILD)/OpenIceStarOS_c_mbr.vhd
	test "$$(wc -c < $(BUILD)/stage3_hybrid.bin)" -eq 4096
	python3 tools/branding.py verify-boot $(IMAGE) $(BUILD)/OpenIceStarOS_c_fat16.vhd --stage3
	test "$$(wc -c < $(BUILD)/init_c.bin)" -eq 2048
	python3 tools/kernel_image.py verify $(IMAGE) $(BUILD)/OpenIceStarOS_c_init.vhd $(BUILD)/openicestaros_kernel.x kernel/patches.json $(BUILD)
	$(BUILD)/fat16_image $(IMAGE) $(BUILD)/init.x
	printf '%s  %s\n' 0364c0f872a844d6603634fe69adabce92dfafca4a6146474bcb858130899007 $(BUILD)/init.x | sha256sum -c -
	$(BUILD)/init_entry_test
	$(BUILD)/init_directory_test $(IMAGE)
	$(BUILD)/init_clusters_test $(IMAGE) $(BUILD)/dat.bin $(BUILD)/icestar.x
	printf '%s  %s\n' 01a8f491fefa8563238533936beca1ea7cafe7cbebfb55a9743a436b27b61ea8 $(BUILD)/dat.bin 9edc55ea50b889f86ec7b00a1305eada39c979221113f6d86e13fd2b31f99ae3 $(BUILD)/icestar.x | sha256sum -c -
	$(BUILD)/init_transfer_test
	$(BUILD)/pci_test
	$(BUILD)/kernel_reset_test
	$(BUILD)/kernel_state_test
	$(BUILD)/kernel_linked_test
	$(BUILD)/kernel_events_test
	$(BUILD)/kernel_events_linked_test
	$(BUILD)/kernel_poll_test
	$(BUILD)/kernel_poll_linked_test
	$(BUILD)/kernel_pointer_test
	$(BUILD)/kernel_pointer_linked_test
	$(BUILD)/kernel_queue_test
	$(BUILD)/kernel_queue_linked_test
	$(BUILD)/kernel_input_test
	$(BUILD)/kernel_input_linked_test
	$(BUILD)/kernel_pointer_init_test
	$(BUILD)/kernel_pointer_init_linked_test
	$(BUILD)/kernel_keyboard_init_test
	$(BUILD)/kernel_keyboard_init_linked_test
	$(BUILD)/kernel_keyboard_test
	$(BUILD)/kernel_keyboard_linked_test
	$(BUILD)/kernel_service_forward_test
	$(BUILD)/kernel_service_forward_linked_test
	$(BUILD)/kernel_service_optional_test
	$(BUILD)/kernel_service_optional_linked_test
	$(BUILD)/kernel_runtime_test
	$(BUILD)/kernel_runtime_linked_test
	$(BUILD)/kernel_tasks_test
	$(BUILD)/kernel_tasks_linked_test
	$(BUILD)/kernel_task_create_test
	$(BUILD)/kernel_task_create_linked_test
	$(BUILD)/kernel_task_lifecycle_test
	$(BUILD)/kernel_task_lifecycle_linked_test
	$(BUILD)/kernel_helpers_test
	$(BUILD)/kernel_helpers_linked_test
	$(BUILD)/kernel_memory_test
	$(BUILD)/kernel_memory_linked_test
	$(BUILD)/kernel_memory_init_test
	$(BUILD)/kernel_memory_init_linked_test
	$(BUILD)/kernel_memory_ops_test
	$(BUILD)/kernel_memory_ops_linked_test
	$(BUILD)/kernel_memory_accel_test
	$(BUILD)/kernel_memory_accel_linked_test
	$(BUILD)/kernel_graphics_primitives_test
	$(BUILD)/kernel_graphics_primitives_linked_test
	$(BUILD)/kernel_cpuid_test
	$(BUILD)/kernel_cpuid_linked_test
	$(BUILD)/kernel_fpu_test
	$(BUILD)/kernel_fpu_linked_test
	$(BUILD)/kernel_interrupts_test
	$(BUILD)/kernel_interrupts_linked_test
	python3 tests/interrupt_entries_exact.py --installed
	$(BUILD)/kernel_interrupt_callbacks_test
	$(BUILD)/kernel_interrupt_callbacks_linked_test
	python3 tests/cmos_clock_exact.py --installed
	$(BUILD)/kernel_syscalls_test
	$(BUILD)/kernel_syscalls_linked_test
	$(BUILD)/kernel_syscall_services_test
	$(BUILD)/kernel_syscall_services_linked_test
	$(BUILD)/kernel_syscall_surface_test
	$(BUILD)/kernel_syscall_surface_linked_test
	$(BUILD)/kernel_surface_refresh_test
	$(BUILD)/kernel_surface_refresh_linked_test
	$(BUILD)/kernel_surface_composite_test
	$(BUILD)/kernel_surface_composite_linked_test
	$(BUILD)/kernel_surface_present_test
	$(BUILD)/kernel_surface_present_linked_test
	$(BUILD)/kernel_surface_draw_test
	$(BUILD)/kernel_surface_draw_linked_test
	$(BUILD)/kernel_surface_alpha_test
	$(BUILD)/kernel_surface_alpha_linked_test
	$(BUILD)/kernel_surface_draw32_test
	$(BUILD)/kernel_surface_draw32_linked_test
	$(BUILD)/kernel_surface_move_test
	$(BUILD)/kernel_surface_move_linked_test
	$(BUILD)/kernel_surface_scale_test
	$(BUILD)/kernel_surface_scale_linked_test
	$(BUILD)/kernel_surface_lifecycle_test
	$(BUILD)/kernel_surface_lifecycle_linked_test
	$(BUILD)/kernel_surface_order_test
	$(BUILD)/kernel_surface_order_linked_test
	$(BUILD)/kernel_cursor_test
	$(BUILD)/kernel_cursor_linked_test
	python3 tests/kernel_entry.py --installed
	python3 tests/kernel_startup_state.py --installed
	python3 tests/kernel_startup_apps.py --installed
	python3 tests/kernel_display_palette.py --installed
	python3 tests/display_dac_qemu.py --installed
	python3 tests/paging_qemu.py --installed
	python3 tests/kernel_startup_desktop.py --installed
	python3 tests/kernel_startup_ui.py --installed
	python3 tests/kernel_object_registry.py --installed
	python3 tests/kernel_timer_objects.py --installed
	python3 tests/kernel_clock_display.py --installed
	python3 tests/timer_flags_qemu.py --installed
	python3 tests/kernel_cmos_fields.py --installed
	python3 tests/kernel_icon_directory.py --installed
	python3 tests/kernel_icon_files.py --installed
	python3 tests/kernel_icon_dib.py --installed
	python3 tests/kernel_icon_pixels.py --installed
	python3 tests/kernel_icon_render.py --installed
	python3 tests/kernel_pixel_access.py --installed
	python3 tests/kernel_raster.py --installed
	python3 tests/kernel_raster_fill.py --installed
	python3 tests/kernel_raster_outline.py --installed
	python3 tests/kernel_raster_circle.py --installed
	python3 tests/kernel_raster_disc.py --installed
	python3 tests/kernel_raster_gradient.py --installed
	python3 tests/kernel_raster_brush.py --installed
	python3 tests/kernel_raster_rounded.py --installed
	python3 tests/kernel_text_glyph.py --installed
	python3 tests/kernel_text_wide.py --installed
	python3 tests/kernel_text_run.py --installed
	python3 tests/kernel_text_number.py --installed
	python3 tests/kernel_text_format.py --installed
	python3 tests/kernel_bitmap.py --installed
	python3 tests/kernel_bitmap_files.py --installed
	python3 tests/kernel_bitmap_fit.py --installed
	python3 tests/kernel_bitmap_surface_fit.py --installed
	python3 tests/kernel_bitmap_key.py --installed
	python3 tests/kernel_jpeg_bits.py --installed
	python3 tests/kernel_jpeg_frame.py --installed
	python3 tests/kernel_jpeg_tables.py --installed
	python3 tests/kernel_jpeg_huffman.py --installed
	python3 tests/kernel_jpeg_scan.py --installed
	python3 tests/kernel_string_extra.py --installed
	python3 tests/kernel_simd.py --installed
	python3 tests/kernel_math_series.py --installed
	python3 tests/kernel_math_extra.py --installed
	python3 tests/kernel_math_format.py --installed
	python3 tests/entry_trace.py
	python3 tests/firmware_scan_qemu.py --installed
	python3 tests/timestamp_exact.py --installed
	python3 tests/xformat.py
	python3 tests/snake.py
	python3 tests/color.py
	python3 tests/mov.py
	python3 tests/starscr.py
	python3 tests/clock.py
	python3 tests/clock.py --kernel $(BUILD)/openicestaros_kernel.x
	python3 tests/app_image.py
	python3 tools/app_image.py verify $(BUILD)/OpenIceStarOS_c_init.vhd apps/snake_image.json $(BUILD) $(BUILD)/OpenIceStarOS_c_snake.vhd
	python3 tools/app_image.py verify $(BUILD)/OpenIceStarOS_c_init.vhd apps/images.json $(BUILD) $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/binary_inventory.py --check

verify: $(BUILD)/kernel_input_test $(BUILD)/kernel_input_linked_test
verify: $(BUILD)/kernel_keyboard_init_test $(BUILD)/kernel_keyboard_init_linked_test
verify: $(BUILD)/kernel_keyboard_test $(BUILD)/kernel_keyboard_linked_test
verify: $(BUILD)/kernel_service_forward_test $(BUILD)/kernel_service_forward_linked_test
verify: $(BUILD)/kernel_service_optional_test $(BUILD)/kernel_service_optional_linked_test
verify: $(BUILD)/kernel_runtime_test $(BUILD)/kernel_runtime_linked_test
verify: $(BUILD)/kernel_tasks_test $(BUILD)/kernel_tasks_linked_test
verify: $(BUILD)/kernel_task_create_test $(BUILD)/kernel_task_create_linked_test
verify: $(BUILD)/kernel_task_lifecycle_test $(BUILD)/kernel_task_lifecycle_linked_test
verify: $(BUILD)/kernel_helpers_test $(BUILD)/kernel_helpers_linked_test
verify: $(BUILD)/kernel_memory_test $(BUILD)/kernel_memory_linked_test
verify: $(BUILD)/kernel_memory_init_test $(BUILD)/kernel_memory_init_linked_test
verify: $(BUILD)/kernel_memory_ops_test $(BUILD)/kernel_memory_ops_linked_test
verify: $(BUILD)/kernel_memory_accel_test $(BUILD)/kernel_memory_accel_linked_test
verify: $(BUILD)/kernel_graphics_primitives_test $(BUILD)/kernel_graphics_primitives_linked_test
verify: $(BUILD)/kernel_fpu_test $(BUILD)/kernel_fpu_linked_test
verify: $(BUILD)/kernel_interrupts_test $(BUILD)/kernel_interrupts_linked_test
verify: $(BUILD)/kernel_interrupt_callbacks_test $(BUILD)/kernel_interrupt_callbacks_linked_test
verify: $(BUILD)/kernel_syscalls_test $(BUILD)/kernel_syscalls_linked_test
verify: $(BUILD)/kernel_syscall_services_test $(BUILD)/kernel_syscall_services_linked_test
verify: $(BUILD)/kernel_syscall_surface_test $(BUILD)/kernel_syscall_surface_linked_test
verify: $(BUILD)/kernel_surface_refresh_test $(BUILD)/kernel_surface_refresh_linked_test
verify: $(BUILD)/kernel_surface_composite_test $(BUILD)/kernel_surface_composite_linked_test
verify: $(BUILD)/kernel_surface_present_test $(BUILD)/kernel_surface_present_linked_test
verify: $(BUILD)/kernel_surface_draw_test $(BUILD)/kernel_surface_draw_linked_test
verify: $(BUILD)/kernel_surface_alpha_test $(BUILD)/kernel_surface_alpha_linked_test
verify: $(BUILD)/kernel_surface_draw32_test $(BUILD)/kernel_surface_draw32_linked_test
verify: $(BUILD)/kernel_surface_move_test $(BUILD)/kernel_surface_move_linked_test
verify: $(BUILD)/kernel_surface_scale_test $(BUILD)/kernel_surface_scale_linked_test
verify: $(BUILD)/kernel_surface_lifecycle_test $(BUILD)/kernel_surface_lifecycle_linked_test
verify: $(BUILD)/kernel_surface_order_test $(BUILD)/kernel_surface_order_linked_test
verify: $(BUILD)/kernel_cursor_test $(BUILD)/kernel_cursor_linked_test
verify: $(BUILD)/kernel_cpuid_test $(BUILD)/kernel_cpuid_linked_test
verify: $(BUILD)/snake.x
verify: $(BUILD)/color.x
verify: $(BUILD)/mov.x
verify: $(BUILD)/starscr.x
verify: $(BUILD)/clock.x
verify: $(BUILD)/OpenIceStarOS_c_snake.vhd
verify: $(BUILD)/OpenIceStarOS_c_apps.vhd

run: $(BUILD)/OpenIceStarOS_c_mbr.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

run-hybrid: $(BUILD)/OpenIceStarOS_c_fat16.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

run-c-init: $(BUILD)/OpenIceStarOS_c_init.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

$(BUILD)/snake.o: apps/snake.c apps/snake.h apps/xapi.h apps/xapi.def | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/xapi.o: apps/xapi.c apps/xapi.def | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/snake.elf: $(BUILD)/snake.o $(BUILD)/xapi.o apps/snake.ld
	$(LD) -m elf_i386 --emit-relocs -T apps/snake.ld -Map=$(BUILD)/snake.map -o $@ $(BUILD)/snake.o $(BUILD)/xapi.o

$(BUILD)/snake.x: $(BUILD)/snake.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --file-size 0xb24 --metadata $(BUILD)/snake.symbols.json

$(BUILD)/OpenIceStarOS_c_snake.vhd: $(BUILD)/OpenIceStarOS_c_init.vhd $(BUILD)/snake.x apps/snake_image.json tools/app_image.py tools/kernel_image.py
	python3 tools/app_image.py install $< apps/snake_image.json $(BUILD) $@

run-c-snake: $(BUILD)/OpenIceStarOS_c_snake.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

smoke-snake: $(BUILD)/OpenIceStarOS_c_snake.vhd
	python3 tools/boot_smoke.py --snake-check

$(BUILD)/color.o: apps/color.c apps/color.h apps/legacy_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/legacy_api.o: apps/legacy_api.c apps/legacy_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/color.elf: $(BUILD)/color.o $(BUILD)/legacy_api.o apps/color.ld
	$(LD) -m elf_i386 --emit-relocs -T apps/color.ld -Map=$(BUILD)/color.map -o $@ $(BUILD)/color.o $(BUILD)/legacy_api.o

$(BUILD)/color.x: $(BUILD)/color.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/color.symbols.json

$(BUILD)/mov.o: apps/mov.c apps/mov.h apps/legacy_api.h apps/legacy_image_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/legacy_image_api.o: apps/legacy_image_api.c apps/legacy_image_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/mov.elf: $(BUILD)/mov.o $(BUILD)/legacy_api.o $(BUILD)/legacy_image_api.o apps/mov.ld
	$(LD) -m elf_i386 --emit-relocs --gc-sections -T apps/mov.ld -Map=$(BUILD)/mov.map -o $@ $(BUILD)/mov.o $(BUILD)/legacy_api.o $(BUILD)/legacy_image_api.o

$(BUILD)/mov.x: $(BUILD)/mov.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x3a54 --metadata $(BUILD)/mov.symbols.json

$(BUILD)/starscr.o: apps/starscr.c apps/starscr.h apps/legacy_api.h apps/legacy_image_api.h apps/legacy_draw_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/legacy_draw_api.o: apps/legacy_draw_api.c apps/legacy_draw_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/starscr.elf: $(BUILD)/starscr.o $(BUILD)/legacy_api.o $(BUILD)/legacy_image_api.o $(BUILD)/legacy_draw_api.o apps/starscr.ld
	$(LD) -m elf_i386 --emit-relocs --gc-sections -T apps/starscr.ld -Map=$(BUILD)/starscr.map -o $@ $(BUILD)/starscr.o $(BUILD)/legacy_api.o $(BUILD)/legacy_image_api.o $(BUILD)/legacy_draw_api.o

$(BUILD)/starscr.x: $(BUILD)/starscr.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x3eb4 --metadata $(BUILD)/starscr.symbols.json

$(BUILD)/clock.o: apps/clock.c apps/clock.h apps/legacy_api.h apps/legacy_image_api.h apps/legacy_clock_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -fexcess-precision=fast -frounding-math -c $< -o $@

$(BUILD)/legacy_clock_api.o: apps/legacy_clock_api.c apps/legacy_clock_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/legacy_format.o: apps/legacy_format.c apps/legacy_clock_api.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/clock.elf: $(BUILD)/clock.o $(BUILD)/legacy_api.o $(BUILD)/legacy_image_api.o $(BUILD)/legacy_clock_api.o $(BUILD)/legacy_format.o apps/clock.ld
	$(LD) -m elf_i386 --emit-relocs --gc-sections -T apps/clock.ld -Map=$(BUILD)/clock.map -o $@ $(filter %.o,$^)

$(BUILD)/clock.x: $(BUILD)/clock.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x3cd8 --metadata $(BUILD)/clock.symbols.json

$(BUILD)/OpenIceStarOS_c_apps.vhd: $(BUILD)/OpenIceStarOS_c_init.vhd $(BUILD)/snake.x $(BUILD)/color.x $(BUILD)/mov.x $(BUILD)/starscr.x $(BUILD)/clock.x apps/images.json tools/app_image.py tools/kernel_image.py
	python3 tools/app_image.py install $< apps/images.json $(BUILD) $@

run-c-apps: $(BUILD)/OpenIceStarOS_c_apps.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

smoke-color: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --color-check

$(BUILD)/surface_scale_guest.o: tests/surface_scale_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/surface_scale_guest.elf: $(BUILD)/surface_scale_guest.o tests/surface_scale_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/surface_scale_guest.ld -Map=$(BUILD)/surface_scale_guest.map -o $@ $<

$(BUILD)/surface_scale_guest.x: $(BUILD)/surface_scale_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/surface_scale_guest.symbols.json

$(BUILD)/OpenIceStarOS_scale_diagnostic.vhd: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/surface_scale_guest.x tools/surface_scale_smoke.py tools/kernel_image.py
	python3 tools/surface_scale_smoke.py $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/surface_scale_guest.x $@

.PHONY: smoke-surface-scale
smoke-surface-scale: $(BUILD)/OpenIceStarOS_scale_diagnostic.vhd
	python3 tools/boot_smoke.py --surface-scale-check

$(BUILD)/fat_relative_guest.o: tests/fat_relative_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/fat_relative_guest.elf: $(BUILD)/fat_relative_guest.o tests/fat_relative_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_relative_guest.ld -Map=$(BUILD)/fat_relative_guest.map -o $@ $<

$(BUILD)/fat_relative_guest.x: $(BUILD)/fat_relative_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/fat_relative_guest.symbols.json

$(BUILD)/OpenIceStarOS_fat_diagnostic.vhd: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/fat_relative_guest.x tools/fat_relative_smoke.py tools/kernel_image.py
	python3 tools/fat_relative_smoke.py $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/fat_relative_guest.x $@

.PHONY: smoke-fat-relative
smoke-fat-relative: $(BUILD)/OpenIceStarOS_fat_diagnostic.vhd
	python3 tools/boot_smoke.py --fat-relative-check

.PHONY: smoke-surface-lifecycle
smoke-surface-lifecycle: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --surface-lifecycle-check --memory-check --task-check --color-check

.PHONY: smoke-surface-order
smoke-surface-order: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --surface-order-check --memory-check --task-check --color-check

.PHONY: smoke-cursor
smoke-cursor: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --cursor-check --memory-check --task-check --color-check

.PHONY: test-firmware-scan-preview
test-firmware-scan-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/firmware_scan_qemu.py

.PHONY: test-firmware-scan
test-firmware-scan: $(BUILD)/openicestaros_kernel.x
	python3 tests/firmware_scan_qemu.py --installed

.PHONY: smoke-firmware
smoke-firmware: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --firmware-check --memory-check --task-check --input-check --color-check

.PHONY: test-kernel-entry-preview test-kernel-entry smoke-kernel-entry
test-kernel-entry-preview:
	python3 tests/kernel_entry.py

test-kernel-entry: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_entry.py --installed

smoke-kernel-entry: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --entry-check --memory-check --task-check --input-check --color-check

smoke-tasks: $(BUILD)/OpenIceStarOS_c_init.vhd
	python3 tools/boot_smoke.py --task-check --input-check

smoke-memory: $(BUILD)/OpenIceStarOS_c_init.vhd
	python3 tools/boot_smoke.py --memory-check --task-check --input-check

smoke-mov: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --mov-check

smoke-starscr: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --starscr-check

.PHONY: smoke-clock
smoke-clock: $(BUILD)/OpenIceStarOS_c_apps.vhd
	python3 tools/boot_smoke.py --clock-check

smoke-starscr-reference:
	python3 tools/boot_smoke.py --starscr-check --starscr-reference

.PHONY: test-startup-state test-startup-apps test-display-palette test-display-dac test-paging test-startup-desktop test-startup-ui
test-startup-state: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_startup_state.py --installed

test-startup-apps: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_startup_apps.py --installed

test-display-palette: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_display_palette.py --installed

test-display-dac: $(BUILD)/openicestaros_kernel.x
	python3 tests/display_dac_qemu.py --installed

test-paging: $(BUILD)/openicestaros_kernel.x
	python3 tests/paging_qemu.py --installed

test-startup-desktop: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_startup_desktop.py --installed

test-startup-ui: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_startup_ui.py --installed

.PHONY: test-object-registry
test-object-registry: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_object_registry.py --installed

.PHONY: test-timer-objects test-clock-display test-timer-flags test-cmos-fields

test-timer-objects: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_timer_objects.py --installed

test-clock-display: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_clock_display.py --installed

test-timer-flags: $(BUILD)/openicestaros_kernel.x
	python3 tests/timer_flags_qemu.py --installed

test-cmos-fields: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_cmos_fields.py --installed

.PHONY: test-icon-directory
test-icon-directory: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_icon_directory.py --installed

.PHONY: test-icon-files
test-icon-files: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_icon_files.py --installed

.PHONY: test-icon-dib
test-icon-dib: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_icon_dib.py --installed

.PHONY: smoke-icon-dib
smoke-icon-dib: $(BUILD)/OpenIceStarOS_c_init.vhd
	python3 tools/boot_smoke.py --icon-dib-check --memory-check --task-check --input-check

.PHONY: test-icon-pixels test-icon-render
test-icon-pixels: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_icon_pixels.py --installed

test-icon-render: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_icon_render.py --installed

.PHONY: smoke-icon-render
smoke-icon-render: $(BUILD)/OpenIceStarOS_c_init.vhd
	python3 tools/boot_smoke.py --icon-render-check --memory-check --task-check --input-check

.PHONY: test-pixel-access
test-pixel-access: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_pixel_access.py --installed

.PHONY: test-raster
test-raster: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster.py --installed

.PHONY: test-raster-fill
test-raster-fill: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_fill.py --installed

.PHONY: test-raster-outline
test-raster-outline: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_outline.py --installed

.PHONY: inventory inventory-check
inventory:
	python3 tools/binary_inventory.py

inventory-check:
	python3 tools/binary_inventory.py --check

.PHONY: test-raster-circle test-raster-disc test-raster-disc-preview
test-raster-circle: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_circle.py --installed

test-raster-disc: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_disc.py --installed

test-raster-disc-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_disc.py

.PHONY: test-raster-gradient test-raster-gradient-preview
test-raster-gradient: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_gradient.py --installed

test-raster-gradient-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_gradient.py

.PHONY: test-raster-brush test-raster-brush-preview
test-raster-brush: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_brush.py --installed

test-raster-brush-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_brush.py

.PHONY: test-raster-rounded test-raster-rounded-preview
test-raster-rounded: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_rounded.py --installed

test-raster-rounded-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_raster_rounded.py

.PHONY: test-text-glyph test-text-glyph-preview
test-text-glyph: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_glyph.py --installed

test-text-glyph-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_glyph.py

.PHONY: test-text-wide test-text-wide-preview
test-text-wide: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_wide.py --installed

test-text-wide-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_wide.py

.PHONY: test-text-run test-text-run-preview
test-text-run: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_run.py --installed

test-text-run-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_run.py

.PHONY: test-text-number test-text-number-preview
test-text-number: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_number.py --installed

test-text-number-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_number.py

.PHONY: test-text-format test-text-format-preview
test-text-format: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_format.py --installed

test-text-format-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_text_format.py

.PHONY: test-bitmap test-bitmap-preview
test-bitmap: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap.py --installed

test-bitmap-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap.py

.PHONY: test-bitmap-files test-bitmap-files-preview
test-bitmap-files: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_files.py --installed

test-bitmap-files-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_files.py

.PHONY: test-bitmap-fit test-bitmap-fit-preview
test-bitmap-fit: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_fit.py --installed

test-bitmap-fit-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_fit.py

.PHONY: test-bitmap-surface-fit test-bitmap-surface-fit-preview
test-bitmap-surface-fit: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_surface_fit.py --installed

test-bitmap-surface-fit-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_surface_fit.py

.PHONY: test-bitmap-key test-bitmap-key-preview
test-bitmap-key: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_key.py --installed

test-bitmap-key-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_bitmap_key.py

.PHONY: test-jpeg-bits test-jpeg-bits-preview
test-jpeg-bits: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_bits.py --installed

test-jpeg-bits-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_bits.py

.PHONY: test-jpeg-frame test-jpeg-frame-preview test-jpeg-tables test-jpeg-tables-preview
test-jpeg-frame: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_frame.py --installed

test-jpeg-frame-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_frame.py

test-jpeg-tables: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_tables.py --installed

test-jpeg-tables-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_tables.py

.PHONY: test-string-extra test-string-extra-preview
test-string-extra: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_string_extra.py --installed

test-string-extra-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_string_extra.py

.PHONY: test-jpeg-huffman test-jpeg-huffman-preview
test-jpeg-huffman: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_huffman.py --installed

test-jpeg-huffman-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_huffman.py

.PHONY: test-jpeg-scan test-jpeg-scan-preview
test-jpeg-scan: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_scan.py --installed

test-jpeg-scan-preview: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_jpeg_scan.py

# Destructive filesystem checks use temporary disposable images only.
$(BUILD)/fat_mutation_guest.o: tests/fat_mutation_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/fat_mutation_guest.elf: $(BUILD)/fat_mutation_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/fat_mutation_guest.map -o $@ $<

$(BUILD)/fat_mutation_guest.x: $(BUILD)/fat_mutation_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/fat_mutation_guest.symbols.json

.PHONY: test-fat-mutation-qemu
test-fat-mutation-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/fat_mutation_guest.x
	python3 tests/kernel_fat_mutation_qemu.py

$(BUILD)/fat_write_guest.o: tests/fat_write_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/fat_write_guest.elf: $(BUILD)/fat_write_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/fat_write_guest.map -o $@ $<

$(BUILD)/fat_write_guest.x: $(BUILD)/fat_write_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/fat_write_guest.symbols.json

.PHONY: test-fat-write-qemu
test-fat-write-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/fat_write_guest.x
	python3 tests/kernel_fat_write_qemu.py

$(BUILD)/fat_write_path_guest.o: tests/fat_write_path_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/fat_write_path_guest.elf: $(BUILD)/fat_write_path_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/fat_write_path_guest.map -o $@ $<

$(BUILD)/fat_write_path_guest.x: $(BUILD)/fat_write_path_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/fat_write_path_guest.symbols.json

.PHONY: test-fat-write-path-qemu
test-fat-write-path-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/fat_write_path_guest.x
	python3 tests/kernel_fat_write_path_qemu.py

.PHONY: test-fat-block
test-fat-block: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_fat_block.py --installed

verify: test-fat-block

$(BUILD)/fat_block_guest.o: tests/fat_block_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/fat_block_guest.elf: $(BUILD)/fat_block_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/fat_block_guest.map -o $@ $<

$(BUILD)/fat_block_guest.x: $(BUILD)/fat_block_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/fat_block_guest.symbols.json

.PHONY: test-fat-block-qemu
test-fat-block-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/fat_block_guest.x
	python3 tests/kernel_fat_block_qemu.py

.PHONY: test-font-parse
test-font-parse: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_font_parse.py --installed

verify: test-font-parse

$(BUILD)/font_parse_guest.o: tests/font_parse_guest.c kernel/font_parse.h | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/font_parse_guest.elf: $(BUILD)/font_parse_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/font_parse_guest.map -o $@ $<

$(BUILD)/font_parse_guest.x: $(BUILD)/font_parse_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/font_parse_guest.symbols.json

.PHONY: test-font-parse-qemu
test-font-parse-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/font_parse_guest.x
	python3 tests/kernel_font_parse_qemu.py

.PHONY: test-font-metrics
test-font-metrics: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_font_metrics.py --installed

verify: test-font-metrics

$(BUILD)/font_metrics_guest.o: tests/font_metrics_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/font_metrics_guest.elf: $(BUILD)/font_metrics_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/font_metrics_guest.map -o $@ $<

$(BUILD)/font_metrics_guest.x: $(BUILD)/font_metrics_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/font_metrics_guest.symbols.json

.PHONY: test-font-metrics-qemu
test-font-metrics-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/font_metrics_guest.x
	python3 tests/kernel_font_metrics_qemu.py

.PHONY: test-font-cmap
test-font-cmap: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_font_cmap.py --installed

verify: test-font-cmap

$(BUILD)/font_cmap_guest.o: tests/font_cmap_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/font_cmap_guest.elf: $(BUILD)/font_cmap_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/font_cmap_guest.map -o $@ $<

$(BUILD)/font_cmap_guest.x: $(BUILD)/font_cmap_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/font_cmap_guest.symbols.json

.PHONY: test-font-cmap-qemu
test-font-cmap-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/font_cmap_guest.x
	python3 tests/kernel_font_cmap_qemu.py

.PHONY: test-font-glyph
test-font-glyph: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_font_glyph.py --installed

verify: test-font-glyph

$(BUILD)/font_glyph_guest.o: tests/font_glyph_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/font_glyph_guest.elf: $(BUILD)/font_glyph_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/font_glyph_guest.map -o $@ $<

$(BUILD)/font_glyph_guest.x: $(BUILD)/font_glyph_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/font_glyph_guest.symbols.json

.PHONY: test-font-glyph-qemu
test-font-glyph-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/font_glyph_guest.x
	python3 tests/kernel_font_glyph_qemu.py

.PHONY: test-font-init
test-font-init: $(BUILD)/openicestaros_kernel.x
	python3 tests/kernel_font_init.py --installed

verify: test-font-init

$(BUILD)/font_init_guest.o: tests/font_init_guest.c | $(BUILD)
	$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD)/font_init_guest.elf: $(BUILD)/font_init_guest.o tests/fat_mutation_guest.ld
	$(LD) -m elf_i386 --emit-relocs -T tests/fat_mutation_guest.ld -Map=$(BUILD)/font_init_guest.map -o $@ $<

$(BUILD)/font_init_guest.x: $(BUILD)/font_init_guest.elf tools/xformat.py
	python3 tools/xformat.py $< $@ --kind 0x0a --file-size 0x38b4 --metadata $(BUILD)/font_init_guest.symbols.json

.PHONY: test-font-init-qemu
test-font-init-qemu: $(BUILD)/OpenIceStarOS_c_apps.vhd $(BUILD)/font_init_guest.x
	python3 tests/kernel_font_init_qemu.py
