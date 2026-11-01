#ifndef OPENICESTAR_KERNEL_PNG_H
#define OPENICESTAR_KERNEL_PNG_H
#include <stdint.h>

/* All pointers and object fields use the original 32-bit x86 ABI. */
uint32_t kernel_png_paeth_8393a1(uint32_t a,uint32_t b,uint32_t c);
void kernel_png_scanline_839459(volatile uint32_t p,volatile uint32_t out,
    volatile uint32_t in,volatile uint32_t previous,volatile uint32_t bytewidth,
    uint32_t filter,volatile uint32_t length);
void kernel_png_unfilter_8397dd(volatile uint32_t p,volatile uint32_t out,
    volatile uint32_t in,volatile uint32_t width,volatile uint32_t height,
    volatile uint32_t bpp);
void kernel_png_unpad_839893(volatile uint32_t out,volatile uint32_t in,
    volatile uint32_t linebits,volatile uint32_t paddedbits,volatile uint32_t height);
void kernel_png_postprocess_839991(volatile uint32_t p,volatile uint32_t out,
    volatile uint32_t in,volatile uint32_t info);
uint32_t kernel_png_format_839a9f(volatile uint32_t p);
void kernel_png_release_input_839be9(volatile uint32_t p);
uint32_t kernel_png_header_839c23(volatile uint32_t p);
uint32_t kernel_png_decode_839f0d(volatile uint32_t p);
uint32_t kernel_png_create_83a4c9(void);
uint32_t kernel_png_from_memory_83a579(volatile uint32_t input,volatile uint32_t size);
void kernel_png_destroy_83a5bb(volatile uint32_t p);
uint32_t kernel_png_error_83a5f1(uint32_t p);
uint32_t kernel_png_error_line_83a5fd(uint32_t p);
uint32_t kernel_png_width_83a609(uint32_t p);
uint32_t kernel_png_height_83a613(uint32_t p);
uint32_t kernel_png_bpp_83a61f(volatile uint32_t p);
uint32_t kernel_png_channels_83a649(volatile uint32_t p);
uint32_t kernel_png_depth_83a6a3(uint32_t p);
uint32_t kernel_png_padded_bpp_83a6af(volatile uint32_t p);
uint32_t kernel_png_pixel_format_83a6eb(uint32_t p);
uint32_t kernel_png_pixels_83a6f7(uint32_t p);
uint32_t kernel_png_size_83a703(uint32_t p);

#endif
