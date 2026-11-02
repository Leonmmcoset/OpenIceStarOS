#ifndef OPENICESTAR_KERNEL_WINDOW_LIFECYCLE_H
#define OPENICESTAR_KERNEL_WINDOW_LIFECYCLE_H
#include <stdint.h>

int32_t kernel_window_create_807a0a(volatile uint32_t x,volatile uint32_t y,
    volatile uint32_t width,volatile uint32_t height,volatile int32_t style,
    volatile uint32_t parameter,volatile uint32_t mode,volatile uint32_t title);
void kernel_window_close_81021c(volatile uint32_t id);

#endif
