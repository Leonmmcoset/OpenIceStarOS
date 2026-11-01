#ifndef OPENICESTAR_KERNEL_FONT_SHAPE_H
#define OPENICESTAR_KERNEL_FONT_SHAPE_H
#include <stdint.h>
int32_t kernel_font_truetype_shape_82d386(uint32_t info, int32_t glyph, uint32_t output);
int32_t kernel_font_cff_shape_82f756(uint32_t info, int32_t glyph, uint32_t output);
int32_t kernel_font_shape_82f90a(uint32_t info, int32_t glyph, uint32_t output);
int32_t kernel_font_codepoint_shape_82cede(uint32_t info, uint32_t codepoint, uint32_t output);
#endif
