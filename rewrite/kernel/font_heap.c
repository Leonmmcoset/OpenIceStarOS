/* Font rasterizer object arena: chunk allocation, free-list reuse, cleanup.
 * Chunk counts and unchecked unsigned address arithmetic match original x86.
 * Cleanup frees chunks without clearing the original arena descriptor. */
#include "font_heap.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_font_malloc_817590(uint32_t size);
void kernel_font_free_8175a4(uint32_t address);

uint32_t kernel_font_heap_alloc_830b4c(uint32_t heap, uint32_t size, uint32_t userdata)
{
    (void)userdata;
    if (D(heap + 4u)) {
        uint32_t result = D(heap + 4u);
        D(heap + 4u) = D(result);
        return result;
    }
    if (!D(heap + 8u)) {
        uint32_t count = size < 32u ? 2000u : size <= 127u ? 800u : 100u;
        uint32_t block = kernel_font_malloc_817590(size * count + 4u);
        if (!block) return 0;
        D(block) = D(heap);
        D(heap) = block;
        D(heap + 8u) = count;
    }
    D(heap + 8u) -= 1u;
    return D(heap) + D(heap + 8u) * size + 4u;
}

void kernel_font_heap_free_830c0e(uint32_t heap, uint32_t object)
{
    D(object) = D(heap + 4u);
    D(heap + 4u) = object;
}

void kernel_font_heap_cleanup_830c28(uint32_t heap, uint32_t userdata)
{
    (void)userdata;
    uint32_t block = D(heap);
    while (block) {
        uint32_t next = D(block);
        kernel_font_free_8175a4(block);
        block = next;
    }
}
