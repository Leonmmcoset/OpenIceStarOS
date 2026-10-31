/* Test-only X: call original addresses, including aggregate-return entries. */
#include <stdint.h>
#include "../kernel/font_parse.h"
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*two_fn)(uint32_t,uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*seek_fn)(uint32_t,int32_t);
typedef KernelFontBuffer (*buffer_fn)(uint32_t,int32_t);
typedef KernelFontBuffer (*range_fn)(uint32_t,int32_t,int32_t);
typedef KernelFontBuffer (*aggregate_fn)(uint32_t);
typedef KernelFontBuffer (*dict_fn)(uint32_t,uint32_t);
typedef KernelFontBuffer (*item_fn)(KernelFontBuffer,int32_t);
typedef KernelFontBuffer (*subrs_fn)(KernelFontBuffer,KernelFontBuffer);
typedef void (*ints_fn)(uint32_t,uint32_t,int32_t,uint32_t);
typedef void (*no_fn)(void);
static const uint8_t bytes[] = {0x12,0x34,0x56,0x78};
static const uint8_t index_bytes[] = {0,2,1,1,4,6,'a','b','c','d','e'};
static const uint8_t integer[] = {247,0};
static const uint8_t real[] = {30,0x12,0x3f};
static const uint8_t dict_bytes[] = {247,0,254,255,18};
static const uint8_t private_bytes[] = {141,171,18};
static uint8_t font_data[128];
static uint32_t info[48];
volatile uint32_t font_report[35];

static KernelFontBuffer buffer(const uint8_t *data,uint32_t size)
{
    return (KernelFontBuffer){P(data),0,(int32_t)size};
}

void fat_guest_entry(void)
{
    font_report[0]=0x464e5431u;
    KernelFontBuffer b=buffer(bytes,sizeof(bytes)),r;
    font_report[1]=((one_fn)0x82b838)(P(&b));
    font_report[2]=((one_fn)0x82b86e)(P(&b));
    ((seek_fn)0x82b8a2)(P(&b),2);
    font_report[3]=(uint32_t)b.cursor;
    ((seek_fn)0x82b8de)(P(&b),-1);
    font_report[4]=(uint32_t)b.cursor;
    font_report[5]=((two_fn)0x82b8fe)(P(&b),2);
    r=((buffer_fn)0x82b946)(P(bytes),4);
    font_report[6]=r.data==P(bytes) && r.cursor==0 && r.size==4;
    r=((range_fn)0x82b962)(P(&b),1,2);
    font_report[7]=r.data==P(bytes)+1u && r.cursor==0 && r.size==2;
    b=buffer(index_bytes,sizeof(index_bytes));
    r=((aggregate_fn)0x82b9ca)(P(&b));
    font_report[8]=r.data==P(index_bytes) && r.cursor==0 && r.size==11;
    font_report[9]=(uint32_t)b.cursor;
    b=buffer(integer,sizeof(integer));
    font_report[10]=((one_fn)0x82ba70)(P(&b));
    b=buffer(real,sizeof(real));
    ((one_fn)0x82bb70)(P(&b));
    font_report[11]=(uint32_t)b.cursor;
    b=buffer(dict_bytes,sizeof(dict_bytes));
    r=((dict_fn)0x82bbe4)(P(&b),18);
    font_report[12]=r.data==P(dict_bytes) && r.cursor==0 && r.size==4;
    uint32_t values[2]={0,0};
    ((ints_fn)0x82bcc8)(P(&b),18,2,P(values));
    font_report[13]=values[0];font_report[14]=values[1];
    b=buffer(index_bytes,sizeof(index_bytes));
    font_report[15]=((one_fn)0x82bd28)(P(&b));
    r=((item_fn)0x82bd56)(b,1);
    font_report[16]=r.data==P(index_bytes)+9u && r.cursor==0 && r.size==2;
    font_report[17]=((one_fn)0x82be14)(P(bytes));
    font_report[18]=((one_fn)0x82be34)(P(dict_bytes));
    font_report[19]=((one_fn)0x82be52)(P(bytes));
    font_report[20]=((one_fn)0x82be86)(P(dict_bytes));
    font_data[32]=155;font_data[33]=19;
    for (uint32_t i=0;i<sizeof(index_bytes);++i)font_data[48+i]=index_bytes[i];
    b=buffer(font_data,sizeof(font_data));
    KernelFontBuffer dictionary=buffer(private_bytes,sizeof(private_bytes));
    r=((subrs_fn)0x82c232)(b,dictionary);
    font_report[21]=r.data==P(font_data)+48u && r.cursor==0 && r.size==11;
    uint32_t font=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    font_report[22]=font!=0;
    font_report[23]=((one_fn)0x82beba)(font);
    font_report[24]=((two_fn)0x82c0b2)(font,0);
    font_report[25]=((one_fn)0x82c196)(font);
    font_report[26]=((three_fn)0x82bfe6)(font,0,P("head"));
    info[1]=font;info[2]=0;info[11]=0xffffffffu;
    font_report[27]=((one_fn)0x82c34c)(P(info));
    font_report[28]=info[11];
    /* Execute the unrecovered full font initializer with recovered helpers,
     * using the original shipped font rather than a parser-only fixture. */
    font_report[29]=((three_fn)0x82c3bc)(P(info),font,0);
    font_report[30]=info[3];
    font_report[31]=info[4];
    font_report[32]=info[5];
    font_report[33]=info[6];
    font_report[34]=info[7];
    for(uint32_t i=0;i<35;++i){
        uint32_t value=font_report[i];
        for(uint32_t j=0;j<4;++j){
            __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
            value>>=8;
        }
    }
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
