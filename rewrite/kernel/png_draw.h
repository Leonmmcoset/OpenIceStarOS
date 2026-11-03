#ifndef OPENICESTAR_KERNEL_PNG_DRAW_H
#define OPENICESTAR_KERNEL_PNG_DRAW_H
#include <stdint.h>

void kernel_png_surface_draw_824014(volatile uint32_t target,volatile uint32_t x,volatile uint32_t y,volatile uint32_t data,volatile uint32_t length);
void kernel_png_direct_draw_8241d0(volatile uint32_t target,volatile uint32_t x,volatile uint32_t y,volatile uint32_t data,volatile uint32_t length);
void kernel_png_file_draw_82436c(volatile uint32_t target,volatile uint32_t x,volatile uint32_t y,volatile uint32_t path);

#endif
