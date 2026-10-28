#ifndef OPENICESTAR_LEGACY_API_H
#define OPENICESTAR_LEGACY_API_H
#include <stdint.h>

uint32_t legacy_rectangle(uint32_t surface, uint32_t x1, uint32_t y1,
                          uint32_t x2, uint32_t y2, uint32_t color);
uint32_t legacy_window_create(uint32_t x, uint32_t y, uint32_t width,
                              uint32_t height, uint32_t flags, uint32_t color,
                              uint32_t style, const char *title);
uint32_t legacy_window_show(uint32_t window);
uint32_t legacy_service_6b(uint32_t handle);
uint32_t legacy_poll(void);
uint32_t legacy_surface(uint32_t window);
#endif
