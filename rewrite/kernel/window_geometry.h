#ifndef OPENICESTAR_KERNEL_WINDOW_GEOMETRY_H
#define OPENICESTAR_KERNEL_WINDOW_GEOMETRY_H
#include <stdint.h>

void kernel_window_maximize_80ed56(volatile uint32_t id);
void kernel_window_layout_all_80f4be(void);
void kernel_window_unmaximize_80fbfc(volatile uint32_t id);

#endif
