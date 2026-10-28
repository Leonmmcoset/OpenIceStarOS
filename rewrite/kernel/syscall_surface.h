#ifndef OPENICESTAR_SYSCALL_SURFACE_H
#define OPENICESTAR_SYSCALL_SURFACE_H
#include "syscalls.h"

/* The selector is recovered separately in surface_refresh.c. The forwarder
 * fixture observes this boundary to isolate its own argument-read contract. */
uint32_t kernel_syscall_target_81905e(uint32_t layer, uint32_t a1,
    uint32_t a2, uint32_t a3, uint32_t a4);
#endif
