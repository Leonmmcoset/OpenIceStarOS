/* Diagnostic X: full C candidates share real SFNT/shape/raster/heap calls. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
typedef uint32_t alias32 __attribute__((may_alias));
#define D(a) (*(volatile alias32 *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*two_fn)(uint32_t,uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*no_fn)(void);
typedef int32_t (*bake_fn)(uint32_t,uint32_t,float,uint32_t,uint32_t,uint32_t,uint32_t,int32_t,uint32_t);
typedef void (*prefilter_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,float,float,float,float,int32_t,int32_t,uint32_t,uint32_t,int32_t);
typedef uint32_t (*render_fn)(uint32_t,uint32_t,uint32_t,int32_t,uint32_t);
typedef uint32_t (*pack_fn)(uint32_t,uint32_t,int32_t,uint32_t,int32_t);
typedef uint32_t (*range_fn)(uint32_t,uint32_t,int32_t,float,uint32_t,int32_t,uint32_t);
typedef uint32_t (*begin_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*metrics_fn)(uint32_t,int32_t,float,uint32_t,uint32_t,uint32_t);
static uint8_t font[4096],pixels[4096],chars[256];
static uint32_t info[40],ctx[11],ranges[6],rect[6];
static float shifts[2],metrics[3];
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));value>>=8;}
}
static void be16(uint32_t off,uint32_t value){font[off]=value>>8;font[off+1]=value;}
static void be32(uint32_t off,uint32_t value){be16(off,value>>16);be16(off+2,value);}
static void table(uint32_t index,uint32_t tag,uint32_t offset,uint32_t length)
{
    uint32_t p=12+index*16;be32(p,tag);be32(p+8,offset);be32(p+12,length);
}
static void initialize(void)
{
    be32(0,0x10000);be16(4,7);
    table(0,0x636d6170,256,274);table(1,0x6c6f6361,1024,16);
    table(2,0x68656164,1280,64);table(3,0x676c7966,1536,44);
    table(4,0x68686561,2048,40);table(5,0x686d7478,2304,12);table(6,0x6d617870,2560,6);
    be16(258,1);be16(260,3);be16(262,1);be32(264,12);
    be16(270,262);font[274+65]=1;font[274+66]=2;
    be32(1028,22);be32(1032,44);be32(1036,44);
    be16(1298,16);be16(1330,1);
    const uint8_t glyph[]={0,1,0,1,0,0,0,7,0,4,0,3,0,0,59,1,53,35,1,6,6,4};
    for(uint32_t i=0;i<sizeof(glyph);++i){font[1536+i]=glyph[i];font[1558+i]=glyph[i];}
    be16(2052,12);be16(2054,0xfffc);be16(2056,2);be16(2082,3);
    be16(2304,10);be16(2306,1);be16(2308,12);be16(2310,1);be16(2312,8);be16(2564,3);
}
static void reset(void)
{
    for(uint32_t i=0;i<sizeof(pixels);++i)pixels[i]=0xa5;
    for(uint32_t i=0;i<sizeof(chars);++i)chars[i]=0xa5;
}
static uint32_t hash(void)
{
    uint32_t value=2166136261u;
    for(uint32_t i=0;i<sizeof(pixels);++i)value=(value^pixels[i])*16777619u;
    return value;
}
static void emit_chars(uint32_t count)
{
    for(uint32_t i=0;i<count;++i)emit(D(P(chars+i*4)));
}
void fat_guest_entry(void)
{
    emit(0x42414b31);initialize();emit(((one_fn)0x836fd6)(P(font)));
    emit(((two_fn)0x836fbc)(P(font),0));emit(((two_fn)0x836fbc)(P(font),1));
    emit(((three_fn)0x836fea)(P(info),P(font),0));
    ((metrics_fn)0x834612)(P(font),0,16,P(metrics),P(metrics+1),P(metrics+2));
    for(uint32_t i=0;i<3;++i)emit(D(P(metrics+i)));
    ((metrics_fn)0x834612)(P(font),0,-8,P(metrics),P(metrics+1),P(metrics+2));
    for(uint32_t i=0;i<3;++i)emit(D(P(metrics+i)));
    for(uint32_t api=0;api<2;++api){
        reset();emit(((bake_fn)(api?0x836f70:0x832a98))(P(font),0,16,P(pixels),16,12,64,3,P(chars)));
        emit_chars(15);emit(hash());
    }
    reset();((prefilter_fn)0x833bbc)(P(info),P(pixels),8,4,32,1,1,0,0,1,1,P(shifts),P(shifts+1),1);
    emit(D(P(shifts)));emit(D(P(shifts+1)));emit(hash());
    reset();ctx[0]=77;ctx[2]=32;ctx[3]=16;ctx[4]=32;ctx[5]=1;ctx[6]=0;ctx[7]=3;ctx[8]=4;ctx[9]=P(pixels);
    ranges[0]=0x41800000;ranges[1]=65;ranges[3]=1;ranges[4]=P(chars);ranges[5]=0x101;
    rect[0]=2;rect[1]=3;rect[2]=99;rect[3]=7;rect[4]=5;rect[5]=1;
    emit(((render_fn)0x833c94)(P(ctx),P(info),P(ranges),1,P(rect)));
    for(uint32_t i=0;i<6;++i)emit(rect[i]);
    emit_chars(7);emit(hash());emit(ctx[7]);emit(ctx[8]);
    for(uint32_t api=0;api<2;++api){
        reset();emit(((begin_fn)0x8330f6)(P(ctx),P(pixels),16,12,0,1,77));
        if(api)emit(((range_fn)0x8345c2)(P(ctx),P(font),0,16,64,3,P(chars)));
        else{ranges[0]=0x41800000;ranges[1]=64;ranges[2]=0;ranges[3]=3;ranges[4]=P(chars);
            emit(((pack_fn)0x8342e8)(P(ctx),P(font),0,P(ranges),1));}
        emit_chars(21);emit(hash());((one_fn)0x83323c)(P(ctx));
    }
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
