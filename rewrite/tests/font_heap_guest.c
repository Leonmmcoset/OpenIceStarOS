/* Test-only X: rasterizer arenas use the real guest malloc/free services. */
#include <stdint.h>
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*alloc_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*free_fn)(uint32_t,uint32_t);
typedef void (*no_fn)(void);
static uint32_t heap[3];
static void emit(uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        __asm__ volatile("outb %0, %1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));
        value>>=8;
    }
}
void fat_guest_entry(void)
{
    emit(0x48454131);
    for(uint32_t size=20;size<=128;size=size==20?32:129){
        for(uint32_t i=0;i<3;++i)heap[i]=0;
        uint32_t first=((alloc_fn)0x830b4c)(P(heap),size,123);
        emit(first!=0);emit(heap[2]);
        uint32_t second=((alloc_fn)0x830b4c)(P(heap),size,456);
        emit(first-second==size);emit(heap[2]);
        ((free_fn)0x830c0e)(P(heap),first);
        uint32_t reused=((alloc_fn)0x830b4c)(P(heap),size,789);
        emit(reused==first);emit(heap[2]);
        uint32_t chunk=heap[0];
        ((free_fn)0x830c28)(P(heap),0);
        emit(D(chunk-8)==0);emit(heap[0]==chunk);
    }
    for(uint32_t i=0;i<3;++i)heap[i]=0;
    uint32_t first=((alloc_fn)0x830b4c)(P(heap),128,0);
    emit(first!=0);emit(heap[2]);
    ((free_fn)0x830c28)(P(heap),0);
    for(;;)((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
