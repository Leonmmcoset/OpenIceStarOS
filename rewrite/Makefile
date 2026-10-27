CC := gcc
LD := ld
OBJCOPY := objcopy
CFLAGS := -m16 -march=i386 -Os -ffreestanding -fno-pic -fno-pie \
          -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra
BUILD := build
IMAGE := ../IceStarOS_x86_32M.vhd

.PHONY: all verify run
all: $(BUILD)/IceStarOS_c_mbr.vhd

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

$(BUILD)/menu.bin: $(BUILD)/menu_entry.o $(BUILD)/menu.o boot/menu.ld
	$(LD) -m elf_i386 -T boot/menu.ld -o $@ $(BUILD)/menu_entry.o $(BUILD)/menu.o

$(BUILD)/IceStarOS_c_mbr.vhd: $(IMAGE) $(BUILD)/mbr.bin $(BUILD)/menu.bin
	cp $(IMAGE) $@
	dd if=$(BUILD)/mbr.bin of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD)/menu.bin of=$@ bs=512 seek=1 count=3 conv=notrunc status=none

verify: $(BUILD)/IceStarOS_c_mbr.vhd
	test "$$(wc -c < $(BUILD)/mbr.bin)" -eq 512
	test "$$(wc -c < $(BUILD)/menu.bin)" -eq 1536
	cmp -i 446:446 -n 66 $(IMAGE) $(BUILD)/mbr.bin
	cmp -i 2048:2048 $(IMAGE) $(BUILD)/IceStarOS_c_mbr.vhd

run: $(BUILD)/IceStarOS_c_mbr.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c
