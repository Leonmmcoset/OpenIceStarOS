/* Diagnostic X: real pack allocations, glyph bounds, row layout and quads. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*begin_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*one_fn)(uint32_t);
typedef void (*two_fn)(uint32_t,uint32_t);
typedef void (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef float (*shift_fn)(uint32_t);
typedef int32_t (*gather_fn)(uint32_t,uint32_t,uint32_t,int32_t,uint32_t);
typedef void (*quad_fn)(uint32_t,int32_t,int32_t,int32_t,uint32_t,uint32_t,uint32_t,int32_t);
typedef void (*no_fn)(void);
static uint32_t ctx[11],info[40],ranges[12],rects[60],cps[]={65,0,9999,65,0};
static uint8_t table[0x3000],pixels[16*12*4+16];
static float xy[2],quad[8];
static const uint16_t texture[]={2,3,8,9};
static struct { uint16_t uv[4]; float offsets[5]; } ch;
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));value>>=8;
    }
}
static void float_emit(float value){union{float f;uint32_t u;}v={value};emit(v.u);}
void fat_guest_entry(void)
{
    emit(0x41544c31);
    for(uint32_t i=0;i<sizeof(pixels);++i)pixels[i]=0xa5;
    emit(((begin_fn)0x8330f6)(P(ctx),P(pixels),16,12,0,1,77));
    emit(ctx[0]);emit(ctx[2]);emit(ctx[3]);emit(ctx[4]);emit(ctx[5]);
    uint32_t zero=0;for(uint32_t i=0;i<16*12*4;++i)zero+=pixels[i]!=0;
    emit(zero);emit(pixels[16*12*4]);
    ((three_fn)0x833260)(P(ctx),2,3);emit(ctx[7]);emit(ctx[8]);
    ((three_fn)0x833260)(P(ctx),5,0xffffffff);emit(ctx[7]);emit(ctx[8]);
    ((two_fn)0x833284)(P(ctx),1);emit(ctx[6]);
    ((two_fn)0x833284)(P(ctx),0);
    ((three_fn)0x833260)(P(ctx),1,1);
    for(uint32_t i=0;i<5;++i)float_emit(((shift_fn)0x8338c8)(i));
    info[1]=P(table);info[3]=2;info[4]=128;info[5]=0x2000;info[6]=1024;info[7]=0x2100;info[12]=0x2200;info[13]=1;
    table[135]=22;table[139]=44;
    table[0x2013]=16;table[0x2105]=12;table[0x2106]=0xff;table[0x2107]=0xfc;
    table[0x2202]=1;table[0x2203]=6;table[0x2206+65]=1;
    const uint8_t glyph[]={0,1,0,1,0,0,0,7,0,4,0,3,0,0,59,1,53,35,1,6,6,4};
    for(uint32_t i=0;i<sizeof(glyph);++i){table[1024+i]=glyph[i];table[1046+i]=glyph[i];}
    ranges[0]=0x41800000;ranges[2]=P(cps);ranges[3]=5;
    ranges[6]=0xc1800000;ranges[8]=P(cps);ranges[9]=2;
    emit(((gather_fn)0x8338fa)(P(ctx),P(info),P(ranges),2,P(rects)));
    emit(ranges[5]);emit(ranges[11]);
    for(uint32_t i=0;i<7;++i){emit(rects[i*6+3]);emit(rects[i*6+4]);}
    rects[7*6+3]=16;rects[7*6+4]=3;
    rects[8*6+3]=2;rects[8*6+4]=1;
    rects[9*6+3]=1;rects[9*6+4]=1;
    ((three_fn)0x8342c4)(P(ctx),P(rects),10);
    for(uint32_t i=0;i<10;++i){emit(rects[i*6]);emit(rects[i*6+1]);emit(rects[i*6+5]);}
    for(uint32_t i=0;i<4;++i)ch.uv[i]=texture[i];
    const float offsets[]={1.25f,-1.5f,7.5f,9.25f,2.5f};
    for(uint32_t i=0;i<5;++i)ch.offsets[i]=offsets[i];
    for(uint32_t packed=0;packed<2;++packed)for(uint32_t flag=0;flag<2;++flag){
        xy[0]=10.25f;xy[1]=-2.75f;
        ((quad_fn)(packed?0x8346e2:0x832dbe))(P(&ch),64,32,0,P(xy),P(xy+1),P(quad),flag);
        for(uint32_t i=0;i<8;++i)float_emit(quad[i]);
        float_emit(xy[0]);float_emit(xy[1]);
    }
    ((one_fn)0x83323c)(P(ctx));
    emit(ctx[0]);emit(ctx[2]);emit(ctx[3]);emit(ctx[4]);emit(ctx[5]);emit(ctx[7]);emit(ctx[8]);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
