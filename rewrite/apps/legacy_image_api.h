#ifndef ICESTAR_LEGACY_IMAGE_API_H
#define ICESTAR_LEGACY_IMAGE_API_H
#include <stdint.h>

uint32_t legacy_image_create(uint32_t window, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height, uint32_t flags,
                              const char *path);
uint32_t legacy_image_release(uint32_t image);
uint32_t legacy_image_show(uint32_t image);
uint32_t legacy_window_refresh(uint32_t window);
#endif
