#ifndef ICESTAR_SYSCALL_SURFACE_H
#define ICESTAR_SYSCALL_SURFACE_H
#include "syscalls.h"

/* The compositor body at this address is still original. Its five arguments
 * are verified here; coordinate/extent names await its own reconstruction. */
uint32_t kernel_syscall_target_81905e(uint32_t layer, uint32_t a1,
    uint32_t a2, uint32_t a3, uint32_t a4);
#endif
