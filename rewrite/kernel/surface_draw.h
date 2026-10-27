#ifndef ICESTAR_KERNEL_SURFACE_DRAW_H
#define ICESTAR_KERNEL_SURFACE_DRAW_H
#include <stdint.h>

uint32_t kernel_surface_draw8(uint32_t layer, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height);
uint32_t kernel_surface_draw32(uint32_t layer, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height);
#endif
