/* FAT16 file/first-sector readers, 0x827454..0x82798c.
 * Preserve the original exact 0xffff sentinel, live BPB reads, cached FAT
 * pointer and data LBA, unsigned ceiling arithmetic and ignored read errors.
 * These routines deliberately do not introduce a hardened FAT policy. */
#include "fat_files.h"
#include "memory.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

uint32_t kernel_fat_read_sectors_8254ca(uint32_t lba, uint32_t count, uint32_t destination);
uint32_t kernel_fat_read_sector_828b44(uint32_t lba, uint32_t destination);
int32_t kernel_x_find_path_8263c0(uint32_t name, uint32_t record);

static __attribute__((always_inline)) inline uint32_t cluster_bytes(void)
{
    uint32_t sectors = B(BPB + 13u);
    return sectors * W(BPB + 11u);
}

static __attribute__((always_inline)) inline uint32_t data_lba(void)
{
    uint32_t root = W(BPB + 17u) * 32u;
    uint32_t numerator = root + W(BPB + 11u) - 1u;
    uint32_t root_sectors = numerator / W(BPB + 11u);
    uint32_t fat_size = W(BPB + 22u);
    uint32_t fat_sectors = fat_size * B(BPB + 16u);
    uint32_t reserved = W(BPB + 14u);
    return root_sectors + fat_sectors + reserved + D(0x7dc6u);
}

static __attribute__((always_inline)) inline uint32_t read_chain(uint32_t record, uint32_t destination)
{
    uint32_t fat = D(0x1a83au), start = data_lba();
    uint32_t next = W(fat + W(record + 26u) * 2u), index = 1;
    uint32_t count = B(BPB + 13u);
    uint32_t lba = (W(record + 26u) - 2u) * B(BPB + 13u) + start;
    (void)kernel_fat_read_sectors_8254ca(lba, count, destination);
    while (next != 0xffffu) {
        uint32_t stride = B(BPB + 13u) * index;
        uint32_t target = destination + stride * W(BPB + 11u);
        count = B(BPB + 13u);
        lba = (next - 2u) * B(BPB + 13u) + start;
        (void)kernel_fat_read_sectors_8254ca(lba, count, target);
        next = W(fat + next * 2u);
        ++index;
    }
    return destination;
}

uint32_t kernel_fat_read_alloc_827454(uint32_t record)
{
    uint32_t numerator = cluster_bytes() + D(record + 28u) - 1u;
    uint32_t count = numerator / cluster_bytes();
    if (B(record + 11u) & 16u) count = 1;
    uint32_t sectors = B(BPB + 13u) * count;
    uint32_t destination = kernel_allocate(sectors * W(BPB + 11u));
    if (!destination) return 0;
    return read_chain(record, destination);
}

uint32_t kernel_x_read_path_82760c(uint32_t record, uint32_t destination)
{
    uint32_t numerator = cluster_bytes() + D(record + 28u) - 1u;
    /* Although the original never uses this count, DIV faults occur before
     * its null-destination check. Keep that observable computation. */
    volatile uint32_t count = numerator / cluster_bytes();
    if ((int32_t)count <= 0) count = 1;
    if (!destination) return 0;
    return read_chain(record, destination);
}

uint32_t kernel_icon_read_absolute_827798(uint32_t name)
{
    uint8_t record[32];
    if (kernel_x_find_path_8263c0(name, (uint32_t)(uintptr_t)record) != 1) return 0;
    return kernel_fat_read_alloc_827454((uint32_t)(uintptr_t)record);
}

uint32_t kernel_fat_read_named_8277dc(uint32_t name, uint32_t destination)
{
    uint8_t record[32];
    if (kernel_x_find_path_8263c0(name, (uint32_t)(uintptr_t)record) != 1) return 0;
    return kernel_x_read_path_82760c((uint32_t)(uintptr_t)record, destination);
}

uint32_t kernel_x_header_relative_827826(uint32_t index, uint32_t destination)
{
    uint32_t reserved = W(BPB + 14u), fats = B(BPB + 16u);
    uint32_t root_bytes = (reserved + fats * W(BPB + 22u)) * W(BPB + 11u);
    uint32_t record = BPB + root_bytes + (index << 5);
    (void)D(0x1a83au);
    uint32_t start = data_lba();
    uint32_t lba = (W(record + 26u) - 2u) * B(BPB + 13u) + start;
    return kernel_fat_read_sector_828b44(lba, destination);
}

uint32_t kernel_x_header_path_8278f4(uint32_t record, uint32_t destination)
{
    (void)D(0x1a83au);
    uint32_t start = data_lba();
    uint32_t lba = (W(record + 26u) - 2u) * B(BPB + 13u) + start;
    return kernel_fat_read_sector_828b44(lba, destination);
}
