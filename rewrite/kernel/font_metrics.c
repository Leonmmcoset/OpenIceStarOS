/* Font metrics, scaling and SVG document access recovered from the original
 * SFNT implementation. Preserve signed metric values, cached/live fields,
 * optional outputs and unchecked glyph/SVG behavior. */
#include "font_metrics.h"
#include "font_parse.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

int32_t kernel_font_glyph_index_82ca7c(uint32_t info, uint32_t codepoint);

void kernel_font_hmetrics_82f958(uint32_t info, int32_t glyph, uint32_t advance, uint32_t bearing)
{
    uint32_t count = (uint16_t)kernel_font_u16_82be14(D(info + 28u) + D(info + 4u) + 34u);
    if ((int32_t)count > glyph) {
        if (advance) D(advance) = (uint32_t)(int16_t)kernel_font_s16_82be34(
            D(info + 32u) + D(info + 4u) + ((uint32_t)glyph << 2));
        if (bearing) D(bearing) = (uint32_t)(int16_t)kernel_font_s16_82be34(
            D(info + 32u) + D(info + 4u) + ((uint32_t)glyph << 2) + 2u);
    } else {
        if (advance) D(advance) = (uint32_t)(int16_t)kernel_font_s16_82be34(
            D(info + 32u) + D(info + 4u) + (count << 2) - 4u);
        if (bearing) D(bearing) = (uint32_t)(int16_t)kernel_font_s16_82be34(
            D(info + 32u) + D(info + 4u) + ((count * 2u + (uint32_t)glyph - count) << 1));
    }
}

void kernel_font_vmetrics_83054a(uint32_t info, uint32_t ascent, uint32_t descent, uint32_t gap)
{
    if (ascent) D(ascent) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 28u) + D(info + 4u) + 4u);
    if (descent) D(descent) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 28u) + D(info + 4u) + 6u);
    if (gap) D(gap) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 28u) + D(info + 4u) + 8u);
}

uint32_t kernel_font_typo_metrics_8305c0(uint32_t info, uint32_t ascent, uint32_t descent, uint32_t gap)
{
    uint32_t table = kernel_font_table_82bfe6(D(info + 4u), D(info + 8u), 0x83d053u);
    if (!table) return 0;
    if (ascent) D(ascent) = (uint32_t)(int16_t)kernel_font_s16_82be34(table + D(info + 4u) + 68u);
    if (descent) D(descent) = (uint32_t)(int16_t)kernel_font_s16_82be34(table + D(info + 4u) + 70u);
    if (gap) D(gap) = (uint32_t)(int16_t)kernel_font_s16_82be34(table + D(info + 4u) + 72u);
    return 1;
}

void kernel_font_bounds_83066a(uint32_t info, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    D(x0) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 20u) + D(info + 4u) + 36u);
    D(y0) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 20u) + D(info + 4u) + 38u);
    D(x1) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 20u) + D(info + 4u) + 40u);
    D(y1) = (uint32_t)(int16_t)kernel_font_s16_82be34(D(info + 20u) + D(info + 4u) + 42u);
}

float kernel_font_scale_height_8306ec(uint32_t info, float height)
{
    int32_t ascent = (int16_t)kernel_font_s16_82be34(D(info + 28u) + D(info + 4u) + 4u);
    int32_t descent = (int16_t)kernel_font_s16_82be34(D(info + 28u) + D(info + 4u) + 6u);
    return height / (float)(ascent - descent);
}

float kernel_font_scale_em_83073a(uint32_t info, float pixels)
{
    uint32_t units = (uint16_t)kernel_font_u16_82be14(D(info + 20u) + D(info + 4u) + 18u);
    return pixels / (float)units;
}

uint32_t kernel_font_svg_record_83077c(uint32_t info, int32_t glyph)
{
    uint32_t data = D(info + 4u);
    uint32_t list = kernel_font_svg_82c34c(info) + data;
    uint32_t count = (uint16_t)kernel_font_u16_82be14(list);
    uint32_t records = list + 2u;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t record = records + i * 12u;
        if ((int32_t)(uint16_t)kernel_font_u16_82be14(record) <= glyph &&
            (int32_t)(uint16_t)kernel_font_u16_82be14(record + 2u) >= glyph)
            return record;
    }
    return 0;
}

uint32_t kernel_font_glyph_svg_83081c(uint32_t info, int32_t glyph, uint32_t output)
{
    uint32_t data = D(info + 4u);
    if (!D(info + 44u)) return 0;
    uint32_t record = kernel_font_svg_record_83077c(info, glyph);
    if (!record) return 0;
    uint32_t list = data + D(info + 44u);
    D(output) = list + kernel_font_u32_82be52(record + 4u);
    return kernel_font_u32_82be52(record + 8u);
}

uint32_t kernel_font_codepoint_svg_8308a0(uint32_t info, uint32_t codepoint, uint32_t output)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    return kernel_font_glyph_svg_83081c(info, glyph, output);
}
