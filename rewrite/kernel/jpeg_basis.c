/* Build the 8x8x8x8 fixed-point inverse-cosine basis at 0x83b41b.
 * The zero-frequency row uses cos(pi/4). Products are shifted arithmetically
 * by fifteen without rounding. Raw EAX retains the address of the original
 * outer loop's stack counter (twelve bytes below its saved-frame pointer).
 * Destination is reloaded for each store, as in the original stack argument. */
#include "jpeg_basis.h"
static const int32_t cosine[32]={
    32768,32138,30274,27246,23170,18205,12540,6393,
    0,-6393,-12540,-18205,-23170,-27246,-30274,-32138,
    -32768,-32138,-30274,-27246,-23170,-18205,-12540,-6393,
    0,6393,12540,18205,23170,27246,30274,32138
};
uint32_t kernel_jpeg_basis_83b41b(volatile uint32_t destination)
{
    int32_t value=0;
    for(uint32_t u=0;u<8u;++u)for(uint32_t v=0;v<8u;++v)
        for(uint32_t x=0;x<8u;++x)for(uint32_t y=0;y<8u;++y){
            uint32_t a=u?((2u*x+1u)*u)&31u:4u;
            uint32_t b=v?((2u*y+1u)*v)&31u:4u;
            value=(cosine[a]*cosine[b])>>15u;
            uint32_t offset=(((u*8u+v)*8u+x)*8u+y)*4u;
            *(volatile uint32_t *)(uintptr_t)(destination+offset)=(uint32_t)value;
        }
    return (uint32_t)(uintptr_t)__builtin_frame_address(0)-12u;
}
