#ifndef ICESTAR_KERNEL_SURFACE_PRESENT_H
#define ICESTAR_KERNEL_SURFACE_PRESENT_H
#include <stdint.h>

uint32_t kernel_surface_regions_test(uint32_t ax, uint32_t ay,
    uint32_t aw, uint32_t ah, uint32_t bx, uint32_t by,
    uint32_t bw, uint32_t bh);

/* Both functions may clamp or swap the caller's x/y stack words. */
uint32_t kernel_surface_present8(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height);
uint32_t kernel_surface_present32(uint32_t x, uint32_t y,
    uint32_t width, uint32_t height);
#endif
