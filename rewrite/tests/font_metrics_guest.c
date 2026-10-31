/* Diagnostic X: original font loading/initialization and all nine new APIs. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*two_fn)(uint32_t,uint32_t);
typedef uint32_t (*three_fn)(uint32_t,uint32_t,uint32_t);
typedef uint32_t (*metrics_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*bounds_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef float (*scale_fn)(uint32_t,float);
typedef void (*no_fn)(void);
static uint32_t info[48];
static uint8_t svg[600];
volatile uint32_t metrics_report[25];

static void big16(uint32_t offset,uint32_t value)
{
    svg[offset]=(uint8_t)(value>>8);svg[offset+1]=(uint8_t)value;
}

void fat_guest_entry(void)
{
    metrics_report[0]=0x4d455431u;
    uint32_t font=((one_fn)0x827798)(P("C:\\ICESTAR\\FONT\\FIRA.TTF"));
    metrics_report[1]=font!=0;
    metrics_report[2]=((three_fn)0x82c3bc)(P(info),font,0);
    uint32_t values[4]={0,0,0,0};
    ((metrics_fn)0x83054a)(P(info),P(values),P(values+1),P(values+2));
    for(uint32_t i=0;i<3;++i)metrics_report[3+i]=values[i];
    metrics_report[6]=((metrics_fn)0x8305c0)(P(info),P(values),P(values+1),P(values+2));
    for(uint32_t i=0;i<3;++i)metrics_report[7+i]=values[i];
    ((bounds_fn)0x83066a)(P(info),P(values),P(values+1),P(values+2),P(values+3));
    for(uint32_t i=0;i<4;++i)metrics_report[10+i]=values[i];
    ((metrics_fn)0x82f958)(P(info),1,P(values),P(values+1));
    metrics_report[14]=values[0];metrics_report[15]=values[1];
    union {float value;uint32_t bits;} scale;
    scale.value=((scale_fn)0x8306ec)(P(info),24.0f);metrics_report[16]=scale.bits;
    scale.value=((scale_fn)0x83073a)(P(info),24.0f);metrics_report[17]=scale.bits;
    /* A minimal cmap format 0 and SVG list make all selection branches
     * independent of whether the shipped TrueType font contains SVG data. */
    info[1]=P(svg);info[11]=64;info[12]=256;
    big16(64,2);
    big16(66,1);big16(68,3);svg[73]=100;svg[77]=10;
    big16(78,5);big16(80,8);svg[85]=120;svg[89]=20;
    big16(256,0);big16(258,262);
    for(uint32_t i=0;i<256;++i)svg[262+i]=(uint8_t)i;
    metrics_report[18]=((two_fn)0x83077c)(P(info),2)==P(svg)+66u;
    uint32_t document=0;
    metrics_report[19]=((three_fn)0x83081c)(P(info),2,P(&document));
    metrics_report[20]=document==P(svg)+164u;
    document=0;
    metrics_report[21]=((three_fn)0x8308a0)(P(info),6,P(&document));
    metrics_report[22]=document==P(svg)+184u;
    metrics_report[23]=((two_fn)0x83077c)(P(info),4);
    info[11]=0;document=0x12345678;
    metrics_report[24]=((three_fn)0x83081c)(P(info),2,P(&document))==0 && document==0x12345678;
    for(uint32_t i=0;i<25;++i){
        uint32_t value=metrics_report[i];
        for(uint32_t j=0;j<4;++j){
            __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
            value>>=8;
        }
    }
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
