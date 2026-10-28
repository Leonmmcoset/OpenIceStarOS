#ifndef OPENICESTAR_TEXT_RUN_H
#define OPENICESTAR_TEXT_RUN_H
#include <stdint.h>
#define TEXT_PARAMETERS uint32_t id,uint32_t x,uint32_t y,uint32_t foreground,uint32_t background,uint32_t text
uint32_t kernel_text_run8_81e7b2(TEXT_PARAMETERS);
uint32_t kernel_text_vertical8_81e87c(TEXT_PARAMETERS);
uint32_t kernel_text_run32_81e952(TEXT_PARAMETERS);
uint32_t kernel_text_vertical32_81ea1c(TEXT_PARAMETERS);
#undef TEXT_PARAMETERS
#endif
