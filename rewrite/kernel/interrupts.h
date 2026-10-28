#ifndef OPENICESTAR_KERNEL_INTERRUPTS_H
#define OPENICESTAR_KERNEL_INTERRUPTS_H
#include <stdint.h>
void kernel_init_interrupts(void);
void kernel_install_interrupt_gate(uint32_t handler, uint32_t selector,
                                   uint32_t attributes, uint32_t vector);
uint32_t kernel_boot_code_selector(void);
void kernel_init_pic(void);
void kernel_init_pit(void);
void kernel_init_rtc(void);
#endif
