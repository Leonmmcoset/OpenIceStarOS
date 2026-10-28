/* Color-key BMP drawing at 0x821446..0x821624. Preserve forward file rows,
 * reversed destination Y, live full-width key/input/header reads, DWORD
 * 24-bit fetches and the original 24-bit branch's trailing "t=" text. */
#include "bitmap.h"
#include "pixel_access.h"
#include "text_format.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_bitmap_key_draw_821446(volatile uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t bitmap,volatile uint32_t key)
{
    uint32_t file=bitmap,header=bitmap+14u,palette=bitmap+54u;
    uint32_t colors=DWORD(0x1b0e0u),surface=DWORD(0x500000u+id*4u);
    if(DWORD(surface+36u)!=32u)return surface;
    if(WORD(header+14u)==24u){
        /* The extra diagnostic vararg is never consumed by the fixed
         * literal "t=". Zero makes its empty-image value defined in C. */
        uint32_t color=0;
        for(uint32_t row=0;DWORD(header+8u)>row;++row)
            for(uint32_t column=0;DWORD(header+4u)>column;++column){
                color=DWORD(bitmap+DWORD(file+10u)+(DWORD(header+4u)*row+column)*3u)&0xffffffu;
                if(color!=key)
                    kernel_pixel_put32_81b2f4(id,x+column,y+DWORD(header+8u)-row-1u,color);
            }
        return kernel_text_format_81f88a(id,10u,10u,DWORD(colors),DWORD(colors+56u),0x83cf53u,color);
    }
    if(WORD(header+14u)==8u){
        uint32_t row=0,height;
        while((height=DWORD(header+8u))>row){
            for(uint32_t column=0;DWORD(header+4u)>column;++column){
                uint32_t color=DWORD(palette+BYTE(bitmap+DWORD(file+10u)+DWORD(header+4u)*row+column)*4u);
                if(color!=key)
                    kernel_pixel_put32_81b2f4(id,x+column,y+DWORD(header+8u)-row-1u,color);
            }
            ++row;
        }
        return height;
    }
    return header;
}
