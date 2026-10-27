#ifndef ICESTAR_KERNEL_SURFACE_LIFECYCLE_H
#define ICESTAR_KERNEL_SURFACE_LIFECYCLE_H
#include <stdint.h>
#include "surface_order.h"

uint32_t kernel_surfaces_init(void);
uint32_t kernel_surface_find_free(void);
uint32_t kernel_surface_create(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
uint32_t kernel_surface_delete(uint32_t layer);
#endif
