/* Test-only X: scanline helpers and the actual original raster driver. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*active_fn)(uint32_t,uint32_t,int32_t,float,uint32_t);
typedef void (*fill_fn)(uint32_t,int32_t,uint32_t,int32_t);
typedef void (*sort_fn)(uint32_t,int32_t);
typedef void (*cleanup_fn)(uint32_t,uint32_t);
typedef void (*raster_fn)(uint32_t,uint32_t,int32_t,int32_t,int32_t,int32_t,uint32_t);
typedef void (*no_fn)(void);
typedef struct {float x0,y0,x1,y1;uint32_t invert;} Edge;
static uint32_t heap[3],active_nodes[10],bitmap[4];
static Edge edges[25];
static uint8_t pixels[128];
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
static void sort(uint32_t entry,uint32_t count,uint32_t reverse)
{
    for(uint32_t i=0;i<count;++i){
        uint32_t value=reverse?count-i-1:i;
        edges[i].x0=(float)value;edges[i].y0=(float)value;
        edges[i].x1=(float)(value+100);edges[i].y1=(float)(value+200);edges[i].invert=value;
    }
    ((sort_fn)entry)(P(edges),count);
    for(uint32_t i=0;i<count;++i)emit(edges[i].invert);
}
void fat_guest_entry(void)
{
    emit(0x53434e31);
    edges[0]=(Edge){1.25f,-.5f,6.75f,3.5f,1};
    uint32_t active=((active_fn)0x830c5a)(P(heap),P(edges),1,.5f,0);
    emit(active!=0);
    for(uint32_t i=0;i<5;++i)emit(D(active+i*4));
    emit(heap[2]);((cleanup_fn)0x830c28)(P(heap),0);
    active_nodes[0]=P(active_nodes+5);active_nodes[1]=256;active_nodes[4]=1;
    active_nodes[5]=0;active_nodes[6]=1792;active_nodes[9]=0xffffffff;
    for(uint32_t i=0;i<32;++i)pixels[i]=0xa5;
    ((fill_fn)0x830dc4)(P(pixels),8,P(active_nodes),63);
    for(uint32_t i=0;i<8;++i)emit(pixels[i]);
    sort(0x831340,25,1);sort(0x831480,13,0);sort(0x831858,25,1);
    edges[0]=(Edge){1.25f,0.f,1.25f,16.f,1};
    edges[1]=(Edge){6.75f,0.f,6.75f,16.f,0};
    edges[2]=(Edge){0};
    ((sort_fn)0x831858)(P(edges),2);
    for(uint32_t i=0;i<128;++i)pixels[i]=0xa5;
    bitmap[0]=8;bitmap[1]=4;bitmap[2]=32;bitmap[3]=P(pixels);
    ((raster_fn)0x830f24)(P(bitmap),P(edges),2,4,0,0,0);
    for(uint32_t i=0;i<128;++i)emit(pixels[i]);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
