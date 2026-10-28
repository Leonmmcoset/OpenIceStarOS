#ifndef ICESTAR_RASTER_DISC_H
#define ICESTAR_RASTER_DISC_H
#include <stdint.h>
uint32_t kernel_raster_disc8_81ca82(uint32_t id,uint32_t x,uint32_t y,
                                  uint32_t radius,uint32_t color);
uint32_t kernel_raster_disc32_81cd50(uint32_t id,uint32_t x,uint32_t y,
                                   uint32_t radius,uint32_t color);
#endif
