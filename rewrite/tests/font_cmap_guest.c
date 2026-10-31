/* Test-only X: execute real font initialization and each original cmap kind. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*two_fn)(uint32_t,uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t info[48];
static uint8_t table[600];
static const uint32_t points[]={0,1,32,65,66,68,70,71,86,97,255,256,1024,
    0x1234,0xfffd,65535,65536,65538,65540,0x1f600,0x10ffff,0xffffffff};

static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
static void b16(uint32_t at,uint32_t value)
{
    table[at]=(uint8_t)(value>>8);table[at+1]=(uint8_t)value;
}
static void b32(uint32_t at,uint32_t value)
{
    b16(at,value>>16);b16(at+2,value);
}
static void lookup(void)
{
    for(uint32_t i=0;i<sizeof(points)/sizeof(*points);++i)
        emit(((two_fn)0x82ca7c)(P(info),points[i]));
}
void fat_guest_entry(void)
{
    emit(0x434d5031);
    uint32_t font=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    emit(font!=0);emit(((three_fn)0x82c3bc)(P(info),font,0));lookup();
    info[1]=P(table);info[12]=64;
    b16(64,0);b16(66,262);
    for(uint32_t i=0;i<256;++i)table[70+i]=(uint8_t)(i*17+9);
    /* Avoid the negative signed format-0 out-of-bounds case in a real guest. */
    for(uint32_t i=0;i<sizeof(points)/sizeof(*points)-1;++i)
        emit(((two_fn)0x82ca7c)(P(info),points[i]));
    b16(64,6);b16(70,65);b16(72,4);
    for(uint32_t i=0;i<4;++i)b16(74+i*2,100+i);
    lookup();
    for(uint32_t i=0;i<sizeof(table);++i)table[i]=0;
    /* Three segments, including an indirect array whose idDelta is ignored. */
    b16(64,4);b16(66,46);b16(70,6);b16(72,4);b16(74,1);b16(76,2);
    b16(78,68);b16(80,97);b16(82,65535);
    b16(86,65);b16(88,97);b16(90,65535);
    b16(92,0xfffd);b16(94,17);b16(96,1);
    b16(100,4);b16(104,700);
    lookup();
    for(uint32_t format=12;format<=13;++format){
        b16(64,format);b32(76,3);
        b32(80,65);b32(84,70);b32(88,100);
        b32(92,65536);b32(96,65540);b32(100,200);
        b32(104,0xffffffff);b32(108,0xffffffff);b32(112,0xffff);
        lookup();
    }
    b16(64,2);lookup();
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
