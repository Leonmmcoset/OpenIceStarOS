#ifndef OPENICESTAR_KERNEL_FONT_ATLAS_H
#define OPENICESTAR_KERNEL_FONT_ATLAS_H
#include <stdint.h>
/* Original atlas layouts: baked chars 20B, packed chars 28B, quads 32B,
 * rectangles 24B, packer 20B, pack context 44B, range descriptors 24B. */
void kernel_font_baked_quad_832dbe(uint32_t chars, int32_t width, int32_t height, int32_t index, uint32_t x, uint32_t y, uint32_t quad, int32_t opengl);
void kernel_font_rect_init_832f44(uint32_t context, uint32_t width, uint32_t height, uint32_t nodes, uint32_t count);
void kernel_font_rect_pack_832f78(uint32_t context, uint32_t rects, int32_t count);
uint32_t kernel_font_pack_begin_8330f6(uint32_t context, uint32_t pixels, uint32_t width, uint32_t height, uint32_t stride, uint32_t padding, uint32_t userdata);
void kernel_font_pack_end_83323c(uint32_t context);
void kernel_font_pack_oversample_833260(uint32_t context, uint32_t horizontal, uint32_t vertical);
void kernel_font_pack_skip_833284(uint32_t context, uint32_t skip);
float kernel_font_oversample_shift_8338c8(uint32_t factor);
int32_t kernel_font_gather_rects_8338fa(uint32_t context, uint32_t info, uint32_t ranges, int32_t count, uint32_t rects);
void kernel_font_pack_rects_8342c4(uint32_t context, uint32_t rects, int32_t count);
void kernel_font_packed_quad_8346e2(uint32_t chars, int32_t width, int32_t height, int32_t index, uint32_t x, uint32_t y, uint32_t quad, int32_t align);
#endif
