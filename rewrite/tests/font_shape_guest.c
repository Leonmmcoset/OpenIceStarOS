/* Test-only X: actual glyph allocation, recursive transforms and CFF output. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*free_fn)(uint32_t);
typedef void (*no_fn)(void);
static uint32_t info[48],output;
static uint8_t table[4096];
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
static void reset(void)
{
    for(uint32_t i=0;i<48;++i)info[i]=0;
    for(uint32_t i=0;i<4096;++i)table[i]=0;
}
static void shape(uint32_t entry,uint32_t glyph)
{
    uint32_t count=((three_fn)entry)(P(info),glyph,P(&output));
    uint32_t hash=2166136261u;
    emit(count);
    for(uint32_t i=0;i<count;++i){
        const uint8_t *vertex=(const uint8_t *)(uintptr_t)(output+i*14u);
        uint32_t type=vertex[12],bytes=type==4?12:type==3?8:4;
        hash=(hash^type)*16777619u;
        for(uint32_t j=0;j<bytes;++j)hash=(hash^vertex[j])*16777619u;
    }
    emit(hash);
    if(output)((free_fn)0x8175a4)(output);
}
void fat_guest_entry(void)
{
    emit(0x53485031);
    uint32_t font=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    emit(font!=0);emit(((three_fn)0x82c3bc)(P(info),font,0));
    shape(0x82d386,1);shape(0x82f90a,14);shape(0x82cede,65);
    reset();info[1]=P(table);info[3]=4;info[4]=128;info[6]=1024;info[13]=1;
    b32(128,0);b32(132,22);b32(136,40);b32(140,60);b32(144,60);
    b16(1024,1);b16(1034,3);
    const uint8_t flags[]={0x31,0x33,0x35,0x23,100,100,100};
    for(uint32_t i=0;i<sizeof(flags);++i)table[1038+i]=flags[i];
    b16(1046,0xffff);b16(1056,3);b16(1058,0);b16(1060,31);b16(1062,(uint16_t)-27);
    b16(1064,0xffff);b16(1074,11);b16(1076,1);b16(1078,20);b16(1080,(uint16_t)-10);b16(1082,8192);
    shape(0x82d386,0);shape(0x82f90a,1);shape(0x82f90a,2);shape(0x82f90a,3);
    reset();info[16]=1;info[17]=P(table);info[19]=13;
    const uint8_t cff[]={0,1,1,1,9,139,139,21,149,159,169,179,189,199,8,14};
    for(uint32_t i=0;i<sizeof(cff);++i)table[i]=cff[i];
    table[4]=12;info[19]=sizeof(cff);
    shape(0x82f756,0);shape(0x82f90a,0);
    table[5]=0; /* Unsupported operator: zero output and failure count. */
    shape(0x82f756,0);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
