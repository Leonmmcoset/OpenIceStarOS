#ifndef ICESTAR_LEGACY_CLOCK_API_H
#define ICESTAR_LEGACY_CLOCK_API_H
#include <stdint.h>
uint32_t legacy_character(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
uint32_t legacy_chinese(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, const char *);
uint32_t legacy_string(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, const char *);
uint32_t legacy_number(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t legacy_thick_line(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t legacy_thick_circle(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t legacy_digit_count(uint32_t, uint32_t);
double legacy_sine(double);
double legacy_cosine(double);
void legacy_format(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, const char *, ...);
#endif
