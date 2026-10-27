/* Application-task construction, removal, thread creation, and active-state
 * query recovered from 0x814a56..0x815106. Allocation/free remain boundaries.
 * Both task constructors use the original shared CR3, not separate page maps. */
#include "tasks.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define TASK(index, offset) WORD(table + (index) * 108u + (offset))

uint32_t kernel_read_flags(void);
void kernel_disable_interrupts(void);
void kernel_write_flags(uint32_t flags);

int32_t kernel_create_application_task(uint32_t entry, uint32_t stack,
                                      uint32_t code_selector, uint32_t data_selector,
                                      uint32_t kind, uint32_t owner, uint32_t name)
{
    uint32_t table = WORD(0x1b0c8);
    uint32_t pages = (kernel_allocation_size(owner) + 0xfffu) >> 12;
    uint32_t flags = kernel_read_flags(), index, tss, descriptor_base, descriptor;
    kernel_disable_interrupts();
    index = kernel_find_free_task();
    if ((int32_t)index >= 0) {
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
        WORD(tss + 100u) = 0;
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
        WORD(tss + 8u) = 0x38;
        WORD(tss + 20u) = owner + (pages << 12) - 1u;
        WORD(tss + 24u) = data_selector;
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

void kernel_delete_task(uint32_t index)
{
    uint32_t flags = kernel_read_flags(), table, previous, next;
    kernel_disable_interrupts();
    table = WORD(0x1b0c8);
    /* Reload links after each write: self-links and overlapping records are
       legal inputs to the original unchecked routine. */
    previous = TASK(index, 4);
    next = TASK(index, 8);
    TASK(previous, 8) = next;
    next = TASK(index, 8);
    previous = TASK(index, 4);
    TASK(next, 4) = previous;
    TASK(index, 0) = 0;
    if (TASK(index, 36) != 0) {
        (void)TASK(index, 36); /* Original unused owner snapshot. */
        kernel_free_allocation(TASK(index, 36));
    }
    if (index == WORD(0x1b118)) WORD(0x1b118) = TASK(index, 4);
    WORD(0x1b0d4) += 1u;
    kernel_write_flags(flags);
}

int32_t kernel_create_thread(uint32_t entry, uint32_t stack_size, uint32_t parent)
{
    uint32_t table = WORD(0x1b0c8);
    uint32_t stack = kernel_allocate(stack_size);
    int32_t index = kernel_create_task(entry, stack + stack_size - 1u,
                                       0x40, 0x38, 8, stack,
                                       table + parent * 108u + 44u);
    if (index >= 0) TASK((uint32_t)index, 40) = parent;
    return index;
}

uint32_t kernel_task_is_active(uint32_t index)
{
    uint32_t table = WORD(0x1b0c8);
    if (index < WORD(0x1b0d0) && (int32_t)index >= 0 && TASK(index, 0) != 0)
        return 1;
    return 0;
}

#undef TASK
#undef WORD
