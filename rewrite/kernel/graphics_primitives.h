#ifndef ICESTAR_KERNEL_GRAPHICS_PRIMITIVES_H
#define ICESTAR_KERNEL_GRAPHICS_PRIMITIVES_H
#include <stdint.h>

uint32_t kernel_blend_color(uint32_t foreground, uint32_t background,
                            uint32_t alpha, uint32_t depth);
void kernel_wait_vertical_retrace(void);
#endif
