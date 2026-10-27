/* Rectangle predicate 0x818d14 and framebuffer submission 0x818d5e/0x818f22.
 * Names describe call sites; the original predicate is not conventional
 * rectangle intersection, and its two OR expressions are intentional. */
#include "surface_present.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))

uint32_t kernel_surface_regions_test(uint32_t ax, uint32_t ay,
    uint32_t aw, uint32_t ah, uint32_t bx, uint32_t by,
    uint32_t bw, uint32_t bh)
{
    return ((int32_t)(aw + ax) > (int32_t)bx ||
            (int32_t)(ah + ay) > (int32_t)by) &&
           ((int32_t)(bw + bx) > (int32_t)ax ||
            (int32_t)(bh + by) > (int32_t)ay);
}

/* Preserve separate table reads, signed comparisons, wrapped endpoint
 * arithmetic and the original swaps for zero/negative-size rectangles.
 * Only x/y are changed in caller storage; width/height remain untouched. */
#define CAPTURE_AND_CLIP() \
    uint32_t output = WORD(WORD(0x500000)); \
    uint32_t input = WORD(0x1b070); \
    uint32_t pitch = WORD(WORD(0x500000) + 12); \
    uint32_t screen_height = WORD(WORD(0x500000) + 16); \
    uint32_t right, bottom, temporary, row; \
    if ((int32_t)a[0] < 0) a[0] = 0; \
    if ((int32_t)a[1] < 0) a[1] = 0; \
    if ((int32_t)a[0] >= (int32_t)pitch) return a[0]; \
    if ((int32_t)a[1] >= (int32_t)screen_height) return a[1]; \
    right = a[2]; right += a[0]; --right; \
    bottom = a[3]; bottom += a[1]; --bottom; \
    if ((int32_t)right >= (int32_t)pitch) right = pitch - 1; \
    if ((int32_t)bottom >= (int32_t)screen_height) bottom = screen_height - 1; \
    if ((int32_t)a[0] > (int32_t)right) { \
        temporary = a[0]; a[0] = right; right = temporary; \
    } \
    if ((int32_t)a[1] > (int32_t)bottom) { \
        temporary = a[1]; a[1] = bottom; bottom = temporary; \
    }

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint32_t surface_present8_body(volatile uint32_t *a)
{
    CAPTURE_AND_CLIP();
    uint32_t span = right - a[0] + 1;
    int32_t words = (int32_t)span >> 2;
    int32_t tail = (int32_t)span % 4;
    for (row = a[1]; (int32_t)row <= (int32_t)bottom; ++row) {
        uint32_t offset = row * pitch + a[0], i;
        for (i = 0; (int32_t)i < words; ++i)
            WORD(output + offset + 4 * i) = WORD(input + offset + 4 * i);
        for (i = 0; (int32_t)i < tail; ++i)
            BYTE(output + offset + 4 * (uint32_t)words + i) =
                BYTE(input + offset + 4 * (uint32_t)words + i);
    }
    return row;
}

__attribute__((used, noinline, optimize("no-optimize-sibling-calls")))
static uint32_t surface_present32_body(volatile uint32_t *a)
{
    CAPTURE_AND_CLIP();
    uint32_t count = right - a[0] + 1;
    typedef uint32_t (*Copy)(uint32_t, uint32_t, uint32_t);
    Copy copy = (Copy)(uintptr_t)WORD(WORD(0x1b0e4) + 68);
    for (row = a[1]; (int32_t)row <= (int32_t)bottom; ++row) {
        uint32_t offset = 4 * (row * pitch + a[0]);
        copy(output + offset, input + offset, count);
    }
    return row;
}
#undef CAPTURE_AND_CLIP

/* Give C the actual caller words, retaining observable clipping stores. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define ENTRY(name, body) \
    __attribute__((naked, noinline)) \
    uint32_t name(uint32_t x, uint32_t y, uint32_t width, uint32_t height) \
    { \
        __asm__("leal 4(%esp),%eax;pushl %eax;call " #body ";" \
            "leal 4(%esp),%esp;ret"); \
        __builtin_unreachable(); \
    }
ENTRY(kernel_surface_present8, surface_present8_body)
ENTRY(kernel_surface_present32, surface_present32_body)
#undef ENTRY
#pragma GCC diagnostic pop
