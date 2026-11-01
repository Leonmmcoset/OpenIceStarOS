#ifndef OPENICESTAR_KERNEL_INFLATE_H
#define OPENICESTAR_KERNEL_INFLATE_H
#include <stdint.h>

uint32_t kernel_png_bit_8380ef(volatile uint32_t position,volatile uint32_t input);
uint32_t kernel_png_bits_838123(volatile uint32_t position,volatile uint32_t input,
    volatile uint32_t count);
void kernel_png_tree_init_83816d(volatile uint32_t tree,volatile uint32_t nodes,
    volatile uint32_t symbols,volatile uint32_t maxbits);
void kernel_png_tree_build_83818d(volatile uint32_t p,volatile uint32_t tree,
    volatile uint32_t lengths);
uint32_t kernel_png_symbol_838519(volatile uint32_t p,volatile uint32_t input,
    volatile uint32_t position,volatile uint32_t tree,volatile uint32_t size);
void kernel_png_dynamic_8385df(volatile uint32_t p,volatile uint32_t lit,
    volatile uint32_t dist,volatile uint32_t code,volatile uint32_t input,
    volatile uint32_t position,volatile uint32_t size);
void kernel_png_compressed_838bc3(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t position,
    volatile uint32_t written,volatile uint32_t size,volatile uint32_t type);
void kernel_png_stored_838faf(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t position,
    volatile uint32_t written,volatile uint32_t size);
uint32_t kernel_png_inflate_8390ed(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t size,
    volatile uint32_t offset);
uint32_t kernel_png_zlib_839263(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t size);

#endif
