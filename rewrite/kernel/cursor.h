#ifndef ICESTAR_KERNEL_CURSOR_H
#define ICESTAR_KERNEL_CURSOR_H
#include <stdint.h>

uint32_t kernel_cursor_show(void);
uint32_t kernel_cursor_hide(void);
uint32_t kernel_cursor_select(uint32_t shape);
uint32_t kernel_cursor_move(uint32_t shape, uint32_t x, uint32_t y);
#endif
