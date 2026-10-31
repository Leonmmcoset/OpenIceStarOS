#ifndef OPENICESTAR_KERNEL_FONT_METRICS_H
#define OPENICESTAR_KERNEL_FONT_METRICS_H
#include <stdint.h>

void kernel_font_hmetrics_82f958(uint32_t info, int32_t glyph, uint32_t advance, uint32_t bearing);
void kernel_font_vmetrics_83054a(uint32_t info, uint32_t ascent, uint32_t descent, uint32_t gap);
uint32_t kernel_font_typo_metrics_8305c0(uint32_t info, uint32_t ascent, uint32_t descent, uint32_t gap);
void kernel_font_bounds_83066a(uint32_t info, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
float kernel_font_scale_height_8306ec(uint32_t info, float height);
float kernel_font_scale_em_83073a(uint32_t info, float pixels);
uint32_t kernel_font_svg_record_83077c(uint32_t info, int32_t glyph);
uint32_t kernel_font_glyph_svg_83081c(uint32_t info, int32_t glyph, uint32_t output);
uint32_t kernel_font_codepoint_svg_8308a0(uint32_t info, uint32_t codepoint, uint32_t output);
#endif
