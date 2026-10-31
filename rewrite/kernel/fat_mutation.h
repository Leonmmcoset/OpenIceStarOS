#ifndef OPENICESTAR_KERNEL_FAT_MUTATION_H
#define OPENICESTAR_KERNEL_FAT_MUTATION_H

#include <stdint.h>

uint32_t kernel_fat_release_root_chain_827d4a(uint32_t index);
uint32_t kernel_fat_release_record_chain_827e6c(uint32_t record);
int32_t kernel_fat_delete_root_827f58(uint32_t index);
int32_t kernel_fat_delete_relative_827fe2(uint32_t name);

#endif
