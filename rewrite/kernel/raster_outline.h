#ifndef OPENICESTAR_RASTER_OUTLINE_H
#define OPENICESTAR_RASTER_OUTLINE_H
#include <stdint.h>
uint32_t kernel_raster_outline8_81c058(uint32_t id, uint32_t x0, uint32_t y0,
                                     uint32_t x1, uint32_t y1, uint32_t color);
uint32_t kernel_raster_outline32_81c1e4(uint32_t id, uint32_t x0, uint32_t y0,
                                      uint32_t x1, uint32_t y1, uint32_t color);
#endif
