/* Surface-ID pixel services. Table indexing and effective addresses wrap at
 * 32 bits. Reads retain the original missing negative-coordinate checks.
 * Repeated volatile table/field reads follow the original access sequence.
 */
#include "pixel_access.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SURFACE(id) DWORD(0x500000u + (id) * 4u)

void kernel_pixel_put8_81b26e(uint32_t id, uint32_t x, uint32_t y, uint32_t color)
{
    if ((int32_t)x < 0 || (int32_t)DWORD(SURFACE(id) + 12u) <= (int32_t)x) return;
    if ((int32_t)y < 0 || (int32_t)DWORD(SURFACE(id) + 16u) <= (int32_t)y) return;
    uint32_t surface = SURFACE(id);
    uint32_t row = DWORD(SURFACE(id) + 12u) * y;
    uint32_t pixels = DWORD(surface);
    BYTE(pixels + x + row) = (uint8_t)color;
}

void kernel_pixel_put32_81b2f4(uint32_t id, uint32_t x, uint32_t y, uint32_t color)
{
    if ((int32_t)x < 0 || (int32_t)DWORD(SURFACE(id) + 12u) <= (int32_t)x) return;
    if ((int32_t)y < 0 || (int32_t)DWORD(SURFACE(id) + 16u) <= (int32_t)y) return;
    uint32_t offset = (DWORD(SURFACE(id) + 12u) * y + x) * 4u;
    uint32_t surface = SURFACE(id);
    DWORD(DWORD(surface) + offset) = color;
}

uint32_t kernel_pixel_get8_81b406(uint32_t id, uint32_t x, uint32_t y)
{
    if ((int32_t)DWORD(SURFACE(id) + 12u) <= (int32_t)x) return UINT32_MAX;
    if ((int32_t)DWORD(SURFACE(id) + 16u) <= (int32_t)y) return UINT32_MAX;
    uint32_t surface = SURFACE(id);
    uint32_t row = DWORD(SURFACE(id) + 12u) * y;
    return BYTE(DWORD(surface) + x + row);
}

uint32_t kernel_pixel_get32_81b486(uint32_t id, uint32_t x, uint32_t y)
{
    if ((int32_t)DWORD(SURFACE(id) + 12u) <= (int32_t)x) return UINT32_MAX;
    if ((int32_t)DWORD(SURFACE(id) + 16u) <= (int32_t)y) return UINT32_MAX;
    uint32_t offset = (DWORD(SURFACE(id) + 12u) * y + x) * 4u;
    uint32_t surface = SURFACE(id);
    return DWORD(DWORD(surface) + offset);
}
