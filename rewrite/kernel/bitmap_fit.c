/* Legacy 24-bit BMP fitting at 0x8204ac..0x820d66. The percent counters
 * intentionally use the dimension difference, signed wrapped arithmetic,
 * asymmetric row/column repetition and live surface/header/argument reads. */
#include "bitmap.h"
#include "pixel_access.h"
#include "icon_pixels.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define SURFACE DWORD(0x500000u+id*4u)
#define WIDTH DWORD(SURFACE+12u)
#define HEIGHT DWORD(SURFACE+16u)
#define SOURCE (bitmap+DWORD(file+10u)+(DWORD(header+8u)-source_y-1u)*3u*DWORD(header+4u)+source_x*3u)
#define THRESHOLD(position,count,ratio) ((int32_t)((position)*100u+100u)>=(int32_t)((count)*(ratio)))
uint32_t kernel_bitmap_fit_8204ac(volatile uint32_t id,volatile uint32_t bitmap)
{
    uint32_t file=bitmap,header=bitmap+14u;
    (void)DWORD(0x1b0e0u);
    uint32_t ratio_x=0,ratio_y=0,difference;
    if(DWORD(header+4u)>WIDTH){
        difference=DWORD(header+4u)-WIDTH;
        ratio_x=(uint32_t)((int32_t)(WIDTH*100u)/(int32_t)difference);
    }else if(DWORD(header+4u)<WIDTH){
        difference=WIDTH-DWORD(header+4u);
        ratio_x=(uint32_t)((int32_t)(WIDTH*100u)/(int32_t)difference);
    }
    if(DWORD(header+8u)>HEIGHT){
        difference=DWORD(header+8u)-HEIGHT;
        ratio_y=(uint32_t)((int32_t)(HEIGHT*100u)/(int32_t)difference);
    }else if(DWORD(header+8u)<HEIGHT){
        difference=HEIGHT-DWORD(header+8u);
        ratio_y=(uint32_t)((int32_t)(HEIGHT*100u)/(int32_t)difference);
    }
    uint32_t surface=SURFACE,mode;
    if(DWORD(surface+36u)==32u)mode=0;
    else{
        surface=SURFACE;
        if(DWORD(surface+36u)!=8u)return surface;
        mode=1;
    }
    if(WORD(header+14u)!=24u)return header;
    uint32_t row=0,source_y=0,y_count=1;
    for(;;){
        uint32_t height=HEIGHT;
        if((int32_t)height<=(int32_t)row)return height;
        if(source_y>=DWORD(header+8u))source_y=DWORD(header+8u)-1u;
        uint32_t column=0,source_x=0,x_count=1;
        while((int32_t)WIDTH>(int32_t)column){
            if(source_x>=DWORD(header+4u))source_x=DWORD(header+4u)-1u;
            uint32_t source;
            if(!mode){
                uint32_t color=DWORD(SOURCE)&0xffffffu;
                kernel_pixel_put32_81b2f4(id,column,row,color);
            }else{
                source=SOURCE;
                uint32_t color=kernel_icon_quantize_81fd90(BYTE(source+2u),BYTE(source+1u),BYTE(source),column,row);
                kernel_pixel_put8_81b26e(id,column,row,(uint8_t)color);
            }
            if(DWORD(header+4u)>WIDTH){
                while(THRESHOLD(column,x_count,ratio_x)&&(int32_t)(column+1u)<(int32_t)WIDTH){
                    ++source_x;++x_count;
                }
            }else if(DWORD(header+4u)<WIDTH){
                while(THRESHOLD(column,x_count,ratio_x)&&(int32_t)(column+1u)<(int32_t)WIDTH){
                    if(!mode){
                        uint32_t color=DWORD(SOURCE)&0xffffffu;
                        kernel_pixel_put32_81b2f4(id,column+1u,row,color);
                    }else{
                        /* Reuse this pixel's captured address; dither uses
                         * column although the destination is column+1. */
                        uint32_t color=kernel_icon_quantize_81fd90(BYTE(source+2u),BYTE(source+1u),BYTE(source),column,row);
                        kernel_pixel_put8_81b26e(id,column+1u,row,(uint8_t)color);
                    }
                    ++column;++x_count;
                }
            }
            ++column;++source_x;
        }
        if(DWORD(header+8u)>HEIGHT){
            while(THRESHOLD(row,y_count,ratio_y)&&(int32_t)(row+1u)<(int32_t)HEIGHT){
                ++source_y;++y_count;
            }
        }else if(DWORD(header+8u)<HEIGHT){
            if(THRESHOLD(row,y_count,ratio_y)&&(int32_t)(row+1u)<(int32_t)HEIGHT){
                --source_y;++y_count;
            }
        }
        ++row;++source_y;
    }
}
