/* FAT16 chain release and root-record deletion, 0x827d4a..0x828018.
 * Retain the original unchecked indices, exact chain sentinels, and separate
 * release/delete operations. Deleting a record does not release its chain. */
#include "fat_mutation.h"
#include "fat_metadata.h"
#include "fat_directory.h"

#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

static __attribute__((always_inline)) inline uint32_t root_record(uint32_t index)
{
    uint32_t reserved = W(BPB + 14u);
    uint32_t fats = B(BPB + 16u);
    uint32_t fat_sectors = W(BPB + 22u);
    uint32_t bytes_per_sector = W(BPB + 11u);
    return BPB + (reserved + fats * fat_sectors) * bytes_per_sector + (index << 5);
}

static __attribute__((always_inline)) inline void release_chain(uint32_t fat,
                                                                uint32_t record)
{
    uint32_t high = W(record + 20u);
    uint32_t cluster = (high << 16) + W(record + 26u);
    uint32_t entry = fat + (cluster << 1);
    uint16_t next;
    do {
        next = W(entry);
        W(entry) = 0;
        entry = fat + ((uint32_t)next << 1);
    } while (next != 0xffffu && next != 0);
}

uint32_t kernel_fat_release_root_chain_827d4a(uint32_t index)
{
    uint32_t record = root_record(index);
    uint32_t fat1 = D(0x1a83au);
    uint32_t fat2 = D(0x1a83eu);
    if (B(BPB + 16u) != 0) release_chain(fat1, record);
    if (B(BPB + 16u) > 1) release_chain(fat2, record);
    return B(BPB + 16u);
}

uint32_t kernel_fat_release_record_chain_827e6c(uint32_t record)
{
    uint32_t fat1 = D(0x1a83au);
    uint32_t fat2 = D(0x1a83eu);
    if (B(BPB + 16u) != 0) release_chain(fat1, record);
    if (B(BPB + 16u) > 1) release_chain(fat2, record);
    return B(BPB + 16u);
}

int32_t kernel_fat_delete_root_827f58(uint32_t index)
{
    uint32_t record = root_record(index);
    if (B(record) == 0 || B(record) == 0xe5u) return -1;
    B(record) = 0xe5u;
    kernel_fat_flush_metadata_827cc4();
    return 1;
}

int32_t kernel_fat_delete_relative_827fe2(uint32_t name)
{
    int32_t index = kernel_x_find_relative_825c8e(name);
    if (index > 0) return kernel_fat_delete_root_827f58((uint32_t)index);
    return -1;
}
