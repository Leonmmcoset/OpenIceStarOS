/* Timer records at 0x82aa33..0x82ac53. Preserve signed table decisions,
 * unchecked wrapped indices and the original failure paths that leave
 * interrupts disabled. This is recovery, not a correction of that behavior. */
#include "timer_objects.h"
#include "memory.h"

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(address))
#define SLOT(index) (0x6c0000u + (index) * 4u)

__attribute__((noinline)) uint32_t kernel_timer_find_free(void)
{
    uint32_t index;
    if ((int32_t)DWORD(0x1b0c0) > 0)
        for (index = 0; (int32_t)index < (int32_t)DWORD(0x1b0bc); ++index)
            if (DWORD(SLOT(index)) == 0) return index;
    if ((int32_t)DWORD(0x1b0bc) < (int32_t)DWORD(0x1b0b8))
        return DWORD(0x1b0bc);
    return 0xffffffffu;
}

uint32_t kernel_timer_create(uint32_t delay, uint32_t payload)
{
    uint32_t flags = kernel_read_flags(), index, record, deadline;
    kernel_disable_interrupts();
    index = kernel_timer_find_free();
    if ((int32_t)index < 0) return 0xffffffffu;
    DWORD(SLOT(index)) = kernel_allocate(12);
    if (DWORD(SLOT(index)) == 0) return 0xffffffffu;
    record = DWORD(SLOT(index));
    deadline = delay + DWORD(0x1b084);
    DWORD(record + 4u) = deadline;
    record = DWORD(SLOT(index));
    DWORD(record) = payload;
    record = DWORD(SLOT(index));
    DWORD(record + 8u) = 0;
    if ((int32_t)DWORD(0x1b0bc) > (int32_t)index)
        DWORD(0x1b0c0) = DWORD(0x1b0c0) - 1u;
    else
        DWORD(0x1b0bc) = DWORD(0x1b0bc) + 1u;
    kernel_write_flags(flags);
    return index;
}

void kernel_timer_delete(uint32_t index)
{
    uint32_t flags = kernel_read_flags();
    kernel_disable_interrupts();
    if ((int32_t)DWORD(SLOT(index)) > 0) {
        kernel_free_allocation(DWORD(SLOT(index)));
        DWORD(SLOT(index)) = 0;
        DWORD(0x1b0c0) = DWORD(0x1b0c0) + 1u;
    }
    kernel_write_flags(flags);
}

uint32_t kernel_timer_rearm(uint32_t index, uint32_t delay)
{
    uint32_t record = DWORD(SLOT(index));
    uint32_t deadline = delay + DWORD(0x1b084);
    DWORD(record + 4u) = deadline;
    return deadline;
}
