/* FAT16 contiguous-sector writer recovered from 0x827c72.
 * The original repeatedly calls the ATA one-sector entry, rereads the BPB
 * bytes-per-sector field on every iteration, and uses unsigned 32-bit wrap
 * arithmetic for both the end LBA and the loop comparison. */
#include "fat_io.h"

#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define BPB 0x520000u

void kernel_fat_write_sector_828c18(uint32_t lba, uint32_t source);

void kernel_fat_write_sectors_827c72(uint32_t lba, uint32_t source,
                                    uint32_t count)
{
    uint32_t end = lba + count;
    while (lba < end) {
        kernel_fat_write_sector_828c18(lba, source);
        source += W(BPB + 11u);
        ++lba;
    }
}
