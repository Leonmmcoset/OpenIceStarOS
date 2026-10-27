#include "clusters.h"

static uint16_t word(const uint8_t *p)
{
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}

int init_read_chain(const struct init_disk *disk, const uint8_t *entry,
                    uint8_t *destination)
{
    const uint8_t *bpb = disk->bpb;
    uint32_t bytes = word(bpb + 11), sectors = bpb[13];
    uint32_t root_sectors = (word(bpb + 17) * 32u + bytes - 1) / bytes;
    uint32_t data_lba = disk->partition_lba + word(bpb + 14) +
                        bpb[16] * word(bpb + 22) + root_sectors;
    uint32_t cluster = word(entry + 26), count = 0;

    do {
        uint32_t next = word(disk->fat + cluster * 2);
        uint32_t i;
        for (i = 0; i < sectors; ++i)
            disk->read_sector(disk->context, data_lba + (cluster - 2) * sectors + i,
                              destination + count * sectors * bytes + i * 512);
        ++count;
        cluster = next;
    } while (cluster != 0xffff);
    return (int)count;
}

#ifndef INIT_CLUSTERS_HOST_TEST
#include "entry.h"

static uint8_t in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static void out8(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a" (value), "Nd" (port));
}

static void read_sector(void *context, uint32_t lba, uint8_t *destination)
{
    volatile uint32_t *lock = (volatile uint32_t *)0x1b108;
    unsigned i;
    (void)context;
    while (*lock) { }
    *lock = 1;
    while (in8(0x1f7) & 0x88) { }
    out8(0x1f2, 1);
    out8(0x1f3, (uint8_t)lba);
    out8(0x1f4, (uint8_t)(lba >> 8));
    out8(0x1f5, (uint8_t)(lba >> 16));
    out8(0x1f6, (uint8_t)(0xe0 | (lba >> 24)));
    out8(0x1f7, 0x20);
    while (in8(0x1f7) != 0x58) { }
    for (i = 0; i < 256; ++i) {
        uint16_t value;
        __asm__ volatile ("inw %1, %0" : "=a" (value) : "Nd" ((uint16_t)0x1f0));
        destination[i * 2] = (uint8_t)value;
        destination[i * 2 + 1] = (uint8_t)(value >> 8);
    }
    *lock = 0;
}

static struct init_disk current_disk(void)
{
    struct init_disk disk;
    disk.bpb = (const uint8_t *)0x520000;
    disk.fat = (const uint8_t *)(uintptr_t)*(volatile uint32_t *)0x1a83a;
    disk.partition_lba = *(volatile uint32_t *)0x7dc6;
    disk.read_sector = read_sector;
    disk.context = 0;
    return disk;
}

int init_read_root_directory(int index, uint8_t *destination)
{
    struct init_disk disk = current_disk();
    const uint8_t *bpb = disk.bpb;
    const uint8_t *root = bpb + (word(bpb + 14) + bpb[16] * word(bpb + 22)) * word(bpb + 11);
    return init_read_chain(&disk, root + index * 32, destination);
}

int init_read_file(const uint8_t *entry, uint8_t *destination)
{
    struct init_disk disk = current_disk();
    if (!destination) return 0;
    init_read_chain(&disk, entry, destination);
    return (int)(uintptr_t)destination;
}
#endif
