/* Color blending at 0x828f1b and VGA status polling at 0x828f0f. */
#include "graphics_primitives.h"

uint32_t kernel_blend_color(uint32_t foreground, uint32_t background,
                            uint32_t alpha, uint32_t depth)
{
    uint32_t shift, result = 0;
    alpha = (uint8_t)alpha;
    if (depth != 32) return alpha ? foreground : background;
    /* Only B/G/R participate. Division is by 256, not 255, and the
     * foreground is never fully opaque for an eight-bit alpha value. */
    for (shift = 0; shift < 24; shift += 8) {
        uint32_t first = (foreground >> shift) & 0xffu;
        uint32_t second = (background >> shift) & 0xffu;
        result |= ((first * alpha + second * (256u - alpha)) >> 8) << shift;
    }
    return result;
}

__attribute__((naked, noinline)) void kernel_wait_vertical_retrace(void)
{
    /* Preserve EDX and all of EAX except AL. Wait for bit 3 to be set;
     * there is no preliminary wait for it to clear. Incoming DF survives. */
    __asm__ ("pushl %edx\n\tmovw $0x3da,%dx\n\t"
             "1:\tinb %dx,%al\n\ttestb $8,%al\n\tjz 1b\n\tpopl %edx\n\tret");
    __builtin_unreachable();
}
