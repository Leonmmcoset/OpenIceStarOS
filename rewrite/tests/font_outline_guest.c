/* Test-only X: quadratic/CFF/CID helpers and real guest font pixel bounds. */
#include <stdint.h>
#include "../kernel/font_outline.h"
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef uint32_t (*close_fn)(uint32_t,uint32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t);
typedef KernelFontBuffer (*sub_fn)(KernelFontBuffer,int32_t);
typedef KernelFontBuffer (*cid_fn)(uint32_t,int32_t);
typedef void (*box_fn)(uint32_t,uint32_t,float,float,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*shift_fn)(uint32_t,uint32_t,float,float,float,float,uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*cff_box_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t info[48],values[4];
static uint8_t vertices[28],table[1024];
static const uint8_t subr_index[]={0,1,1,1,3,0xab,11};
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
static void buffer(uint32_t word,uint32_t offset,uint32_t size)
{
    info[word]=P(table+offset);info[word+1]=0;info[word+2]=size;
}
static void reset(void)
{
    for(uint32_t i=0;i<48;++i)info[i]=0;
    for(uint32_t i=0;i<1024;++i)table[i]=0;
}
void fat_guest_entry(void)
{
    emit(0x4f555431);
    for(uint32_t a=0;a<2;++a)for(uint32_t b=0;b<2;++b){
        for(uint32_t i=0;i<28;++i)vertices[i]=0xa5;
        emit(((close_fn)0x82d24a)(P(vertices),0,a,b,10,20,30,40,50,60));
        for(uint32_t i=0;i<7;++i)emit(D(P(vertices)+4*i));
    }
    KernelFontBuffer input={P(subr_index),19,sizeof(subr_index)};
    for(int32_t index=-108;index<=-107;++index){
        KernelFontBuffer result=((sub_fn)0x82e4ac)(input,index);
        emit(result.data?result.data-P(subr_index):0);emit(result.cursor);emit(result.size);
    }
    /* A single FD dictionary refers to private {Subrs=8} at offset 128. */
    reset();
    const uint8_t dict[]={0,1,1,1,5,141,247,20,18};
    for(uint32_t i=0;i<sizeof(dict);++i)table[64+i]=dict[i];
    table[128]=147;table[129]=19;
    const uint8_t local[]={0,1,1,1,5,149,159,5,11};
    for(uint32_t i=0;i<sizeof(local);++i)table[136+i]=local[i];
    info[14]=P(table);info[16]=sizeof(table);buffer(26,64,sizeof(dict));
    for(uint32_t format=0;format<=3;format+=3){
        table[256]=(uint8_t)format;
        if(format){table[257]=0;table[258]=1;table[259]=0;table[260]=0;table[261]=0;table[262]=0;table[263]=10;}
        buffer(29,256,format?8:2);
        KernelFontBuffer result=((cid_fn)0x82e552)(P(info),0);
        emit(result.data-P(table));emit(result.cursor);emit(result.size);
    }
    /* Type-2 interpreter calls recovered subroutines for local/global/CID. */
    const uint8_t program[]={0,1,1,1,12,139,139,21,32,10,14};
    for(uint32_t mode=0;mode<3;++mode){
        for(uint32_t i=0;i<sizeof(program);++i)table[512+i]=program[i];
        table[516]=7; /* INDEX end offset: six program bytes plus one. */
        table[521]=mode==1?29:10;
        buffer(17,512,sizeof(program));
        buffer(20,136,sizeof(local));buffer(23,136,sizeof(local));
        if(mode==2)info[20]=0;
        emit(((cff_box_fn)0x82f80a)(P(info),0,P(values),P(values+1),P(values+2),P(values+3)));
        for(uint32_t i=0;i<4;++i)emit(values[i]);
    }
    reset();
    uint32_t font=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    emit(font!=0);emit(((three_fn)0x82c3bc)(P(info),font,0));
    for(uint32_t entry=0;entry<4;++entry){
        uint32_t arg=entry<2?34:65;
        if(entry==0||entry==2)((shift_fn)(entry?0x830a92:0x8308d2))(P(info),arg,.03125f,.0625f,.75f,-.5f,P(values),P(values+1),P(values+2),P(values+3));
        else ((box_fn)(entry==1?0x830a3c:0x830af6))(P(info),arg,.03125f,.0625f,P(values),P(values+1),P(values+2),P(values+3));
        for(uint32_t i=0;i<4;++i)emit(values[i]);
    }
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
