/* Quadratic contour closure, CFF subroutines/CID selection and bitmap bounds.
 * Preserve the original caller-area stores, signed tests and rounding calls.
 * The TrueType outline decoder and Type-2 interpreter remain separate entries.
 */
#include "font_outline.h"
#include "font_glyph.h"
#include "font_cmap.h"
#include "math_extra.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))

uint32_t kernel_font_close_shape_82d24a(volatile uint32_t vertices, uint32_t count, volatile int32_t off, volatile int32_t start_off, volatile int32_t sx, volatile int32_t sy, volatile int32_t scx, volatile int32_t scy, volatile int32_t cx, volatile int32_t cy)
{
    /* These two entries modify their original argument slots. Merely taking
     * &count lets GCC use a private parameter copy, which loses that ABI. */
    (void)count;
    volatile uint32_t *live_count = (volatile uint32_t *)__builtin_frame_address(0) + 3;
    if (start_off) {
        if (off) {
            int32_t mx = (int32_t)((uint32_t)cx + (uint32_t)scx) >> 1;
            int32_t my = (int32_t)((uint32_t)cy + (uint32_t)scy) >> 1;
            uint32_t target = vertices + *live_count * 14u;
            ++*live_count;
            kernel_font_vertex_82cf10(target, 3, mx, my, cx, cy);
        }
        uint32_t target = vertices + *live_count * 14u;
        ++*live_count;
        kernel_font_vertex_82cf10(target, 3, sx, sy, scx, scy);
    } else {
        uint32_t target = vertices + *live_count * 14u;
        ++*live_count;
        kernel_font_vertex_82cf10(target, off ? 3 : 2, sx, sy, off ? cx : 0, off ? cy : 0);
    }
    return *live_count;
}

KernelFontBuffer kernel_font_subroutine_82e4ac(KernelFontBuffer buffer, int32_t index)
{
    int32_t count = (int32_t)kernel_font_cff_count_82bd28(P(&buffer));
    uint32_t bias = count > 33899 ? 32768u : count > 1239 ? 1131u : 107u;
    (void)index;
    volatile int32_t *live_index = (volatile int32_t *)__builtin_frame_address(0) + 6;
    *live_index = (int32_t)((uint32_t)*live_index + bias);
    if (*live_index < 0 || *live_index >= count) return kernel_font_buffer_82b946(0, 0);
    return kernel_font_cff_item_82bd56(buffer, *live_index);
}

KernelFontBuffer kernel_font_cid_subrs_82e552(uint32_t info, int32_t glyph)
{
    KernelFontBuffer select = *(volatile KernelFontBuffer *)(uintptr_t)(info + 116u);
    int32_t selected = -1;
    kernel_font_seek_82b8a2(P(&select), 0);
    uint32_t format = (uint8_t)kernel_font_get8_82b838(P(&select));
    if (format == 0) {
        kernel_font_skip_82b8de(P(&select), glyph);
        selected = (uint8_t)kernel_font_get8_82b838(P(&select));
    } else if (format == 3) {
        int32_t ranges = (int32_t)kernel_font_getn_82b8fe(P(&select), 2);
        int32_t first = (int32_t)kernel_font_getn_82b8fe(P(&select), 2);
        for (int32_t i = 0; i < ranges; ++i) {
            uint32_t fd = (uint8_t)kernel_font_get8_82b838(P(&select));
            int32_t last = (int32_t)kernel_font_getn_82b8fe(P(&select), 2);
            if (glyph >= first && glyph < last) { selected = (int32_t)fd; break; }
            first = last;
        }
    }
    /* Original constructs an empty local on a miss, then unconditionally
     * overwrites it with INDEX item -1. Preserve that unchecked lookup. */
    KernelFontBuffer dictionary = kernel_font_cff_item_82bd56(
        *(volatile KernelFontBuffer *)(uintptr_t)(info + 104u), selected);
    return kernel_font_cff_subrs_82c232(
        *(volatile KernelFontBuffer *)(uintptr_t)(info + 56u), dictionary);
}

void kernel_font_bitmap_box_subpixel_8308d2(uint32_t info, int32_t glyph, volatile float sx, volatile float sy, volatile float dx, volatile float dy, volatile uint32_t x0, volatile uint32_t y0, volatile uint32_t x1, volatile uint32_t y1)
{
    /* Outputs can overlap later arguments. The original reads each pointer
     * and float operand from the live caller area immediately before use. */
    int32_t box[4] = {0};
    int32_t ok = kernel_font_glyph_box_82d062(info, glyph, P(box), P(box + 1), P(box + 2), P(box + 3));
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t output = i == 0 ? x0 : i == 1 ? y0 : i == 2 ? x1 : y1;
        if (!output) continue;
        uint32_t result = 0;
        if (ok) {
            int32_t value = (i & 1u) ? (int32_t)(0u - (uint32_t)box[4u - i]) : box[i];
            double scaled = (double)value * ((i & 1u) ? sy : sx) + ((i & 1u) ? dy : dx);
            result = (uint32_t)(int32_t)(i < 2 ? kernel_floor_81a9ee(scaled) : kernel_ceil_81aa52(scaled));
        }
        D(output) = result;
    }
}

void kernel_font_bitmap_box_830a3c(uint32_t info, int32_t glyph, float sx, float sy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    kernel_font_bitmap_box_subpixel_8308d2(info, glyph, sx, sy, 0.0f, 0.0f, x0, y0, x1, y1);
}

void kernel_font_codepoint_bitmap_box_subpixel_830a92(uint32_t info, uint32_t codepoint, float sx, float sy, float dx, float dy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    kernel_font_bitmap_box_subpixel_8308d2(info, glyph, sx, sy, dx, dy, x0, y0, x1, y1);
}

void kernel_font_codepoint_bitmap_box_830af6(uint32_t info, uint32_t codepoint, float sx, float sy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    kernel_font_codepoint_bitmap_box_subpixel_830a92(info, codepoint, sx, sy, 0.0f, 0.0f, x0, y0, x1, y1);
}
