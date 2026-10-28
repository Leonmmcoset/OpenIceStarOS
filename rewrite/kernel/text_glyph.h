#ifndef OPENICESTAR_TEXT_GLYPH_H
#define OPENICESTAR_TEXT_GLYPH_H
#include <stdint.h>
#define GLYPH_PARAMETERS uint32_t id,uint32_t x,uint32_t y, \
    uint32_t foreground,uint32_t background,uint32_t character
uint32_t kernel_text_glyph8_81db5a(GLYPH_PARAMETERS);
uint32_t kernel_text_rotated8_81dcc0(GLYPH_PARAMETERS);
uint32_t kernel_text_glyph32_81de2a(GLYPH_PARAMETERS);
uint32_t kernel_text_rotated32_81df96(GLYPH_PARAMETERS);
#undef GLYPH_PARAMETERS
#endif
