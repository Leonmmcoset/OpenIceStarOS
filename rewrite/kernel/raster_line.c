/* Original inclusive DDA lines: 10-bit fixed-point positions, wrapped
 * arithmetic, signed division toward zero and arithmetic right shifts.
 * No clipping. Pixel writes may change the table, metadata or caller words.
 */
#include "raster_line.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u + (id) * 4u)

/* Inlining keeps both entry points self-contained in their original slots. */
#define GEOMETRY() \
    uint32_t dx = x1 - x0, dy = y1 - y0; \
    uint32_t fx = x0 << 10, fy = y0 << 10, count; \
    if ((int32_t)dx < 0) dx = 0u - dx; \
    if ((int32_t)dy < 0) dy = 0u - dy; \
    if ((int32_t)dx >= (int32_t)dy) { \
        count = dx + 1u; \
        dx = (int32_t)x0 > (int32_t)x1 ? 0xfffffc00u : 1024u; \
        uint32_t numerator = (y1 - y0 + ((int32_t)y0 <= (int32_t)y1 ? 1u : UINT32_MAX)) << 10; \
        dy = (uint32_t)((int32_t)numerator / (int32_t)count); \
    } else { \
        count = dy + 1u; \
        dy = (int32_t)y0 > (int32_t)y1 ? 0xfffffc00u : 1024u; \
        uint32_t numerator = (x1 - x0 + ((int32_t)x0 <= (int32_t)x1 ? 1u : UINT32_MAX)) << 10; \
        dx = (uint32_t)((int32_t)numerator / (int32_t)count); \
    }

uint32_t kernel_raster_line8_81b734(volatile uint32_t id, volatile uint32_t x0,
    volatile uint32_t y0, volatile uint32_t x1, volatile uint32_t y1, volatile uint32_t color)
{
    GEOMETRY();
    uint32_t i;
    for (i = 0; (int32_t)i < (int32_t)count; ++i) {
        uint32_t surface = SLOT(id);
        uint32_t row = (uint32_t)((int32_t)fy >> 10) * DWORD(SLOT(id) + 12u);
        uint32_t x = (uint32_t)((int32_t)fx >> 10);
        uint32_t pixels = DWORD(surface);
        uint32_t value = color;
        BYTE(pixels + x + row) = (uint8_t)value;
        fx += dx;
        fy += dy;
    }
    return i;
}

uint32_t kernel_raster_line32_81b8dc(volatile uint32_t id, volatile uint32_t x0,
    volatile uint32_t y0, volatile uint32_t x1, volatile uint32_t y1, volatile uint32_t color)
{
    GEOMETRY();
    uint32_t i;
    for (i = 0; (int32_t)i < (int32_t)count; ++i) {
        uint32_t row = (uint32_t)((int32_t)fy >> 10) * DWORD(SLOT(id) + 12u);
        uint32_t offset = (uint32_t)((int32_t)fx >> 10) + row;
        uint32_t surface = SLOT(id);
        uint32_t pixels = DWORD(surface);
        uint32_t value = color;
        DWORD(pixels + offset * 4u) = value;
        fx += dx;
        fy += dy;
    }
    return i;
}
