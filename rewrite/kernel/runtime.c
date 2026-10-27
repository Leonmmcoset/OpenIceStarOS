/* Shared services 237 and 113, recovered from 0x81b714..0x81b734
 * and 0x82ad81..0x82adab of the original kernel. */
#include <stdint.h>
#include "runtime.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

uint32_t kernel_palette_color(uint32_t index)
{
    uint32_t palette = WORD(0x1b0e0);
    /* The original accepts unchecked indices; both operations wrap. */
    return WORD(palette + index * 4u);
}

uint32_t kernel_wait_ticks(uint32_t duration)
{
    uint32_t deadline = duration + WORD(0x1b084), now;
    /* This is an unsigned absolute deadline, including its wraparound bug.
       Yielding can change the tick count in either direction. */
    while ((now = WORD(0x1b084)) < deadline) kernel_yield_task();
    return now;
}
