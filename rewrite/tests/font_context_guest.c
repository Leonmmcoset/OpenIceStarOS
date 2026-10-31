/* Test-only X: execute six CFF drawing helpers and the real interpreter. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef void (*bounds_fn)(uint32_t,int32_t,int32_t);
typedef void (*step_fn)(uint32_t,float,float);
typedef void (*curve_fn)(uint32_t,float,float,float,float,float,float);
typedef void (*close_fn)(uint32_t);
typedef uint32_t (*box_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t context[12],info[48],box[4];
static uint8_t vertices[56];
static const uint8_t program[]={0,1,1,1,12,139,139,21,149,159,169,179,189,199,8,14};
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
void fat_guest_entry(void)
{
    emit(0x43545831);
    for(uint32_t mode=0;mode<=1;++mode){
        for(uint32_t i=0;i<12;++i)context[i]=0;
        for(uint32_t i=0;i<56;++i)vertices[i]=0xa5;
        context[0]=mode;context[10]=P(vertices);
        if(mode)((bounds_fn)0x82e05c)(P(context),2,-3);
        ((step_fn)0x82e27c)(P(context),1.0f,2.0f);
        ((step_fn)0x82e334)(P(context),3.0f,4.0f);
        ((curve_fn)0x82e3c8)(P(context),1.0f,1.0f,2.0f,2.0f,3.0f,3.0f);
        ((close_fn)0x82e1d6)(P(context));
        for(uint32_t i=0;i<12;++i)emit(i==10?context[i]==P(vertices):context[i]);
        for(uint32_t i=0;i<14;++i)emit(D(P(vertices)+i*4));
    }
    info[16]=1;info[17]=P(program);info[18]=0;info[19]=sizeof(program);
    emit(((box_fn)0x82f80a)(P(info),0,P(box),P(box+1),P(box+2),P(box+3)));
    for(uint32_t i=0;i<4;++i)emit(box[i]);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
