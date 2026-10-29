#ifndef OPENICESTAR_KERNEL_FAT_ALLOC_H
#define OPENICESTAR_KERNEL_FAT_ALLOC_H
#include <stdint.h>
uint32_t kernel_fat_allocate_index_82711e(uint32_t directory, int32_t index);
uint32_t kernel_fat_allocate_clusters_8272f4(uint32_t cluster, uint32_t requested);
#endif
