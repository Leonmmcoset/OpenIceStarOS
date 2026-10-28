#ifndef OPENICESTAR_MATH_FORMAT_H
#define OPENICESTAR_MATH_FORMAT_H
#include <stdint.h>
/* Returns the address immediately after the final output byte; no NUL. */
uint32_t kernel_format_float_81a634(uint32_t output, double value,
                                   int32_t width, int32_t precision, uint32_t flags);
#endif
