#ifndef ICESTAR_RASTER_BRUSH_H
#define ICESTAR_RASTER_BRUSH_H
#include <stdint.h>
uint32_t kernel_palette_lookup_81b714(uint32_t index);
uint32_t kernel_brush_square_81d032(uint32_t id,uint32_t x,uint32_t y,
    uint32_t color,uint32_t side);
uint32_t kernel_brush_line_81d070(uint32_t id,uint32_t x0,uint32_t y0,
    uint32_t x1,uint32_t y1,uint32_t color,uint32_t side);
uint32_t kernel_brush_circle_81d1fc(uint32_t id,uint32_t x,uint32_t y,
    uint32_t radius,uint32_t color,uint32_t side);
uint32_t kernel_circle_quadrants_81d4e6(uint32_t id,uint32_t x,uint32_t y,
    uint32_t radius,uint32_t quadrants,uint32_t color);
#endif
