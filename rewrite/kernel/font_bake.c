/* Character baking and packed range rendering, preserving original row
 * boundaries, write order, missing-character copy and DWORD pixel operations. */
#include "font_bake.h"
#include "font_atlas.h"
#include "font_init.h"
#include "font_parse.h"
#include "font_metrics.h"
#include "font_cmap.h"
typedef uint32_t alias32 __attribute__((may_alias));
typedef uint16_t alias16 __attribute__((may_alias));
typedef float alias_float __attribute__((may_alias));
#define D(a) (*(volatile alias32 *)(uintptr_t)(uint32_t)(a))
#define W(a) (*(volatile alias16 *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define F(a) (*(volatile alias_float *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
#define V(a) F(P(&(a)))
uint32_t kernel_font_malloc_817590(uint32_t size);
void kernel_font_free_8175a4(uint32_t pointer);
void kernel_font_clear_81752e(uint32_t pointer, uint32_t value, uint32_t dwords);
void kernel_font_make_glyph_832852(uint32_t info, uint32_t output, int32_t width, int32_t height, int32_t stride, uint32_t sx, uint32_t sy, int32_t glyph);
void kernel_font_make_glyph_subpixel_832756(uint32_t info, uint32_t output, int32_t width, int32_t height, int32_t stride, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, int32_t glyph);
void kernel_font_bitmap_box_830a3c(uint32_t info, int32_t glyph, uint32_t sx, uint32_t sy, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void kernel_font_filter_h_833292(uint32_t pixels, uint32_t width, uint32_t height, uint32_t stride, uint32_t samples);
void kernel_font_filter_v_83357c(uint32_t pixels, uint32_t width, uint32_t height, uint32_t stride, uint32_t samples);
static __attribute__((always_inline)) inline uint32_t bits(float value)
{
    union {float f;uint32_t u;} word={value};return word.u;
}

int32_t kernel_font_bake_832a98(volatile uint32_t data, volatile uint32_t offset,
    volatile uint32_t height, volatile uint32_t pixels, volatile uint32_t width,
    volatile uint32_t rows, volatile uint32_t first, volatile int32_t count,
    volatile uint32_t chars)
{
    uint32_t info[36];info[0]=0;
    if (!kernel_font_init_api_836fea(P(info),data,offset)) return -1;
    kernel_font_clear_81752e(pixels,0,width*rows);
    uint32_t x=1,y=1,bottom=1;
    volatile float scale=kernel_font_scale_height_8306ec(P(info),V(height));
    for(int32_t i=0;i<count;++i){
        int32_t glyph=kernel_font_glyph_index_82ca7c(P(info),first+(uint32_t)i);
        int32_t advance,bearing,box[4];
        kernel_font_hmetrics_82f958(P(info),glyph,P(&advance),P(&bearing));
        kernel_font_bitmap_box_830a3c(P(info),glyph,bits(scale),bits(scale),P(box),P(box+1),P(box+2),P(box+3));
        uint32_t w=(uint32_t)box[2]-(uint32_t)box[0],h=(uint32_t)box[3]-(uint32_t)box[1];
        if((int32_t)(x+w+1u)>=(int32_t)width){y=bottom;x=1;}
        if((int32_t)(y+h+1u)>=(int32_t)rows)return (int32_t)(0u-(uint32_t)i);
        kernel_font_make_glyph_832852(P(info),pixels+x+y*width,(int32_t)w,(int32_t)h,(int32_t)width,bits(scale),bits(scale),glyph);
        W(chars+(uint32_t)i*20u)=x;
        W(chars+(uint32_t)i*20u+2u)=y;
        W(chars+(uint32_t)i*20u+4u)=x+w;
        W(chars+(uint32_t)i*20u+6u)=y+h;
        F(chars+(uint32_t)i*20u+16u)=advance*scale;
        F(chars+(uint32_t)i*20u+8u)=box[0];
        F(chars+(uint32_t)i*20u+12u)=box[1];
        x+=w+1u;
        if((int32_t)(y+h+1u)>(int32_t)bottom)bottom=y+h+1u;
    }
    return (int32_t)bottom;
}

void kernel_font_prefilter_833bbc(volatile uint32_t info, volatile uint32_t output,
    volatile uint32_t width, volatile uint32_t height, volatile uint32_t stride,
    volatile uint32_t sx, volatile uint32_t sy, volatile uint32_t dx, volatile uint32_t dy,
    volatile int32_t horizontal, volatile int32_t vertical, volatile uint32_t subx,
    volatile uint32_t suby, volatile int32_t glyph)
{
    kernel_font_make_glyph_subpixel_832756(info,output,(int32_t)(width-(uint32_t)horizontal+1u),
        (int32_t)(height-(uint32_t)vertical+1u),(int32_t)stride,sx,sy,dx,dy,glyph);
    if(horizontal>1)kernel_font_filter_h_833292(output,width,height,stride,(uint32_t)horizontal);
    if(vertical>1)kernel_font_filter_v_83357c(output,width,height,stride,(uint32_t)vertical);
    F(subx)=kernel_font_oversample_shift_8338c8((uint32_t)horizontal);
    F(suby)=kernel_font_oversample_shift_8338c8((uint32_t)vertical);
}

uint32_t kernel_font_render_rects_833c94(volatile uint32_t context,
    volatile uint32_t info, volatile uint32_t ranges, volatile int32_t count,
    volatile uint32_t rects)
{
    int32_t missing=-1;uint32_t ok=1,total=0;
    uint32_t old_h=D(context+28u),old_v=D(context+32u);
    for(int32_t i=0;i<count;++i){
        uint32_t size=D(ranges+(uint32_t)i*24u);
        volatile float scale;
        if(__builtin_isgreater(V(size),0.0f))scale=kernel_font_scale_height_8306ec(info,V(size));
        else{size^=0x80000000u;scale=kernel_font_scale_em_83073a(info,V(size));}
        D(context+28u)=B(ranges+(uint32_t)i*24u+20u);
        D(context+32u)=B(ranges+(uint32_t)i*24u+21u);
        volatile float iw=1.0f/D(context+28u),ih=1.0f/D(context+32u);
        volatile float shift_x=kernel_font_oversample_shift_8338c8(D(context+28u));
        volatile float shift_y=kernel_font_oversample_shift_8338c8(D(context+32u));
        for(int32_t j=0;j<(int32_t)D(ranges+(uint32_t)i*24u+12u);++j,++total){
            uint32_t rect=rects+total*24u;
            if(D(rect+20u)&&D(rect+12u)&&D(rect+16u)){
                uint32_t ch=D(ranges+(uint32_t)i*24u+16u)+(uint32_t)j*28u;
                uint32_t range=ranges+(uint32_t)i*24u;
                uint32_t cp=D(range+8u)?D(D(range+8u)+(uint32_t)j*4u):D(range+4u)+(uint32_t)j;
                int32_t glyph=kernel_font_glyph_index_82ca7c(info,cp);
                uint32_t padding=D(context+20u);
                D(rect)=D(rect)+padding;D(rect+4u)=D(rect+4u)+padding;
                D(rect+12u)=D(rect+12u)-padding;D(rect+16u)=D(rect+16u)-padding;
                int32_t advance,bearing,box[4];
                kernel_font_hmetrics_82f958(info,glyph,P(&advance),P(&bearing));
                volatile float sy=(long double)D(context+32u)*scale;
                volatile float sx=(long double)D(context+28u)*scale;
                kernel_font_bitmap_box_830a3c(info,glyph,bits(sx),bits(sy),P(box),P(box+1),P(box+2),P(box+3));
                sy=(long double)D(context+32u)*scale;sx=(long double)D(context+28u)*scale;
                kernel_font_make_glyph_subpixel_832756(info,D(context+36u)+D(rect)+D(rect+4u)*D(context+16u),
                    (int32_t)(D(rect+12u)-D(context+28u)+1u),(int32_t)(D(rect+16u)-D(context+32u)+1u),
                    (int32_t)D(context+16u),bits(sx),bits(sy),0,0,glyph);
                if(D(context+28u)>1u)kernel_font_filter_h_833292(D(context+36u)+D(rect)+D(rect+4u)*D(context+16u),D(rect+12u),D(rect+16u),D(context+16u),D(context+28u));
                if(D(context+32u)>1u)kernel_font_filter_v_83357c(D(context+36u)+D(rect)+D(rect+4u)*D(context+16u),D(rect+12u),D(rect+16u),D(context+16u),D(context+32u));
                W(ch)=D(rect);W(ch+2u)=D(rect+4u);
                W(ch+4u)=D(rect)+D(rect+12u);W(ch+6u)=D(rect+4u)+D(rect+16u);
                F(ch+16u)=advance*scale;
                F(ch+8u)=box[0]*iw+shift_x;F(ch+12u)=box[1]*ih+shift_y;
                F(ch+20u)=(int32_t)((uint32_t)box[0]+D(rect+12u))*iw+shift_x;
                F(ch+24u)=(int32_t)((uint32_t)box[1]+D(rect+16u))*ih+shift_y;
                if(!glyph)missing=j;
            }else if(D(context+24u)){ok=0;}
            else if(D(rect+20u)&&!D(rect+12u)&&!D(rect+16u)&&missing>=0){
                uint32_t dest=D(ranges+(uint32_t)i*24u+16u)+(uint32_t)j*28u;
                uint32_t src=D(ranges+(uint32_t)i*24u+16u)+(uint32_t)missing*28u;
                /* Forward DWORD copy, including when source and dest overlap.
                 * The missing index persists across ranges; the original
                 * still indexes into the CURRENT range's packed-char array. */
                for(uint32_t n=0;n<7;++n)D(dest+n*4u)=D(src+n*4u);
            }else ok=0;
        }
    }
    D(context+28u)=old_h;D(context+32u)=old_v;
    return ok;
}

uint32_t kernel_font_pack_ranges_8342e8(volatile uint32_t context,
    volatile uint32_t data, volatile int32_t index, volatile uint32_t ranges,
    volatile int32_t count)
{
    for(int32_t i=0;i<count;++i)for(int32_t j=0;j<(int32_t)D(ranges+(uint32_t)i*24u+12u);++j){
        uint32_t ch=D(ranges+(uint32_t)i*24u+16u)+(uint32_t)j*28u;
        W(ch+6u)=0;W(ch+4u)=0;W(ch+2u)=0;W(ch)=0;
    }
    uint32_t total=0;
    for(int32_t i=0;i<count;++i)total+=D(ranges+(uint32_t)i*24u+12u);
    uint32_t rects=kernel_font_malloc_817590(total*24u);
    if(!rects)return 0;
    uint32_t info[36];info[0]=D(context);
    kernel_font_init_api_836fea(P(info),data,(uint32_t)kernel_font_offset_api_836fbc(data,index));
    total=(uint32_t)kernel_font_gather_rects_8338fa(context,P(info),ranges,count,rects);
    kernel_font_pack_rects_8342c4(context,rects,(int32_t)total);
    uint32_t ok=kernel_font_render_rects_833c94(context,P(info),ranges,count,rects);
    kernel_font_free_8175a4(rects);
    return ok;
}

uint32_t kernel_font_pack_range_8345c2(uint32_t context,uint32_t data,int32_t index,
    uint32_t height,uint32_t first,int32_t count,uint32_t chars)
{
    uint32_t range[6];range[0]=height;range[1]=first;range[2]=0;range[3]=(uint32_t)count;range[4]=chars;
    return kernel_font_pack_ranges_8342e8(context,data,index,P(range),1);
}

void kernel_font_scaled_metrics_834612(volatile uint32_t data,volatile int32_t index,
    volatile uint32_t size,volatile uint32_t ascent,volatile uint32_t descent,
    volatile uint32_t gap)
{
    uint32_t info[36];
    kernel_font_init_api_836fea(P(info),data,(uint32_t)kernel_font_offset_api_836fbc(data,index));
    volatile float scale;
    if(__builtin_isgreater(V(size),0.0f))scale=kernel_font_scale_height_8306ec(P(info),V(size));
    else{uint32_t neg=size^0x80000000u;scale=kernel_font_scale_em_83073a(P(info),V(neg));}
    int32_t a,d,g;kernel_font_vmetrics_83054a(P(info),P(&a),P(&d),P(&g));
    F(ascent)=a*scale;F(descent)=d*scale;F(gap)=g*scale;
}

int32_t kernel_font_bake_api_836f70(uint32_t data,uint32_t offset,uint32_t height,
    uint32_t pixels,uint32_t width,uint32_t rows,uint32_t first,int32_t count,uint32_t chars)
{return kernel_font_bake_832a98(data,offset,height,pixels,width,rows,first,count,chars);}
int32_t kernel_font_offset_api_836fbc(uint32_t data,int32_t index)
{return kernel_font_offset_82c0b2(data,index);}
int32_t kernel_font_count_api_836fd6(uint32_t data)
{return kernel_font_count_82c196(data);}
int32_t kernel_font_init_api_836fea(uint32_t info,uint32_t data,uint32_t offset)
{return kernel_font_init_82c3bc(info,data,offset);}
