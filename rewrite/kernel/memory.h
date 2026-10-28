#ifndef OPENICESTAR_KERNEL_MEMORY_H
#define OPENICESTAR_KERNEL_MEMORY_H
#include <stdint.h>

uint32_t kernel_allocate(uint32_t size);
uint32_t kernel_allocate_for_task(uint32_t size, uint32_t owner);
uint32_t kernel_allocate_aligned(uint32_t size, uint32_t owner, uint32_t alignment);
void kernel_free_allocation(uint32_t address);
uint32_t kernel_allocation_size(uint32_t address);
uint32_t kernel_task_allocation_total(uint32_t owner);
#endif
