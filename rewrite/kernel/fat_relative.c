/* Loaded-root FAT16 readers, 0x826ade..0x82711e. Retain signed index
 * gates, exact 0xffff termination, cluster-count returns, and the two
 * different end-of-chain behaviors of the single-cluster entry points. */
#include "fat_relative.h"
#include "memory.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

int32_t kernel_x_find_relative_825c8e(uint32_t name);
uint32_t kernel_fat_read_sectors_8254ca(uint32_t lba, uint32_t count, uint32_t destination);

static __attribute__((always_inline)) inline uint32_t root_record(uint32_t index)
{
    uint32_t sectors = W(BPB + 14u) + B(BPB + 16u) * W(BPB + 22u);
    return BPB + sectors * W(BPB + 11u) + (index << 5);
}

static __attribute__((always_inline)) inline uint32_t data_lba(void)
{
    /* The original uses signed IDIV here, unlike the record readers. */
    int32_t numerator = (int32_t)(W(BPB + 17u) * 32u + W(BPB + 11u) - 1u);
    uint32_t root_sectors = (uint32_t)(numerator / (int32_t)W(BPB + 11u));
    uint32_t fats = W(BPB + 22u) * B(BPB + 16u);
    uint32_t reserved = W(BPB + 14u);
    return root_sectors + fats + reserved + D(0x7dc6u);
}

uint32_t kernel_x_read_relative_826ade(uint32_t index, uint32_t destination)
{
    uint32_t record = root_record(index), fat = D(0x1a83au), start = data_lba();
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
    return count;
}

int32_t kernel_fat_read_relative_named_826c3a(uint32_t name, uint32_t destination)
{
    int32_t index = kernel_x_find_relative_825c8e(name);
    if (index > 0) return (int32_t)kernel_x_read_relative_826ade((uint32_t)index, destination);
    return index;
}

/* The indexed entry reads only when skip clusters actually exist. The named
 * entry instead reads the last reachable cluster after exhausting the chain,
 * including for a negative skip. Both return the number of links traversed. */
static __attribute__((always_inline)) inline uint32_t read_cluster(
    uint32_t index, uint32_t destination, int32_t skip, uint32_t require_exact)
{
    uint32_t record = root_record(index), fat = D(0x1a83au), start = data_lba();
    uint32_t next = W(fat + W(record + 26u) * 2u);
    uint32_t cluster = W(record + 26u), traversed = 0;
    if (skip > 0) {
        while (next != 0xffffu && traversed != (uint32_t)skip) {
            cluster = next;
            next = W(fat + next * 2u);
            ++traversed;
        }
    }
    if (!require_exact || traversed == (uint32_t)skip) {
        uint32_t target = destination + B(BPB + 13u) * traversed * W(BPB + 11u);
        uint32_t sectors = B(BPB + 13u);
        uint32_t lba = (cluster - 2u) * B(BPB + 13u) + start;
        (void)kernel_fat_read_sectors_8254ca(lba, sectors, target);
    }
    return traversed;
}

uint32_t kernel_fat_read_relative_cluster_826c6e(uint32_t index, uint32_t destination, int32_t skip)
{
    if ((int32_t)index <= 0) return 0;
    return read_cluster(index, destination, skip, 1);
}

uint32_t kernel_fat_read_relative_named_cluster_826dd0(uint32_t name, uint32_t destination, int32_t skip)
{
    int32_t index = kernel_x_find_relative_825c8e(name);
    if (index <= 0) return 0;
    return read_cluster((uint32_t)index, destination, skip, 0);
}

static __attribute__((always_inline)) inline uint32_t allocate_relative(uint32_t index)
{
    uint32_t record = root_record(index);
    uint32_t bytes = B(BPB + 13u) * W(BPB + 11u);
    uint32_t numerator = bytes + D(record + 28u) - 1u;
    uint32_t clusters = numerator / (B(BPB + 13u) * W(BPB + 11u));
    uint32_t destination = kernel_allocate(B(BPB + 13u) * clusters * W(BPB + 11u));
    if (!destination) return 0;
    (void)kernel_x_read_relative_826ade(index, destination);
    return destination;
}

uint32_t kernel_icon_read_relative_826f38(uint32_t name)
{
    int32_t index = kernel_x_find_relative_825c8e(name);
    if (index < 0) return 0;
    return allocate_relative((uint32_t)index);
}

uint32_t kernel_bitmap_read_relative_827032(uint32_t index)
{
    if ((int32_t)index < 0) return 0;
    return allocate_relative(index);
}
