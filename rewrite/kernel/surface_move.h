#ifndef ICESTAR_KERNEL_SURFACE_MOVE_H
#define ICESTAR_KERNEL_SURFACE_MOVE_H
#include <stdint.h>

/* Redraw a moved layer and the exposed old rectangle. Coordinates are
 * absolute; width/height are shared by the old and new rectangles. */
uint32_t kernel_surface_move_refresh(uint32_t layer, uint32_t old_x,
    uint32_t old_y, uint32_t width, uint32_t height,
    uint32_t new_x, uint32_t new_y);
#endif
