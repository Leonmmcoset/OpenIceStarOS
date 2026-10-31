/* FAT16 free directory-slot and cluster scans recovered from
 * 0x82798c..0x827bba.  The original uses the BPB root-entry count for both
 * directory scans and a signed half-FAT-size bound for cluster scans. */
#include "fat_slots.h"

#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

static __attribute__((always_inline)) inline uint32_t root_directory(void)
{
    uint32_t reserved = W(BPB + 14u);
    uint32_t fats = B(BPB + 16u);
    uint32_t fat_sectors = W(BPB + 22u);
    uint32_t bytes_per_sector = W(BPB + 11u);
    return BPB + (reserved + fats * fat_sectors) * bytes_per_sector;
}

static __attribute__((always_inline)) inline int32_t fat_entry_count(void)
{
    /* This is the compiler's signed divide-by-two sequence for the 32-bit
     * IMUL product.  Make the wrap and truncation explicit instead of relying
     * on signed-overflow behavior in the recovered source. */
    uint32_t product = (uint32_t)W(BPB + 22u) * (uint32_t)W(BPB + 11u);
    int32_t signed_product = (int32_t)product;
    return signed_product / 2;
}

int32_t kernel_fat_root_empty_82798c(void)
{
    uint32_t directory = root_directory();
    int32_t index = 0;
    while (index < (int32_t)W(BPB + 17u)) {
        if (B(directory + ((uint32_t)index << 5)) == 0) return index;
        ++index;
    }
    index = 0;
    while (index < (int32_t)W(BPB + 17u)) {
        if (B(directory + ((uint32_t)index << 5)) == 0xe5u) return index;
        ++index;
    }
    return -1;
}

int32_t kernel_fat_directory_empty_827a40(uint32_t directory)
{
    int32_t index = 0;
    while (index < (int32_t)W(BPB + 17u)) {
        if (B(directory + ((uint32_t)index << 5)) == 0) return index;
        ++index;
    }
    index = 0;
    while (index < (int32_t)W(BPB + 17u)) {
        if (B(directory + ((uint32_t)index << 5)) == 0xe5u) return index;
        ++index;
    }
    return -1;
}

int32_t kernel_fat_find_free_cluster_827ace(void)
{
    uint32_t fat = *(volatile uint32_t *)(uintptr_t)0x1a83au;
    int32_t limit = fat_entry_count();
    int32_t cluster = 2;
    while (cluster < limit) {
        if (*(volatile uint16_t *)(uintptr_t)(fat + ((uint32_t)cluster << 1)) == 0)
            return cluster;
        ++cluster;
    }
    return -1;
}

int32_t kernel_fat_find_free_after_827b44(uint32_t cluster)
{
    uint32_t fat = *(volatile uint32_t *)(uintptr_t)0x1a83au;
    int32_t limit = fat_entry_count();
    int32_t candidate = (int32_t)(cluster + 1u);
    while (candidate < limit) {
        if (*(volatile uint16_t *)(uintptr_t)(fat + ((uint32_t)candidate << 1)) == 0)
            return candidate;
        ++candidate;
    }
    return -1;
}
