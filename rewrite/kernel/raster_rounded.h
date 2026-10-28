#ifndef OPENICESTAR_RASTER_ROUNDED_H
#define OPENICESTAR_RASTER_ROUNDED_H
#include <stdint.h>
uint32_t kernel_rounded_outline_81d6c2(uint32_t id,uint32_t x,uint32_t y,
    uint32_t width,uint32_t height,uint32_t radius,uint32_t color);
uint32_t kernel_rounded_fill_81d8ca(uint32_t id,uint32_t x,uint32_t y,
    uint32_t width,uint32_t height,uint32_t radius,uint32_t color);
uint32_t kernel_chamfer_fill_81da68(uint32_t id,uint32_t x,uint32_t y,
    uint32_t width,uint32_t height,uint32_t radius,uint32_t color);
#endif
