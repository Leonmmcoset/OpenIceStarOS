/* Float-to-decimal helper at 0x81a634..0x81a9ee, shared-table slot 254.
 * Keep original low-DWORD truncations, reversed fraction digits and padding
 * order. It always writes '.', including zero/negative precisions, and does
 * not append a NUL. Incoming output/width/precision/flags remain live. */
#include "math_format.h"
#include "math_series.h"
#include "math_extra.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define TEN (*(volatile double *)(uintptr_t)0x83cf3b)

uint32_t kernel_format_float_81a634(volatile uint32_t output, double value,
    volatile int32_t width, volatile int32_t precision, volatile uint32_t flags)
{
    volatile double saved = value;
    uint32_t digits = flags & 64 ? 0x83cf13 : 0x83ceeb;
    uint8_t pad = flags & 1 ? '0' : ' ', sign = 0;
    if ((flags & 128) && __builtin_isless(saved, 0.0)) {
        sign = '-';
        union { double value; uint32_t words[2]; } bits = {saved};
        bits.words[1] ^= 0x80000000;
        saved = bits.value;
    } else if (flags & 8) sign = '+';
    else if (flags & 16) sign = ' ';
    if (sign) width = (int32_t)((uint32_t)width - 1u);

    uint8_t integer_digits[128], fraction_digits[352];
    int32_t integer_count = 0, fraction_count = 0;
    /* The original FISTPLL is signed 64-bit; only its low DWORD is kept.
       A volatile intermediate prevents replacement by a 32-bit FISTP. */
    volatile int64_t integer_raw = (int64_t)saved;
    uint32_t integer = (uint32_t)integer_raw;
    volatile double fraction = saved - (double)integer;
    double factor = kernel_power_81a538(TEN, (double)precision);
    volatile double scaled = factor * fraction;
    double rounded = kernel_round_81a4fc(scaled);
    volatile int64_t fraction_raw = (int64_t)rounded;
    uint32_t fractional = (uint32_t)fraction_raw;

    if (saved == 0.0 || integer == 0) integer_digits[integer_count++] = '0';
    else while (integer) {
        integer_digits[integer_count++] = BYTE(digits + integer % 10u);
        integer /= 10u;
    }
    while (fractional) {
        fraction_digits[fraction_count++] = BYTE(digits + fractional % 10u);
        fractional /= 10u;
    }
    width = (int32_t)((uint32_t)width - ((uint32_t)integer_count + (uint32_t)precision) - 1u);
    if (!(flags & 2)) {
        for (;;) {
            int32_t previous = width;
            width = (int32_t)((uint32_t)width - 1u);
            if (previous <= 0) break;
            BYTE(output) = pad;
            ++output;
        }
    }
    if (sign) { BYTE(output) = sign; ++output; }
    while (integer_count > 0) {
        BYTE(output) = integer_digits[integer_count - 1];
        ++output; --integer_count;
    }
    BYTE(output) = '.'; ++output;
    int32_t original_fraction_count = fraction_count;
    for (int32_t i=0; i<precision; ++i) {
        int32_t previous = fraction_count--;
        if (previous <= 0) break;
        BYTE(output) = fraction_digits[fraction_count];
        ++output;
    }
    while (original_fraction_count < precision) {
        ++original_fraction_count;
        BYTE(output) = '0'; ++output;
    }
    for (;;) {
        int32_t previous = width;
        width = (int32_t)((uint32_t)width - 1u);
        if (previous <= 0) break;
        BYTE(output) = ' '; ++output;
    }
    return output;
}
