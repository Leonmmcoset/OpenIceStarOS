#ifndef OPENICESTAR_KERNEL_FAT_IO_H
#define OPENICESTAR_KERNEL_FAT_IO_H

#include <stdint.h>

void kernel_fat_write_sectors_827c72(uint32_t lba, uint32_t source,
                                    uint32_t count);

#endif
