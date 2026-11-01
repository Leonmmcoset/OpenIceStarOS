/* TrueType surface text services, 0x829003..0x82a32b. Coordinates wrap
 * as x86 words. The renderer deliberately retains original upper-only clips,
 * glyph bitmap allocation/free, spacing, and multiline early-return leak. */
#include "font_text.h"
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
#define F(a) (*(volatile float *)(uintptr_t)(a))
float kernel_text_scale_8306ec(uint32_t,float);
uint32_t kernel_text_bitmap_8329ec(uint32_t,float,float,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_text_free_832530(uint32_t,uint32_t);
uint32_t kernel_text_blend_828f1b(uint32_t,uint32_t,uint32_t,uint32_t);
void kernel_text_put_8374cf(uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t kernel_text_get_837939(uint32_t,uint32_t,uint32_t);
void kernel_text_fill_837545(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
int32_t kernel_text_offset_836fbc(uint32_t,int32_t);
int32_t kernel_text_init_836fea(uint32_t,uint32_t,int32_t);
#define GLYPH_DRAW(CLIP,ALPHA) \
    float scale=kernel_text_scale_8306ec(info,height); \
    uint32_t bitmap=kernel_text_bitmap_8329ec(info,scale,scale,code,w,h,ox,oy); \
    uint32_t px=x+D(ox),py=y+D(oy); \
    if(bitmap){ \
        for(uint32_t j=0;(int32_t)j<(int32_t)D(h);++j){ \
            if((CLIP) && (int32_t)(py+j)>(int32_t)maxy)continue; \
            for(uint32_t i=0;(int32_t)i<(int32_t)D(w);++i){ \
                if((CLIP) && (int32_t)(px+i)>(int32_t)maxx)continue; \
                if((ALPHA) && !B(bitmap+j*D(w)+i))continue; \
                uint32_t background=(ALPHA)?kernel_text_get_837939(surface,px+i,py+j):bg; \
                uint32_t color=kernel_text_blend_828f1b(fg,background,B(bitmap+j*D(w)+i),D(D(0x500000+surface*4)+36)); \
                kernel_text_put_8374cf(surface,px+i,py+j,color); \
            } \
        }kernel_text_free_832530(bitmap,0); \
    }
void kernel_font_glyph_opaque_829003(volatile uint32_t info,volatile float height,volatile uint32_t code,volatile uint32_t surface,volatile uint32_t x,volatile uint32_t y,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t w,volatile uint32_t h,volatile uint32_t ox,volatile uint32_t oy)
{const uint32_t maxx=0,maxy=0;GLYPH_DRAW(0,0)}
void kernel_font_glyph_clip_829133(volatile uint32_t info,volatile float height,volatile uint32_t code,volatile uint32_t surface,volatile uint32_t x,volatile uint32_t y,volatile uint32_t maxx,volatile uint32_t maxy,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t w,volatile uint32_t h,volatile uint32_t ox,volatile uint32_t oy)
{GLYPH_DRAW(1,0)}
void kernel_font_glyph_alpha_82927d(volatile uint32_t info,volatile float height,volatile uint32_t code,volatile uint32_t surface,volatile uint32_t x,volatile uint32_t y,volatile uint32_t fg,volatile uint32_t w,volatile uint32_t h,volatile uint32_t ox,volatile uint32_t oy)
{const uint32_t maxx=0,maxy=0,bg=0;GLYPH_DRAW(0,1)}
uint32_t kernel_font_legacy_code_8293e9(volatile uint32_t code)
{
    uint32_t table=D(0x1b1a8);
    if((int32_t)code>0xa1a0 && (int32_t)code<=0xa9fe)return D(table+(((uint32_t)((int32_t)(code-0xa100)>>8)*94)+(code&0x5e))*12+8);
    if((int32_t)code>0xb0a0 && (int32_t)code<=0xf7fe)return D(table+(((uint32_t)((int32_t)(code-0xb000)>>8)*94)+(code&255))*12+0x201c+8);
    return 0;
}
/* Macro expansion keeps each recovered function wholly inside its slot.
 * On original paths that read indeterminate locals (invalid legacy control
 * byte before any glyph; spacing after a first failed allocation), C uses
 * zero locals. Those paths have no claimed normal-return equivalence. */
#define STRING_SETUP \
    float scale=kernel_text_scale_8306ec(info,height); \
    uint32_t index=0,x=startx,y=0,w=0,h=0,ox=0,oy=0,code=0
#define SPACE(divisor) x+=(uint32_t)(int32_t)((long double)height/F(divisor))+gap
#define LEGACY_CODE \
    uint32_t first=B(text+index),second=B(text+index+1); \
    if((int8_t)B(text+index)>32 && B(text+index)!=127)code=(uint32_t)(int32_t)(int8_t)B(text+index); \
    else if(first>0xa0 && first!=0xff && second>0xa0 && second!=0xff){code=kernel_font_legacy_code_8293e9((first<<8)+second);++index;}
#define BITMAP uint32_t bitmap=kernel_text_bitmap_8329ec(info,scale,scale,code,P(&w),P(&h),P(&ox),P(&oy))
#define DRAW_STRING(CLIP,ALPHA) \
    for(uint32_t j=0;(int32_t)j<(int32_t)h;++j){ \
        if((CLIP) && (int32_t)(y+j)>(int32_t)maxy)continue; \
        for(uint32_t i=0;(int32_t)i<(int32_t)w;++i){ \
            if((CLIP) && (int32_t)(x+i)>(int32_t)maxx)continue; \
            if((ALPHA) && !B(bitmap+j*w+i))continue; \
            uint32_t background=bg; \
            if(ALPHA){background=kernel_text_get_837939(surface,x+i,y+j); \
                if(background==D(D(0x500000+surface*4)+40))background=kernel_text_get_837939(2,x+i,y+j);} \
            uint32_t color=kernel_text_blend_828f1b(fg,background,B(bitmap+j*w+i),D(D(0x500000+surface*4)+36)); \
            kernel_text_put_8374cf(surface,x+i,y+j,color); \
        } \
    }
#define FINISH_BITMAP x+=w+gap;kernel_text_free_832530(bitmap,0)
#define FILL_GAP if(B(text+index))kernel_text_fill_837545(surface,x-gap,y,x,y+h,bg)
void kernel_font_string_legacy_829485(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap)
{
    const uint32_t maxx=0,maxy=0;STRING_SETUP;
    while(B(text+index)){
        if(B(text+index)==32){SPACE(0x83cf5b);++index;continue;}
        LEGACY_CODE;BITMAP;
        if(bitmap){x+=ox;y=oy+starty;DRAW_STRING(0,0);FINISH_BITMAP;}
        ++index;FILL_GAP;
    }
}
uint32_t kernel_font_utf8_cont_8296d7(uint32_t byte)
{return (byte&0xc0)==0x80;}
int32_t kernel_font_utf8_decode_829707(volatile uint32_t text,volatile uint32_t consumed)
{
    if(!text || !consumed)return -1;
    uint32_t first=B(text);D(consumed)=0;
    if(first<128){D(consumed)=1;return first;}
    uint32_t n,code;
    if((first&0xe0)==0xc0){n=2;code=first&31;}
    else if((first&0xf0)==0xe0){n=3;code=first&15;}
    else if((first&0xf8)==0xf0){n=4;code=first&7;}
    else return -1;
    for(uint32_t i=1;i<n;++i)if(!kernel_font_utf8_cont_8296d7(B(text+i)))return -1;
    /* Re-read after validation: consumed can alias the input bytes. */
    for(uint32_t i=1;i<n;++i)code=(code<<6)|(B(text+i)&63);
    D(consumed)=n;
    if(n==2)return code>127?(int32_t)code:-1;
    if(n==3)return code>2047?(int32_t)code:-1;
    return code>65535 && code<=0x10ffff?(int32_t)code:-1;
}
void kernel_font_string_utf8_829941(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap)
{
    const uint32_t maxx=0,maxy=0;STRING_SETUP;uint32_t consumed=0;
    while(B(text+index)){
        code=kernel_font_utf8_decode_829707(text+index,P(&consumed));
        if(code==0xffffffff){++index;continue;}
        if(B(text+index)==32){SPACE(0x83cf5f);++index;continue;}
        BITMAP;if(bitmap){x+=ox;y=oy+starty;DRAW_STRING(0,0);FINISH_BITMAP;}
        index+=consumed;FILL_GAP;
    }
}
uint32_t kernel_font_string_lines_829b4d(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t maxx,volatile uint32_t maxy,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap,volatile uint32_t lineheight,volatile uint32_t maxbytes)
{
    STRING_SETUP;uint32_t consumed=0,lines=0;
    while(B(text+index) && (int32_t)index<(int32_t)maxbytes){
        code=kernel_font_utf8_decode_829707(text+index,P(&consumed));
        if(code==0xffffffff){++index;continue;}
        if(code==32){SPACE(0x83cf63);++index;continue;}
        if(code==10){++lines;++index;continue;}
        if(code==13){x=startx;++index;continue;}
        BITMAP;if(bitmap){
            if((int32_t)(x+ox+w)>=(int32_t)maxx){x=startx;++lines;}
            x+=ox;y=oy+starty+lines*lineheight;
            if((int32_t)y>=(int32_t)maxy)return lines;
            DRAW_STRING(0,0);FINISH_BITMAP;
        }index+=consumed;FILL_GAP;
    }return lines;
}
void kernel_font_string_legacy_clip_829dd3(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t maxx,volatile uint32_t maxy,volatile uint32_t fg,volatile uint32_t bg,volatile uint32_t gap)
{
    STRING_SETUP;
    while(B(text+index)){
        if(B(text+index)==32){SPACE(0x83cf67);++index;continue;}
        LEGACY_CODE;BITMAP;
        if(bitmap){x+=ox;y=oy+starty;DRAW_STRING(1,0);FINISH_BITMAP;}
        ++index;FILL_GAP;
    }
}
void kernel_font_string_legacy_alpha_82a03f(volatile uint32_t info,volatile float height,volatile uint32_t text,volatile uint32_t surface,volatile uint32_t startx,volatile uint32_t starty,volatile uint32_t fg,volatile uint32_t gap)
{
    const uint32_t maxx=0,maxy=0,bg=0;STRING_SETUP;
    while(B(text+index)){
        if(B(text+index)==32){SPACE(0x83cf6b);++index;continue;}
        LEGACY_CODE;BITMAP;
        if(bitmap){x+=ox;y=oy+starty;DRAW_STRING(0,1);FINISH_BITMAP;}
        ++index;
    }
}
uint32_t kernel_font_text_init_82a2c5(volatile uint32_t data,volatile uint32_t info)
{
    if(!data)return 0;
    int32_t offset=kernel_text_offset_836fbc(data,0);
    return kernel_text_init_836fea(info,data,offset)>0;
}
