#ifndef OPENICESTAR_KERNEL_IMAGE_CONTROLS_H
#define OPENICESTAR_KERNEL_IMAGE_CONTROLS_H
#include <stdint.h>

int32_t kernel_image_create_8121d4(volatile uint32_t owner,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,
    volatile int32_t mode,volatile uint32_t path);
void kernel_image_release_8125dc(volatile uint32_t id);
void kernel_image_file_81282e(volatile uint32_t id,volatile uint32_t path);
void kernel_image_draw_8129ca(volatile uint32_t id);

#endif
