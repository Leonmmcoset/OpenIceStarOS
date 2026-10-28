#ifndef OPENICESTAR_RASTER_FILL_H
#define OPENICESTAR_RASTER_FILL_H
#include <stdint.h>

uint32_t kernel_raster_fill8_81ba8a(uint32_t id, uint32_t x0, uint32_t y0,
                                  uint32_t x1, uint32_t y1, uint32_t color);
uint32_t kernel_raster_fill32_81bd26(uint32_t id, uint32_t x0, uint32_t y0,
                                   uint32_t x1, uint32_t y1, uint32_t color);
#endif
