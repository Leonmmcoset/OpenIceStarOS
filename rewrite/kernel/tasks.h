#ifndef ICESTAR_KERNEL_TASKS_H
#define ICESTAR_KERNEL_TASKS_H
#include <stdint.h>

void kernel_init_tasks(void);
uint32_t kernel_find_free_task(void);
int32_t kernel_create_task(uint32_t entry, uint32_t stack,
                           uint32_t code_selector, uint32_t data_selector,
                           uint32_t kind, uint32_t owner, uint32_t name);

/* Descriptor construction and string copying still use original routines. */
void kernel_set_segment_descriptor(uint32_t address, uint32_t limit,
                                   uint32_t base, uint32_t access);
uint32_t kernel_copy_string(uint32_t destination, uint32_t source);
void kernel_load_gdt(uint32_t limit, uint32_t base);
void kernel_load_task_register(uint32_t selector);
#endif
