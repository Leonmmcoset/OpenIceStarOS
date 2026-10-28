/* Recovered SDK formatter at CLOCK.X +0x3686..0x3b8c. This is the
 * original single-digit width grammar, rather than ISO printf. */
#include "legacy_clock_api.h"
#include <stdarg.h>

void legacy_format(volatile uint32_t surface, volatile uint32_t x, volatile uint32_t y,
                   volatile uint32_t fore, volatile uint32_t back, const char *volatile format, ...)
{
    va_list ap;
    va_start(ap, format);
    volatile uint32_t *args = (volatile uint32_t *)ap;
    const volatile unsigned char *text = (const volatile unsigned char *)format;
    uint32_t i = 0;
    while (text[i]) {
        if (text[i] == '%') {
            int32_t width = 0;
            uint32_t advance;
            if ((int8_t)text[i+1] > '1' && (int8_t)text[i+1] <= '9' &&
                (text[i+2] == 'd' || text[i+2] == 'D' ||
                 text[i+2] == 'x' || text[i+2] == 'X')) {
                width = (int8_t)text[i+1] - '0';
                ++i;
            }
            if (text[i+1] == 'c' || text[i+1] == 'C') {
                advance = legacy_character(surface,x,y,fore,back,(int8_t)*args);
                ++i; ++args;
            } else if (text[i+1] == 'd' || text[i+1] == 'D') {
                uint32_t value = *args;
                if ((int32_t)value < 0) {
                    value = 0u - value;
                    legacy_character(surface,x,y,fore,back,'-');
                    x += 8;
                }
                int32_t digits = (int32_t)legacy_digit_count(10,value);
                while (width > digits) {
                    legacy_character(surface,x,y,fore,back,'0');
                    x += 8; --width;
                }
                advance = legacy_number(surface,x,y,fore,back,10,value);
                ++i; ++args;
            } else if (text[i+1] == 'u' || text[i+1] == 'U' ||
                       text[i+1] == 'x' || text[i+1] == 'X') {
                uint32_t base = (text[i+1] == 'u' || text[i+1] == 'U') ? 10 : 16;
                int32_t digits = (int32_t)legacy_digit_count(base,*args);
                while (width > digits) {
                    legacy_character(surface,x,y,fore,back,'0');
                    x += 8; --width;
                }
                advance = legacy_number(surface,x,y,fore,back,base,*args);
                ++i; ++args;
            } else if (text[i+1] == 's' || text[i+1] == 'S') {
                advance = legacy_string(surface,x,y,fore,back,(const char *)(uintptr_t)*args);
                ++i; ++args;
            } else {
                advance = legacy_character(surface,x,y,fore,back,(int8_t)text[i]);
            }
            x += advance * 8u;
        } else if (text[i] == '\r') {
            x = 0;
        } else if (text[i] == '\n') {
            y += 16;
        } else if (text[i] <= 0x9f) {
            legacy_character(surface,x,y,fore,back,(int8_t)text[i]);
            x += 8;
        } else {
            legacy_chinese(surface,x,y,fore,back,(const char *)(text+i));
            x += 16; ++i;
        }
        ++i;
    }
    va_end(ap);
}
