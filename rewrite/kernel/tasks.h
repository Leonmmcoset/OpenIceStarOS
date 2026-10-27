#ifndef ICESTAR_KERNEL_TASKS_H
#define ICESTAR_KERNEL_TASKS_H
#include <stdint.h>
#include "helpers.h"
#include "memory.h"

void kernel_init_tasks(void);
uint32_t kernel_find_free_task(void);
int32_t kernel_create_task(uint32_t entry, uint32_t stack,
                           uint32_t code_selector, uint32_t data_selector,
                           uint32_t kind, uint32_t owner, uint32_t name);
int32_t kernel_create_application_task(uint32_t entry, uint32_t stack,
                                      uint32_t code_selector, uint32_t data_selector,
                                      uint32_t kind, uint32_t owner, uint32_t name);
void kernel_delete_task(uint32_t index);
int32_t kernel_create_thread(uint32_t entry, uint32_t stack_size, uint32_t parent);
uint32_t kernel_task_is_active(uint32_t index);

void kernel_load_gdt(uint32_t limit, uint32_t base);
void kernel_load_task_register(uint32_t selector);
#endif
