/* Contiguous readers and single-cluster directory readers, 0x8252b4..8258a6.
 * Preserve signed counts, fixed 512-byte disk stride, unchecked allocation
 * results, low cluster words, and live metadata after external calls. The
 * ATA hardware sector entry remains at its original address. */
#include "fat_block.h"
#include "memory.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

uint32_t kernel_fat_read_sector_828b44(uint32_t lba, uint32_t destination);

static __attribute__((always_inline)) inline int32_t image_sectors(void)
{
    int32_t size = (int32_t)D(0x18a04u);
    /* The original rereads size after testing its signed remainder. */
    if (size % 512 > 0) return (int32_t)D(0x18a04u) / 512 + 1;
    return (int32_t)D(0x18a04u) / 512;
}

uint32_t kernel_x_read_lba_alloc_8252b4(uint32_t lba)
{
    (void)kernel_fat_read_sector_828b44(lba, 0x18a00u);
    int32_t count = image_sectors();
    uint32_t destination = kernel_allocate((uint32_t)count << 9);
    for (uint32_t i = 0; (int32_t)i < count; ++i)
        (void)kernel_fat_read_sector_828b44(lba + i, destination + (i << 9));
    return destination;
}

uint32_t kernel_x_read_lba_into_82539e(uint32_t lba, uint32_t destination)
{
    (void)kernel_fat_read_sector_828b44(lba, 0x18a00u);
    int32_t count = image_sectors();
    for (uint32_t i = 0; (int32_t)i < count; ++i)
        (void)kernel_fat_read_sector_828b44(lba + i, destination + (i << 9));
    return destination;
}

uint32_t kernel_fat_read_sectors_alloc_825476(uint32_t lba, uint32_t count)
{
    uint32_t destination = kernel_allocate(count << 9);
    if ((int32_t)destination > 0)
        for (uint32_t i = 0; (int32_t)i < (int32_t)count; ++i)
            (void)kernel_fat_read_sector_828b44(lba + i, destination + (i << 9));
    return destination;
}

uint32_t kernel_fat_read_sectors_8254ca(uint32_t lba, uint32_t count,
                                      uint32_t destination)
{
    for (uint32_t i = 0; (int32_t)i < (int32_t)count; ++i)
        (void)kernel_fat_read_sector_828b44(lba + i, destination + (i << 9));
    return destination;
}

uint32_t kernel_fat_read_partition_825506(uint32_t index, uint32_t destination)
{
    uint32_t partition = 0x7dbeu + (index << 4);
    (void)kernel_fat_read_sector_828b44(D(partition + 8u), destination);
    uint32_t reserved = W(destination + 14u), bps = W(destination + 11u);
    uint32_t target = destination + reserved * bps;
    uint32_t fats = B(destination + 16u), fat_sectors = W(destination + 22u);
    uint32_t numerator = W(destination + 17u) * 32u + W(destination + 11u) - 1u;
    int32_t root = (int32_t)numerator / (int32_t)W(destination + 11u);
    uint32_t count = fats * fat_sectors + (uint32_t)root;
    uint32_t start = W(destination + 14u) + D(partition + 8u);
    return kernel_fat_read_sectors_8254ca(start, count, target);
}

static __attribute__((always_inline)) inline uint32_t root_record(uint32_t index)
{
    uint32_t reserved = W(BPB + 14u), fats = B(BPB + 16u);
    uint32_t sectors = W(BPB + 22u), bps = W(BPB + 11u);
    return BPB + (reserved + fats * sectors) * bps + (index << 5);
}

static __attribute__((always_inline)) inline uint32_t data_lba(void)
{
    uint32_t numerator = W(BPB + 17u) * 32u + W(BPB + 11u) - 1u;
    int32_t root = (int32_t)numerator / (int32_t)W(BPB + 11u);
    uint32_t sectors = W(BPB + 22u), fats = B(BPB + 16u);
    uint32_t reserved = W(BPB + 14u);
    return (uint32_t)root + sectors * fats + reserved + D(0x7dc6u);
}

static __attribute__((always_inline)) inline void read_directory(uint32_t record,
                                                               uint32_t destination)
{
    uint32_t start = data_lba(), count = B(BPB + 13u);
    uint32_t cluster = W(record + 26u), spc = B(BPB + 13u);
    (void)kernel_fat_read_sectors_8254ca((cluster - 2u) * spc + start, count, destination);
}

uint32_t kernel_fat_read_root_directory_8255ae(uint32_t index, uint32_t destination)
{
    uint32_t record = root_record(index);
    if (!(B(record + 11u) & 16u)) return 0;
    read_directory(record, destination);
    return 1;
}

uint32_t kernel_fat_alloc_root_directory_8256a8(uint32_t index)
{
    uint32_t record = root_record(index);
    if (!(B(record + 11u) & 16u)) return 0;
    uint32_t spc = B(BPB + 13u), bps = W(BPB + 11u);
    uint32_t destination = kernel_allocate(spc * bps);
    read_directory(record, destination);
    return destination;
}

uint32_t kernel_fat_alloc_directory_8257bc(uint32_t index, uint32_t directory)
{
    uint32_t record = directory + (index << 5);
    if (!(B(record + 11u) & 16u)) return 0;
    uint32_t spc = B(BPB + 13u), bps = W(BPB + 11u);
    uint32_t destination = kernel_allocate(spc * bps);
    read_directory(record, destination);
    return destination;
}
