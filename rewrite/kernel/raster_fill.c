/* Inclusive rectangle fills, recovered from 0x81ba8a..0x81bf50.
 * Each endpoint is independently clipped against signed (dimension - 1).
 * Zero/negative dimensions retain the original wrapped, possibly negative
 * endpoints. Stores may alias metadata, the ID table or incoming arguments.
 */
#include "raster_fill.h"
#include "service_forward.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u + (id) * 4u)
#define EXTREMA(a, b, low, high) \
    uint32_t low = a; \
    if ((int32_t)low > (int32_t)b) low = b; \
    uint32_t high = a; \
    if ((int32_t)high < (int32_t)b) high = b
#define CLIP(coordinate, field) do { \
    if ((int32_t)coordinate < 0) coordinate = 0; \
    if ((int32_t)(DWORD(SLOT(id) + field) - 1u) < (int32_t)coordinate) \
        coordinate = DWORD(SLOT(id) + field) - 1u; \
} while (0)
#define CLIP_ALL() \
    CLIP(xmin, 12u); CLIP(xmax, 12u); \
    CLIP(ymin, 16u); CLIP(ymax, 16u)

uint32_t kernel_raster_fill8_81ba8a(volatile uint32_t id, volatile uint32_t x0,
    volatile uint32_t y0, volatile uint32_t x1, volatile uint32_t y1, volatile uint32_t color)
{
    EXTREMA(y0, y1, ymin, ymax);
    uint32_t packed = color << 8;
    packed |= color;
    packed |= color << 16;
    packed |= color << 24;
    EXTREMA(x0, x1, xmin, xmax);
    CLIP_ALL();
    uint32_t y;
    for (y = ymin; (int32_t)y <= (int32_t)ymax; ++y) {
        uint32_t span = xmax - xmin + 1u;
        int32_t blocks = (int32_t)span / 4;
        int32_t tail = (int32_t)span % 4;
        uint32_t block;
        for (block = 0; (int32_t)block < blocks; ++block) {
            uint32_t surface = SLOT(id);
            uint32_t row = y * DWORD(SLOT(id) + 12u);
            uint32_t pixels = DWORD(surface);
            DWORD(pixels + row + block * 4u + xmin) = packed;
        }
        for (uint32_t byte = 0; (int32_t)byte < tail; ++byte) {
            uint32_t surface = SLOT(id);
            uint32_t row = y * DWORD(SLOT(id) + 12u);
            uint32_t pixels = DWORD(surface);
            uint32_t value = color;
            BYTE(pixels + row + block * 4u + xmin + byte) = (uint8_t)value;
        }
    }
    return y;
}

uint32_t kernel_raster_fill32_81bd26(volatile uint32_t id, volatile uint32_t x0,
    volatile uint32_t y0, volatile uint32_t x1, volatile uint32_t y1, volatile uint32_t color)
{
    EXTREMA(y0, y1, ymin, ymax);
    EXTREMA(x0, x1, xmin, xmax);
    CLIP_ALL();
    uint32_t pitch = DWORD(SLOT(id) + 12u) * 4u;
    uint32_t offset = ymin * pitch + xmin * 4u;
    uint32_t first = DWORD(SLOT(id)) + offset;
    uint32_t next = first, count = xmax - xmin + 1u;
    while (--count != UINT32_MAX) {
        uint32_t value = color;
        DWORD(next) = value;
        next += 4u;
    }
    uint32_t rows = ymax - ymin;
    next = first + pitch;
    uint32_t span = xmax - xmin + 1u;
    while (--rows != UINT32_MAX) {
        kernel_service_11(next, first, span);
        next += pitch;
    }
    /* The original final loop test loads EBP-40 into EAX. It returns that
     * expired local's address, even when no copy runs, not the service result.
     * Preserve this observable register value relative to the incoming frame;
     * the builtin also requires GCC to establish an EBP frame here.
     */
    return (uint32_t)(uintptr_t)__builtin_frame_address(0) - 40u;
}
