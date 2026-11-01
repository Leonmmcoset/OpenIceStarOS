/* Atlas dimensions, shelf placement and drawing coordinates recovered from
 * original x86. The original DWORD pixel clear is intentionally preserved. */
#include "font_atlas.h"
#include "font_metrics.h"
#include "font_cmap.h"
#include "font_outline.h"
#include "math_extra.h"
typedef uint32_t alias32 __attribute__((may_alias));
typedef uint16_t alias16 __attribute__((may_alias));
typedef float alias_float __attribute__((may_alias));
#define D(a) (*(volatile alias32 *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile alias16 *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define F(a) (*(volatile alias_float *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
uint32_t kernel_font_malloc_817590(uint32_t size);
void kernel_font_free_8175a4(uint32_t pointer);
void kernel_font_clear_81752e(uint32_t pointer, uint32_t value, uint32_t dwords);

void kernel_font_baked_quad_832dbe(uint32_t chars, int32_t width,
    int32_t height, int32_t index, volatile uint32_t x,
    volatile uint32_t y, volatile uint32_t quad, int32_t opengl)
{
    volatile float bias = opengl ? 0.0f : F(0x83d073u);
    volatile float iw = 1.0f / width, ih = 1.0f / height;
    uint32_t ch = chars + (uint32_t)index * 20u;
    int32_t rx = (int32_t)kernel_floor_81a9ee((double)(F(x) + F(ch + 8u) + F(0x83d077u)));
    int32_t ry = (int32_t)kernel_floor_81a9ee((double)(F(y) + F(ch + 12u) + F(0x83d077u)));
    F(quad) = rx + bias;
    F(quad + 4u) = ry + bias;
    F(quad + 16u) = (int32_t)((uint32_t)rx + W(ch + 4u) - W(ch)) + bias;
    F(quad + 20u) = (int32_t)((uint32_t)ry + W(ch + 6u) - W(ch + 2u)) + bias;
    F(quad + 8u) = W(ch) * iw;
    F(quad + 12u) = W(ch + 2u) * ih;
    F(quad + 24u) = W(ch + 4u) * iw;
    F(quad + 28u) = W(ch + 6u) * ih;
    F(x) = F(x) + F(ch + 16u);
}

void kernel_font_rect_init_832f44(volatile uint32_t context, volatile uint32_t width,
    volatile uint32_t height, uint32_t nodes, uint32_t count)
{
    (void)nodes; (void)count;
    D(context) = width;
    D(context + 4u) = height;
    D(context + 8u) = 0;
    D(context + 12u) = 0;
    D(context + 16u) = 0;
}

void kernel_font_rect_pack_832f78(volatile uint32_t context,
    volatile uint32_t rects, volatile int32_t count)
{
    int32_t i = 0;
    for (; i < count; ++i) {
        if ((int32_t)(D(context + 8u) + D(rects + (uint32_t)i * 24u + 12u)) > (int32_t)D(context)) {
            D(context + 8u) = 0;
            D(context + 12u) = D(context + 16u);
        }
        if ((int32_t)(D(context + 12u) + D(rects + (uint32_t)i * 24u + 16u)) > (int32_t)D(context + 4u)) break;
        D(rects + (uint32_t)i * 24u) = D(context + 8u);
        D(rects + (uint32_t)i * 24u + 4u) = D(context + 12u);
        D(rects + (uint32_t)i * 24u + 20u) = 1;
        D(context + 8u) = D(context + 8u) + D(rects + (uint32_t)i * 24u + 12u);
        if ((int32_t)(D(context + 12u) + D(rects + (uint32_t)i * 24u + 16u)) > (int32_t)D(context + 16u))
            D(context + 16u) = D(context + 12u) + D(rects + (uint32_t)i * 24u + 16u);
    }
    for (; i < count; ++i) D(rects + (uint32_t)i * 24u + 20u) = 0;
}

uint32_t kernel_font_pack_begin_8330f6(volatile uint32_t context,
    volatile uint32_t pixels, volatile uint32_t width, volatile uint32_t height,
    volatile uint32_t stride, volatile uint32_t padding, volatile uint32_t userdata)
{
    uint32_t packer = kernel_font_malloc_817590(20);
    uint32_t node_count = width - padding;
    uint32_t nodes = kernel_font_malloc_817590(node_count);
    if (!packer || !nodes) {
        if (packer) kernel_font_free_8175a4(packer);
        if (nodes) kernel_font_free_8175a4(nodes);
        return 0;
    }
    D(context) = userdata;
    D(context + 8u) = width;
    D(context + 12u) = height;
    D(context + 36u) = pixels;
    D(context + 4u) = packer;
    D(context + 40u) = nodes;
    D(context + 20u) = padding;
    uint32_t target = context;
    uint32_t actual_stride = stride;
    if (!stride) actual_stride = width;
    D(target + 16u) = actual_stride;
    D(context + 28u) = 1;
    D(context + 32u) = 1;
    D(context + 24u) = 0;
    kernel_font_rect_init_832f44(packer, width - padding, height - padding, nodes, node_count);
    if (pixels) kernel_font_clear_81752e(pixels, 0, width * height);
    return 1;
}

void kernel_font_pack_end_83323c(volatile uint32_t context)
{
    kernel_font_free_8175a4(D(context + 40u));
    kernel_font_free_8175a4(D(context + 4u));
}

void kernel_font_pack_oversample_833260(volatile uint32_t context,
    uint32_t horizontal, volatile uint32_t vertical)
{
    if (horizontal <= 4u) D(context + 28u) = horizontal;
    if (vertical <= 4u) D(context + 32u) = vertical;
}

void kernel_font_pack_skip_833284(volatile uint32_t context, volatile uint32_t skip)
{
    D(context + 24u) = skip;
}

float kernel_font_oversample_shift_8338c8(uint32_t factor)
{
    if (!factor) return 0.0f;
    volatile float value = (int32_t)(1u - factor) / ((float)(int32_t)factor * 2.0f);
    return value;
}

int32_t kernel_font_gather_rects_8338fa(volatile uint32_t context,
    volatile uint32_t info, volatile uint32_t ranges, volatile int32_t count,
    volatile uint32_t rects)
{
    uint32_t total = 0, missing_added = 0;
    for (int32_t i = 0; i < count; ++i) {
        uint32_t range = ranges + (uint32_t)i * 24u;
        uint32_t size_bits = D(range);
        volatile float scale;
        if (__builtin_isgreater(F(P(&size_bits)), 0.0f))
            scale = kernel_font_scale_height_8306ec(info, F(P(&size_bits)));
        else {
            size_bits ^= 0x80000000u;
            scale = kernel_font_scale_em_83073a(info, F(P(&size_bits)));
        }
        B(ranges + (uint32_t)i * 24u + 20u) = (uint8_t)D(context + 28u);
        B(ranges + (uint32_t)i * 24u + 21u) = (uint8_t)D(context + 32u);
        for (int32_t j = 0; j < (int32_t)D(ranges + (uint32_t)i * 24u + 12u); ++j) {
            range = ranges + (uint32_t)i * 24u;
            uint32_t cp = D(range + 8u) ? D(D(range + 8u) + (uint32_t)j * 4u) : D(range + 4u) + (uint32_t)j;
            int32_t glyph = kernel_font_glyph_index_82ca7c(info, cp);
            if (!glyph && (D(context + 24u) || missing_added)) {
                /* Original stores height before width on the missing path. */
                uint32_t output = rects + total * 24u;
                D(rects + total * 24u + 16u) = 0;
                D(output + 12u) = 0;
            } else {
                int32_t box[4];
                volatile float sy = (long double)D(context + 32u) * scale;
                volatile float sx = (long double)D(context + 28u) * scale;
                kernel_font_bitmap_box_subpixel_8308d2(info, glyph, sx, sy, 0.0f, 0.0f,
                    P(box), P(box + 1), P(box + 2), P(box + 3));
                D(rects + total * 24u + 12u) = (uint32_t)box[2] - (uint32_t)box[0] + D(context + 20u) + D(context + 28u) - 1u;
                D(rects + total * 24u + 16u) = (uint32_t)box[3] - (uint32_t)box[1] + D(context + 20u) + D(context + 32u) - 1u;
                if (!glyph) missing_added = 1;
            }
            ++total;
        }
    }
    return (int32_t)total;
}

void kernel_font_pack_rects_8342c4(uint32_t context, uint32_t rects, int32_t count)
{
    kernel_font_rect_pack_832f78(D(context + 4u), rects, count);
}

void kernel_font_packed_quad_8346e2(uint32_t chars, int32_t width,
    int32_t height, int32_t index, volatile uint32_t x,
    volatile uint32_t y, volatile uint32_t quad, int32_t align)
{
    volatile float iw = 1.0f / width, ih = 1.0f / height;
    uint32_t ch = chars + (uint32_t)index * 28u;
    if (align) {
        volatile float rx = (int32_t)kernel_floor_81a9ee((double)(F(x) + F(ch + 8u) + F(0x83d07bu)));
        volatile float ry = (int32_t)kernel_floor_81a9ee((double)(F(y) + F(ch + 12u) + F(0x83d07bu)));
        D(quad) = D(P(&rx));
        D(quad + 4u) = D(P(&ry));
        F(quad + 16u) = F(ch + 20u) + rx - F(ch + 8u);
        F(quad + 20u) = F(ch + 24u) + ry - F(ch + 12u);
    } else {
        F(quad) = F(x) + F(ch + 8u);
        F(quad + 4u) = F(y) + F(ch + 12u);
        F(quad + 16u) = F(x) + F(ch + 20u);
        F(quad + 20u) = F(y) + F(ch + 24u);
    }
    F(quad + 8u) = W(ch) * iw;
    F(quad + 12u) = W(ch + 2u) * ih;
    F(quad + 24u) = W(ch + 4u) * iw;
    F(quad + 28u) = W(ch + 6u) * ih;
    F(x) = F(x) + F(ch + 16u);
}
