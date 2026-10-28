#ifndef ICESTAR_KERNEL_ICON_FILES_H
#define ICESTAR_KERNEL_ICON_FILES_H
#include <stdint.h>

void kernel_icon_draw_file(uint32_t surface_id, uint32_t x, uint32_t y,
                           uint32_t name, uint32_t frame);
uint32_t kernel_icon_draw_file_auto(uint32_t surface_id, uint32_t x, uint32_t y,
                                    uint32_t name, uint32_t frame);
#endif
