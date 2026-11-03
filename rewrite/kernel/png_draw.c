/* PNG adapters: original surface-ID, direct-surface and file dispatch.
 * Decode errors and the original 8-bit transparency test are preserved. */
#include "png_draw.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
uint32_t kernel_png_draw_memory_83a579(uint32_t,uint32_t);
uint32_t kernel_png_draw_error_83a5f1(uint32_t);
uint32_t kernel_png_draw_decode_839f0d(uint32_t);
uint32_t kernel_png_draw_width_83a609(uint32_t);
uint32_t kernel_png_draw_height_83a613(uint32_t);
uint32_t kernel_png_draw_bpp_83a61f(uint32_t);
uint32_t kernel_png_draw_pixels_83a6f7(uint32_t);
void kernel_png_draw_destroy_83a5bb(uint32_t);
void kernel_png_draw_pixel_8374cf(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_png_draw_direct32_81b3c0(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_png_draw_direct8_81b37c(uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t kernel_png_draw_quantize_81fd90(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
int32_t kernel_png_draw_lookup_8263c0(uint32_t,uint32_t);
uint32_t kernel_png_draw_read_827454(uint32_t);
int32_t kernel_png_draw_relative_825c8e(uint32_t);
uint32_t kernel_png_draw_read_relative_827032(uint32_t);
int32_t kernel_png_draw_length_826940(uint32_t);
void kernel_png_draw_free_815978(uint32_t);

/* Inline both adapters to retain fixed slots and the original public calls. */
#define DRAW_BODY(direct) \
    uint32_t png=kernel_png_draw_memory_83a579(data,length); \
    if(kernel_png_draw_error_83a5f1(png))return; \
    (void)kernel_png_draw_decode_839f0d(png); \
    uint32_t width=kernel_png_draw_width_83a609(png),height=kernel_png_draw_height_83a613(png); \
    uint32_t stride=kernel_png_draw_bpp_83a61f(png)>>3,pixels=kernel_png_draw_pixels_83a6f7(png); \
    for(uint32_t row=0;row!=height;++row){ \
        for(uint32_t col=0;col!=width;++col){ \
            uint32_t pixel=pixels+(row*width+col)*stride; \
            if(D((direct?target:D(0x500000u+target*4u))+36)==32){ \
                if(!B(pixel+3))continue; \
                uint32_t color=(uint32_t)B(pixel+2)|((uint32_t)B(pixel+1)<<8)|((uint32_t)B(pixel)<<16); \
                if(direct)kernel_png_draw_direct32_81b3c0(target,col+x,row+y,color); \
                else kernel_png_draw_pixel_8374cf(target,col+x,row+y,color); \
            }else if(D((direct?target:D(0x500000u+target*4u))+36)==8){ \
                if(!D(pixel))continue; \
                uint32_t color=kernel_png_draw_quantize_81fd90(B(pixel),B(pixel+1),B(pixel+2),col,row); \
                if(direct)kernel_png_draw_direct8_81b37c(target,col+x,row+y,(uint8_t)color); \
                else kernel_png_draw_pixel_8374cf(target,col+x,row+y,(uint8_t)color); \
            } \
        } \
    } \
    kernel_png_draw_destroy_83a5bb(png)

void kernel_png_surface_draw_824014(volatile uint32_t target,volatile uint32_t x,volatile uint32_t y,volatile uint32_t data,volatile uint32_t length)
{
    DRAW_BODY(0);
}
void kernel_png_direct_draw_8241d0(volatile uint32_t target,volatile uint32_t x,volatile uint32_t y,volatile uint32_t data,volatile uint32_t length)
{
    DRAW_BODY(1);
}
#undef DRAW_BODY

void kernel_png_file_draw_82436c(volatile uint32_t target,volatile uint32_t x,volatile uint32_t y,volatile uint32_t path)
{
    uint32_t data;int32_t length;
    if((int8_t)B(path)>'B' && (int8_t)B(path)<='F' && B(path+1)==':' && B(path+2)=='\\'){
        uint8_t record[32];
        if(kernel_png_draw_lookup_8263c0(path,P(record))<=0)return;
        data=kernel_png_draw_read_827454(P(record));length=(int32_t)D(P(record)+28);
    }else{
        int32_t index=kernel_png_draw_relative_825c8e(path);if(index<0)return;
        data=kernel_png_draw_read_relative_827032((uint32_t)index);
        length=kernel_png_draw_length_826940((uint32_t)index);
    }
    if(length<=0 || !data)return;
    kernel_png_surface_draw_824014(target,x,y,data,(uint32_t)length);
    kernel_png_draw_free_815978(data);
}
