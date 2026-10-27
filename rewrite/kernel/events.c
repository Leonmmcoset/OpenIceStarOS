#include <stdint.h>

/* Shared record at [0x1b040], with the original DWORD field offsets.
 * These routines retain the original unchecked API-table indexing. */
void kernel_queue_api_request(uint32_t index, uint32_t argument)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    record[0x10 / 4] = 1;
    record[0x14 / 4] = index;
    record[0x1c / 4] = argument;
}

__attribute__((noinline)) void kernel_dispatch_event(void)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    volatile uint32_t *table =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b0a8;
    ((void (*)(void))(uintptr_t)table[record[0x18 / 4]])();
    record[0x18 / 4] = 0;
}

void kernel_dispatch_api_request(void)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    volatile uint32_t *table =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b0a8;
    uint32_t index = record[0x14 / 4];
    uint32_t argument = record[0x1c / 4];
    ((void (*)(uint32_t))(uintptr_t)table[index])(argument);
    record[0x14 / 4] = 0;
    record[0x1c / 4] = 0;
}

void kernel_dispatch_slot1(void)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    record[0x18 / 4] = 1;
    kernel_dispatch_event();
}

void kernel_dispatch_slot2(void)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    record[0x18 / 4] = 2;
    kernel_dispatch_event();
}

void kernel_dispatch_slot3(void)
{
    volatile uint32_t *record =
        (volatile uint32_t *)(uintptr_t)*(volatile uint32_t *)0x1b040;
    record[0x18 / 4] = 3;
    kernel_dispatch_event();
}
