#ifndef OPENICESTAR_KERNEL_MEMORY_OPS_H
#define OPENICESTAR_KERNEL_MEMORY_OPS_H
#include <stdint.h>

uint32_t kernel_move_bytes(uint32_t destination, uint32_t source, uint32_t size);
void kernel_fill_bytes(uint32_t destination, uint32_t value, uint32_t size);
void kernel_copy_1200_bytes(uint32_t destination, uint32_t source);
void kernel_copy_dwords(uint32_t destination, uint32_t source, int32_t count);
uint32_t kernel_memory_differs(uint32_t first, uint32_t second, int32_t count);
uint32_t kernel_malloc(uint32_t size);
void kernel_free(uint32_t address);
#endif
