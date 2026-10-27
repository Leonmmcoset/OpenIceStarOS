/* Same-depth surface scaling, original interval 0x8194e4..0x819c40.
 * Keep the original integer thresholds (not conventional nearest-neighbor
 * rounding), live metadata/arguments, forward aliases and raw EAX returns. */
#include "surface_scale.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define OBJECT(n) WORD(0x500000u + 4u * (n))
#define FIELD(n, offset) WORD(OBJECT(n) + (offset))

static inline uint32_t divide(uint32_t numerator, uint32_t denominator)
{
    uint32_t quotient, remainder;
    /* Preserve x86 #DE for zero divisors and INT_MIN/-1. A guarded C
     * division would change observable behavior, even for rejected depths. */
    __asm__ volatile("cltd; idivl %2" : "=&a"(quotient), "=&d"(remainder)
        : "rm"(denominator), "0"(numerator) : "cc");
    return quotient;
}

__attribute__((used, noinline))
static uint32_t surface_scale_body(volatile uint32_t *a)
{
    uint32_t rx = 0, ry = 0, difference, object, size, row = 0, sy = 0, vc = 1;
    if ((int32_t)FIELD(a[1], 12) > (int32_t)a[4]) {
        difference = FIELD(a[1], 12) - a[4];
        rx = divide(difference * 100u, a[4]);
    } else if ((int32_t)FIELD(a[1], 12) < (int32_t)a[4]) {
        difference = a[4] - FIELD(a[1], 12);
        rx = divide(FIELD(a[1], 12) * 100u, difference);
    }
    if ((int32_t)FIELD(a[1], 16) > (int32_t)a[5]) {
        difference = FIELD(a[1], 16) - a[5];
        ry = divide(difference * 100u, a[5]);
    } else if ((int32_t)FIELD(a[1], 16) < (int32_t)a[5]) {
        difference = a[5] - FIELD(a[1], 16);
        ry = divide(FIELD(a[1], 16) * 100u, difference);
    }
    if (FIELD(a[0], 36) == 32 && FIELD(a[1], 36) == 32) size = 4;
    else {
        object = OBJECT(a[0]);
        if (WORD(object + 36) != 8) return object;
        object = OBJECT(a[1]);
        if (WORD(object + 36) != 8) return object;
        size = 1;
    }
    while ((int32_t)row < (int32_t)a[5]) {
        uint32_t hc = 1, col = 0, sx = 0, dest, source, offset;
        if ((int32_t)FIELD(a[1], 16) <= (int32_t)sy)
            sy = FIELD(a[1], 16) - 1u;
        object = OBJECT(a[0]);
        offset = (a[3] + row) * FIELD(a[0], 12) + a[2];
        dest = WORD(object) + size * offset;
        object = OBJECT(a[1]);
        offset = FIELD(a[1], 12) * sy;
        source = WORD(object) + size * offset;
        while ((int32_t)col < (int32_t)a[4]) {
            if ((int32_t)FIELD(a[1], 12) <= (int32_t)sx)
                sx = FIELD(a[1], 12) - 1u;
            if (size == 4) WORD(dest + 4u * col) = WORD(source + 4u * sx) & 0xffffffu;
            else BYTE(dest + col) = BYTE(source + sx);
            if ((int32_t)FIELD(a[1], 12) > (int32_t)a[4]) {
                while ((int32_t)(hc * 100u) < (int32_t)(col * rx) &&
                    (int32_t)(FIELD(a[1], 12) - 1u) > (int32_t)sx) {
                    ++sx; ++hc;
                }
            } else if ((int32_t)FIELD(a[1], 12) < (int32_t)a[4]) {
                while ((int32_t)((col + 1u) * 100u) >= (int32_t)(hc * rx) &&
                    (int32_t)(col + 1u) < (int32_t)a[4]) {
                    if (size == 4)
                        WORD(dest + 4u * (col + 1u)) = WORD(source + 4u * sx) & 0xffffffu;
                    else BYTE(dest + col + 1u) = BYTE(source + sx);
                    ++col; ++hc;
                }
            }
            ++col; ++sx;
        }
        if ((int32_t)FIELD(a[1], 16) > (int32_t)a[5]) {
            while ((int32_t)(vc * 100u) < (int32_t)(row * ry) &&
                (int32_t)(FIELD(a[1], 16) - 1u) > (int32_t)sy) {
                ++sy; ++vc;
            }
        } else if ((int32_t)FIELD(a[1], 16) < (int32_t)a[5]) {
            /* Deliberately one adjustment per row, unlike horizontal fill. */
            if ((int32_t)((row + 1u) * 100u) >= (int32_t)(vc * ry) &&
                (int32_t)(row + 1u) < (int32_t)a[5]) {
                --sy; ++vc;
            }
        }
        ++row; ++sy;
    }
    return row;
}

/* Pixel output can overwrite the caller's six words. The adapter exposes
 * that original storage instead of a compiler-created argument copy. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked, noinline))
uint32_t kernel_surface_scale(uint32_t destination, uint32_t source,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;call surface_scale_body;"
        "leal 4(%esp),%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
