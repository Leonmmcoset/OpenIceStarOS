/* Cursor surface helpers, 0x8130cc..0x813320. The cursor is layer 1;
 * hiding it refreshes the newly exposed region starting from layer 2. */
#include "cursor.h"
#include "surface_order.h"
#include "surface_refresh.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define CURSOR WORD(0x500004u)
#define BODY __attribute__((used, noinline, regparm(1), \
    optimize("no-optimize-sibling-calls", "omit-frame-pointer")))

__attribute__((optimize("no-optimize-sibling-calls")))
uint32_t kernel_cursor_show(void)
{
    uint32_t descriptor = CURSOR;
    if (!WORD(descriptor + 44u)) {
        uint32_t height, width, y, x;
        kernel_surface_order_append(1);
        height = WORD(CURSOR + 16u);
        width = WORD(CURSOR + 12u);
        y = WORD(CURSOR + 8u);
        x = WORD(CURSOR + 4u);
        return kernel_syscall_target_81905e(1, x, y, width, height);
    }
    return descriptor;
}

__attribute__((optimize("no-optimize-sibling-calls")))
uint32_t kernel_cursor_hide(void)
{
    uint32_t descriptor = CURSOR;
    if (WORD(descriptor + 44u)) {
        uint32_t height, width, y, x;
        kernel_surface_order_remove(1);
        height = WORD(CURSOR + 16u);
        width = WORD(CURSOR + 12u);
        y = WORD(CURSOR + 8u);
        x = WORD(CURSOR + 4u);
        return kernel_syscall_target_81905e(2, x, y, width, height);
    }
    return descriptor;
}

static __attribute__((always_inline)) inline
void select_pixels(uint32_t descriptor, uint32_t shape)
{
    if (shape == 0) WORD(descriptor) = WORD(0x1b03cu);
    else if (shape == 1) WORD(descriptor) = WORD(0x1b11cu);
    else if (shape == 2 || shape == 3) WORD(descriptor) = WORD(0x1b140u);
    else if (shape == 4 || shape == 5) WORD(descriptor) = WORD(0x1b144u);
    else if (shape == 6 || shape == 7) WORD(descriptor) = WORD(0x1b148u);
    else if (shape == 8 || shape == 9) WORD(descriptor) = WORD(0x1b14cu);
}

BODY static uint32_t cursor_select_body(volatile uint32_t *a)
{
    uint32_t descriptor = WORD(0x1b04cu);
    kernel_cursor_hide();
    select_pixels(descriptor, a[0]);
    WORD(0x1b120u) = a[0];
    return kernel_cursor_show();
}

BODY static uint32_t cursor_move_body(volatile uint32_t *a)
{
    uint32_t descriptor = WORD(0x1b04cu);
    kernel_cursor_hide();
    select_pixels(descriptor, a[0]);
    WORD(0x1b120u) = a[0];
    WORD(descriptor + 4u) = a[1];
    WORD(descriptor + 8u) = a[2];
    return kernel_cursor_show();
}

/* Expose the original live incoming argument words, including aliases
 * changed by hide callbacks, pixel selection and coordinate stores. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline)) uint32_t kernel_cursor_select(uint32_t shape)
{
    __asm__("leal 4(%esp),%eax;call cursor_select_body;ret");
    __builtin_unreachable();
}
__attribute__((naked, noinline))
uint32_t kernel_cursor_move(uint32_t shape, uint32_t x, uint32_t y)
{
    __asm__("leal 4(%esp),%eax;call cursor_move_body;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
