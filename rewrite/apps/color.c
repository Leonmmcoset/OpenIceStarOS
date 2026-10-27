/* Recovered COLOR.X application routines at file offsets 0x40, 0xe6, 0xfe.
 * Original SHA-256:
 * b693c210f98a8295d5cad90b2370302b1a527c895bdca23d925b40d18773c6c3.
 * The original bundles an unused SDK; only its six reachable wrappers are
 * linked. Palette, window-pool and record-field addresses retain the original
 * shared-address-space ABI and its unchecked indexing behavior. */
#include "color.h"
#include "legacy_api.h"

volatile struct color_state color_state __attribute__((section(".bss.color"), aligned(4)));
const char color_title[5] = "\xd1\xd5\xc9\xab"; /* Original GBK title: 颜色. */
_Static_assert(sizeof(struct color_state) == 16, "COLOR.X BSS size");

void color_draw(void)
{
    volatile uint32_t *palette = (volatile uint32_t *)(uintptr_t)
                                 *(volatile uint32_t *)0x1b0e0;
    uint32_t row, column, index = 0;
    for (row = 0; row < 20; ++row)
        for (column = 0; column < 30; ++column) {
            legacy_rectangle(color_state.surface, 60u + column * 20u,
                             50u + row * 26u, 80u + column * 20u,
                             70u + row * 26u, palette[index]);
            ++index;
        }
}

uint32_t color_close(void)
{
    return legacy_service_6b(*(volatile uint32_t *)(uintptr_t)
                              (color_state.window_record + 4u));
}

void color_entry(void)
{
    uint32_t window = legacy_window_create(100, 80, 800, 600, 1, 0, 2, color_title);
    if ((int32_t)window > 0) {
        color_state.window_record = *(volatile uint32_t *)(uintptr_t)(0x6a0000u + window * 4u);
        *(volatile uint32_t *)(uintptr_t)(color_state.window_record + 0x5b7u) =
            (uint32_t)(uintptr_t)color_close;
        color_state.surface = legacy_surface(window);
        color_draw();
        legacy_window_show(window);
    }
    for (;;) legacy_poll();
}
