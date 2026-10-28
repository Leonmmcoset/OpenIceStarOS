#ifndef OPENICESTAR_LEGACY_DRAW_API_H
#define OPENICESTAR_LEGACY_DRAW_API_H
#include <stdint.h>
uint32_t legacy_circle(uint32_t surface, uint32_t x, uint32_t y,
                       uint32_t radius, uint32_t color);
uint32_t legacy_service_71(uint32_t value);
uint32_t legacy_window_close(uint32_t window);
uint32_t legacy_palette_color(uint32_t index);
#endif
