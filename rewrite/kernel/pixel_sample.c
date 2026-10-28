/* Sample descending visible layers below a surface's captured order position.
 * The original uses open rectangle edges and always reads DWORD pixels,
 * irrespective of the candidate surface's depth. IDs/order addresses wrap.
 */
#include "pixel_sample.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SLOT(id) DWORD(0x500000u + (id) * 4u)
#define ORDER(n) DWORD(0x510000u + (n) * 4u)
#define LAYER(n) SLOT(ORDER(n))

uint32_t kernel_pixel_sample_below_81b514(uint32_t id, uint32_t x, uint32_t y)
{
    uint32_t n = DWORD(SLOT(id) + 44u) - 1u;
    for (; (int32_t)n > 0; --n) {
        if ((int32_t)DWORD(LAYER(n) + 4u) >= (int32_t)x) continue;
        uint32_t surface = LAYER(n);
        uint32_t right = DWORD(LAYER(n) + 4u);
        right += DWORD(surface + 12u);
        if ((int32_t)right <= (int32_t)x) continue;
        if ((int32_t)DWORD(LAYER(n) + 8u) >= (int32_t)y) continue;
        surface = LAYER(n);
        uint32_t bottom = DWORD(LAYER(n) + 16u);
        bottom += DWORD(surface + 8u);
        if ((int32_t)bottom <= (int32_t)y) continue;
        uint32_t row = y - DWORD(LAYER(n) + 8u);
        row *= DWORD(LAYER(n) + 12u);
        uint32_t offset = x - DWORD(LAYER(n) + 4u) + row;
        surface = LAYER(n);
        uint32_t pixel = DWORD(DWORD(surface) + offset * 4u);
        if (DWORD(LAYER(n) + 40u) != pixel) return pixel;
    }
    return UINT32_MAX;
}
