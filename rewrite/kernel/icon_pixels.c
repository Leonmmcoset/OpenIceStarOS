/* Direct surface writes and ordered-dither palette conversion. Signed bounds,
 * full-width color arguments, truncating signed division, and 32-bit address
 * wrapping follow the original instructions rather than a generic RGB API.
 */
#include "icon_pixels.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

void kernel_icon_put8_81b37c(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t color)
{
    if ((int32_t)x < 0 || (int32_t)DWORD(surface + 12u) <= (int32_t)x) return;
    if ((int32_t)y < 0 || (int32_t)DWORD(surface + 16u) <= (int32_t)y) return;
    uint32_t row = DWORD(surface + 12u) * y;
    uint32_t pixels = DWORD(surface);
    BYTE(pixels + x + row) = (uint8_t)color;
}

void kernel_icon_put32_81b3c0(uint32_t surface, uint32_t x, uint32_t y,
                            uint32_t color)
{
    if ((int32_t)x < 0 || (int32_t)DWORD(surface + 12u) <= (int32_t)x) return;
    if ((int32_t)y < 0 || (int32_t)DWORD(surface + 16u) <= (int32_t)y) return;
    uint32_t offset = (DWORD(surface + 12u) * y + x) * 4u;
    DWORD(DWORD(surface) + offset) = color;
}

uint32_t kernel_icon_quantize_81fd90(uint32_t red, uint32_t green,
                                   uint32_t blue, uint32_t x, uint32_t y)
{
    /* Threshold matrix, indexed [y & 1][x & 1]: {{3, 1}, {0, 2}}. */
    uint32_t threshold = (y & 1u) ? ((x & 1u) ? 2u : 0u)
                                  : ((x & 1u) ? 1u : 3u);
    /* The multiply wraps before interpretation as signed. These are signed
     * divisions toward zero, including for out-of-range DWORD arguments. */
    int32_t r = ((int32_t)(red * 21u) / 256 + (int32_t)threshold) / 4;
    int32_t g = ((int32_t)(green * 21u) / 256 + (int32_t)threshold) / 4;
    int32_t b = ((int32_t)(blue * 21u) / 256 + (int32_t)threshold) / 4;
    return ((uint32_t)r + (uint32_t)g * 6u + (uint32_t)b * 36u + 16u) & 255u;
}
