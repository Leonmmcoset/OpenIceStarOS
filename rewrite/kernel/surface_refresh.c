/* Refresh selection at 0x81905e. The lower compositor at 0x817cd0 and the
 * service-table framebuffer callback are separate, still-original bodies. */
#include "surface_refresh.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*Refresh)(uint32_t, uint32_t, uint32_t, uint32_t);

/* The upper return word tells the hardware boundary whether to restore
 * EFLAGS. Keep outgoing arguments separate from incoming caller storage. */
__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint64_t surface_refresh_body(uint32_t layer, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height)
{
    uint32_t surface = WORD(0x500000 + 4 * layer);
    if (!WORD(surface + 44)) return surface;

    Refresh refresh = (Refresh)(uintptr_t)WORD(WORD(0x1b0e4) + 44);
    uint32_t flags = kernel_read_flags();
    uint32_t selected;
    kernel_disable_interrupts();
    if ((int32_t)WORD(0x1b138) > 0) {
        uint32_t requested = WORD(0x500000 + 4 * layer);
        uint32_t active = WORD(0x500000 + 4 * WORD(0x1b138));
        uint32_t requested_order = WORD(requested + 44);
        uint32_t active_order = WORD(active + 44);
        if (requested_order > active_order &&
            (int32_t)WORD(WORD(0x500000 + 4 * layer) + 40) < 0)
            selected = layer;
        else
            selected = WORD(0x1b138);
    } else if ((int32_t)WORD(WORD(0x500000 + 4 * layer) + 40) < 0) {
        selected = layer;
    } else {
        selected = 2;
    }
    kernel_surface_target_817cd0(selected, x, y, width, height);
    refresh(x, y, width, height);
    return (UINT64_C(1) << 32) | flags;
}

/* GCC may emit ADD ESP after a final C call, changing the restored flags.
 * Keep only this EFLAGS/return boundary in assembly; all selection and
 * callback logic above is ordinary C. LEAVE/RET do not change EFLAGS. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
uint32_t kernel_syscall_target_81905e(uint32_t layer, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height)
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $40,%esp;"
        "movl 24(%ebp),%eax;movl %eax,16(%esp);"
        "movl 20(%ebp),%eax;movl %eax,12(%esp);"
        "movl 16(%ebp),%eax;movl %eax,8(%esp);"
        "movl 12(%ebp),%eax;movl %eax,4(%esp);"
        "movl 8(%ebp),%eax;movl %eax,0(%esp);"
        "call surface_refresh_body;testl %edx,%edx;jz 1f;"
        "movl %eax,0(%esp);call kernel_write_flags;"
        "1:leave;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
