#ifndef OPENICESTAR_KERNEL_ICON_PIXELS_H
#define OPENICESTAR_KERNEL_ICON_PIXELS_H
#include <stdint.h>

void kernel_icon_put8_81b37c(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t color);
void kernel_icon_put32_81b3c0(uint32_t surface, uint32_t x, uint32_t y,
                            uint32_t color);
uint32_t kernel_icon_quantize_81fd90(uint32_t red, uint32_t green,
                                   uint32_t blue, uint32_t x, uint32_t y);
#endif
