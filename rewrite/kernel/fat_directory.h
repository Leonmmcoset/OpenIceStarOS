#ifndef OPENICESTAR_KERNEL_FAT_DIRECTORY_H
#define OPENICESTAR_KERNEL_FAT_DIRECTORY_H
#include <stdint.h>
int32_t kernel_fat_root_name_8258a6(uint32_t index, uint32_t output);
int32_t kernel_fat_directory_name_825a7e(uint32_t directory, uint32_t index, uint32_t output);
uint32_t kernel_fat_root_extension_825bcc(uint32_t index, uint32_t output);
uint32_t kernel_fat_directory_extension_825c46(uint32_t directory, uint32_t index, uint32_t output);
int32_t kernel_x_find_relative_825c8e(uint32_t name);
int32_t kernel_fat_directory_find_825d34(uint32_t directory, uint32_t name);
uint32_t kernel_fat_root_size_826940(uint32_t index);
uint32_t kernel_fat_directory_size_82698c(uint32_t directory, uint32_t index);
uint32_t kernel_fat_root_attributes_8269a6(uint32_t index);
uint32_t kernel_fat_directory_attributes_8269f2(uint32_t directory, uint32_t index);
#endif
