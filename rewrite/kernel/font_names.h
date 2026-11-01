#ifndef OPENICESTAR_KERNEL_FONT_NAMES_H
#define OPENICESTAR_KERNEL_FONT_NAMES_H
#include <stdint.h>

int32_t kernel_font_name_prefix_8365b2(volatile uint32_t text,volatile int32_t length,
    volatile uint32_t utf16,volatile uint32_t bytes);
uint32_t kernel_font_name_equal_83689a(volatile uint32_t text,volatile int32_t length,
    volatile uint32_t utf16,volatile uint32_t bytes);
uint32_t kernel_font_name_string_8368d8(volatile uint32_t info,volatile uint32_t length,
    volatile uint32_t platform,volatile uint32_t encoding,volatile uint32_t language,
    volatile uint32_t name);
uint32_t kernel_font_name_pair_836a34(volatile uint32_t data,volatile uint32_t table,
    volatile uint32_t text,volatile int32_t length,volatile uint32_t first,volatile uint32_t second);
uint32_t kernel_font_match_836cc0(volatile uint32_t data,volatile uint32_t offset,
    volatile uint32_t text,volatile uint32_t flags);
int32_t kernel_font_find_name_836f08(volatile uint32_t data,volatile uint32_t name,volatile uint32_t flags);
int32_t kernel_font_find_name_api_83700c(uint32_t data,uint32_t name,uint32_t flags);
uint32_t kernel_font_name_equal_api_83702e(uint32_t text,int32_t length,uint32_t utf16,uint32_t bytes);

#endif
