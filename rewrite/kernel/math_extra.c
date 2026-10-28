/* Remaining scalar x87 services. Preserve the original 32-bit conversions,
 * twenty-term acos series, quiet comparisons and binary64 store boundaries.
 * Names describe the intended service; edge behavior is deliberately original.
 */
#include <stdint.h>
#include "math_extra.h"
#include "math_series.h"
#define CONSTANT(a) (*(volatile double *)(uintptr_t)(a))
#define NOINLINE __attribute__((noinline))

NOINLINE double kernel_acos_81a3f6(double value)
{
    volatile double saved = value;
    volatile double result;
    if (__builtin_isless(saved, CONSTANT(0x83cebb)) || __builtin_isgreater(saved, 1.0)) {
        volatile double zero = 0.0;
        result = zero / zero;
    } else {
        volatile double sum = 0.0;
        for (int32_t i=0; i<20; ++i) {
            volatile double numerator = kernel_factorial_81a2aa(2*i);
            volatile double four_power = kernel_power_81a538(CONSTANT(0x83cec3), (double)i);
            double factorial = kernel_factorial_81a2aa(i);
            double square = kernel_power_81a538(factorial, CONSTANT(0x83cecb));
            volatile double denominator = (square * four_power) * (2*i+1);
            volatile double ratio = numerator / denominator;
            double power = kernel_power_81a538(saved, (double)(2*i+1));
            volatile double term = power * ratio;
            sum = sum + term;
        }
        result = CONSTANT(0x83ced3) - sum;
    }
    return result;
}

NOINLINE double kernel_round_81a4fc(double value)
{
    volatile double saved = value;
    return (int32_t)(saved + CONSTANT(0x83cedb));
}

NOINLINE double kernel_absolute_81a604(double value)
{
    volatile double saved = value;
    volatile double result = __builtin_isless(saved, 0.0) ? -saved : saved;
    return result; /* -0.0 remains -0.0, unlike fabs. */
}

NOINLINE double kernel_floor_81a9ee(double value)
{
    volatile double saved = value;
    int32_t integral = (int32_t)saved;
    volatile double result;
    if (__builtin_isless(saved, 0.0) && integral != saved)
        result = (int32_t)((uint32_t)integral - 1u);
    else result = integral;
    return result;
}

NOINLINE double kernel_ceil_81aa52(double value)
{
    volatile double saved = value;
    int32_t integral = (int32_t)saved;
    volatile double result;
    if (__builtin_isgreater(saved, 0.0) && integral != saved)
        result = (int32_t)((uint32_t)integral + 1u);
    else result = integral;
    return result;
}

NOINLINE double kernel_remainder_81aab8(double value, double divisor)
{
    volatile double saved = value, denominator = divisor;
    volatile double result;
    if (denominator == 0.0) result = 0.0;
    else {
        int32_t integral = (int32_t)(saved / denominator);
        volatile double quotient = integral;
        result = saved - quotient * denominator;
    }
    return result;
}

NOINLINE double kernel_sqrt_81ab2a(double value)
{
    return __builtin_sqrt(value);
}
