#ifndef ICESTAR_KERNEL_RUNTIME_H
#define ICESTAR_KERNEL_RUNTIME_H
#include <stdint.h>
uint32_t kernel_palette_color(uint32_t index);
uint32_t kernel_wait_ticks(uint32_t duration);
void kernel_yield_task(void);
void kernel_schedule_task(void);
void kernel_timer_tick(void);
#endif
