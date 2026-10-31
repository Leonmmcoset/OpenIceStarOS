/* CFF Type-2 drawing context, recovered from original x86. The interpreter
 * still calls these at their original addresses. Bound mode includes curve
 * control points; output mode stores 14-byte vertices and a wrapping count. */
#include "font_context.h"
#include "font_glyph.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define F(a) (*(volatile float *)(uintptr_t)(uint32_t)(a))

void kernel_font_context_bounds_82e05c(uint32_t context, int32_t x, int32_t y)
{
    if ((int32_t)D(context + 28u) < x || !D(context + 4u)) D(context + 28u) = (uint32_t)x;
    if ((int32_t)D(context + 36u) < y || !D(context + 4u)) D(context + 36u) = (uint32_t)y;
    if ((int32_t)D(context + 24u) > x || !D(context + 4u)) D(context + 24u) = (uint32_t)x;
    if ((int32_t)D(context + 32u) > y || !D(context + 4u)) D(context + 32u) = (uint32_t)y;
    D(context + 4u) = 1;
}

void kernel_font_context_vertex_82e0e0(uint32_t context, uint32_t type, int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t cx1, int32_t cy1)
{
    type = (uint8_t)type;
    if (D(context)) {
        kernel_font_context_bounds_82e05c(context, x, y);
        if (type == 4) {
            kernel_font_context_bounds_82e05c(context, cx, cy);
            kernel_font_context_bounds_82e05c(context, cx1, cy1);
        }
    } else {
        kernel_font_vertex_82cf10(D(context + 40u) + D(context + 44u) * 14u, type, x, y, cx, cy);
        W(D(context + 40u) + D(context + 44u) * 14u + 8u) = (uint16_t)cx1;
        W(D(context + 40u) + D(context + 44u) * 14u + 10u) = (uint16_t)cy1;
    }
    D(context + 44u) += 1u;
}

void kernel_font_context_close_82e1d6(uint32_t context)
{
    if (F(context + 8u) == F(context + 16u) && F(context + 12u) == F(context + 20u)) return;
    int32_t y = (int32_t)F(context + 12u);
    int32_t x = (int32_t)F(context + 8u);
    kernel_font_context_vertex_82e0e0(context, 2, x, y, 0, 0, 0, 0);
}

void kernel_font_context_move_82e27c(uint32_t context, float dx, float dy)
{
    kernel_font_context_close_82e1d6(context);
    volatile float x = F(context + 16u) + dx;
    F(context + 16u) = x;
    F(context + 8u) = x;
    volatile float y = F(context + 20u) + dy;
    F(context + 20u) = y;
    F(context + 12u) = y;
    int32_t iy = (int32_t)F(context + 20u);
    int32_t ix = (int32_t)F(context + 16u);
    kernel_font_context_vertex_82e0e0(context, 1, ix, iy, 0, 0, 0, 0);
}

void kernel_font_context_line_82e334(uint32_t context, float dx, float dy)
{
    F(context + 16u) = F(context + 16u) + dx;
    F(context + 20u) = F(context + 20u) + dy;
    int32_t y = (int32_t)F(context + 20u);
    int32_t x = (int32_t)F(context + 16u);
    kernel_font_context_vertex_82e0e0(context, 2, x, y, 0, 0, 0, 0);
}

void kernel_font_context_curve_82e3c8(uint32_t context, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
    volatile float x1 = F(context + 16u) + dx1;
    volatile float y1 = F(context + 20u) + dy1;
    volatile float x2 = x1 + dx2;
    volatile float y2 = y1 + dy2;
    F(context + 16u) = x2 + dx3;
    F(context + 20u) = y2 + dy3;
    int32_t icy1 = (int32_t)y2;
    int32_t icx1 = (int32_t)x2;
    int32_t icy = (int32_t)y1;
    int32_t icx = (int32_t)x1;
    int32_t iy = (int32_t)F(context + 20u);
    int32_t ix = (int32_t)F(context + 16u);
    kernel_font_context_vertex_82e0e0(context, 4, ix, iy, icx, icy, icx1, icy1);
}
