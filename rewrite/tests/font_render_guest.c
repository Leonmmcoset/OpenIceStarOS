/* Test X: all glyph/codepoint bitmap entries, with real heap and raster calls. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*get_sub_fn)(uint32_t,float,float,float,float,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*get_fn)(uint32_t,float,float,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*make_sub_fn)(uint32_t,uint32_t,int32_t,int32_t,int32_t,float,float,float,float,uint32_t);
typedef void (*make_fn)(uint32_t,uint32_t,int32_t,int32_t,int32_t,float,float,uint32_t);
typedef void (*free_fn)(uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t info[40],box[4];
static uint8_t table[4096],pixels[128];
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
static uint32_t signature(uint32_t address,uint32_t count)
{
    uint32_t hash=2166136261u;
    for(uint32_t i=0;i<count;++i)hash=(hash^*(volatile uint8_t *)(uintptr_t)(address+i))*16777619u;
    return hash;
}
void fat_guest_entry(void)
{
    emit(0x524e4431);
    info[1]=P(table);info[3]=1;info[4]=128;info[6]=1024;info[12]=512;info[13]=1;
    table[135]=22;table[514]=1;table[515]=6; /* long loca; format-0 cmap */
    const uint8_t glyph[]={0,1,0,1,0,0,0,7,0,4,0,3,0,0,59,1,53,35,1,6,6,4};
    for(uint32_t i=0;i<sizeof(glyph);++i)table[1024+i]=glyph[i];
    const uint32_t entries[]={0x832544,0x832700,0x8328a8,0x8329ec,0x832756,0x832852,0x83298a,0x832a42};
    for(uint32_t i=0;i<8;++i){
        uint32_t character=(i==2||i==3||i==6||i==7)?65:0;
        if(i<4){
            uint32_t bitmap=(i&1)?((get_fn)entries[i])(P(info),1,1,character,P(box),P(box+1),P(box+2),P(box+3))
                :((get_sub_fn)entries[i])(P(info),1,1,0,0,character,P(box),P(box+1),P(box+2),P(box+3));
            emit(bitmap!=0);for(uint32_t j=0;j<4;++j)emit(box[j]);
            emit(signature(bitmap,42));((free_fn)0x832530)(bitmap,0);
        }else{
            for(uint32_t j=0;j<128;++j)pixels[j]=0xa5;
            if(i&1)((make_fn)entries[i])(P(info),P(pixels),8,4,32,1,1,character);
            else ((make_sub_fn)entries[i])(P(info),P(pixels),8,4,32,1,1,0,0,character);
            emit(signature(P(pixels),128));
        }
    }
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
