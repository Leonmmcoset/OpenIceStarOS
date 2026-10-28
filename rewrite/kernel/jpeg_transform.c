/* Inverse transform accumulation and JPEG signed-magnitude extension.
 * Unsigned products/sums preserve the original low 32 bits before SAR.
 * Source, destination and basis can overlap; no restrict qualifiers or
 * cached destination argument may change the original read/write ordering. */
#include "jpeg_transform.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_jpeg_transform_83b653(volatile uint32_t coefficients,
                                    volatile uint32_t output,volatile uint32_t basis)
{
    for(uint32_t i=0;i<64u;++i)DWORD(output+i*4u)=0;
    for(uint32_t i=0;i<64u;++i){
        uint32_t coefficient=DWORD(coefficients+i*4u);
        if(coefficient)for(uint32_t j=0;j<64u;++j)
            DWORD(output+j*4u)=DWORD(basis+(i*64u+j)*4u)*coefficient+DWORD(output+j*4u);
    }
    for(uint32_t i=0;i<64u;++i)
        DWORD(output+i*4u)=(uint32_t)((int32_t)DWORD(output+i*4u)>>17u);
    return (uint32_t)(uintptr_t)__builtin_frame_address(0)-16u;
}
uint32_t kernel_jpeg_magnitude_83b73f(volatile uint32_t context,volatile uint32_t count)
{
    if(!count)return 0;
    typedef uint32_t (*reader)(uint32_t,uint32_t);
    uint32_t value=((reader)0x83a835u)(context,count)&0xffffu;
    if(!((value>>((count-1u)&31u))&1u))value=value-(1u<<(count&31u))+1u;
    return value;
}
