/* Accelerated DWORD-copy dispatcher recovered from 0x8173da..0x817484. */
#include "memory_accel.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))

void kernel_copy_dwords_sse(uint32_t destination, uint32_t source, int32_t count)
{
    /* The original uses arithmetic shift for the block count and signed
     * remainder for the tail. In particular, negative counts access nothing. */
    int32_t whole = count >> 2, remainder = count % 4, i;
    if (whole > 0) {
        kernel_copy_sse16_blocks(destination, source, (uint32_t)whole);
        destination += (uint32_t)whole << 4;
        source += (uint32_t)whole << 4;
    }
    for (i = 0; i < remainder; ++i)
        WORD(destination + (uint32_t)i * 4u) = WORD(source + (uint32_t)i * 4u);
}

#undef WORD
