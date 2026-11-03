#ifndef OPENICESTAR_KERNEL_WINDOW_FOCUS_H
#define OPENICESTAR_KERNEL_WINDOW_FOCUS_H
#include <stdint.h>

void kernel_window_raise_8084a0(volatile uint32_t id);
void kernel_window_activate_8087e0(volatile uint32_t id);
void kernel_window_deactivate_808b78(volatile uint32_t id);
int32_t kernel_window_button_hit_808db8(uint32_t id,int32_t x,int32_t y);

#endif
