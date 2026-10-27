/* VGA palette construction 0x81ae14..0x81afbc and software color table
 * initialization 0x81afbc..0x81b26e. The DAC writer remains original code. */
#include <stdint.h>
#include "display_palette.h"

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

void kernel_display_write_palette(uint32_t first, uint32_t last, uint8_t *rgb);

void kernel_display_init_palette(void)
{
    uint8_t rgb[216 * 3];
    uint32_t blue, green, red;
    kernel_display_write_palette(0, 15, (uint8_t *)(uintptr_t)DWORD(0x1b030));
    for (blue = 0; blue < 6; ++blue)
        for (green = 0; green < 6; ++green)
            for (red = 0; red < 6; ++red) {
                uint32_t offset = (blue * 36 + green * 6 + red) * 3;
                rgb[offset] = (uint8_t)(red * 51);
                rgb[offset + 1] = (uint8_t)(green * 51);
                rgb[offset + 2] = (uint8_t)(blue * 51);
            }
    kernel_display_write_palette(16, 231, rgb);
}

/* These are the original software RGB values, which deliberately differ
 * from the 51-step VGA cube above. Keep them in this function's fixed slot. */
static const uint32_t classic[16] = {
    0x000000, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
    0xc6c6c6, 0x840000, 0x008400, 0x848400, 0x000084, 0x840084, 0x008484, 0x848484
};
static const uint32_t levels[6] = {0, 0x30, 0x65, 0x99, 0xce, 0xff};

void kernel_display_select_depth(uint32_t depth)
{
    uint32_t table = DWORD(0x1b0e0), i, red, green, blue;
    if (depth == 8) {
        for (i = 0; i < 256; ++i) DWORD(table + i * 4) = i;
    } else if (depth == 32) {
        for (i = 0; i < 16; ++i) DWORD(table + i * 4) = classic[i];
        for (blue = 0; blue < 6; ++blue)
            for (green = 0; green < 6; ++green)
                for (red = 0; red < 6; ++red) {
                    i = 16 + blue * 36 + green * 6 + red;
                    DWORD(table + i * 4) =
                        (levels[red] << 16) | (levels[green] << 8) | levels[blue];
                }
        /* Original extra entry 232; entries 233..255 remain untouched. */
        DWORD(table + 0x3a0) = 0x07f00000;
    }
}
