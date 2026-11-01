#ifndef OPENICESTAR_KERNEL_FONT_BAKE_H
#define OPENICESTAR_KERNEL_FONT_BAKE_H
#include <stdint.h>
int32_t kernel_font_bake_832a98(uint32_t data, uint32_t offset, uint32_t height, uint32_t pixels, uint32_t width, uint32_t rows, uint32_t first, int32_t count, uint32_t chars);
void kernel_font_prefilter_833bbc(uint32_t info, uint32_t output, uint32_t width, uint32_t height, uint32_t stride, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, int32_t horizontal, int32_t vertical, uint32_t subx, uint32_t suby, int32_t glyph);
uint32_t kernel_font_render_rects_833c94(uint32_t context, uint32_t info, uint32_t ranges, int32_t count, uint32_t rects);
uint32_t kernel_font_pack_ranges_8342e8(uint32_t context, uint32_t data, int32_t index, uint32_t ranges, int32_t count);
uint32_t kernel_font_pack_range_8345c2(uint32_t context, uint32_t data, int32_t index, uint32_t height, uint32_t first, int32_t count, uint32_t chars);
void kernel_font_scaled_metrics_834612(uint32_t data, int32_t index, uint32_t size, uint32_t ascent, uint32_t descent, uint32_t gap);
int32_t kernel_font_bake_api_836f70(uint32_t data, uint32_t offset, uint32_t height, uint32_t pixels, uint32_t width, uint32_t rows, uint32_t first, int32_t count, uint32_t chars);
int32_t kernel_font_offset_api_836fbc(uint32_t data, int32_t index);
int32_t kernel_font_count_api_836fd6(uint32_t data);
int32_t kernel_font_init_api_836fea(uint32_t info, uint32_t data, uint32_t offset);
#endif
