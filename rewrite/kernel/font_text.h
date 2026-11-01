#ifndef OPENICESTAR_KERNEL_FONT_TEXT_H
#define OPENICESTAR_KERNEL_FONT_TEXT_H
#include <stdint.h>

void kernel_font_glyph_opaque_829003(volatile uint32_t info,volatile float height,volatile uint32_t code,volatile uint32_t surface,volatile uint32_t x,volatile uint32_t y,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t w,volatile uint32_t h,volatile uint32_t ox,volatile uint32_t oy);
void kernel_font_glyph_clip_829133(volatile uint32_t info,volatile float height,volatile uint32_t code,volatile uint32_t surface,volatile uint32_t x,volatile uint32_t y,volatile uint32_t maxx,volatile uint32_t maxy,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t w,volatile uint32_t h,volatile uint32_t ox,volatile uint32_t oy);
void kernel_font_glyph_alpha_82927d(volatile uint32_t info,volatile float height,volatile uint32_t code,volatile uint32_t surface,volatile uint32_t x,volatile uint32_t y,volatile uint32_t fg,volatile uint32_t w,volatile uint32_t h,volatile uint32_t ox,volatile uint32_t oy);
uint32_t kernel_font_legacy_code_8293e9(volatile uint32_t code);
void kernel_font_string_legacy_829485(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap);
uint32_t kernel_font_utf8_cont_8296d7(uint32_t byte);
int32_t kernel_font_utf8_decode_829707(volatile uint32_t text,volatile uint32_t consumed);
void kernel_font_string_utf8_829941(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap);
uint32_t kernel_font_string_lines_829b4d(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t maxx,volatile uint32_t maxy,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap,volatile uint32_t lineheight,volatile uint32_t maxbytes);
void kernel_font_string_legacy_clip_829dd3(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t maxx,volatile uint32_t maxy,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap);
void kernel_font_string_legacy_alpha_82a03f(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t fg,volatile uint32_t gap);
uint32_t kernel_font_text_init_82a2c5(volatile uint32_t data,volatile uint32_t info);

#endif
