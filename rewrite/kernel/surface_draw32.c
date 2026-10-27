/* 32-bit destination rasterizer at 0x81846a..0x818d14. Source depth may
 * be 8 (palette conversion) or 32 (copy/alpha); other depths do nothing. */
#include "surface_draw.h"
#include "surface_present.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define FIELD(offset) WORD(WORD(0x500000u + 4 * a[0]) + (offset))
typedef uint32_t (*Copy)(uint32_t, uint32_t, uint32_t);

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint32_t surface_draw32_body(volatile uint32_t *a)
{
    uint32_t output = WORD(0x1b070);
    uint32_t input8 = FIELD(0), input32 = FIELD(0);
    uint32_t palette = WORD(0x1b0e0);
    uint32_t pitch = WORD(WORD(0x500000) + 12);
    uint32_t screen_height = WORD(WORD(0x500000) + 16);
    uint32_t left, top, layer_width, layer_height, right, bottom, row, key;
    uint32_t requested_x, requested_y, requested_right, requested_bottom;
    if ((int32_t)a[1] >= (int32_t)pitch) return a[1];
    if ((int32_t)a[2] >= (int32_t)screen_height) return a[2];
    left = FIELD(4); top = FIELD(8);
    layer_width = FIELD(12); layer_height = FIELD(16);
    right = layer_width + left - 1; bottom = layer_height + top - 1;
    requested_x = a[1]; requested_y = a[2];
    requested_right = a[3] + a[1] - 1;
    requested_bottom = a[4] + a[2] - 1;
    if (!kernel_surface_regions_test(left, top, layer_width, layer_height,
                                    requested_x, requested_y, pitch, screen_height))
        return 0;
    if ((int32_t)left < (int32_t)requested_x) left = requested_x;
    if ((int32_t)top < (int32_t)requested_y) top = requested_y;
    if ((int32_t)right > (int32_t)requested_right) right = requested_right;
    if ((int32_t)bottom > (int32_t)requested_bottom) bottom = requested_bottom;
    key = FIELD(40);
    if (FIELD(36) == 32) {
        uint32_t span = right - left + 1;
        if ((int32_t)key >= 0) {
            Copy blend = (Copy)(uintptr_t)WORD(WORD(0x1b0e4) + 72);
            int32_t groups = (int32_t)span >> 2;
            int32_t remainder = (int32_t)span % 4;
            uint32_t layer_x = FIELD(4), layer_y = FIELD(8);
            uint32_t source_row = input32 +
                4 * (left - layer_x + (top - layer_y) * layer_width);
            uint32_t dest_row = output + 4 * (top * pitch + left);
            uint32_t weights[4];
            for (row = top; (int32_t)row <= (int32_t)bottom; ++row) {
                uint32_t source = source_row, dest = dest_row, i, p;
                for (i = 0; (int32_t)i < groups; ++i) {
                    for (p = 0; p < 4; ++p) {
                        uint32_t pixel = WORD(source + 4 * p);
                        weights[p] = pixel == key ? 128 : BYTE(source + 4 * p + 3);
                    }
                    blend(dest, source, (uint32_t)(uintptr_t)weights);
                    source += 16; dest += 16;
                }
                /* Even a zero/negative remainder initializes all three
                 * weights; only positive active entries read source. */
                for (p = 0; p < 3; ++p) {
                    weights[p] = 128;
                    if ((int32_t)p < remainder) {
                        uint32_t pixel = WORD(source + 4 * p);
                        if (pixel != key) weights[p] = BYTE(source + 4 * p + 3);
                    }
                }
                for (p = 0; p < 3; ++p) {
                    uint32_t weight = weights[p];
                    int32_t channel;
                    if ((int32_t)weight > 127) continue;
                    if ((int32_t)weight <= 0) {
                        WORD(dest + 4 * p) = WORD(source + 4 * p);
                        continue;
                    }
                    for (channel = 2; channel >= 0; --channel) {
                        uint32_t offset = 4 * p + (uint32_t)channel;
                        uint32_t old = BYTE(dest + offset);
                        uint32_t incoming = BYTE(source + offset);
                        BYTE(dest + offset) = (uint8_t)((int32_t)
                            (old * weight + incoming * (128 - weight)) >> 7);
                    }
                }
                source_row += 4 * layer_width; dest_row += 4 * pitch;
            }
        } else {
            Copy copy = (Copy)(uintptr_t)WORD(WORD(0x1b0e4) + 68);
            for (row = top; (int32_t)row <= (int32_t)bottom; ++row) {
                uint32_t layer_x = FIELD(4), layer_y = FIELD(8);
                uint32_t source = input32 +
                    4 * ((row - layer_y) * layer_width + left - layer_x);
                uint32_t dest = output + 4 * (row * pitch + left);
                copy(dest, source, span);
            }
        }
    } else {
        uint32_t descriptor = WORD(0x500000u + 4 * a[0]);
        if (WORD(descriptor + 36) != 8) return descriptor;
        for (row = top; (int32_t)row <= (int32_t)bottom; ++row) {
            uint32_t col;
            for (col = left; (int32_t)col <= (int32_t)right; ++col) {
                uint32_t relative_x = col - FIELD(4);
                uint32_t relative_y = row - FIELD(8);
                uint32_t source = input8 + relative_y * layer_width + relative_x;
                if ((int32_t)key >= 0 && BYTE(source) == key) continue;
                WORD(output + 4 * (row * pitch + col)) =
                    WORD(palette + 4 * (uint32_t)BYTE(source));
            }
        }
    }
    return row;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
uint32_t kernel_surface_draw32(uint32_t layer, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;call surface_draw32_body;"
        "leal 4(%esp),%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
