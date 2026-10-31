/* TrueType loca/glyf access and CFF bounds recovered from original x86.
 * Signed glyph/format checks, wrapped offsets, partial output writes and
 * CFF's distinct failure conventions are part of the original ABI. */
#include "font_glyph.h"
#include "font_parse.h"
#include "font_cmap.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
int32_t kernel_font_charstring_82e6f0(uint32_t info, int32_t glyph, uint32_t context);

void kernel_font_vertex_82cf10(uint32_t vertex, uint32_t type, int32_t x, int32_t y, int32_t cx, int32_t cy)
{
    B(vertex + 12u) = (uint8_t)type;
    W(vertex) = (uint16_t)x;
    W(vertex + 2u) = (uint16_t)y;
    W(vertex + 4u) = (uint16_t)cx;
    W(vertex + 6u) = (uint16_t)cy;
}

int32_t kernel_font_glyph_offset_82cf50(uint32_t info, int32_t glyph)
{
    if ((int32_t)D(info + 12u) <= glyph || (int32_t)D(info + 52u) > 1) return -1;
    uint32_t first, last;
    if (D(info + 52u) == 0) {
        first = 2u * (uint16_t)kernel_font_u16_82be14(D(info + 4u) + D(info + 16u) + (uint32_t)glyph * 2u) + D(info + 24u);
        last = 2u * (uint16_t)kernel_font_u16_82be14(D(info + 4u) + D(info + 16u) + (uint32_t)glyph * 2u + 2u) + D(info + 24u);
    } else {
        first = kernel_font_u32_82be52(D(info + 4u) + D(info + 16u) + (uint32_t)glyph * 4u) + D(info + 24u);
        last = kernel_font_u32_82be52(D(info + 4u) + D(info + 16u) + (uint32_t)glyph * 4u + 4u) + D(info + 24u);
    }
    return first == last ? -1 : (int32_t)first;
}

int32_t kernel_font_cff_box_82f80a(uint32_t info, int32_t glyph, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    uint32_t context[12] = {1};
    int32_t ok = kernel_font_charstring_82e6f0(info, glyph, (uint32_t)(uintptr_t)context);
    if (x0) D(x0) = ok ? context[6] : 0;
    if (y0) D(y0) = ok ? context[8] : 0;
    if (x1) D(x1) = ok ? context[7] : 0;
    if (y1) D(y1) = ok ? context[9] : 0;
    return ok ? (int32_t)context[11] : 0;
}

int32_t kernel_font_glyph_box_82d062(uint32_t info, int32_t glyph, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    if (D(info + 64u)) {
        kernel_font_cff_box_82f80a(info, glyph, x0, y0, x1, y1);
    } else {
        int32_t offset = kernel_font_glyph_offset_82cf50(info, glyph);
        if (offset < 0) return 0;
        if (x0) D(x0) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 4u) + (uint32_t)offset + 2u);
        if (y0) D(y0) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 4u) + (uint32_t)offset + 4u);
        if (x1) D(x1) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 4u) + (uint32_t)offset + 6u);
        if (y1) D(y1) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 4u) + (uint32_t)offset + 8u);
    }
    return 1;
}

int32_t kernel_font_codepoint_box_82d15c(uint32_t info, uint32_t codepoint, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    return kernel_font_glyph_box_82d062(info, glyph, x0, y0, x1, y1);
}

int32_t kernel_font_glyph_empty_82d1a4(uint32_t info, int32_t glyph)
{
    if (D(info + 64u)) return !kernel_font_cff_box_82f80a(info, glyph, 0, 0, 0, 0);
    int32_t offset = kernel_font_glyph_offset_82cf50(info, glyph);
    if (offset < 0) return 1;
    return (int16_t)kernel_font_s16_82be34(D(info + 4u) + (uint32_t)offset) == 0;
}
