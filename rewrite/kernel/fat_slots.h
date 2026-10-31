#ifndef OPENICESTAR_KERNEL_FAT_SLOTS_H
#define OPENICESTAR_KERNEL_FAT_SLOTS_H

#include <stdint.h>

int32_t kernel_fat_root_empty_82798c(void);
int32_t kernel_fat_directory_empty_827a40(uint32_t directory);
int32_t kernel_fat_find_free_cluster_827ace(void);
int32_t kernel_fat_find_free_after_827b44(uint32_t cluster);

#endif
