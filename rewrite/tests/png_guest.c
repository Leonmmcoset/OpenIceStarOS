/* Real guest diagnostic; no substitute PNG, heap or inflater implementations. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one)(uint32_t);
typedef uint32_t (*two)(uint32_t,uint32_t);
typedef uint32_t (*three)(uint32_t,uint32_t,uint32_t);
typedef void (*four)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*five)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*six)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*seven)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static uint8_t image[128],raw[64],out[128],previous[64];
static uint32_t info[13];
static void emit(uint32_t value){for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0,%1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));value>>=8;}}
static void be32(uint32_t at,uint32_t v){image[at]=v>>24;image[at+1]=v>>16;image[at+2]=v>>8;image[at+3]=v;}
static uint32_t hash(uint32_t p,uint32_t n){uint32_t v=2166136261u;for(uint32_t i=0;i<n;++i)v=(v^(*(uint8_t *)(uintptr_t)(p+i)))*16777619u;return v;}
void fat_guest_entry(void)
{
    emit(0x504e4731);
    const uint8_t signature[]={137,80,78,71,13,10,26,10};
    for(uint32_t i=0;i<8;++i)image[i]=signature[i];
    be32(8,13);be32(12,0x49484452);be32(16,3);be32(20,2);image[24]=8;image[25]=6;
    be32(33,37);be32(37,0x49444154);image[41]=0x78;image[42]=1;image[43]=1;
    image[44]=26;image[45]=0;image[46]=229;image[47]=255;
    for(uint32_t y=0;y<2;++y){image[48+y*13]=0;for(uint32_t i=0;i<12;++i)image[49+y*13+i]=y*12+i+1;}
    be32(82,0);be32(86,0x49454e44);
    uint32_t p=((two)0x83a579)(P(image),94);emit(p!=0);
    emit(((one)0x839c23)(p));emit(((one)0x83a609)(p));emit(((one)0x83a613)(p));
    emit(((one)0x83a61f)(p));emit(((one)0x83a649)(p));emit(((one)0x83a6a3)(p));emit(((one)0x83a6af)(p));
    emit(((one)0x839a9f)(p));emit(((one)0x83a6eb)(p));emit(((one)0x839f0d)(p));
    emit(((one)0x83a5f1)(p));emit(((one)0x83a5fd)(p));emit(((one)0x83a703)(p));
    emit(hash(((one)0x83a6f7)(p),24));emit(D(p+36));emit(D(p+40));emit(D(p+44));
    ((one)0x83a5bb)(p);
    p=((uint32_t (*)(void))0x83a4c9)();emit(p!=0);((one)0x839be9)(p);emit(D(p+40));((one)0x83a5bb)(p);
    emit(((three)0x8393a1)(20,70,50));
    for(uint32_t i=0;i<16;++i){raw[i]=i*13+7;previous[i]=i*5+3;out[i]=0xa5;}
    ((seven)0x839459)(P(info),P(out),P(raw),P(previous),3,4,16);emit(hash(P(out),16));
    for(uint32_t i=0;i<16;++i)out[i]=0xa5;
    raw[0]=0;raw[1]=0xa0;raw[2]=0;raw[3]=0x60;
    ((six)0x8397dd)(P(info),P(out),P(raw),3,2,1);emit(out[0]);emit(out[1]);
    out[0]=0xa5;((five)0x839893)(P(out),P(out),3,8,2);emit(out[0]);
    raw[0]=0;raw[1]=0xa0;raw[2]=0;raw[3]=0x60;out[0]=0xa5;
    info[0]=3;info[1]=2;info[2]=0;info[3]=1;
    ((four)0x839991)(P(info),P(out),P(raw),P(info));emit(out[0]);emit(info[7]);
    for(;;)((void (*)(void))(uintptr_t)D(D(0x1b0e4)+108*4))();
}
