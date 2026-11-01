/* Original outline-to-polyline conversion: count pass, allocation, fill pass.
 * Keep float32 spill boundaries, the depth-17 drop, live aliases and the
 * original allocator. The edge builder and raster driver remain external. */
#include "font_flatten.h"
#include "math_extra.h"
typedef uint32_t raw32 __attribute__((may_alias));
#define D(a) (*(volatile raw32 *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define S(a) (*(volatile int16_t *)(uintptr_t)(uint32_t)(a))
typedef float raw_float __attribute__((may_alias));
#define F(a) (*(volatile raw_float *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
uint32_t kernel_font_malloc_817590(uint32_t size);
void kernel_font_free_8175a4(uint32_t address);

void kernel_font_point_831ba8(volatile uint32_t points, volatile uint32_t index,
    volatile uint32_t x_bits, volatile uint32_t y_bits)
{
    if (!points) return;
    D(points + index * 8u) = x_bits;
    D(points + index * 8u + 4u) = y_bits;
}

/* Float arguments occupy raw cdecl words. Copying an endpoint must not quiet
 * a signaling NaN; only the arithmetic spill boundaries perform FP stores. */
static __attribute__((always_inline)) inline uint32_t bits(float value)
{
    union { float value; uint32_t bits; } converted = {value};
    return converted.bits;
}
#define x0 F(P(&x0_bits))
#define y0 F(P(&y0_bits))
#define x1 F(P(&x1_bits))
#define y1 F(P(&y1_bits))
#define x2 F(P(&x2_bits))
#define y2 F(P(&y2_bits))
#define x3 F(P(&x3_bits))
#define y3 F(P(&y3_bits))
#define flatness_squared F(P(&flatness_bits))

static __attribute__((always_inline)) inline float spill(float value)
{ volatile float rounded = value; return rounded; }

static __attribute__((noinline)) void kernel_font_curve_endpoint(
    uint32_t points, uint32_t count_address, uint32_t x_bits, uint32_t y_bits)
{
    kernel_font_point_831ba8(points, D(D(count_address)), x_bits, y_bits);
    D(D(count_address)) += 1u;
}

/* Avoid GCC's temporary-expression substitution here: retaining these
 * temporaries both respects the spill boundaries and fits the original slot. */
__attribute__((optimize("no-tree-ter")))
int32_t kernel_font_quadratic_831bdc(volatile uint32_t points, volatile uint32_t count,
    volatile uint32_t x0_bits, volatile uint32_t y0_bits, volatile uint32_t x1_bits, volatile uint32_t y1_bits,
    volatile uint32_t x2_bits, volatile uint32_t y2_bits, volatile uint32_t flatness_bits, volatile int32_t depth)
{
    float mx = spill(((long double)x1 * 2 + x0 + x2) / F(0x83d067));
    float my = spill(((long double)y1 * 2 + y0 + y2) / F(0x83d067));
    float dx = spill((x0 + x2) / F(0x83d06b) - mx);
    float dy = spill((y0 + y2) / F(0x83d06b) - my);
    if (depth > 16) return 1;
    if (__builtin_isgreater((long double)dx * dx + (long double)dy * dy, flatness_squared)) {
        kernel_font_quadratic_831bdc(points, count, x0_bits, y0_bits,
            bits((x0 + x1) / F(0x83d06b)), bits((y0 + y1) / F(0x83d06b)), D(P(&mx)), D(P(&my)),
            flatness_bits, (int32_t)((uint32_t)depth + 1u));
        kernel_font_quadratic_831bdc(points, count, D(P(&mx)), D(P(&my)),
            bits((x1 + x2) / F(0x83d06b)), bits((y1 + y2) / F(0x83d06b)), x2_bits, y2_bits,
            flatness_bits, (int32_t)((uint32_t)depth + 1u));
    } else {
        kernel_font_curve_endpoint(points, P(&count), x2_bits, y2_bits);
    }
    return 1;
}

static __attribute__((noinline)) float kernel_font_cubic_error(
    volatile uint32_t x0_bits, volatile uint32_t y0_bits, volatile uint32_t x1_bits, volatile uint32_t y1_bits,
    volatile uint32_t x2_bits, volatile uint32_t y2_bits, volatile uint32_t x3_bits, volatile uint32_t y3_bits)
{
    float dx0 = spill(x1 - x0), dy0 = spill(y1 - y0);
    float dx1 = spill(x2 - x1), dy1 = spill(y2 - y1);
    float dx2 = spill(x3 - x2), dy2 = spill(y3 - y2);
    float dx = spill(x3 - x0), dy = spill(y3 - y0);
    volatile double first __attribute__((aligned(4))) = kernel_sqrt_81ab2a((double)dx0 * dx0 + (double)dy0 * dy0);
    volatile double first_two __attribute__((aligned(4))) = kernel_sqrt_81ab2a((double)dx1 * dx1 + (double)dy1 * dy1) + first;
    float length = spill(kernel_sqrt_81ab2a((double)dx2 * dx2 + (double)dy2 * dy2) + first_two);
    float chord = spill(kernel_sqrt_81ab2a((double)dx * dx + (double)dy * dy));
    return spill((long double)length * length - (long double)chord * chord);
}

void kernel_font_cubic_831d64(volatile uint32_t points, volatile uint32_t count,
    volatile uint32_t x0_bits, volatile uint32_t y0_bits, volatile uint32_t x1_bits, volatile uint32_t y1_bits,
    volatile uint32_t x2_bits, volatile uint32_t y2_bits, volatile uint32_t x3_bits, volatile uint32_t y3_bits,
    volatile uint32_t flatness_bits, volatile int32_t depth)
{
    float error = kernel_font_cubic_error(x0_bits, y0_bits, x1_bits, y1_bits, x2_bits, y2_bits, x3_bits, y3_bits);
    if (depth > 16) return;
    if (__builtin_isgreater(error, flatness_squared)) {
        volatile float x01 = (x0 + x1) / F(0x83d06f), y01 = (y0 + y1) / F(0x83d06f);
        volatile float x12 = (x1 + x2) / F(0x83d06f), y12 = (y1 + y2) / F(0x83d06f);
        volatile float x23 = (x2 + x3) / F(0x83d06f), y23 = (y2 + y3) / F(0x83d06f);
        volatile float xa = (x01 + x12) / F(0x83d06f), ya = (y01 + y12) / F(0x83d06f);
        volatile float xb = (x12 + x23) / F(0x83d06f), yb = (y12 + y23) / F(0x83d06f);
        volatile float mx = (xa + xb) / F(0x83d06f), my = (ya + yb) / F(0x83d06f);
        kernel_font_cubic_831d64(points, count, x0_bits, y0_bits, D(P(&x01)), D(P(&y01)), D(P(&xa)), D(P(&ya)), D(P(&mx)), D(P(&my)),
            flatness_bits, (int32_t)((uint32_t)depth + 1u));
        kernel_font_cubic_831d64(points, count, D(P(&mx)), D(P(&my)), D(P(&xb)), D(P(&yb)), D(P(&x23)), D(P(&y23)), x3_bits, y3_bits,
            flatness_bits, (int32_t)((uint32_t)depth + 1u));
    } else {
        kernel_font_curve_endpoint(points, P(&count), x3_bits, y3_bits);
    }
}

#undef flatness_squared

uint32_t kernel_font_flatten_832000(volatile uint32_t vertices, volatile int32_t count,
    float flatness, volatile uint32_t lengths, volatile uint32_t contours, uint32_t userdata)
{
    (void)userdata;
    uint32_t points = 0, point_count = 0, contour_count = 0, contour_start = 0;
    float flatness_squared = flatness * flatness;
    for (int32_t i = 0; i < count; ++i)
        if (B(vertices + (uint32_t)i * 14u + 12u) == 1) ++contour_count;
    D(contours) = contour_count;
    if (!contour_count) return 0;
    D(lengths) = kernel_font_malloc_817590(contour_count * 4u);
    if (!D(lengths)) { D(contours) = 0; return 0; }
    for (uint32_t pass = 0; pass < 2; ++pass) {
        float x = 0, y = 0;
        if (pass == 1) {
            points = kernel_font_malloc_817590(point_count * 8u);
            if (!points) {
                kernel_font_free_8175a4(points);
                kernel_font_free_8175a4(D(lengths));
                D(lengths) = 0;
                D(contours) = 0;
                return 0;
            }
        }
        point_count = 0;
        contour_count = 0xffffffffu;
        for (int32_t i = 0; i < count; ++i) {
            uint32_t vertex = vertices + (uint32_t)i * 14u;
            switch (B(vertex + 12u)) {
            case 1:
                if ((int32_t)contour_count >= 0)
                    D(D(lengths) + contour_count * 4u) = point_count - contour_start;
                ++contour_count;
                contour_start = point_count;
                x = S(vertex); y = S(vertex + 2u);
                kernel_font_point_831ba8(points, point_count++, D(P(&x)), D(P(&y)));
                break;
            case 2:
                x = S(vertex); y = S(vertex + 2u);
                kernel_font_point_831ba8(points, point_count++, D(P(&x)), D(P(&y)));
                break;
            case 3:
                kernel_font_quadratic_831bdc(points, P(&point_count), D(P(&x)), D(P(&y)),
                    bits(S(vertex + 4u)), bits(S(vertex + 6u)), bits(S(vertex)), bits(S(vertex + 2u)), D(P(&flatness_squared)), 0);
                x = S(vertex); y = S(vertex + 2u);
                break;
            case 4:
                kernel_font_cubic_831d64(points, P(&point_count), D(P(&x)), D(P(&y)),
                    bits(S(vertex + 4u)), bits(S(vertex + 6u)), bits(S(vertex + 8u)), bits(S(vertex + 10u)),
                    bits(S(vertex)), bits(S(vertex + 2u)), D(P(&flatness_squared)), 0);
                x = S(vertex); y = S(vertex + 2u);
                break;
            }
        }
        D(D(lengths) + contour_count * 4u) = point_count - contour_start;
    }
    return points;
}
