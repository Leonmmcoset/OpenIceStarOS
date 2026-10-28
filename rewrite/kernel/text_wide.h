#ifndef OPENICESTAR_TEXT_WIDE_H
#define OPENICESTAR_TEXT_WIDE_H
#include <stdint.h>
uint32_t kernel_text_wide8_81e106(uint32_t id,uint32_t x,uint32_t y,
    uint32_t foreground,uint32_t background,uint32_t text);
uint32_t kernel_text_wide8_mask_81e2cc(uint32_t id,uint32_t x,uint32_t y,
    uint32_t foreground,uint32_t text);
uint32_t kernel_text_wide32_81e45a(uint32_t id,uint32_t x,uint32_t y,
    uint32_t foreground,uint32_t background,uint32_t text);
uint32_t kernel_text_wide32_mask_81e624(uint32_t id,uint32_t x,uint32_t y,
    uint32_t foreground,uint32_t text);
#endif
