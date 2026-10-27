#ifndef ICESTAR_KERNEL_SURFACE_ORDER_H
#define ICESTAR_KERNEL_SURFACE_ORDER_H
#include <stdint.h>

uint32_t kernel_surface_order_remove(uint32_t layer);
uint32_t kernel_surface_order_append(uint32_t layer);
uint32_t kernel_surface_order_front(uint32_t layer);
uint32_t kernel_surface_order_move(uint32_t layer, uint32_t position);
#endif
