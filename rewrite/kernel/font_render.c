/* Outline edges, bitmap allocation/rendering and codepoint wrappers.
 * The original scan driver and its DWORD-width clear/copy remain external. */
#include "font_render.h"
#include "font_shape.h"
#include "font_cmap.h"
#include "font_scan.h"
typedef uint32_t raw32 __attribute__((may_alias));
typedef float raw_float __attribute__((may_alias));
#define D(a) (*(volatile raw32 *)(uintptr_t)(uint32_t)(a))
#define F(a) (*(volatile raw_float *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
#define V(a) F(P(&(a)))
uint32_t kernel_font_malloc_817590(uint32_t size);
void kernel_font_free_8175a4(uint32_t address);
uint32_t kernel_font_flatten_832000(uint32_t vertices, int32_t count, uint32_t flatness,
    uint32_t lengths, uint32_t contours, uint32_t userdata);
void kernel_font_driver_830f24(uint32_t bitmap, uint32_t edges, int32_t count,
    int32_t samples, int32_t offset_x, int32_t offset_y, uint32_t userdata);
void kernel_font_bitmap_box_subpixel_8308d2(uint32_t info, int32_t glyph,
    uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy,
    uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);

static __attribute__((always_inline)) inline uint32_t bits(float value)
{
    union { float value; uint32_t bits; } result = {value};
    return result.bits;
}

void kernel_font_edges_831884(volatile uint32_t bitmap, volatile uint32_t points,
    volatile uint32_t lengths, volatile int32_t contours, volatile uint32_t sx,
    volatile uint32_t sy, volatile uint32_t dx, volatile uint32_t dy,
    volatile int32_t offset_x, volatile int32_t offset_y, volatile int32_t invert, volatile uint32_t userdata)
{
    volatile float scale_y = V(sy);
    if (invert) D(P(&scale_y)) ^= 0x80000000u;
    int32_t samples = (int32_t)D(bitmap + 4u) > 7 ? 5 : 15;
    uint32_t total = 0;
    for (int32_t i = 0; i < contours; ++i) total += D(lengths + (uint32_t)i * 4u);
    uint32_t edges = kernel_font_malloc_817590(total * 20u + 20u);
    if (!edges) return;
    uint32_t edge_count = 0, first = 0;
    for (int32_t i = 0; i < contours; ++i) {
        uint32_t contour = points + first * 8u;
        first += D(lengths + (uint32_t)i * 4u);
        uint32_t previous = D(lengths + (uint32_t)i * 4u) - 1u;
        for (int32_t j = 0; j < (int32_t)D(lengths + (uint32_t)i * 4u); ++j) {
            uint32_t a = (uint32_t)j, b = previous;
            if (F(contour + previous * 8u + 4u) != F(contour + (uint32_t)j * 8u + 4u)) {
                uint32_t edge = edges + edge_count * 20u;
                D(edge + 16u) = 0;
                if (invert ? __builtin_isgreater(F(contour + previous * 8u + 4u), F(contour + (uint32_t)j * 8u + 4u))
                           : __builtin_isless(F(contour + previous * 8u + 4u), F(contour + (uint32_t)j * 8u + 4u))) {
                    D(edge + 16u) = 1; a = previous; b = (uint32_t)j;
                }
                F(edge) = (long double)F(contour + a * 8u) * V(sx) + V(dx);
                F(edge + 4u) = ((long double)F(contour + a * 8u + 4u) * scale_y + V(dy)) * samples;
                F(edge + 8u) = (long double)F(contour + b * 8u) * V(sx) + V(dx);
                F(edge + 12u) = ((long double)F(contour + b * 8u + 4u) * scale_y + V(dy)) * samples;
                ++edge_count;
            }
            previous = (uint32_t)j;
        }
    }
    kernel_font_edge_sort_831858(edges, (int32_t)edge_count);
    kernel_font_driver_830f24(bitmap, edges, (int32_t)edge_count, samples, offset_x, offset_y, userdata);
    kernel_font_free_8175a4(edges);
}

void kernel_font_raster_83244c(volatile uint32_t bitmap, volatile uint32_t flatness,
    volatile uint32_t vertices, volatile int32_t count, volatile uint32_t sx,
    volatile uint32_t sy, volatile uint32_t dx, volatile uint32_t dy,
    volatile int32_t offset_x, volatile int32_t offset_y, volatile int32_t invert, volatile uint32_t userdata)
{
    volatile float scale = __builtin_isgreater(V(sx), V(sy)) ? V(sy) : V(sx);
    uint32_t lengths = 0, contours = 0;
    uint32_t points = kernel_font_flatten_832000(vertices, count, bits(V(flatness) / scale), P(&lengths), P(&contours), userdata);
    if (points) {
        kernel_font_edges_831884(bitmap, points, lengths, (int32_t)contours, sx, sy, dx, dy, offset_x, offset_y, invert, userdata);
        kernel_font_free_8175a4(lengths);
        kernel_font_free_8175a4(points);
    }
}

void kernel_font_bitmap_free_832530(uint32_t bitmap, uint32_t userdata)
{
    (void)userdata;
    kernel_font_free_8175a4(bitmap);
}

uint32_t kernel_font_get_glyph_subpixel_832544(volatile uint32_t info, uint32_t sx, uint32_t sy,
    volatile uint32_t dx, volatile uint32_t dy, volatile int32_t glyph,
    volatile uint32_t width, volatile uint32_t height, volatile uint32_t xoff, volatile uint32_t yoff)
{
    uint32_t vertices;
    int32_t count = kernel_font_shape_82f90a(info, glyph, P(&vertices));
    /* The original writes fallback scale values into its caller's arguments. */
    (void)sx; (void)sy;
    volatile raw32 *live = (volatile raw32 *)__builtin_frame_address(0) + 2;
    if (V(live[1]) == 0.0f) live[1] = live[2];
    if (V(live[2]) == 0.0f) {
        if (V(live[1]) == 0.0f) { kernel_font_free_8175a4(vertices); return 0; }
        live[2] = live[1];
    }
    uint32_t box[4];
    kernel_font_bitmap_box_subpixel_8308d2(info, glyph, live[1], live[2], dx, dy,
        P(box), P(box + 1), P(box + 2), P(box + 3));
    uint32_t bitmap[4] = {box[2] - box[0], box[3] - box[1], 0, 0};
    if (width) D(width) = bitmap[0];
    if (height) D(height) = bitmap[1];
    if (xoff) D(xoff) = box[0];
    if (yoff) D(yoff) = box[1];
    if (bitmap[0] && bitmap[1]) {
        bitmap[3] = kernel_font_malloc_817590(bitmap[0] * bitmap[1]);
        if (bitmap[3]) {
            bitmap[2] = bitmap[0];
            kernel_font_raster_83244c(P(bitmap), 0x3eb33333u, vertices, count, live[1], live[2], dx, dy,
                (int32_t)box[0], (int32_t)box[1], 1, D(info));
        }
    }
    kernel_font_free_8175a4(vertices);
    return bitmap[3];
}

uint32_t kernel_font_get_glyph_832700(uint32_t info, uint32_t sx, uint32_t sy,
    int32_t glyph, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff)
{
    return kernel_font_get_glyph_subpixel_832544(info, sx, sy, 0, 0, glyph, width, height, xoff, yoff);
}

void kernel_font_make_glyph_subpixel_832756(volatile uint32_t info, volatile uint32_t output,
    volatile int32_t width, volatile int32_t height, volatile int32_t stride, volatile uint32_t sx,
    volatile uint32_t sy, volatile uint32_t dx, volatile uint32_t dy, volatile int32_t glyph)
{
    uint32_t vertices, xoff, yoff;
    int32_t count = kernel_font_shape_82f90a(info, glyph, P(&vertices));
    kernel_font_bitmap_box_subpixel_8308d2(info, glyph, sx, sy, dx, dy, P(&xoff), P(&yoff), 0, 0);
    uint32_t bitmap[4] = {(uint32_t)width, (uint32_t)height, (uint32_t)stride, output};
    if (bitmap[0] && bitmap[1]) kernel_font_raster_83244c(P(bitmap), 0x3eb33333u, vertices, count,
        sx, sy, dx, dy, (int32_t)xoff, (int32_t)yoff, 1, D(info));
    kernel_font_free_8175a4(vertices);
}

void kernel_font_make_glyph_832852(uint32_t info, uint32_t output, int32_t width,
    int32_t height, int32_t stride, uint32_t sx, uint32_t sy, int32_t glyph)
{
    kernel_font_make_glyph_subpixel_832756(info, output, width, height, stride, sx, sy, 0, 0, glyph);
}

uint32_t kernel_font_get_codepoint_subpixel_8328a8(volatile uint32_t info, volatile uint32_t sx,
    volatile uint32_t sy, volatile uint32_t dx, volatile uint32_t dy, uint32_t codepoint,
    volatile uint32_t width, volatile uint32_t height, volatile uint32_t xoff, volatile uint32_t yoff)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    return kernel_font_get_glyph_subpixel_832544(info, sx, sy, dx, dy, glyph, width, height, xoff, yoff);
}

void kernel_font_make_codepoint_subpixel_83298a(volatile uint32_t info, volatile uint32_t output,
    volatile int32_t width, volatile int32_t height, volatile int32_t stride, volatile uint32_t sx,
    volatile uint32_t sy, volatile uint32_t dx, volatile uint32_t dy, uint32_t codepoint)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    kernel_font_make_glyph_subpixel_832756(info, output, width, height, stride, sx, sy, dx, dy, glyph);
}

uint32_t kernel_font_get_codepoint_8329ec(uint32_t info, uint32_t sx, uint32_t sy,
    uint32_t codepoint, uint32_t width, uint32_t height, uint32_t xoff, uint32_t yoff)
{
    return kernel_font_get_codepoint_subpixel_8328a8(info, sx, sy, 0, 0, codepoint, width, height, xoff, yoff);
}

void kernel_font_make_codepoint_832a42(uint32_t info, uint32_t output, int32_t width,
    int32_t height, int32_t stride, uint32_t sx, uint32_t sy, uint32_t codepoint)
{
    kernel_font_make_codepoint_subpixel_83298a(info, output, width, height, stride, sx, sy, 0, 0, codepoint);
}
