/* Layer-list dispatch and signed rectangle clipping, 0x817cd0..0x817dac.
 * The actual per-layer rasterizer remains a service-table dependency. */
#include "surface_composite.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*Draw)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint32_t surface_composite_body(volatile uint32_t *a)
{
    Draw draw = (Draw)(uintptr_t)WORD(WORD(0x1b0e4) + 40);
    uint32_t edge, screen, x, y, order;
    if ((int32_t)a[1] < 0) a[1] = 0;
    if ((int32_t)a[2] < 0) a[2] = 0;
    edge = a[3]; edge += a[1];
    if ((int32_t)edge > (int32_t)WORD(WORD(0x500000) + 12)) {
        screen = WORD(0x500000); x = a[1];
        a[3] = WORD(screen + 12) - x;
    }
    edge = a[4]; edge += a[2];
    if ((int32_t)edge > (int32_t)WORD(WORD(0x500000) + 16)) {
        screen = WORD(0x500000); y = a[2];
        a[4] = WORD(screen + 16) - y;
    }
    order = WORD(WORD(0x500000 + 4 * a[0]) + 44);
    while (order <= WORD(0x510000)) {
        /* Match descending argument loads and the live list lookup.
         * Each callback gets a separate outgoing argument area. */
        uint32_t height = a[4], width = a[3];
        y = a[2]; x = a[1];
        draw(WORD(0x510000 + 4 * order), x, y, width, height);
        ++order;
    }
    return order;
}

/* Expose the real incoming stack words to C, including the original's
 * observable clipping stores. Parameter locals could lose those stores. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
uint32_t kernel_surface_target_817cd0(uint32_t layer, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;call surface_composite_body;"
        "leal 4(%esp),%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
