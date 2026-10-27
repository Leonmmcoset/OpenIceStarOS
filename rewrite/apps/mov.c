/* Recovered MOV.X application functions at offsets 0x40 and 0x262.
 * Original SHA-256:
 * df5429c3e1ff472e0c6142fa003635430eef3114a6fcd256c14319ac83199a68.
 * The four original JPEG assets are external resources, not linked code. */
#include "mov.h"
#include "legacy_api.h"
#include "legacy_image_api.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
volatile uint32_t mov_frames[4] __attribute__((section(".bss.mov"), aligned(4)));
const struct mov_strings mov_strings __attribute__((section(".rodata.mov"), aligned(1))) = {
    "\xb6\xaf\xbb\xad\xd1\xdd\xca\xbe", /* Original GBK: 动画演示. */
    {"C:\\ICESTAR\\GAMES\\MOV\\MOV0.JPG", "C:\\ICESTAR\\GAMES\\MOV\\MOV1.JPG",
     "C:\\ICESTAR\\GAMES\\MOV\\MOV2.JPG", "C:\\ICESTAR\\GAMES\\MOV\\MOV3.JPG"}
};
_Static_assert(sizeof(mov_strings) == 129, "MOV initialized data layout");

uint32_t mov_close(uint32_t window)
{
    uint32_t record = WORD(0x6a0000u + window * 4u);
    legacy_service_6b(WORD(record + 4u));
    legacy_image_release(mov_frames[0]);
    legacy_image_release(mov_frames[1]);
    legacy_image_release(mov_frames[2]);
    return legacy_image_release(mov_frames[3]);
}

void mov_entry(void)
{
    uint32_t display = WORD(0x1b034), palette = WORD(0x1b0e0);
    uint32_t window = legacy_window_create(330,
        (uint32_t)((int32_t)WORD(display + 0x10u) / 2) - 200u,
        400, 280, 1, WORD(palette + 8u), 2, mov_strings.title);
    uint32_t frame = 1, deadline = WORD(0x1b084) + 10u;
    if ((int32_t)window > 0) {
        uint32_t record = WORD(0x6a0000u + window * 4u);
        WORD(record + 0x5b7u) = (uint32_t)(uintptr_t)mov_close;
        mov_frames[0] = legacy_image_create(window, 10, 30, 380, 240, 0, mov_strings.paths[0]);
        mov_frames[1] = legacy_image_create(window, 10, 30, 380, 240, 0, mov_strings.paths[1]);
        mov_frames[2] = legacy_image_create(window, 10, 30, 380, 240, 0, mov_strings.paths[2]);
        mov_frames[3] = legacy_image_create(window, 10, 30, 380, 240, 0, mov_strings.paths[3]);
        legacy_image_show(mov_frames[0]);
        legacy_window_show(window);
    }
    /* Original code animates even after window-creation failure. Comparisons
       are unsigned; deadline addition intentionally wraps at 32 bits. */
    for (;;) {
        if (WORD(0x1b084) >= deadline) {
            legacy_image_show(mov_frames[frame]);
            legacy_window_refresh(window);
            ++frame;
            deadline = WORD(0x1b084) + 10u;
            if (frame == 4) frame = 0;
        }
        legacy_poll();
    }
}
