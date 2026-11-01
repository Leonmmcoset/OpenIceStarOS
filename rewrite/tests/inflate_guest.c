/* Stored, fixed and dynamic DEFLATE streams hand-built from RFC1951. */
#include <stdint.h>
static const uint8_t stream0[]={120,1,1,7,0,248,255,79,112,101,110,73,99,101,10,135,2,164};
static const uint8_t stream1[]={120,1,115,116,2,66,0,3,218,1,72};
static const uint8_t stream2[]={120,1,5,192,183,13,0,0,12,195,176,219,210,243,255,49,59,25,89,61,123,31,89,61,123,15,36,32,4,73};
#define P(a) ((uint32_t)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*zlib_fn)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef uint32_t (*bit_fn)(uint32_t,uint32_t);
typedef uint32_t (*bits_fn)(uint32_t,uint32_t,uint32_t);
typedef void (*tree_fn)(uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*build_fn)(uint32_t,uint32_t,uint32_t);
static uint32_t context[13],position,tree[3],nodes[8],lengths[4]={2,2,2,2};
static uint8_t output[512];
static void emit(uint32_t value){for(uint32_t i=0;i<4;++i){__asm__ volatile("outb %0,%1"::"a"((uint8_t)value),"Nd"((uint16_t)0xe9));value>>=8;}}
static void decode(const uint8_t *data,uint32_t size,uint32_t capacity,uint32_t length)
{
    context[7]=context[8]=0;
    for(uint32_t i=0;i<sizeof(output);++i)output[i]=0xa5;
    emit(((zlib_fn)0x839263)(P(context),P(output),capacity,P(data),size));
    emit(context[7]);emit(context[8]);
    uint32_t hash=2166136261;
    for(uint32_t i=0;i<length;++i)hash=(hash^output[i])*16777619u;
    emit(hash);emit(output[length]);
}
void fat_guest_entry(void)
{
    emit(0x44464c31);
    decode(stream0,sizeof(stream0),64,7);
    decode(stream1,sizeof(stream1),64,5);
    decode(stream2,sizeof(stream2),64,16);
    decode(stream0,sizeof(stream0),7,0);
    position=3;emit(((bit_fn)0x8380ef)(P(&position),P(stream0)));emit(position);
    position=1;emit(((bits_fn)0x838123)(P(&position),P(stream1),40));emit(position);
    ((tree_fn)0x83816d)(P(tree),P(nodes),4,2);emit(tree[1]);emit(tree[2]);
    context[7]=context[8]=0;((build_fn)0x83818d)(P(context),P(tree),P(lengths));
    for(uint32_t i=0;i<8;++i)emit(nodes[i]);
    emit(context[7]);emit(context[8]);
    for(;;)((void (*)(void))(uintptr_t)D(D(0x1b0e4)+108*4))();
}
