#include "syscall_surface.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

/* INT 0x40 operation 16 and the service table both reach this routine. Keep
 * the original independent high-to-low reads: the object layer and surface
 * table entry are reloaded for every field, then the layer is read once more
 * for argument zero. Neither indices nor potentially unaligned fields are
 * validated, and all address arithmetic wraps at 32 bits. */
__attribute__((optimize("no-optimize-sibling-calls")))
uint32_t kernel_syscall_target_81017c(uint32_t index)
{
    uint32_t object = WORD(0x6a0000 + 4 * index);
    uint32_t a4 = WORD(WORD(0x500000 + 4 * WORD(object + 12)) + 16);
    uint32_t a3 = WORD(WORD(0x500000 + 4 * WORD(object + 12)) + 12);
    uint32_t a2 = WORD(WORD(0x500000 + 4 * WORD(object + 12)) + 8);
    uint32_t a1 = WORD(WORD(0x500000 + 4 * WORD(object + 12)) + 4);
    uint32_t layer = WORD(object + 12);
    return kernel_syscall_target_81905e(layer, a1, a2, a3, a4);
}
