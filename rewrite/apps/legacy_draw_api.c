/* STARSCR.X's additional reachable SDK wrappers: shared slots 5, 113,
 * 128, and 237. Preserve table reloads and separate argument copies. */
#include "legacy_draw_api.h"
#define SLOT(n, type) ((type)(uintptr_t)((volatile uint32_t *)(uintptr_t) \
                       *(volatile uint32_t *)0x1b0e4)[n])

uint32_t legacy_circle(uint32_t s, uint32_t x, uint32_t y, uint32_t r, uint32_t c)
{
    typedef uint32_t (*function)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    return SLOT(5, function)(s, x, y, r, c);
}

#define ONE(name, slot) \
    uint32_t name(uint32_t value) { \
        typedef uint32_t (*function)(uint32_t); \
        return SLOT(slot, function)(value); \
    }
ONE(legacy_service_71, 113)
ONE(legacy_window_close, 128)
ONE(legacy_palette_color, 237)
