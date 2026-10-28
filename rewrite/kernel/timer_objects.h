#ifndef OPENICESTAR_KERNEL_TIMER_OBJECTS_H
#define OPENICESTAR_KERNEL_TIMER_OBJECTS_H
#include <stdint.h>

uint32_t kernel_timer_find_free(void);
uint32_t kernel_timer_create(uint32_t delay, uint32_t payload);
void kernel_timer_delete(uint32_t index);
uint32_t kernel_timer_rearm(uint32_t index, uint32_t delay);
#endif
