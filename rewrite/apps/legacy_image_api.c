/* MOV.X's four additional reachable shared-table SDK wrappers.
 * Reload the global table for each invocation, like the original SDK. */
#include "legacy_image_api.h"

#define SLOT(n, type) ((type)(uintptr_t)((volatile uint32_t *)(uintptr_t) \
                       *(volatile uint32_t *)0x1b0e4)[n])

uint32_t legacy_image_create(uint32_t window, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height, uint32_t flags,
                              const char *path)
{
    typedef uint32_t (*function)(uint32_t, uint32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t, const char *);
    return SLOT(61, function)(window, x, y, width, height, flags, path);
}

#define ONE(name, slot) \
    uint32_t name(uint32_t value) { \
        typedef uint32_t (*function)(uint32_t); \
        return SLOT(slot, function)(value); \
    }
ONE(legacy_image_release, 62)
ONE(legacy_image_show, 65)
ONE(legacy_window_refresh, 110)
