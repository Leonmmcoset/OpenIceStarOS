#ifndef OPENICESTAR_KERNEL_FPU_H
#define OPENICESTAR_KERNEL_FPU_H
#include <stdint.h>

uint32_t kernel_fpu_available(void);
uint32_t kernel_fpu_enable(void);
void kernel_fpu_restore_task(void);
void kernel_fpu_mark_lazy(void);
void kernel_fpu_fault(void);

void kernel_fpu_save_legacy(uint32_t address);
void kernel_fpu_restore_legacy(uint32_t address);
void kernel_fpu_save_environment(uint32_t address);
void kernel_fpu_load_environment(uint32_t address);
void kernel_fpu_save_extended(uint32_t address);
void kernel_fpu_restore_extended(uint32_t address);
/* XSAVE/XRSTOR retain the original EAX=address and incoming EDX masks. */
void kernel_fpu_save_xstate(uint32_t address);
void kernel_fpu_restore_xstate(uint32_t address);
#endif
