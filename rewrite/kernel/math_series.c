/* Original x87 series and repeated-product helpers. These intentionally
 * retain finite iteration counts, binary64 stores and unusual fractional
 * exponent behavior. Do not replace with libm, FSIN or FCOS. */
#include "math_series.h"
#define NEGATIVE_ONE (*(volatile double *)(uintptr_t)0x83ceb3)
#define POWER_UNORDERED (*(volatile double *)(uintptr_t)0x83cee3)
#define NOINLINE __attribute__((noinline))

NOINLINE double kernel_factorial_81a2aa(volatile int32_t n)
{
    volatile double product = 1.0;
    for (uint32_t i=1; (int32_t)i<=n; ++i)
        product = (int32_t)i * product;
    return product;
}

NOINLINE double kernel_power_81a538(double base, double exponent)
{
    volatile double value = base, power = exponent;
    volatile double original = value;
    if (power == 0.0) return 1.0;
    if (__builtin_isgreater(power, 0.0)) {
        power = power - 1.0;
        while (__builtin_isgreater(power, 0.0)) {
            value = value * original;
            power = power - 1.0;
        }
        return value;
    }
    if (__builtin_isless(power, 0.0)) {
        power = power - 1.0;
        while (__builtin_isless(power, 0.0)) {
            value = value / original;
            power = power + 1.0;
        }
        return value;
    }
    return POWER_UNORDERED; /* Original NaN-exponent result is -1.0. */
}

NOINLINE double kernel_sine_81a2dc(double angle)
{
    volatile double saved = angle, sum = 0.0;
    for (int32_t i=0; i<10; ++i) {
        int32_t odd = 2*i+1;
        volatile double sign = kernel_power_81a538(NEGATIVE_ONE,(double)i);
        double power = kernel_power_81a538(saved,(double)odd);
        volatile double numerator = power * sign;
        double factorial = kernel_factorial_81a2aa(odd);
        long double term = (long double)numerator / factorial;
        sum = sum + term;
    }
    return sum;
}

NOINLINE double kernel_cosine_81a366(double angle)
{
    volatile double saved = angle, sum = 1.0, denominator = 1.0, power = 1.0;
    int32_t n = 2, sign = -1;
    for (int32_t i=0; i<10; ++i) {
        power = (saved * saved) * power;
        denominator = ((double)(n-1) * n) * denominator;
        sum = ((power / denominator) * sign) + sum;
        n += 2;
        sign = -sign;
    }
    return sum;
}
