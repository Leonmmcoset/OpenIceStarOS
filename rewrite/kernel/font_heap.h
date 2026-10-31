#ifndef OPENICESTAR_KERNEL_FONT_HEAP_H
#define OPENICESTAR_KERNEL_FONT_HEAP_H
#include <stdint.h>
uint32_t kernel_font_heap_alloc_830b4c(uint32_t heap, uint32_t size, uint32_t userdata);
void kernel_font_heap_free_830c0e(uint32_t heap, uint32_t object);
void kernel_font_heap_cleanup_830c28(uint32_t heap, uint32_t userdata);
#endif
