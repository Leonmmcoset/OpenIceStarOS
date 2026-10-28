/* Public JPEG entry points, MCU component assembly and image traversal.
 * The original decoder intentionally ignores marker/block errors; dimensions,
 * sampling, division faults and buffer accesses remain unchecked. */
#include "jpeg_decode.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
typedef uint32_t (*call1)(uint32_t);
typedef uint32_t (*call2)(uint32_t,uint32_t);
typedef uint32_t (*call3)(uint32_t,uint32_t,uint32_t);
typedef uint32_t (*call4)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*call5)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*call7)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t kernel_jpeg_info_83a70f(volatile uint32_t workspace,volatile uint32_t info,
                               volatile uint32_t length,volatile uint32_t input)
{
    uint32_t context=workspace+512u;
    DWORD(context+0x5be0u)=input;
    DWORD(context+0x5be4u)=length+input;
    if(((call1)0x83af27u)(context)||!DWORD(context))return 0;
    DWORD(info)=2;
    DWORD(info+4u)=0;
    DWORD(info+8u)=DWORD(context);
    DWORD(info+12u)=DWORD(context+4u);
    return 1;
}
uint32_t kernel_jpeg_decode_83a7a1(volatile uint32_t workspace,volatile uint32_t length,
                                 volatile uint32_t input,volatile uint32_t mode,
                                 volatile uint32_t output,volatile uint32_t extra_bytes)
{
    uint32_t context=workspace+512u;
    DWORD(context+0x5be0u)=input;
    DWORD(context+0x5be4u)=length+input;
    ((call1)0x83b41bu)(context+0x5bf4u);
    ((call1)0x83af27u)(context);
    DWORD(context+0x5bf0u)=DWORD(context)+(uint32_t)((int32_t)extra_bytes/(int32_t)(mode&127u));
    ((call3)0x83be59u)(context,output,mode);
    return 0;
}
uint32_t kernel_jpeg_mcu_83ba4f(volatile uint32_t context,volatile uint32_t zigzag)
{
    uint32_t coefficients[64],spatial[64];
    for(uint32_t i=0;(int32_t)i<(int32_t)DWORD(context+0x58u);++i){
        uint32_t horizontal=DWORD(context+0x80u+i*4u),vertical=DWORD(context+0x8cu+i*4u);
        for(uint32_t y=0;(int32_t)y<(int32_t)vertical;++y)
            for(uint32_t x=0;(int32_t)x<(int32_t)horizontal;++x){
                ((call4)0x83b7abu)(context,i,(uint32_t)(uintptr_t)coefficients,zigzag);
                ((call3)0x83b653u)((uint32_t)(uintptr_t)coefficients,(uint32_t)(uintptr_t)spatial,context+0x5bf4u);
                uint32_t destination=context+0xa8u+i*4096u;
                /* The original evaluates the four signed divisions in this
                 * order before calling the seven-argument expander. */
                uint32_t y1=(uint32_t)((int32_t)((y+1u)*DWORD(context+12u))/(int32_t)vertical);
                uint32_t x1=(uint32_t)((int32_t)((x+1u)*DWORD(context+8u))/(int32_t)horizontal);
                uint32_t y0=(uint32_t)((int32_t)(y*DWORD(context+12u))/(int32_t)vertical);
                uint32_t x0=(uint32_t)((int32_t)(x*DWORD(context+8u))/(int32_t)horizontal);
                ((call7)0x83b96du)((uint32_t)(uintptr_t)spatial,destination,DWORD(context+8u),x0,y0,x1,y1);
            }
    }
    return 0;
}
static const uint8_t zigzag[64]={
    0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
    12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};
uint32_t kernel_jpeg_image_83be59(volatile uint32_t context,volatile uint32_t output,
                                volatile uint32_t mode)
{
    ((call1)0x83b233u)(context);
    uint32_t columns=(uint32_t)((int32_t)(DWORD(context+8u)+DWORD(context)-1u)/(int32_t)DWORD(context+8u));
    uint32_t rows=(uint32_t)((int32_t)(DWORD(context+12u)+DWORD(context+4u)-1u)/(int32_t)DWORD(context+12u));
    uint32_t count=0;
    for(uint32_t y=0;(int32_t)y<(int32_t)rows;++y)
        for(uint32_t x=0;(int32_t)x<(int32_t)columns;++x){
            ++count;
            ((call2)0x83ba4fu)(context,(uint32_t)(uintptr_t)zigzag);
            ((call5)0x83bc15u)(context,x,y,output,mode&0x7fffu);
            if((int32_t)DWORD(context+0xa4u)>0&&(int32_t)DWORD(context+0xa4u)<=(int32_t)count){
                DWORD(context+0x5becu)&=0xfffffff8u;
                DWORD(context+0x5becu)-=8u;
                DWORD(context+0x40b8u)=0;
                DWORD(context+0x40bcu)=0;
                DWORD(context+0x40c0u)=0;
                count=0;
            }
        }
    return (int32_t)rows>0?rows:0;
}
