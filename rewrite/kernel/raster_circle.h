#ifndef OPENICESTAR_RASTER_CIRCLE_H
#define OPENICESTAR_RASTER_CIRCLE_H
#include <stdint.h>
uint32_t kernel_raster_circle8_81c396(uint32_t id,uint32_t x,uint32_t y,
                                    uint32_t radius,uint32_t color);
uint32_t kernel_raster_circle32_81c6f2(uint32_t id,uint32_t x,uint32_t y,
                                     uint32_t radius,uint32_t color);
#endif
