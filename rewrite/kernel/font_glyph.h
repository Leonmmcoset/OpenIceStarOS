#ifndef OPENICESTAR_KERNEL_FONT_GLYPH_H
#define OPENICESTAR_KERNEL_FONT_GLYPH_H
#include <stdint.h>
void kernel_font_vertex_82cf10(uint32_t vertex, uint32_t type, int32_t x, int32_t y, int32_t cx, int32_t cy);
int32_t kernel_font_glyph_offset_82cf50(uint32_t info, int32_t glyph);
int32_t kernel_font_glyph_box_82d062(uint32_t info, int32_t glyph, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
int32_t kernel_font_codepoint_box_82d15c(uint32_t info, uint32_t codepoint, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
int32_t kernel_font_glyph_empty_82d1a4(uint32_t info, int32_t glyph);
int32_t kernel_font_cff_box_82f80a(uint32_t info, int32_t glyph, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
#endif
