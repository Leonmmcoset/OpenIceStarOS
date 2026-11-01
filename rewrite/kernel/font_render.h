#ifndef OPENICESTAR_KERNEL_FONT_RENDER_H
#define OPENICESTAR_KERNEL_FONT_RENDER_H
#include <stdint.h>
/* Float parameters retain their raw 32-bit cdecl words. */
void kernel_font_edges_831884(uint32_t bitmap, uint32_t points, uint32_t lengths,
    int32_t contours, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy,
    int32_t offset_x, int32_t offset_y, int32_t invert, uint32_t userdata);
void kernel_font_raster_83244c(uint32_t bitmap, uint32_t flatness, uint32_t vertices,
    int32_t count, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy,
    int32_t offset_x, int32_t offset_y, int32_t invert, uint32_t userdata);
void kernel_font_bitmap_free_832530(uint32_t bitmap, uint32_t userdata);
uint32_t kernel_font_get_glyph_subpixel_832544(uint32_t info, uint32_t sx, uint32_t sy,
    uint32_t dx, uint32_t dy, int32_t glyph, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);
uint32_t kernel_font_get_glyph_832700(uint32_t info, uint32_t sx, uint32_t sy,
    int32_t glyph, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);
void kernel_font_make_glyph_subpixel_832756(uint32_t info, uint32_t output, int32_t width,
    int32_t height, int32_t stride, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, int32_t glyph);
void kernel_font_make_glyph_832852(uint32_t info, uint32_t output, int32_t width,
    int32_t height, int32_t stride, uint32_t sx, uint32_t sy, int32_t glyph);
uint32_t kernel_font_get_codepoint_subpixel_8328a8(uint32_t info, uint32_t sx, uint32_t sy,
    uint32_t dx, uint32_t dy, uint32_t codepoint, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);
void kernel_font_make_codepoint_subpixel_83298a(uint32_t info, uint32_t output, int32_t width,
    int32_t height, int32_t stride, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t codepoint);
uint32_t kernel_font_get_codepoint_8329ec(uint32_t info, uint32_t sx, uint32_t sy,
    uint32_t codepoint, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff);
void kernel_font_make_codepoint_832a42(uint32_t info, uint32_t output, int32_t width,
    int32_t height, int32_t stride, uint32_t sx, uint32_t sy, uint32_t codepoint);
#endif
