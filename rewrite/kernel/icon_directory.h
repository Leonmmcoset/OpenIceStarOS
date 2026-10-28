#ifndef ICESTAR_KERNEL_ICON_DIRECTORY_H
#define ICESTAR_KERNEL_ICON_DIRECTORY_H
#include <stdint.h>

void kernel_icon_draw_id(uint32_t surface_id, uint32_t x, uint32_t y,
                         uint32_t package, uint32_t frame);
void kernel_icon_draw_centered(uint32_t surface, uint32_t package);
void kernel_icon_draw_frame(uint32_t surface, uint32_t x, uint32_t y,
                            uint32_t package, uint32_t frame);
#endif
