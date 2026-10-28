#ifndef OPENICESTAR_KERNEL_JPEG_DECODE_H
#define OPENICESTAR_KERNEL_JPEG_DECODE_H
#include <stdint.h>
uint32_t kernel_jpeg_info_83a70f(uint32_t workspace,uint32_t info,uint32_t length,uint32_t input);
uint32_t kernel_jpeg_decode_83a7a1(uint32_t workspace,uint32_t length,uint32_t input,
                                uint32_t mode,uint32_t output,uint32_t extra_bytes);
uint32_t kernel_jpeg_mcu_83ba4f(uint32_t context,uint32_t zigzag);
uint32_t kernel_jpeg_image_83be59(uint32_t context,uint32_t output,uint32_t mode);
#endif
