/* Test-only X: recovered curves, real heap and the original raster pipeline. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef void (*point_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef int32_t (*quad_fn)(uint32_t,uint32_t,float,float,float,float,float,float,float,int32_t);
typedef void (*cubic_fn)(uint32_t,uint32_t,float,float,float,float,float,float,float,float,float,int32_t);
typedef uint32_t (*flatten_fn)(uint32_t,int32_t,float,uint32_t,uint32_t,uint32_t);
typedef void (*free_fn)(uint32_t);
typedef void (*raster_fn)(uint32_t,float,uint32_t,int32_t,float,float,float,float,int32_t,int32_t,int32_t,uint32_t);
typedef void (*no_fn)(void);
typedef struct {int16_t x,y,cx,cy,cx1,cy1;uint8_t kind,padding;} Vertex;
static uint32_t points[64],count,lengths,contours,bitmap[4];
static uint8_t pixels[128];
static Vertex shape[]={
    {0,0,0,0,0,0,1,0},{8,8,0,8,0,0,3,0},{0,0,0,0,0,0,2,0},
    {0,0,0,0,0,0,1,0},{8,0,0,8,8,8,4,0},{0,0,0,0,0,0,2,0}};
static Vertex square[]={
    {1,0,0,0,0,0,1,0},{7,0,0,0,0,0,2,0},{7,4,0,0,0,0,2,0},
    {1,4,0,0,0,0,2,0},{1,0,0,0,0,0,2,0}};
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
void fat_guest_entry(void)
{
    emit(0x464c5431);
    ((point_fn)0x831ba8)(P(points),0,0x7f800001,0x80000000);
    emit(points[0]);emit(points[1]);
    emit(((quad_fn)0x831bdc)(P(points),P(&count),0,0,0,8,8,8,.125f,0));
    emit(count);for(uint32_t i=0;i<count*2;++i)emit(points[i]);
    count=0;
    ((cubic_fn)0x831d64)(P(points),P(&count),0,0,0,8,8,8,8,0,64,0);
    emit(count);for(uint32_t i=0;i<count*2;++i)emit(points[i]);
    count=0;
    emit(((quad_fn)0x831bdc)(P(points),P(&count),0,0,0,8,8,8,0,16));emit(count);
    ((cubic_fn)0x831d64)(P(points),P(&count),0,0,0,8,8,8,8,0,0,16);emit(count);
    uint32_t flattened=((flatten_fn)0x832000)(P(shape),6,8,P(&lengths),P(&contours),0);
    emit(flattened!=0);emit(contours);emit(D(lengths));emit(D(lengths+4));
    for(uint32_t i=0;i<14;++i)emit(D(flattened+i*4));
    ((free_fn)0x8175a4)(flattened);((free_fn)0x8175a4)(lengths);
    for(uint32_t i=0;i<128;++i)pixels[i]=0xa5;
    bitmap[0]=8;bitmap[1]=4;bitmap[2]=32;bitmap[3]=P(pixels);
    ((raster_fn)0x83244c)(P(bitmap),.35f,P(square),5,1,1,0,0,0,0,0,0);
    for(uint32_t i=0;i<128;++i)emit(pixels[i]);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
