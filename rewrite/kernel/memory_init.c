/* Recovered from 0x815320..0x815464. The physical page test and CPU control
 * operations are hardware boundaries; their original ordering is significant. */
#include "memory_init.h"
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

uint32_t kernel_read_flags(void);
void kernel_write_flags(uint32_t flags);
uint32_t kernel_read_cr0(void);
void kernel_write_cr0(uint32_t value);

uint32_t kernel_probe_memory(void)
{
    uint32_t flags = kernel_read_flags(), ac_available, result, control;
    kernel_write_flags(flags | 0x40000u);
    flags = kernel_read_flags();
    ac_available = flags & 0x40000u;
    kernel_write_flags(flags & 0xfffbffffu);
    if (ac_available) {
        control = kernel_read_cr0();
        kernel_write_cr0(control | 0x60000000u);
    }
    result = kernel_probe_pages(0x400000, 0xffffffff);
    if (ac_available) {
        control = kernel_read_cr0();
        kernel_write_cr0(control & 0x9fffffffu);
    }
    return result;
}

void kernel_init_heap(void)
{
    WORD(0x1b05c) = 0x2000000;
    WORD(0x1b050) = kernel_probe_memory();
    WORD(0x1b058) = 0x1000;
    /* Original signed division truncates negative detected sizes toward zero. */
    WORD(0x1b054) = (uint32_t)((int32_t)WORD(0x1b050) / 4096);
    WORD(0x2000000) = 0;
    WORD(0x2000004) = 0;
    WORD(0x2000008) = 0;
    /* The first header's owner DWORD deliberately remains untouched. */
}

#undef WORD
