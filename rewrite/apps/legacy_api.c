/* COLOR.X's reachable SDK wrappers. The type-0x0a program directly calls
 * the shared service table at physical 0x1b0e4 instead of using interrupts.
 * APP_CFLAGS retains separate argument copies at every call boundary. */
#include "legacy_api.h"

#define SLOT(n, type) ((type)(uintptr_t)((volatile uint32_t *)(uintptr_t) \
                       *(volatile uint32_t *)0x1b0e4)[n])

uint32_t legacy_rectangle(uint32_t s, uint32_t x1, uint32_t y1,
                          uint32_t x2, uint32_t y2, uint32_t c)
{
    typedef uint32_t (*function)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    return SLOT(2, function)(s, x1, y1, x2, y2, c);
}

uint32_t legacy_window_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                              uint32_t f, uint32_t c, uint32_t style, const char *title)
{
    typedef uint32_t (*function)(uint32_t, uint32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t, uint32_t, const char *);
    return SLOT(100, function)(x, y, w, h, f, c, style, title);
}

#define ONE(name, slot) \
    uint32_t name(uint32_t value) { \
        typedef uint32_t (*function)(uint32_t); \
        return SLOT(slot, function)(value); \
    }
ONE(legacy_window_show, 105)
ONE(legacy_service_6b, 107)
ONE(legacy_surface, 111)

uint32_t legacy_poll(void)
{
    typedef uint32_t (*function)(void);
    return SLOT(108, function)();
}
