#ifndef ICESTAR_KERNEL_SURFACE_LIFECYCLE_H
#define ICESTAR_KERNEL_SURFACE_LIFECYCLE_H
#include <stdint.h>

uint32_t kernel_surfaces_init(void);
uint32_t kernel_surface_find_free(void);
uint32_t kernel_surface_create(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
uint32_t kernel_surface_delete(uint32_t layer);
/* Existing order-list removal at 0x8178f0, not recovered by this module. */
uint32_t kernel_surface_order_remove(uint32_t entry);
#endif
