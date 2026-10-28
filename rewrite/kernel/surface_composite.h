#ifndef OPENICESTAR_KERNEL_SURFACE_COMPOSITE_H
#define OPENICESTAR_KERNEL_SURFACE_COMPOSITE_H
#include <stdint.h>

/* This routine clips its caller's argument words in place. */
uint32_t kernel_surface_target_817cd0(uint32_t layer, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height);
#endif
