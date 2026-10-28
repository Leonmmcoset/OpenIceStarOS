#ifndef OPENICESTAR_KERNEL_JPEG_OUTPUT_H
#define OPENICESTAR_KERNEL_JPEG_OUTPUT_H
#include <stdint.h>
uint32_t kernel_jpeg_expand_83b96d(uint32_t source,uint32_t destination,uint32_t stride,
                                uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1);
uint32_t kernel_jpeg_color_83bc15(uint32_t context,uint32_t column,uint32_t row,
                               uint32_t output,uint32_t mode);
#endif
