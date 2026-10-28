#ifndef OPENICESTAR_KERNEL_SURFACE_REFRESH_H
#define OPENICESTAR_KERNEL_SURFACE_REFRESH_H
#include <stdint.h>

uint32_t kernel_syscall_target_81905e(uint32_t layer, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height);
uint32_t kernel_surface_target_817cd0(uint32_t layer, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height);
uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);
#endif
