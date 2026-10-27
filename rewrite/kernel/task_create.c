/* Task subsystem initialization, free-slot search, and nonpaged task creation,
 * recovered from 0x814536..0x814a56. The application variant and deletion are
 * in task_lifecycle.c. Task/TSS accesses retain their original DWORD order. */
#include "tasks.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define TASK(index, offset) WORD(table + (index) * 108u + (offset))

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);

uint32_t kernel_find_free_task(void)
{
    uint32_t table = WORD(0x1b0c8), index;
    if (WORD(0x1b0d0) != 0) {
        for (index = 0; index < WORD(0x1b0d0); ++index)
            if (TASK(index, 0) == 0) return index;
    }
    if (WORD(0x1b0d0) < WORD(0x1b0cc)) return WORD(0x1b0d0);
    return 0xffffffffu;
}

int32_t kernel_create_task(uint32_t entry, uint32_t stack,
                           uint32_t code_selector, uint32_t data_selector,
                           uint32_t kind, uint32_t owner, uint32_t name)
{
    uint32_t table = WORD(0x1b0c8);
    uint32_t flags = kernel_read_flags(), index, tss, descriptor_base, descriptor;
    kernel_disable_interrupts();
    index = kernel_find_free_task();
    if ((int32_t)index >= 0) {
        /* The original tests the owner pointer as signed, and stores the
           handle four bytes before it. Negative pointers skip this write. */
        if ((int32_t)owner > 0) WORD(owner - 4u) = index;
        TASK(index, 0) = 1;
        TASK(index, 4) = WORD(0x1b118);
        TASK(index, 8) = 0;
        TASK(index, 12) = 0;
        TASK(index, 16) = WORD(0x1b114);
        TASK(index, 20) = kind;
        TASK(index, 36) = owner;
        TASK(index, 24) = index + 11u;
        TASK(index, 28) = WORD(0x1b160) + index * 104u;
        TASK(index, 32) = WORD(0x1b164) + index * 512u;
        TASK(index, 40) = index;
        kernel_copy_string(table + index * 108u + 44u, name);
        TASK(WORD(0x1b118), 8) = index;
        WORD(0x1b118) = index;
        tss = TASK(index, 28);
        WORD(tss + 96u) = 0;
        WORD(tss + 100u) = 0x40000000u;
        WORD(tss + 32u) = entry;
        WORD(tss + 36u) = 0x202;
        WORD(tss + 40u) = 0;
        WORD(tss + 44u) = 0;
        WORD(tss + 48u) = 0;
        WORD(tss + 52u) = 0;
        WORD(tss + 56u) = stack;
        WORD(tss + 60u) = 0;
        WORD(tss + 64u) = 0;
        WORD(tss + 68u) = 0;
        WORD(tss + 72u) = data_selector;
        WORD(tss + 76u) = code_selector;
        WORD(tss + 80u) = data_selector;
        WORD(tss + 84u) = data_selector;
        WORD(tss + 88u) = data_selector;
        WORD(tss + 92u) = data_selector;
        WORD(tss + 4u) = 0x790000u + index * 1024u;
        WORD(tss + 8u) = data_selector;
        WORD(tss + 28u) = 0x19000;
        descriptor_base = TASK(index, 28);
        descriptor = TASK(index, 24);
        kernel_set_segment_descriptor(0x7e00u + descriptor * 8u,
                                      0x67, descriptor_base, 0x89);
        if (index == WORD(0x1b0d0)) WORD(0x1b0d0) += 1u;
        else WORD(0x1b0d4) -= 1u;
    }
    kernel_write_flags(flags);
    return (int32_t)index;
}

void kernel_init_tasks(void)
{
    uint32_t flags, task;
    /* These two unused local snapshots are reads in the original entry. */
    (void)WORD(0x1b034);
    (void)WORD(0x1b0c8);
    flags = kernel_read_flags();
    kernel_disable_interrupts();
    WORD(0x1b0c8) = 0x6d0000;
    WORD(0x1b0cc) = 0x71c;
    WORD(0x1b0d0) = 0;
    WORD(0x1b0d4) = 0;
    WORD(0x1b118) = 0;
    kernel_load_gdt(0xffff, 0x7e00);
    task = (uint32_t)kernel_create_task(0x8012ca, 0xd00000, 0x40, 0x38,
                                       10, 0, 0x83cea2);
    WORD(0x1b0d8) = task;
    kernel_load_task_register(0x58);
    kernel_write_flags(flags);
}

#undef TASK
#undef WORD
