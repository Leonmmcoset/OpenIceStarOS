#ifndef OPENICESTAR_KERNEL_FONT_CONTEXT_H
#define OPENICESTAR_KERNEL_FONT_CONTEXT_H
#include <stdint.h>
void kernel_font_context_bounds_82e05c(uint32_t context, int32_t x, int32_t y);
void kernel_font_context_vertex_82e0e0(uint32_t context, uint32_t type, int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t cx1, int32_t cy1);
void kernel_font_context_close_82e1d6(uint32_t context);
void kernel_font_context_move_82e27c(uint32_t context, float dx, float dy);
void kernel_font_context_line_82e334(uint32_t context, float dx, float dy);
void kernel_font_context_curve_82e3c8(uint32_t context, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3);
#endif
