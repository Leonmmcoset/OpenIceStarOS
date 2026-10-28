IMAGE := IceStarOS_x86_32M.vhd
BUILD := build
MBR := $(BUILD)/mbr.bin
MENU := $(BUILD)/menu.bin
REBUILT := $(BUILD)/OpenIceStarOS_mbr_rebuilt.vhd

.PHONY: all verify run
all: $(REBUILT)

$(BUILD):
	mkdir -p $@

$(MBR): reverse/boot/mbr.asm | $(BUILD)
	nasm -f bin $< -o $@

$(MENU): reverse/boot/menu.asm | $(BUILD)
	nasm -f bin $< -o $@

$(REBUILT): $(IMAGE) $(MBR) $(MENU)
	cp $(IMAGE) $@
	dd if=$(MBR) of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(MENU) of=$@ bs=512 seek=1 count=3 conv=notrunc status=none

verify: $(REBUILT)
	test "$$(wc -c < $(MBR))" -eq 512
	test "$$(wc -c < $(MENU))" -eq 1536
	cmp -n 512 $(IMAGE) $(MBR)
	cmp -i 512:0 -n 1536 $(IMAGE) $(MENU)
	cmp $(IMAGE) $(REBUILT)

run: $(REBUILT)
	qemu-system-i386 -machine pc -m 128 -drive file=$(REBUILT),format=raw,snapshot=on -boot c
