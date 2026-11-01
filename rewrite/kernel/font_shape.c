/* TrueType simple/compound outlines and CFF shape allocation, original ABI.
 * Flags, short coordinates, implied points and legacy compound transforms
 * follow the executable. Type-2 bytecode interpretation remains external. */
#include "font_shape.h"
#include "font_glyph.h"
#include "font_outline.h"
#include "font_parse.h"
#include "font_cmap.h"
#include "math_extra.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define F(a) (*(volatile float *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
uint32_t kernel_font_malloc_817590(uint32_t size);
void kernel_font_free_8175a4(uint32_t address);
void kernel_font_copy_dwords_817510(uint32_t destination, uint32_t source, uint32_t count);
int32_t kernel_font_charstring_82e6f0(uint32_t info, int32_t glyph, uint32_t context);

int32_t kernel_font_truetype_shape_82d386(uint32_t info, int32_t glyph, uint32_t output)
{
    uint32_t data = D(info + 4u), vertices = 0, count = 0;
    int32_t offset = kernel_font_glyph_offset_82cf50(info, glyph);
    D(output) = 0;
    if (offset < 0) return 0;
    int32_t contours = (int16_t)kernel_font_s16_82be34(data + (uint32_t)offset);
    if (contours > 0) {
        uint32_t endpoints = data + (uint32_t)offset + 10u;
        uint32_t instructions = (uint16_t)kernel_font_u16_82be14(endpoints + (uint32_t)contours * 2u);
        uint32_t stream = endpoints + (uint32_t)contours * 2u + instructions + 2u;
        uint32_t points = (uint16_t)kernel_font_u16_82be14(endpoints + (uint32_t)contours * 2u - 2u) + 1u;
        uint32_t capacity = points + (uint32_t)contours * 2u;
        vertices = kernel_font_malloc_817590(capacity * 14u);
        if (!vertices) return 0;
        uint32_t point_base = capacity - points;
        uint8_t flags = 0, repeat = 0;
        for (uint32_t i = 0; i < points; ++i) {
            if (!repeat) {
                flags = B(stream++);
                if (flags & 8u) repeat = B(stream++);
            } else --repeat;
            B(vertices + (point_base + i) * 14u + 12u) = flags;
        }
        for (uint32_t axis = 0; axis < 2; ++axis) {
            uint32_t coordinate = 0;
            uint32_t short_mask = 2u << axis, same_mask = 16u << axis;
            for (uint32_t i = 0; i < points; ++i) {
                uint32_t point = vertices + (point_base + i) * 14u;
                flags = B(point + 12u);
                if (flags & short_mask) {
                    uint32_t delta = B(stream++);
                    coordinate = flags & same_mask ? coordinate + delta : coordinate - delta;
                } else if (!(flags & same_mask)) {
                    uint32_t delta = B(stream) * 256u + B(stream + 1u);
                    coordinate += (uint32_t)(int32_t)(int16_t)delta;
                    stream += 2u;
                }
                W(point + axis * 2u) = (uint16_t)coordinate;
            }
        }
        uint32_t next = 0, contour = 0;
        int32_t off = 0, start_off = 0, sx = 0, sy = 0, scx = 0, scy = 0, cx = 0, cy = 0;
        for (uint32_t i = 0; i < points; ++i) {
            uint32_t point = vertices + (point_base + i) * 14u;
            flags = B(point + 12u);
            int32_t x = (int16_t)W(point), y = (int16_t)W(point + 2u);
            if (i == next) {
                if (i) count = (uint32_t)kernel_font_close_shape_82d24a(vertices, count, off, start_off, sx, sy, scx, scy, cx, cy);
                start_off = !(flags & 1u);
                if (start_off) {
                    scx = x; scy = y;
                    if (!(B(point + 26u) & 1u)) {
                        sx = (x + (int16_t)W(point + 14u)) >> 1;
                        sy = (y + (int16_t)W(point + 16u)) >> 1;
                    } else {
                        sx = (int16_t)W(point + 14u);
                        sy = (int16_t)W(point + 16u);
                        ++i;
                    }
                } else { sx = x; sy = y; }
                kernel_font_vertex_82cf10(vertices + count++ * 14u, 1, sx, sy, 0, 0);
                off = 0;
                next = (uint16_t)kernel_font_u16_82be14(endpoints + contour++ * 2u) + 1u;
            } else if (!(flags & 1u)) {
                if (off) kernel_font_vertex_82cf10(vertices + count++ * 14u, 3, (x + cx) >> 1, (y + cy) >> 1, cx, cy);
                cx = x; cy = y; off = 1;
            } else {
                kernel_font_vertex_82cf10(vertices + count++ * 14u, off ? 3 : 2, x, y, off ? cx : 0, off ? cy : 0);
                off = 0;
            }
        }
        count = (uint32_t)kernel_font_close_shape_82d24a(vertices, count, off, start_off, sx, sy, scx, scy, cx, cy);
    } else if (contours < 0) {
        uint32_t stream = data + (uint32_t)offset + 10u, more = 1;
        while (more) {
            uint32_t component = 0;
            volatile float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, tx = 0.0f, ty = 0.0f;
            uint16_t flags = (uint16_t)kernel_font_s16_82be34(stream); stream += 2u;
            uint16_t index = (uint16_t)kernel_font_s16_82be34(stream); stream += 2u;
            if (flags & 2u) {
                if (flags & 1u) {
                    tx = (int16_t)kernel_font_s16_82be34(stream); stream += 2u;
                    ty = (int16_t)kernel_font_s16_82be34(stream); stream += 2u;
                } else {
                    tx = (int8_t)B(stream++);
                    ty = (int8_t)B(stream++);
                }
            }
            if (flags & 8u) {
                volatile float scale = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb);
                a = scale; d = scale; stream += 2u;
                b = 0.0f; c = 0.0f;
            } else if (flags & 64u) {
                a = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb); stream += 2u;
                b = 0.0f; c = 0.0f;
                d = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb); stream += 2u;
            } else if (flags & 128u) {
                a = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb); stream += 2u;
                b = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb); stream += 2u;
                c = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb); stream += 2u;
                d = (int16_t)kernel_font_s16_82be34(stream) / F(0x83cfcb); stream += 2u;
            }
            volatile float m = kernel_sqrt_81ab2a((double)a * a + (double)b * b);
            volatile float n = kernel_sqrt_81ab2a((double)c * c + (double)d * d);
            int32_t length = kernel_font_shape_82f90a(info, index, P(&component));
            if (length > 0) {
                for (uint32_t i = 0; (int32_t)i < length; ++i) {
                    uint32_t vertex = component + i * 14u;
                    int16_t x = (int16_t)W(vertex), y = (int16_t)W(vertex + 2u);
                    W(vertex) = (int16_t)(m * ((double)a * x + (double)c * y + tx));
                    W(vertex + 2u) = (int16_t)(n * ((double)b * x + (double)d * y + ty));
                    x = (int16_t)W(vertex + 4u); y = (int16_t)W(vertex + 6u);
                    W(vertex + 4u) = (int16_t)(m * ((double)a * x + (double)c * y + tx));
                    W(vertex + 6u) = (int16_t)(n * ((double)b * x + (double)d * y + ty));
                }
                uint32_t joined = kernel_font_malloc_817590((count + (uint32_t)length) * 14u);
                if (!joined) {
                    if (vertices) kernel_font_free_8175a4(vertices);
                    if (component) kernel_font_free_8175a4(component);
                    return 0;
                }
                /* Original passes the vertex byte size to REP MOVSD, which
                 * copies four times that size and can corrupt heap headers.
                 * Preserve the original call/count, including this defect. */
                if ((int32_t)count > 0 && vertices) kernel_font_copy_dwords_817510(joined, vertices, count * 14u);
                kernel_font_copy_dwords_817510(joined + count * 14u, component, (uint32_t)length * 14u);
                if (vertices) kernel_font_free_8175a4(vertices);
                vertices = joined;
                kernel_font_free_8175a4(component);
                count += (uint32_t)length;
            }
            more = flags & 32u;
        }
    }
    D(output) = vertices;
    return (int32_t)count;
}

int32_t kernel_font_cff_shape_82f756(uint32_t info, int32_t glyph, uint32_t output)
{
    uint32_t bounds[12] = {1}, context[12] = {0};
    if (kernel_font_charstring_82e6f0(info, glyph, P(bounds))) {
        D(output) = kernel_font_malloc_817590(bounds[11] * 14u);
        context[10] = D(output);
        if (kernel_font_charstring_82e6f0(info, glyph, P(context))) return (int32_t)context[11];
    }
    D(output) = 0;
    return 0;
}

int32_t kernel_font_shape_82f90a(uint32_t info, int32_t glyph, uint32_t output)
{
    return D(info + 64u) ? kernel_font_cff_shape_82f756(info, glyph, output) : kernel_font_truetype_shape_82d386(info, glyph, output);
}

int32_t kernel_font_codepoint_shape_82cede(uint32_t info, uint32_t codepoint, uint32_t output)
{
    int32_t glyph = kernel_font_glyph_index_82ca7c(info, codepoint);
    return kernel_font_shape_82f90a(info, glyph, output);
}
