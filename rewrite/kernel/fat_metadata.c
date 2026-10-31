/* FAT and root-directory metadata flush recovered from 0x827cc4.  The
 * original writes the contiguous FAT/root span from the cached FAT buffer and
 * returns the computed sector count after the write completes. */
#include "fat_metadata.h"
#include "fat_io.h"

#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u
#define PARTITION 0x7dbeu

uint32_t kernel_fat_flush_metadata_827cc4(void)
{
    uint32_t fat = D(0x1a83au);
    uint32_t start = (uint32_t)W(BPB + 14u) + D(PARTITION + 8u);
    uint32_t fat_sectors = (uint32_t)B(BPB + 16u) * W(BPB + 22u);
    int32_t root_sectors = (int32_t)(W(BPB + 17u) * 32u) /
                           (int32_t)W(BPB + 11u);
    uint32_t count = fat_sectors + (uint32_t)root_sectors;
    kernel_fat_write_sectors_827c72(start, fat, count);
    return count;
}
