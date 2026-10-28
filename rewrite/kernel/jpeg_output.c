/* Nearest-neighbour component expansion and legacy YCbCr output. Mode 4
 * writes B/G/R while retaining the fourth byte. Mode 2 deliberately keeps
 * reading the same component sample within a row, as the original does. */
#include "jpeg_output.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_jpeg_expand_83b96d(volatile uint32_t source,volatile uint32_t destination,
                                 volatile uint32_t stride,volatile uint32_t x0,
                                 volatile uint32_t y0,volatile uint32_t x1,volatile uint32_t y1)
{
    uint32_t width=x1-x0,height=y1-y0;
    destination+=(y0*stride+x0)*4u;
    stride-=width;
    for(uint32_t y=0;(int32_t)y<(int32_t)height;++y){
        uint32_t row=(uint32_t)((int32_t)(y*8u)/(int32_t)height)*8u;
        for(uint32_t x=0;(int32_t)x<(int32_t)width;++x){
            uint32_t offset=(uint32_t)((int32_t)(x*8u)/(int32_t)width)+row;
            DWORD(destination)=DWORD(source+offset*4u);
            destination+=4u;
        }
        destination+=stride*4u;
    }
    return (int32_t)height>0?height:0;
}
static inline uint32_t limit(uint32_t v)
{return v&0xffffff00u?(uint32_t)((int32_t)~v>>24u):v;}
uint32_t kernel_jpeg_color_83bc15(volatile uint32_t context,volatile uint32_t column,
                                volatile uint32_t row,volatile uint32_t output,volatile uint32_t mode)
{
    uint32_t stride=DWORD(context+8u),height=DWORD(context+12u);
    uint32_t x=DWORD(context+16u)*column*8u,y=DWORD(context+20u)*row*8u;
    uint32_t width=DWORD(context)-x,lines=DWORD(context+4u)-y;
    if((int32_t)width>(int32_t)stride)width=stride;
    if((int32_t)lines>(int32_t)height)lines=height;
    uint32_t source=context+0xa8u;
    output+=(DWORD(context+0x5bf0u)*y+x)*(mode&127u);
    uint32_t skip=(DWORD(context+0x5bf0u)-width)*(mode&127u);
    for(uint32_t j=0;(int32_t)j<(int32_t)lines;++j){
        for(uint32_t i=0;(int32_t)i<(int32_t)width;++i){
            uint32_t luma=DWORD(source)*4096u,cr=DWORD(source+0x2000u);
            uint32_t blue=limit((uint32_t)((int32_t)(luma-cr*4u+DWORD(source+0x1000u)*7257u)>>12u)+128u);
            uint32_t green=limit((uint32_t)((int32_t)(luma-cr*2924u)>>12u)+128u);
            uint32_t red=limit((uint32_t)((int32_t)(luma+cr*5742u)>>12u)+128u);
            if(mode==4u){
                BYTE(output)=(uint8_t)blue;
                BYTE(output+1u)=(uint8_t)green;
                BYTE(output+2u)=(uint8_t)red;
                source+=4u;output+=4u;
            }else if(mode==2u){
                WORD(output)=(uint16_t)(((red&255u)>>3u)<<11u|((green&255u)>>2u)<<5u|((blue&255u)>>3u));
                output+=2u;
            }
        }
        source+=(stride-width)*4u;
        output+=skip;
    }
    return 0;
}
