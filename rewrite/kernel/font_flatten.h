#ifndef OPENICESTAR_KERNEL_FONT_FLATTEN_H
#define OPENICESTAR_KERNEL_FONT_FLATTEN_H
#include <stdint.h>
/* Original cdecl float arguments are passed as their unmodified 32-bit words. */
void kernel_font_point_831ba8(uint32_t points, uint32_t index, uint32_t x_bits, uint32_t y_bits);
int32_t kernel_font_quadratic_831bdc(uint32_t points, uint32_t count, uint32_t x0_bits, uint32_t y0_bits,
    uint32_t x1_bits, uint32_t y1_bits, uint32_t x2_bits, uint32_t y2_bits, uint32_t flatness_bits, int32_t depth);
void kernel_font_cubic_831d64(uint32_t points, uint32_t count, uint32_t x0_bits, uint32_t y0_bits,
    uint32_t x1_bits, uint32_t y1_bits, uint32_t x2_bits, uint32_t y2_bits, uint32_t x3_bits, uint32_t y3_bits, uint32_t flatness_bits, int32_t depth);
uint32_t kernel_font_flatten_832000(uint32_t vertices, int32_t count, float flatness,
    uint32_t lengths, uint32_t contours, uint32_t userdata);
#endif
