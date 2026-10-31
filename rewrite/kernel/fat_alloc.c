/* Allocating FAT16 readers, 0x82711e..0x827454. Keep the original
 * unsigned allocation arithmetic, signed root-sector division, exact
 * 0xffff chain terminator and unconditional first-cluster read. */
#include "fat_alloc.h"
#include "memory.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

uint32_t kernel_fat_read_sectors_8254ca(uint32_t lba, uint32_t count, uint32_t destination);

static __attribute__((always_inline)) inline uint32_t data_lba(void)
{
    int32_t numerator = (int32_t)(W(BPB + 17u) * 32u + W(BPB + 11u) - 1u);
    uint32_t root = (uint32_t)(numerator / (int32_t)W(BPB + 11u));
    uint32_t fats = W(BPB + 22u) * B(BPB + 16u);
    uint32_t reserved = W(BPB + 14u);
    return root + fats + reserved + D(0x7dc6u);
}

uint32_t kernel_fat_allocate_index_82711e(uint32_t directory, int32_t index)
{
    if (index < 0) return 0;
    uint32_t record = directory + ((uint32_t)index << 5);
    uint32_t bytes = B(BPB + 13u) * W(BPB + 11u);
    uint32_t numerator = bytes + D(record + 28u) - 1u;
    /* Directory attributes override the result only after the original DIV. */
    volatile uint32_t clusters = numerator / (B(BPB + 13u) * W(BPB + 11u));
    if (B(record + 11u) & 16u) clusters = 1;
    uint32_t destination = kernel_allocate(B(BPB + 13u) * clusters * W(BPB + 11u));
    if (!destination) return 0;
    uint32_t fat = D(0x1a83au), start = data_lba();
    uint32_t next = W(fat + W(record + 26u) * 2u), count = 1;
    uint32_t sectors = B(BPB + 13u);
    uint32_t lba = (W(record + 26u) - 2u) * B(BPB + 13u) + start;
    (void)kernel_fat_read_sectors_8254ca(lba, sectors, destination);
    while (next != 0xffffu) {
        uint32_t target = destination + B(BPB + 13u) * count * W(BPB + 11u);
        sectors = B(BPB + 13u);
        lba = (next - 2u) * B(BPB + 13u) + start;
        (void)kernel_fat_read_sectors_8254ca(lba, sectors, target);
        next = W(fat + next * 2u);
        ++count;
    }
    return destination;
}

uint32_t kernel_fat_allocate_clusters_8272f4(uint32_t cluster, uint32_t requested)
{
    uint32_t destination = kernel_allocate(B(BPB + 13u) * requested * W(BPB + 11u));
    if (!destination) return 0;
    uint32_t fat = D(0x1a83au), start = data_lba();
    uint32_t next = W(fat + cluster * 2u), count = 1;
    uint32_t sectors = B(BPB + 13u);
    uint32_t lba = (cluster - 2u) * B(BPB + 13u) + start;
    (void)kernel_fat_read_sectors_8254ca(lba, sectors, destination);
    while (next != 0xffffu && requested > count) {
        uint32_t target = destination + B(BPB + 13u) * count * W(BPB + 11u);
        sectors = B(BPB + 13u);
        lba = (next - 2u) * B(BPB + 13u) + start;
        (void)kernel_fat_read_sectors_8254ca(lba, sectors, target);
        next = W(fat + next * 2u);
        ++count;
    }
    return destination;
}
