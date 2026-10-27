#include <stdint.h>

void kernel_clear_startup_record(volatile uint32_t *record)
{
    unsigned i;
    for (i = 0; i < 8; ++i) record[i] = 0;
}

void kernel_init_startup_record(void)
{
    uint32_t address = *(volatile uint32_t *)0x1b040;
    kernel_clear_startup_record((volatile uint32_t *)(uintptr_t)address);
}
