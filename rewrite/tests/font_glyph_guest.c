/* Test-only X: real font glyphs plus specified legacy kern/GPOS/CFF tables. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*two_fn)(uint32_t,uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*four_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*six_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t info[48];
static uint8_t table[4096];
static uint32_t values[4];
static const uint8_t rectangle[]={0,1,1,1,14,139,139,21,239,139,5,139,239,5,39,139,5,14};
static const uint32_t glyphs[]={0,1,7,13,34,113,138,264};

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
static void outputs(void)
{
    for(uint32_t i=0;i<4;++i)emit(values[i]);
}
static void reset(void)
{
    for(uint32_t i=0;i<48;++i)info[i]=0;
    for(uint32_t i=0;i<4096;++i)table[i]=0;
    info[1]=P(table);
}
static void box(uint32_t entry,uint32_t glyph)
{
    for(uint32_t i=0;i<4;++i)values[i]=0xa5a5a5a5;
    emit(((six_fn)entry)(P(info),glyph,P(values),P(values+1),P(values+2),P(values+3)));
    outputs();
}
void fat_guest_entry(void)
{
    emit(0x474c5931);
    uint32_t font=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    emit(font!=0);emit(((three_fn)0x82c3bc)(P(info),font,0));
    for(uint32_t i=0;i<sizeof(glyphs)/sizeof(*glyphs);++i){
        uint32_t glyph=glyphs[i];
        emit(((two_fn)0x82cf50)(P(info),glyph));
        box(0x82d062,glyph);emit(((two_fn)0x82d1a4)(P(info),glyph));
    }
    box(0x82d15c,65);
    ((four_fn)0x830510)(P(info),65,P(values),P(values+1));
    emit(values[0]);emit(values[1]);
    for(uint32_t i=0;i<16;++i)table[i]=0xa5;
    ((six_fn)0x82cf10)(P(table),0x123,0xffff7fff,65535,0x12345678,0xffffffff);
    for(uint32_t i=0;i<4;++i)emit(D(P(table)+i*4));
    reset();
    for(uint32_t i=0;i<sizeof(rectangle);++i)table[i]=rectangle[i];
    info[16]=1;info[17]=P(table);info[18]=0;info[19]=sizeof(rectangle);
    box(0x82f80a,0);box(0x82d062,0);emit(((two_fn)0x82d1a4)(P(info),0));
    reset();
    /* Legacy kern: pairs (1,2,-70), (3,5,33). */
    info[9]=64;info[12]=3072;
    b16(66,1);b16(72,1);b16(74,2);
    b16(82,1);b16(84,2);b16(86,0xffba);
    b16(88,3);b16(90,5);b16(92,33);
    b16(3072,0);b16(3074,262);
    for(uint32_t i=0;i<256;++i)table[3078+i]=(uint8_t)i;
    emit(((one_fn)0x82fa50)(P(info)));
    emit(((three_fn)0x82faca)(P(info),P(values),1));outputs();
    emit(((three_fn)0x82fc1a)(P(info),1,2));
    emit(((three_fn)0x830442)(P(info),1,2));
    emit(((three_fn)0x8304a8)(P(info),3,5));
    /* GPOS lookup-list at 266, lookup at 270, pair-set subtable at 278. */
    info[10]=256;
    b16(256,1);b16(264,10);b16(266,1);b16(268,4);
    b16(270,2);b16(274,1);b16(276,8);
    b16(278,1);b16(280,18);b16(282,4);b16(284,0);b16(286,1);b16(288,12);
    b16(290,1);b16(292,2);b16(294,0xffa6);
    b16(296,1);b16(298,1);b16(300,1);
    emit(((two_fn)0x82fd28)(P(table)+296,1));
    emit(((three_fn)0x830024)(P(info),1,2));
    emit(((three_fn)0x830442)(P(info),1,2));
    emit(((three_fn)0x8304a8)(P(info),1,2));
    /* Replace pair set with a 2x2 class matrix; advance[1][1] = -120. */
    for(uint32_t i=278;i<400;++i)table[i]=0;
    b16(278,2);b16(280,24);b16(282,4);b16(286,30);b16(288,38);b16(290,2);b16(292,2);
    b16(294,7);b16(296,8);b16(298,9);b16(300,0xff88);
    b16(302,1);b16(304,1);b16(306,1);
    b16(308,1);b16(310,1);b16(312,1);b16(314,1);
    b16(316,2);b16(318,1);b16(320,2);b16(322,2);b16(324,1);
    emit(((two_fn)0x82febe)(P(table)+308,1));
    emit(((two_fn)0x82febe)(P(table)+316,2));
    emit(((three_fn)0x830024)(P(info),1,2));
    emit(((three_fn)0x830442)(P(info),1,2));
    emit(((three_fn)0x8304a8)(P(info),1,2));
    emit(((three_fn)0x830024)(P(info),1,8));
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
