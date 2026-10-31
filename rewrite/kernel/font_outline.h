#ifndef OPENICESTAR_KERNEL_FONT_OUTLINE_H
#define OPENICESTAR_KERNEL_FONT_OUTLINE_H
#include "font_parse.h"
uint32_t kernel_font_close_shape_82d24a(uint32_t vertices, uint32_t count, int32_t off, int32_t start_off, int32_t sx, int32_t sy, int32_t scx, int32_t scy, int32_t cx, int32_t cy);
KernelFontBuffer kernel_font_subroutine_82e4ac(KernelFontBuffer buffer, int32_t index);
KernelFontBuffer kernel_font_cid_subrs_82e552(uint32_t info, int32_t glyph);
void kernel_font_bitmap_box_subpixel_8308d2(uint32_t info, int32_t glyph, float sx, float sy, float dx, float dy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void kernel_font_bitmap_box_830a3c(uint32_t info, int32_t glyph, float sx, float sy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void kernel_font_codepoint_bitmap_box_subpixel_830a92(uint32_t info, uint32_t codepoint, float sx, float sy, float dx, float dy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void kernel_font_codepoint_bitmap_box_830af6(uint32_t info, uint32_t codepoint, float sx, float sy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
#endif
