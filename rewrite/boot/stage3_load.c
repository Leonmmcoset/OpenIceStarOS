#include "fat16.h"

static void out8(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static int ata_read(void *context, uint32_t lba, uint8_t *buffer)
{
    uint32_t timeout;
    unsigned i;
    (void)context;
    if (lba >= 0x10000000) return -1;
    out8(0x1f6, (uint8_t)(0xe0 | (lba >> 24)));
    out8(0x1f2, 1);
    out8(0x1f3, (uint8_t)lba);
    out8(0x1f4, (uint8_t)(lba >> 8));
    out8(0x1f5, (uint8_t)(lba >> 16));
    out8(0x1f7, 0x20);
    for (timeout = 0; timeout < 1000000; ++timeout) {
        uint8_t status = in8(0x1f7);
        if (!(status & 0x80)) {
            if (status & 0x21) return -1;
            if (status & 0x08) break;
        }
    }
    if (timeout == 1000000) return -1;
    for (i = 0; i < 256; ++i) {
        uint16_t value;
        __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"((uint16_t)0x1f0));
        buffer[i * 2] = (uint8_t)value;
        buffer[i * 2 + 1] = (uint8_t)(value >> 8);
    }
    return 0;
}

#ifdef REWRITE_INIT
int stage_load_rewritten_init(void)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) {
        if (ata_read(0, 13 + i, (uint8_t *)(uintptr_t)(0x60000 + i * 512)))
            return -1;
    }
    *(volatile uint32_t *)0x70010 = 0x60000;
    return 0;
}
#endif

int stage_load_init(void)
{
    static const uint8_t name[11] = "INIT    X  ";
    struct fat16_volume volume;
    uint32_t size, lba, fat_address, second_fat, root_address;
    uint32_t partition_lba = *(volatile uint32_t *)0x7dc6;
    int result = fat16_mount(&volume, ata_read, 0, partition_lba);
    if (result != FAT16_OK) return result;

    /* INIT.X and the original kernel expect this exact in-memory layout. */
    if (volume.data_lba - partition_lba > (0x800000 - 0x520000) / 512)
        return FAT16_FORMAT;
    if (ata_read(0, partition_lba, (uint8_t *)0x520000)) return FAT16_IO;
    fat_address = 0x520000 + (volume.fat_lba - partition_lba) * 512;
    second_fat = fat_address + (uint32_t)volume.fat_sectors * 512;
    root_address = second_fat + (uint32_t)volume.fat_sectors * 512;
    for (lba = volume.fat_lba; lba < volume.data_lba; ++lba) {
        uint8_t *destination = (uint8_t *)(uintptr_t)
            (0x520000 + (lba - partition_lba) * 512);
        if (ata_read(0, lba, destination)) return FAT16_IO;
    }
    *(volatile uint32_t *)0x1a83a = fat_address;
    *(volatile uint32_t *)0x1a83e = second_fat;
    *(volatile uint32_t *)0x1a842 = root_address;
    *(volatile uint32_t *)0x1a846 = root_address + (uint32_t)volume.root_entries * 32;
    return fat16_read_root_file(&volume, name, (uint8_t *)0x70000, 0x100000, &size);
}
