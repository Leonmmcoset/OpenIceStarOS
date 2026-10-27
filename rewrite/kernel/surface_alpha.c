/* Four-pixel 32-bit blend helper at 0x818134..0x81846a. */
#include "surface_alpha.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))

uint32_t kernel_surface_blend4(uint32_t destination, uint32_t source,
    uint32_t weights)
{
    uint32_t pixel, result = 0;
    for (pixel = 0; pixel < 4; ++pixel) {
        uint32_t offset = 4 * pixel, alpha = weights + offset;
        int32_t channel;
        result = alpha;
        if ((int32_t)WORD(alpha) > 127) continue;
        if ((int32_t)WORD(alpha) <= 0) {
            WORD(destination + offset) = WORD(source + offset);
            result = destination + offset;
            continue;
        }
        /* The channel order and repeated weight loads are observable when
         * pixels and weight words alias. Only B/G/R bytes are blended. */
        for (channel = 2; channel >= 0; --channel) {
            uint32_t at = offset + (uint32_t)channel;
            uint32_t old = BYTE(destination + at);
            uint32_t weight = WORD(alpha);
            uint32_t incoming = BYTE(source + at);
            uint32_t inverse = 128 - WORD(alpha);
            result = (uint32_t)((int32_t)(old * weight + incoming * inverse) >> 7);
            BYTE(destination + at) = (uint8_t)result;
        }
    }
    return result;
}
