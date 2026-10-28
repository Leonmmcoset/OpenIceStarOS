/* Unclipped inclusive rectangle borders at 0x81c058 and 0x81c1e4.
 * Corners and degenerate edges are written repeatedly. ID/color remain live;
 * only xmin is recaptured from the caller after both horizontal edges.
 */
#include "raster_outline.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u + (id) * 4u)
#define MINIMUM(a, b, low) \
    uint32_t low = a; \
    if ((int32_t)low > (int32_t)b) low = b
#define EXTREMA(a, b, low, high) \
    MINIMUM(a, b, low); \
    uint32_t high = a; \
    if ((int32_t)high < (int32_t)b) high = b

static __attribute__((always_inline)) inline void point8(volatile uint32_t *id,
    uint32_t x, uint32_t y, volatile uint32_t *color)
{
    uint32_t surface = SLOT(*id);
    uint32_t row = y * DWORD(SLOT(*id) + 12u);
    uint32_t pixels = DWORD(surface);
    uint32_t value = *color;
    BYTE(pixels + row + x) = (uint8_t)value;
}
static __attribute__((always_inline)) inline void point32(volatile uint32_t *id,
    uint32_t x, uint32_t y, volatile uint32_t *color)
{
    uint32_t offset = y * DWORD(SLOT(*id) + 12u) + x;
    uint32_t surface = SLOT(*id);
    uint32_t pixels = DWORD(surface);
    uint32_t value = *color;
    DWORD(pixels + offset * 4u) = value;
}
#define OUTLINE(bits, address) \
uint32_t kernel_raster_outline##bits##_##address(volatile uint32_t id, \
    volatile uint32_t x0, volatile uint32_t y0, volatile uint32_t x1, \
    volatile uint32_t y1, volatile uint32_t color) \
{ \
    EXTREMA(x0, x1, xmin, xmax); \
    EXTREMA(y0, y1, ymin, ymax); \
    for (uint32_t x = xmin; (int32_t)x <= (int32_t)xmax; ++x) { \
        point##bits(&id, x, ymin, &color); \
        point##bits(&id, x, ymax, &color); \
    } \
    MINIMUM(x0, x1, left); \
    uint32_t y; \
    for (y = ymin; (int32_t)y <= (int32_t)ymax; ++y) { \
        point##bits(&id, left, y, &color); \
        point##bits(&id, xmax, y, &color); \
    } \
    return y; \
}
OUTLINE(8, 81c058)
OUTLINE(32, 81c1e4)
