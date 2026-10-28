/* Packed CMOS time/date display wrappers at 0x82ac53 and 0x82acdd.
 * Values are captured once and sent as individual decimal nibbles, including
 * non-BCD nibbles 10..15, to the original text renderer. */
#include "clock_display.h"
void kernel_format_text(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                        uint32_t e, const char *format, ...);

void kernel_draw_clock_time(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t value = *(volatile uint32_t *)0x1b004;
    kernel_format_text(a, b, c, d, e, (const char *)0x83cf6f,
        (value >> 20) & 15u, (value >> 16) & 15u, (value >> 12) & 15u,
        (value >> 8) & 15u, (value >> 4) & 15u, value & 15u);
}

void kernel_draw_clock_date(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t value = *(volatile uint32_t *)0x1b008;
    kernel_format_text(a, b, c, d, e, (const char *)0x83cf7e,
        (value >> 28) & 15u, (value >> 24) & 15u, (value >> 20) & 15u,
        (value >> 16) & 15u, (value >> 12) & 15u, (value >> 8) & 15u,
        (value >> 4) & 15u, value & 15u);
}
