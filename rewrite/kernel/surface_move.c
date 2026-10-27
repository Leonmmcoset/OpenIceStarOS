/* Moved-layer damage selection, 0x8191d8..0x8194e4. This retains the
 * original rectangle predicate, including its unconventional OR tests. */
#include "surface_move.h"
#include "surface_composite.h"
#include "surface_present.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*Present)(uint32_t, uint32_t, uint32_t, uint32_t);

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint32_t surface_move_body(volatile uint32_t *a)
{
    Present present = (Present)(uintptr_t)WORD(WORD(0x1b0e4) + 44);
    uint32_t lower, dx, dy, x, y, width, height;
    if ((int32_t)WORD(0x1b138) > 0) lower = WORD(0x1b138);
    else lower = 2;
    x = a[1]; dx = a[5] - x;
    y = a[2]; dy = a[6] - y;
    if ((int32_t)WORD(WORD(0x500000 + 4 * a[0]) + 40) >= 0)
        kernel_surface_target_817cd0(lower, a[5], a[6], a[3], a[4]);
    else
        kernel_surface_target_817cd0(a[0], a[5], a[6], a[3], a[4]);

    if (kernel_surface_regions_test(a[1], a[2], a[3], a[4],
            a[5], a[6], a[3], a[4]) == 1) {
        if ((int32_t)dy > 0) {
            kernel_surface_target_817cd0(lower, a[1], a[2], a[3], dy);
            y = a[2]; height = dy + a[4];
            if ((int32_t)dx > 0) {
                kernel_surface_target_817cd0(lower, a[1], a[6], dx, a[4] - dy);
                x = a[1]; width = dx + a[3];
            } else {
                kernel_surface_target_817cd0(lower, a[5] + a[3], a[6],
                    0u - dx, a[4] - dy);
                x = a[5]; width = a[3] - dx;
            }
        } else {
            kernel_surface_target_817cd0(lower, a[1], a[6] + a[4],
                a[3], 0u - dy);
            y = a[6]; height = a[4] - dy;
            if ((int32_t)dx > 0) {
                kernel_surface_target_817cd0(lower, a[1], a[2], dx, dy + a[4]);
                x = a[1]; width = dx + a[3];
            } else {
                kernel_surface_target_817cd0(lower, a[5] + a[3], a[2],
                    0u - dx, dy + a[4]);
                x = a[5]; width = a[3] - dx;
            }
        }
        return present(x, y, width, height);
    }
    kernel_surface_target_817cd0(lower, a[1], a[2], a[3], a[4]);
    present(a[5], a[6], a[3], a[4]);
    return present(a[1], a[2], a[3], a[4]);
}

/* Callbacks may modify the caller's seven live words. The cached deltas
 * and lower-layer selection above survive those changes. Keep outgoing
 * callback arguments separate from that original incoming storage. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
uint32_t kernel_surface_move_refresh(uint32_t layer, uint32_t old_x,
    uint32_t old_y, uint32_t width, uint32_t height,
    uint32_t new_x, uint32_t new_y)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;call surface_move_body;"
        "leal 4(%esp),%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
