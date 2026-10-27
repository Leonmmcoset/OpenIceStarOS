/* Original 8-bit layer rasterizer at 0x817dac..0x818134. */
#include "surface_draw.h"
#include "surface_present.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define FIELD(offset) WORD(WORD(0x500000u + 4 * layer) + (offset))

uint32_t kernel_surface_draw8(uint32_t layer, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
    uint32_t output = WORD(0x1b070);
    uint32_t input = FIELD(0);
    uint32_t pitch = WORD(WORD(0x500000) + 12);
    uint32_t screen_height = WORD(WORD(0x500000) + 16);
    uint32_t left, top, layer_width, layer_height, right, bottom, row;
    uint32_t requested_right, requested_bottom;
    if ((int32_t)x >= (int32_t)pitch) return x;
    if ((int32_t)y >= (int32_t)screen_height) return y;
    left = FIELD(4); top = FIELD(8);
    layer_width = FIELD(12); layer_height = FIELD(16);
    right = layer_width + left - 1; bottom = layer_height + top - 1;
    requested_right = width + x - 1;
    requested_bottom = height + y - 1;
    /* The original passes the screen dimensions, not the request size. */
    if (!kernel_surface_regions_test(left, top, layer_width, layer_height,
                                    x, y, pitch, screen_height)) return 0;
    if ((int32_t)left < (int32_t)x) left = x;
    if ((int32_t)top < (int32_t)y) top = y;
    if ((int32_t)right > (int32_t)requested_right) right = requested_right;
    if ((int32_t)bottom > (int32_t)requested_bottom) bottom = requested_bottom;
    if ((int32_t)FIELD(40) >= 0) {
        for (row = top; (int32_t)row <= (int32_t)bottom; ++row) {
            uint32_t col;
            for (col = left; (int32_t)col <= (int32_t)right; ++col) {
                uint32_t relative_x = col - FIELD(4);
                uint32_t relative_y = row - FIELD(8);
                uint32_t source = input + relative_y * layer_width + relative_x;
                uint32_t pixel = BYTE(source);
                if (pixel != FIELD(40))
                    BYTE(output + row * pitch + col) = BYTE(source);
            }
        }
    } else {
        uint32_t span = right - left + 1;
        int32_t words = (int32_t)span >> 2;
        int32_t tail = (int32_t)span % 4;
        for (row = top; (int32_t)row <= (int32_t)bottom; ++row) {
            uint32_t relative_x = left - FIELD(4);
            uint32_t relative_y = row - FIELD(8);
            uint32_t source = input + relative_x + relative_y * layer_width;
            uint32_t dest = output + left + row * pitch, i;
            for (i = 0; (int32_t)i < words; ++i)
                WORD(dest + 4 * i) = WORD(source + 4 * i);
            for (i = 0; (int32_t)i < tail; ++i) {
                uint32_t delta = 4 * (uint32_t)words + i;
                BYTE(dest + delta) = BYTE(input + relative_x +
                    relative_y * layer_width + delta);
            }
        }
    }
    return row;
}
#undef FIELD
