#ifndef OPENICESTAR_KERNEL_FAT_BLOCK_H
#define OPENICESTAR_KERNEL_FAT_BLOCK_H
#include <stdint.h>

uint32_t kernel_x_read_lba_alloc_8252b4(uint32_t lba);
uint32_t kernel_x_read_lba_into_82539e(uint32_t lba, uint32_t destination);
uint32_t kernel_fat_read_sectors_alloc_825476(uint32_t lba, uint32_t count);
uint32_t kernel_fat_read_sectors_8254ca(uint32_t lba, uint32_t count, uint32_t destination);
uint32_t kernel_fat_read_partition_825506(uint32_t index, uint32_t destination);
uint32_t kernel_fat_read_root_directory_8255ae(uint32_t index, uint32_t destination);
uint32_t kernel_fat_alloc_root_directory_8256a8(uint32_t index);
uint32_t kernel_fat_alloc_directory_8257bc(uint32_t index, uint32_t directory);
#endif
