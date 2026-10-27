/* Timer task switch and cooperative task yield, recovered from
 * 0x814176..0x814536. Paged creation and destruction remain original code. */
#include "runtime.h"
#include "service_forward.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define TASK(index, offset) WORD(table + (index) * 108u + (offset))

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);
void kernel_fpu_reset(void);
void kernel_far_task_jump(uint32_t offset, uint32_t selector);

/* Called inside the original interrupt path: unlike cooperative yielding,
 * this entry neither captures/restores flags nor changes interrupt state. */
void kernel_schedule_task(void)
{
    uint32_t table = WORD(0x1b0c8);
    uint32_t current = WORD(0x1b0d8);
    if (current != TASK(current, 8)) {
        if (TASK(current, 0) == 2) kernel_service_0f(TASK(current, 32));
        if (TASK(TASK(current, 8), 0) == 1) {
            kernel_fpu_reset();
            TASK(TASK(current, 8), 0) = 2;
        } else {
            kernel_service_10(TASK(TASK(current, 8), 32));
        }
        WORD(0x1b0d8) = TASK(current, 8);
        TASK(WORD(0x1b0d8), 12) += 1u;
        kernel_far_task_jump(0, TASK(current, 8) * 8u + 0x58u);
    }
}

/* IRQ tick handler, recovered from 0x813a5e..0x813a8c. Hardware register
 * preservation, PIC acknowledgement, and IRET stay in the original stub. */
__attribute__((noinline)) static void timer_tick_body(void)
{
    WORD(0x1b114) += 1u;
    WORD(WORD(0x1b040)) = 1;
    WORD(0x1b084) += 1u;
    if (WORD(0x1b0d0) > 1) kernel_schedule_task();
}

/* The original tick slot has only 46 bytes. Keep its C-generated tail-jump
 * entry there and link the body into spare space in the timer-switch slot. */
void kernel_timer_tick(void)
{
    timer_tick_body();
}

void kernel_yield_task(void)
{
    /* Capture the table before reading flags; callbacks/interrupts can change
       its global pointer. Indices, links, and counters retain DWORD wrapping. */
    uint32_t table = WORD(0x1b0c8);
    uint32_t flags = kernel_read_flags();
    uint32_t current;
    kernel_disable_interrupts();
    current = WORD(0x1b0d8);
    if (current != TASK(current, 8)) {
        if (TASK(current, 0) == 2) kernel_service_0f(TASK(current, 32));
        if (TASK(TASK(current, 8), 0) == 1) {
            kernel_fpu_reset();
            TASK(TASK(current, 8), 0) = 2;
        } else {
            kernel_service_10(TASK(TASK(current, 8), 32));
        }
        WORD(0x1b0d8) = TASK(current, 8);
        TASK(WORD(0x1b0d8), 12) += 1u;
        /* The far jump saves this C stack in the old TSS. When that task is
           scheduled again, it resumes here and restores its saved flags. */
        kernel_far_task_jump(0, TASK(current, 8) * 8u + 0x58u);
    }
    kernel_write_flags(flags);
}

#undef TASK
#undef WORD
