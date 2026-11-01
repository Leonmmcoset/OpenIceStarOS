/* PNG decoding recovered from 0x8393a1..0x83a70f. The original's unchecked
 * arithmetic, permissive header/chunk handling and error line values are ABI.
 * No CRC, Adler-32, interlace or palette implementation is added here. */
#include "png.h"
#include "memory.h"
#include "memory_ops.h"
typedef uint32_t alias32 __attribute__((may_alias));
#define D(a) (*(volatile alias32 *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
uint32_t kernel_png_zlib_839263(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static __attribute__((always_inline)) inline uint32_t error(uint32_t p,uint32_t code,uint32_t line)
{
    D(p+28)=code;D(p+32)=line;return D(p+28);
}
static __attribute__((always_inline)) inline uint32_t be32(uint32_t p)
{
    return ((uint32_t)B(p)<<24)|((uint32_t)B(p+1)<<16)|((uint32_t)B(p+2)<<8)|B(p+3);
}
uint32_t kernel_png_paeth_8393a1(uint32_t a,uint32_t b,uint32_t c)
{
    uint32_t p=a+b-c;
    int32_t pa=(int32_t)p>(int32_t)a?(int32_t)(p-a):(int32_t)(a-p);
    int32_t pb=(int32_t)p>(int32_t)b?(int32_t)(p-b):(int32_t)(b-p);
    int32_t pc=(int32_t)p>(int32_t)c?(int32_t)(p-c):(int32_t)(c-p);
    return pa<=pb && pa<=pc?a:pb<=pc?b:c;
}
void kernel_png_scanline_839459(volatile uint32_t p,volatile uint32_t out,
    volatile uint32_t in,volatile uint32_t previous,volatile uint32_t bytewidth,
    uint32_t filter,volatile uint32_t length)
{
    uint8_t f=filter;
    if(f>4){error(p,4,0x2aa);return;}
    uint32_t i=0;
    /* The prefix loops intentionally use bytewidth even if length is smaller. */
    if(f==1 || f==3 || f==4){
        for(;i<bytewidth;++i){
            uint32_t prediction=0;
            if(previous){
                if(f==3)prediction=B(previous+i)>>1;
                if(f==4)prediction=kernel_png_paeth_8393a1(0,B(previous+i),0);
            }
            B(out+i)=(uint8_t)(B(in+i)+prediction);
        }
    }
    for(;i<length;++i){
        uint32_t prediction=0;
        if(f==1)prediction=B(out+i-bytewidth);
        if(f==2 && previous)prediction=B(previous+i);
        if(f==3)prediction=(B(out+i-bytewidth)+(previous?B(previous+i):0u))/2u;
        if(f==4)prediction=kernel_png_paeth_8393a1(B(out+i-bytewidth),
            previous?B(previous+i):0u,previous?B(previous+i-bytewidth):0u);
        B(out+i)=(uint8_t)(B(in+i)+prediction);
    }
}
void kernel_png_unfilter_8397dd(volatile uint32_t p,volatile uint32_t out,
    volatile uint32_t in,volatile uint32_t width,volatile uint32_t height,
    volatile uint32_t bpp)
{
    uint32_t previous=0,bytewidth=(bpp+7u)>>3,linebytes=(width*bpp+7u)>>3;
    for(uint32_t y=0;y<height;++y){
        uint32_t oi=y*linebytes,ii=y*(linebytes+1u);
        uint8_t filter=B(in+ii);
        kernel_png_scanline_839459(p,out+oi,in+ii+1u,previous,bytewidth,filter,linebytes);
        if(D(p+28))break;
        previous=out+oi;
    }
}
void kernel_png_unpad_839893(volatile uint32_t out,volatile uint32_t in,
    volatile uint32_t linebits,volatile uint32_t paddedbits,volatile uint32_t height)
{
    uint32_t padding=paddedbits-linebits,ob=0,ib=0;
    for(uint32_t y=0;y<height;++y){
        for(uint32_t x=0;x<linebits;++x){
            uint8_t bit=(B(in+(ib>>3))>>((~ib)&7u))&1u;++ib;
            uint32_t address=out+(ob>>3),mask=1u<<((~ob)&7u);
            if(bit)B(address)=B(address)|mask;
            else B(address)=B(address)&(uint8_t)~mask;
            ++ob;
        }
        ib+=padding;
    }
}
void kernel_png_postprocess_839991(volatile uint32_t p,volatile uint32_t out,
    volatile uint32_t in,volatile uint32_t info)
{
    uint32_t bpp=kernel_png_bpp_83a61f(info),w=D(info),h=D(info+4);
    if(!bpp){error(p,4,0x2f2);return;}
    if(bpp<8 && w*bpp!=((w*bpp+7u)>>3)*8u){
        kernel_png_unfilter_8397dd(p,in,in,w,h,bpp);
        if(D(p+28))return;
        kernel_png_unpad_839893(out,in,w*bpp,((w*bpp+7u)>>3)*8u,h);
    }else kernel_png_unfilter_8397dd(p,out,in,w,h,bpp);
}
uint32_t kernel_png_format_839a9f(volatile uint32_t p)
{
    if(D(p+8)==0){
        if(D(p+12)==1)return 5;
        if(D(p+12)==2)return 6;
        if(D(p+12)==4)return 7;
        if(D(p+12)==8)return 8;
        return 0;
    }
    if(D(p+8)==2){if(D(p+12)==8)return 1;
        if(D(p+12)==16)return 2;
        return 0;}
    if(D(p+8)==4){
        if(D(p+12)==1)return 9;
        if(D(p+12)==2)return 10;
        if(D(p+12)==4)return 11;
        if(D(p+12)==8)return 12;
        return 0;
    }
    if(D(p+8)==6){if(D(p+12)==8)return 3;
        if(D(p+12)==16)return 4;
        return 0;}
    return 0;
}
void kernel_png_release_input_839be9(volatile uint32_t p)
{
    if(B(p+48))kernel_free_allocation(D(p+40));
    D(p+40)=0;D(p+44)=0;B(p+48)=0;
}
uint32_t kernel_png_header_839c23(volatile uint32_t p)
{
    if(D(p+28) || D(p+36)!=2)return D(p+28);
    if(D(p+44)<=28)return error(p,3,0x34d);
    if(B(D(p+40))!=0x89 || B(D(p+40)+1)!='P' || B(D(p+40)+2)!='N' ||
        B(D(p+40)+3)!='G' || B(D(p+40)+4)!=13 || B(D(p+40)+5)!=10 ||
        B(D(p+40)+6)!=26 || B(D(p+40)+7)!=10)return error(p,3,0x353);
    if(be32(D(p+40)+12)!=0x49484452)return error(p,4,0x359);
    D(p)=be32(D(p+40)+16);D(p+4)=be32(D(p+40)+20);
    D(p+12)=B(D(p+40)+24);D(p+8)=B(D(p+40)+25);
    D(p+16)=kernel_png_format_839a9f(p);
    if(!D(p+16))return error(p,7,0x366);
    if(B(D(p+40)+26))return error(p,4,0x36c);
    if(B(D(p+40)+27))return error(p,4,0x372);
    if(B(D(p+40)+28))return error(p,6,0x378);
    D(p+36)=1;return D(p+28);
}
uint32_t kernel_png_decode_839f0d(volatile uint32_t p)
{
    uint32_t idatsize=0,copied=0;
    if(D(p+28))return D(p+28);
    kernel_png_header_839c23(p);
    if(D(p+28) || D(p+36)!=1)return D(p+28);
    if(D(p+20)){kernel_free_allocation(D(p+20));D(p+20)=0;D(p+24)=0;}
    uint32_t chunk=D(p+40)+33u;
    while(chunk<D(p+40)+D(p+44)){
        if(chunk-D(p+40)+12u>D(p+44))return error(p,4,0x3ac);
        uint32_t length=be32(chunk);
        if((int32_t)length<0)return error(p,4,0x3b3);
        if(chunk-D(p+40)+length+12u>D(p+44))return error(p,4,0x3b9);
        if(be32(chunk+4)==0x49444154)idatsize+=length;
        else if(be32(chunk+4)==0x49454e44)break;
        else if(!(B(chunk+4)&32))return error(p,5,0x3c6);
        chunk+=be32(chunk)+12u;
    }
    uint32_t idat=kernel_allocate(idatsize);
    if(!idat)return error(p,1,0x3d0);
    chunk=D(p+40)+33u;
    while(chunk<D(p+40)+D(p+44)){
        uint32_t length=be32(chunk),body=chunk+8u;
        if(be32(chunk+4)==0x49444154){kernel_move_bytes(idat+copied,body,length);copied+=length;}
        else if(be32(chunk+4)==0x49454e44)break;
        chunk+=be32(chunk)+12u;
    }
    /* This is the actual original product order, including its row overestimate. */
    uint32_t h=D(p+4),w=D(p);
    uint32_t rawsize=((w*(kernel_png_bpp_83a61f(p)*h+7u))>>3)+D(p+4);
    uint32_t raw=kernel_allocate(rawsize);
    if(!raw){kernel_free_allocation(idat);return error(p,1,0x3ee);}
    if(kernel_png_zlib_839263(p,raw,rawsize,idat,idatsize)){
        kernel_free_allocation(idat);kernel_free_allocation(raw);return D(p+28);
    }
    kernel_free_allocation(idat);
    h=D(p+4)*D(p);
    D(p+24)=(h*kernel_png_bpp_83a61f(p)+7u)>>3;
    D(p+20)=kernel_allocate(D(p+24));
    if(!D(p+20)){kernel_free_allocation(raw);D(p+24)=0;return error(p,1,0x403);}
    kernel_png_postprocess_839991(p,D(p+20),raw,p);kernel_free_allocation(raw);
    if(D(p+28)){kernel_free_allocation(D(p+20));D(p+20)=0;D(p+24)=0;}
    else D(p+36)=0;
    kernel_png_release_input_839be9(p);return D(p+28);
}
uint32_t kernel_png_create_83a4c9(void)
{
    uint32_t p=kernel_allocate(52);
        if(!p)return 0;
    D(p+20)=0;D(p+24)=0;D(p+4)=0;D(p)=0;D(p+8)=6;D(p+12)=8;
    D(p+16)=3;D(p+36)=2;D(p+28)=0;D(p+32)=0;D(p+40)=0;D(p+44)=0;B(p+48)=0;
    return p;
}
uint32_t kernel_png_from_memory_83a579(volatile uint32_t input,volatile uint32_t size)
{
    uint32_t p=kernel_png_create_83a4c9();
        if(!p)return 0;
    D(p+40)=input;D(p+44)=size;B(p+48)=0;return p;
}
void kernel_png_destroy_83a5bb(volatile uint32_t p)
{
    if(D(p+20))kernel_free_allocation(D(p+20));
    kernel_png_release_input_839be9(p);kernel_free_allocation(p);
}
uint32_t kernel_png_error_83a5f1(uint32_t p){return D(p+28);}
uint32_t kernel_png_error_line_83a5fd(uint32_t p){return D(p+32);}
uint32_t kernel_png_width_83a609(uint32_t p){return D(p);}
uint32_t kernel_png_height_83a613(uint32_t p){return D(p+4);}
uint32_t kernel_png_bpp_83a61f(volatile uint32_t p)
{
    uint32_t depth=kernel_png_depth_83a6a3(p);return depth*kernel_png_channels_83a649(p);
}
uint32_t kernel_png_channels_83a649(volatile uint32_t p)
{
    if(D(p+8)==0)return 1;
        if(D(p+8)==2)return 3;
    if(D(p+8)==4)return 2;
        if(D(p+8)==6)return 4;
        return 0;
}
uint32_t kernel_png_depth_83a6a3(uint32_t p){return D(p+12);}
uint32_t kernel_png_padded_bpp_83a6af(volatile uint32_t p)
{
    uint32_t depth=kernel_png_depth_83a6a3(p),v=depth*kernel_png_channels_83a649(p);
    return v+(v&7u);
}
uint32_t kernel_png_pixel_format_83a6eb(uint32_t p){return D(p+16);}
uint32_t kernel_png_pixels_83a6f7(uint32_t p){return D(p+20);}
uint32_t kernel_png_size_83a703(uint32_t p){return D(p+24);}
