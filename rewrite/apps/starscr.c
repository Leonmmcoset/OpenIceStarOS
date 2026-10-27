/* Recovered STARSCR.X entry, keyboard, close, and drawing routines at
 * original offsets 0x40, 0x160, 0x180, and 0x198. Original SHA-256:
 * 6d2f3ebfe9a45dcba055ddfe01b7fb8f384e0003a9a9484c9585d912c7f0473c.
 * Only the nine reachable shared-table SDK boundaries are linked. */
#include "starscr.h"
#include "legacy_api.h"
#include "legacy_image_api.h"
#include "legacy_draw_api.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
volatile struct starscr_state starscr_state
    __attribute__((section(".bss.starscr"), aligned(4)));
const char starscr_title[12] __attribute__((section(".rodata.starscr"), aligned(1))) =
    "\xd0\xc7\xbf\xd5\xc6\xc1\xb1\xa3"; /* Original GBK: 星空屏保. */
_Static_assert(sizeof(starscr_state) == 16, "STARSCR BSS layout");

static void circle(uint32_t dx, uint32_t dy, uint32_t index)
{
    uint32_t color = legacy_palette_color(index);
    /* Each palette service may change the record pointer, dimensions, or
       surface. Read them afterwards, as in the original instruction order. */
    uint32_t y = (uint32_t)((int32_t)WORD(starscr_state.record + 0x147u) / 2) + dy;
    uint32_t x = (uint32_t)((int32_t)WORD(starscr_state.record + 0x143u) / 2) + dx;
    legacy_circle(starscr_state.surface, x, y, 120, color);
}

void starscr_draw(uint32_t phase)
{
    uint32_t color = legacy_palette_color(0);
    uint32_t y2 = (uint32_t)((int32_t)WORD(starscr_state.record + 0x147u) / 2) + 220u;
    uint32_t x2 = (uint32_t)((int32_t)WORD(starscr_state.record + 0x143u) / 2) + 220u;
    uint32_t y1 = (uint32_t)((int32_t)WORD(starscr_state.record + 0x147u) / 2) - 220u;
    uint32_t x1 = (uint32_t)((int32_t)WORD(starscr_state.record + 0x143u) / 2) - 220u;
    legacy_rectangle(starscr_state.surface, x1, y1, x2, y2, color);
    if (phase == 0) {
        circle(0, -100u, 1); circle(-100u, 100, 2); circle(100, 100, 3);
    } else if (phase == 1) {
        circle(-100u, -100u, 3); circle(100, -100u, 1); circle(0, 100, 2);
    } else if (phase == 2) {
        circle(0, -100u, 2); circle(-100u, 100, 3); circle(100, 100, 1);
    } else if (phase == 3) {
        circle(-100u, -100u, 1); circle(100, -100u, 2); circle(0, 100, 3);
    }
}

void starscr_key(uint32_t window, uint32_t key, uint32_t pressed)
{
    if (key == 0x1b && pressed == 1) legacy_window_close(window);
}

uint32_t starscr_close(void)
{
    return legacy_service_6b(WORD(starscr_state.record + 4u));
}

uint32_t starscr_entry(void)
{
    uint32_t display = WORD(0x1b034), phase = 0;
    uint32_t height = WORD(display + 0x10u), width = WORD(display + 0xcu);
    uint32_t window = legacy_window_create(0, 0, width, height, 0, 0, 0, starscr_title);
    if ((int32_t)window > 0) {
        starscr_state.record = WORD(0x6a0000u + window * 4u);
        WORD(starscr_state.record + 0x593u) = (uint32_t)(uintptr_t)starscr_key;
        WORD(starscr_state.record + 0x5b7u) = (uint32_t)(uintptr_t)starscr_close;
        WORD(starscr_state.record + 0x11fu) = 4;
        starscr_state.window = WORD(starscr_state.record);
        starscr_state.surface = WORD(starscr_state.record + 0xcu);
        legacy_window_show(window);
        for (;;) {
            if (phase > 3) phase = 0;
            starscr_draw(phase++);
            legacy_window_refresh(window);
            legacy_service_71(10);
        }
    }
    /* The original failure path still dereferences the global record. */
    return legacy_service_6b(WORD(starscr_state.record + 4u));
}
