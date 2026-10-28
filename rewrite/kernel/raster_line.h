#ifndef OPENICESTAR_RASTER_LINE_H
#define OPENICESTAR_RASTER_LINE_H
#include <stdint.h>

uint32_t kernel_raster_line8_81b734(uint32_t id, uint32_t x0, uint32_t y0,
                                  uint32_t x1, uint32_t y1, uint32_t color);
uint32_t kernel_raster_line32_81b8dc(uint32_t id, uint32_t x0, uint32_t y0,
                                   uint32_t x1, uint32_t y1, uint32_t color);

#endif
