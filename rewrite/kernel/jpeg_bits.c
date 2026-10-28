/* JPEG bit reservoir at 0x83a835..0x83a919. A 0xff byte skips the next
 * source byte without validating its value. Refill EOF retains consumed
 * source pointers but discards the local reservoir/count changes. */
#include "jpeg_bits.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_jpeg_bits_83a835(volatile uint32_t context,volatile uint32_t amount)
{
    uint32_t reservoir=DWORD(context+0x5be8u),bits=DWORD(context+0x5becu);
    while((int32_t)bits<=16){
        if(DWORD(context+0x5be0u)>=DWORD(context+0x5be4u))return 0;
        uint32_t source=DWORD(context+0x5be0u);
        ++DWORD(context+0x5be0u);
        uint32_t byte=BYTE(source);
        if(byte==255u){
            if(DWORD(context+0x5be0u)>=DWORD(context+0x5be4u))return 0;
            ++DWORD(context+0x5be0u);
        }
        reservoir=(reservoir<<8u)|byte;bits+=8u;
    }
    uint32_t result=(reservoir>>((bits-amount)&31u))&((1u<<(amount&31u))-1u);
    bits-=amount;
    DWORD(context+0x5becu)=bits;
    DWORD(context+0x5be8u)=reservoir;
    return (uint16_t)result;
}
