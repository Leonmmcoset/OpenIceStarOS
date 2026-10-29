#ifndef OPENICESTAR_KERNEL_FAT_RELATIVE_H
#define OPENICESTAR_KERNEL_FAT_RELATIVE_H
#include <stdint.h>
uint32_t kernel_x_read_relative_826ade(uint32_t index, uint32_t destination);
int32_t kernel_fat_read_relative_named_826c3a(uint32_t name, uint32_t destination);
uint32_t kernel_fat_read_relative_cluster_826c6e(uint32_t index, uint32_t destination, int32_t skip);
uint32_t kernel_fat_read_relative_named_cluster_826dd0(uint32_t name, uint32_t destination, int32_t skip);
uint32_t kernel_icon_read_relative_826f38(uint32_t name);
uint32_t kernel_bitmap_read_relative_827032(uint32_t index);
#endif
