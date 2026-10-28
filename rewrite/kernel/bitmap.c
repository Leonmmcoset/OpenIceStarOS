/* BMP renderer at 0x81ffc4..0x82040c. Preserve tight legacy row strides,
 * low-nibble-first doubled 4-bit stores, live input/geometry and captured
 * header/palette identities. The 24-bit/32-bit path reads four bytes then
 * masks the fourth; it does not validate BMP signatures or compression.
 */
#include "bitmap.h"
#include "pixel_access.h"
#include "icon_pixels.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SURFACE DWORD(0x500000u+id*4u)
#define SOURCE8 (bitmap+DWORD(file+10u)+(DWORD(header+8u)-row-1u)*DWORD(header+4u)+column)
#define SOURCE24 (bitmap+DWORD(file+10u)+(DWORD(header+8u)-row-1u)*3u*DWORD(header+4u)+column*3u)
uint32_t kernel_bitmap_draw_81ffc4(volatile uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t bitmap)
{
    uint32_t result;
    /* Zero-instruction ABI binding: negative X leaves incoming EAX intact. */
    __asm__ volatile("":"=a"(result)::"memory");
    if((int32_t)x<0)return result;
    result=DWORD(SURFACE+12u);
    if((int32_t)result<=(int32_t)x || (int32_t)y<0)return result;
    result=DWORD(SURFACE+16u);
    if((int32_t)result<=(int32_t)y)return result;
    uint32_t file=bitmap,header=bitmap+14u,palette=bitmap+54u;
    (void)DWORD(0x1b0e0u);
    uint32_t remaining=DWORD(SURFACE+16u)-y,height;
    if(DWORD(header+8u)<remaining)height=DWORD(header+8u);
    else height=DWORD(SURFACE+16u)-y;
    remaining=DWORD(SURFACE+12u)-x;
    uint32_t width;
    if(DWORD(header+4u)<remaining)width=DWORD(header+4u);
    else width=DWORD(SURFACE+12u)-x;
    uint32_t surface=SURFACE,mode;
    if(DWORD(surface+36u)==32u){
        if(WORD(header+14u)==24u)mode=0;
        else if(WORD(header+14u)==8u)mode=1;
        else if(WORD(header+14u)==4u)mode=2;
        else return header;
    }else{
        surface=SURFACE;
        if(DWORD(surface+36u)!=8u)return surface;
        if(WORD(header+14u)!=24u)return header;
        mode=3;
    }
    uint32_t row;
    for(row=0;(int32_t)row<(int32_t)height;++row){
        for(uint32_t column=0;(int32_t)column<(int32_t)width;){
            if(mode==0){
                uint32_t color=DWORD(SOURCE24)&0xffffffu;
                kernel_pixel_put32_81b2f4(id,x+column,y+row,color);
            }else if(mode==1){
                uint32_t color=DWORD(palette+BYTE(SOURCE8)*4u);
                kernel_pixel_put32_81b2f4(id,x+column,y+row,color);
            }else if(mode==2){
                uint32_t color=DWORD(palette+(BYTE(SOURCE8)&15u)*4u);
                kernel_pixel_put32_81b2f4(id,x+column,y+row,color);
                color=DWORD(palette+(BYTE(SOURCE8)>>4)*4u);
                kernel_pixel_put32_81b2f4(id,x+column+1u,y+row,color);
            }else{
                uint32_t pixel=SOURCE24;
                uint32_t color=kernel_icon_quantize_81fd90(BYTE(pixel+2u),BYTE(pixel+1u),BYTE(pixel),column,row);
                kernel_pixel_put8_81b26e(id,x+column,y+row,(uint8_t)color);
            }
            column+=mode==2u?2u:1u;
        }
    }
    return row;
}
