CC := gcc
LD := ld
OBJCOPY := objcopy
CFLAGS := -m16 -march=i386 -Os -ffreestanding -fno-pic -fno-pie \
          -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra
BUILD := build
IMAGE := ../IceStarOS_x86_32M.vhd

.PHONY: all verify run run-hybrid run-c-init
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

$(BUILD)/menu_hybrid.o: boot/menu.c | $(BUILD)
	$(CC) $(CFLAGS) -DNEXT_STAGE_SECTORS=8 -c $< -o $@

$(BUILD)/menu.bin: $(BUILD)/menu_entry.o $(BUILD)/menu.o boot/menu.ld
	$(LD) -m elf_i386 -T boot/menu.ld -o $@ $(BUILD)/menu_entry.o $(BUILD)/menu.o

$(BUILD)/menu_hybrid.bin: $(BUILD)/menu_entry.o $(BUILD)/menu_hybrid.o boot/menu.ld
	$(LD) -m elf_i386 -T boot/menu.ld -o $@ $(BUILD)/menu_entry.o $(BUILD)/menu_hybrid.o

$(BUILD)/IceStarOS_c_mbr.vhd: $(IMAGE) $(BUILD)/mbr.bin $(BUILD)/menu.bin
	cp $(IMAGE) $@
	dd if=$(BUILD)/mbr.bin of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD)/menu.bin of=$@ bs=512 seek=1 count=3 conv=notrunc status=none

$(BUILD)/fat16.o: boot/fat16.c boot/fat16.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/stage3_hybrid.o: boot/stage3_hybrid.S $(IMAGE) | $(BUILD)
	$(CC) -m32 -c $< -o $@

$(BUILD)/stage3_c_init.o: boot/stage3_hybrid.S $(IMAGE) | $(BUILD)
	$(CC) -m32 -DREWRITE_INIT -c $< -o $@

$(BUILD)/stage3_load.o: boot/stage3_load.c boot/fat16.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/stage3_load_c_init.o: boot/stage3_load.c boot/fat16.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -DREWRITE_INIT -c $< -o $@

$(BUILD)/init_transfer.o: boot/init_transfer.c boot/init_transfer.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/pci.o: boot/pci.c boot/pci.h | $(BUILD)
	$(CC) -m32 -march=i386 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -c $< -o $@

$(BUILD)/stage3_hybrid.bin: $(BUILD)/stage3_hybrid.o $(BUILD)/stage3_load.o $(BUILD)/fat16.o $(BUILD)/init_transfer.o $(BUILD)/pci.o boot/stage3_hybrid.ld
	$(LD) -m elf_i386 -T boot/stage3_hybrid.ld -o $@ $(BUILD)/stage3_hybrid.o $(BUILD)/stage3_load.o $(BUILD)/fat16.o $(BUILD)/init_transfer.o $(BUILD)/pci.o

$(BUILD)/stage3_c_init.bin: $(BUILD)/stage3_c_init.o $(BUILD)/stage3_load_c_init.o $(BUILD)/fat16.o $(BUILD)/init_transfer.o $(BUILD)/pci.o boot/stage3_hybrid.ld
	$(LD) -m elf_i386 -T boot/stage3_hybrid.ld -o $@ $(BUILD)/stage3_c_init.o $(BUILD)/stage3_load_c_init.o $(BUILD)/fat16.o $(BUILD)/init_transfer.o $(BUILD)/pci.o

$(BUILD)/IceStarOS_c_fat16.vhd: $(IMAGE) $(BUILD)/mbr.bin $(BUILD)/menu_hybrid.bin $(BUILD)/stage3_hybrid.bin
	cp $(IMAGE) $@
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

$(BUILD)/init_c.bin: $(BUILD)/init_entry.o $(BUILD)/init_directory.o $(BUILD)/init_clusters.o init/init.ld
	$(LD) -m elf_i386 -T init/init.ld -o $@ $(BUILD)/init_entry.o $(BUILD)/init_directory.o $(BUILD)/init_clusters.o

$(BUILD)/IceStarOS_c_init.vhd: $(BUILD)/IceStarOS_c_fat16.vhd $(BUILD)/stage3_c_init.bin $(BUILD)/init_c.bin
	cp $(BUILD)/IceStarOS_c_fat16.vhd $@
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

verify: $(BUILD)/IceStarOS_c_mbr.vhd $(BUILD)/IceStarOS_c_fat16.vhd $(BUILD)/IceStarOS_c_init.vhd $(BUILD)/fat16.o $(BUILD)/fat16_image $(BUILD)/init_entry.o $(BUILD)/init_directory.o $(BUILD)/init_clusters.o $(BUILD)/init_directory_test $(BUILD)/init_clusters_test $(BUILD)/init_entry_test $(BUILD)/init_transfer_test $(BUILD)/pci_test
	test "$$(wc -c < $(BUILD)/mbr.bin)" -eq 512
	test "$$(wc -c < $(BUILD)/menu.bin)" -eq 1536
	cmp -i 446:446 -n 66 $(IMAGE) $(BUILD)/mbr.bin
	cmp -i 2048:2048 $(IMAGE) $(BUILD)/IceStarOS_c_mbr.vhd
	test "$$(wc -c < $(BUILD)/stage3_hybrid.bin)" -eq 4096
	cmp -i 6656:6656 $(IMAGE) $(BUILD)/IceStarOS_c_fat16.vhd
	test "$$(wc -c < $(BUILD)/init_c.bin)" -eq 2048
	cmp -i 8704:8704 $(IMAGE) $(BUILD)/IceStarOS_c_init.vhd
	$(BUILD)/fat16_image $(IMAGE) $(BUILD)/init.x
	printf '%s  %s\n' 0364c0f872a844d6603634fe69adabce92dfafca4a6146474bcb858130899007 $(BUILD)/init.x | sha256sum -c -
	$(BUILD)/init_entry_test
	$(BUILD)/init_directory_test $(IMAGE)
	$(BUILD)/init_clusters_test $(IMAGE) $(BUILD)/dat.bin $(BUILD)/icestar.x
	printf '%s  %s\n' 01a8f491fefa8563238533936beca1ea7cafe7cbebfb55a9743a436b27b61ea8 $(BUILD)/dat.bin 9edc55ea50b889f86ec7b00a1305eada39c979221113f6d86e13fd2b31f99ae3 $(BUILD)/icestar.x | sha256sum -c -
	$(BUILD)/init_transfer_test
	$(BUILD)/pci_test

run: $(BUILD)/IceStarOS_c_mbr.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

run-hybrid: $(BUILD)/IceStarOS_c_fat16.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c

run-c-init: $(BUILD)/IceStarOS_c_init.vhd
	qemu-system-i386 -machine pc -m 128 -drive file=$<,format=raw,snapshot=on -boot c
