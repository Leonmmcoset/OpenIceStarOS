#ifndef OPENICESTAR_KERNEL_FAT_FILES_H
#define OPENICESTAR_KERNEL_FAT_FILES_H
#include <stdint.h>
uint32_t kernel_fat_read_alloc_827454(uint32_t record);
uint32_t kernel_x_read_path_82760c(uint32_t record, uint32_t destination);
uint32_t kernel_icon_read_absolute_827798(uint32_t name);
uint32_t kernel_fat_read_named_8277dc(uint32_t name, uint32_t destination);
uint32_t kernel_x_header_relative_827826(uint32_t index, uint32_t destination);
uint32_t kernel_x_header_path_8278f4(uint32_t record, uint32_t destination);
#endif
