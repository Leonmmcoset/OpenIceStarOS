#ifndef OPENICESTAR_KERNEL_FAT_WRITE_H
#define OPENICESTAR_KERNEL_FAT_WRITE_H
#include <stdint.h>

uint32_t kernel_fat_split_parent_828018(uint32_t name, uint32_t parent,
                                      uint32_t filename);
uint32_t kernel_fat_write_root_82867e(uint32_t name, uint32_t source,
                                    uint32_t bytes);
#endif
