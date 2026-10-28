#ifndef OPENICESTAR_KERNEL_SURFACE_SCALE_H
#define OPENICESTAR_KERNEL_SURFACE_SCALE_H
#include <stdint.h>

/* Scale a whole source layer into a destination rectangle. Only matching
 * 8-bit or 32-bit depths copy pixels. There is no clipping or alpha blend. */
uint32_t kernel_surface_scale(uint32_t destination, uint32_t source,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height);
#endif
