#ifndef ICESTAR_KERNEL_MEMORY_INIT_H
#define ICESTAR_KERNEL_MEMORY_INIT_H
#include <stdint.h>

uint32_t kernel_probe_memory(void);
void kernel_init_heap(void);
uint32_t kernel_probe_pages(uint32_t first, uint32_t last);
#endif
