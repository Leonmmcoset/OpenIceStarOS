#ifndef OPENICESTAR_PIXEL_ACCESS_H
#define OPENICESTAR_PIXEL_ACCESS_H
#include <stdint.h>

void kernel_pixel_put8_81b26e(uint32_t id, uint32_t x, uint32_t y, uint32_t color);
void kernel_pixel_put32_81b2f4(uint32_t id, uint32_t x, uint32_t y, uint32_t color);
uint32_t kernel_pixel_get8_81b406(uint32_t id, uint32_t x, uint32_t y);
uint32_t kernel_pixel_get32_81b486(uint32_t id, uint32_t x, uint32_t y);

#endif
