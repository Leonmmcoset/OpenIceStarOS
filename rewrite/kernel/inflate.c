/* Original DEFLATE/zlib control flow at 0x8380ef..0x8393a1. Bounds, error
 * source lines, cyclic backreference copy and unchecked input reads are kept.
 * The original constant Huffman/length/distance data remains at its ABI VA. */
#include "inflate.h"
#include "memory_ops.h"
typedef uint32_t alias32 __attribute__((may_alias));
#define D(a) (*(volatile alias32 *)(uintptr_t)(uint32_t)(a))
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define P(a) ((uint32_t)(uintptr_t)(a))
static __attribute__((always_inline)) inline uint32_t error(uint32_t p,uint32_t line)
{
    D(p+28)=4;D(p+32)=line;return D(p+28);
}
uint32_t kernel_png_bit_8380ef(volatile uint32_t position,volatile uint32_t input)
{
    uint8_t byte=B(input+(D(position)>>3));
    uint32_t value=(byte>>(D(position)&7u))&1u;
    D(position)=D(position)+1u;return value;
}
uint32_t kernel_png_bits_838123(volatile uint32_t position,volatile uint32_t input,
    volatile uint32_t count)
{
    uint32_t value=0;
    for(uint32_t i=0;i<count;++i)value|=kernel_png_bit_8380ef(position,input)<<(i&31u);
    return value;
}
void kernel_png_tree_init_83816d(volatile uint32_t tree,volatile uint32_t nodes,
    volatile uint32_t symbols,volatile uint32_t maxbits)
{
    D(tree)=nodes;D(tree+8)=symbols;D(tree+4)=maxbits;
}
void kernel_png_tree_build_83818d(volatile uint32_t p,volatile uint32_t tree,
    volatile uint32_t lengths)
{
    uint32_t counts[16],next[16],codes[288],filled=0,node=0;
    kernel_fill_bytes(P(counts),0,60);kernel_fill_bytes(P(next),0,64);
    /* The original leaves count[15] uninitialized, then increments it.
     * That count is never consumed (the recurrence reads only 0..14).
     * Initialize the unused C slot to avoid evaluating an indeterminate value. */
    counts[15]=0;
    for(uint32_t i=0;i<D(tree+8);++i)++counts[D(lengths+i*4u)];
    for(uint32_t i=1;i<=D(tree+4);++i)next[i]=(next[i-1]+counts[i-1])*2u;
    for(uint32_t i=0;i<D(tree+8);++i){
        uint32_t length=D(lengths+i*4u);
        if(length)codes[i]=next[length]++;
    }
    for(uint32_t i=0;i<D(tree+8)*2u;++i)D(D(tree)+i*4u)=0x7fff;
    for(uint32_t symbol=0;symbol<D(tree+8);++symbol){
        for(uint32_t bit=0;bit<D(lengths+symbol*4u);++bit){
            uint8_t value=(codes[symbol]>>((D(lengths+symbol*4u)-bit-1u)&31u))&1u;
            if(node>D(tree+8)-2u){error(p,0xaf);return;}
            uint32_t index=node*2u+value;
            if(D(D(tree)+index*4u)==0x7fff){
                if(bit+1u==D(lengths+symbol*4u)){D(D(tree)+index*4u)=symbol;node=0;}
                else{++filled;D(D(tree)+index*4u)=filled+D(tree+8);node=filled;}
            }else node=D(D(tree)+index*4u)-D(tree+8);
        }
    }
    for(uint32_t i=0;i<D(tree+8)*2u;++i)if(D(D(tree)+i*4u)==0x7fff)D(D(tree)+i*4u)=0;
}
uint32_t kernel_png_symbol_838519(volatile uint32_t p,volatile uint32_t input,
    volatile uint32_t position,volatile uint32_t tree,volatile uint32_t size)
{
    uint32_t node=0;
    for(;;){
        if(!(D(position)&7u) && (D(position)>>3)>size){error(p,0xd1);return 0;}
        uint8_t bit=kernel_png_bit_8380ef(position,input);
        uint32_t value=D(D(tree)+(node*2u+bit)*4u);
        if(value<D(tree+8))return value;
        node=value-D(tree+8);
        if(node>=D(tree+8)){error(p,0xde);return 0;}
    }
}
void kernel_png_dynamic_8385df(volatile uint32_t p,volatile uint32_t lit,
    volatile uint32_t dist,volatile uint32_t code,volatile uint32_t input,
    volatile uint32_t position,volatile uint32_t size)
{
    if((D(position)>>3)>=size-2u){error(p,0xf0);return;}
    struct {uint32_t distance[32],literal[288],codes[19];} lengths;
    kernel_fill_bytes(P(lengths.literal),0,1152);kernel_fill_bytes(P(lengths.distance),0,128);
    uint32_t nl=kernel_png_bits_838123(position,input,5)+257u;
    uint32_t nd=kernel_png_bits_838123(position,input,5)+1u;
    uint32_t nc=kernel_png_bits_838123(position,input,4)+4u;
    for(uint32_t i=0;i<19;++i){
        uint32_t index=D(0x83c22fu+i*4u);
        lengths.codes[index]=i<nc?kernel_png_bits_838123(position,input,3):0;
    }
    kernel_png_tree_build_83818d(p,code,P(lengths.codes));
    if(D(p+28))return;
    uint32_t i=0;
    while(i<nl+nd){
        uint32_t symbol=kernel_png_symbol_838519(p,input,position,code,size);
        if(D(p+28))break;
        if(symbol<=15){
            if(i<nl)lengths.literal[i]=symbol;else lengths.distance[i-nl]=symbol;
            ++i;
        }else if(symbol==16 || symbol==17 || symbol==18){
            if((D(position)>>3)>=size){error(p,symbol==16?0x120:symbol==17?0x13e:0x158);break;}
            uint32_t repeat=(symbol==18?11u:3u)+kernel_png_bits_838123(position,input,symbol==16?2:symbol==17?3:7);
            uint32_t value=0;
            if(symbol==16)value=i-1u<nl?D(P(lengths.literal)+(i-1u)*4u):D(P(lengths.distance)+(i-nl-1u)*4u);
            for(uint32_t n=0;n<repeat;++n){
                if(i>=nl+nd){error(p,symbol==16?0x130:symbol==17?0x149:0x162);break;}
                if(i<nl)lengths.literal[i]=value;else lengths.distance[i-nl]=value;
                ++i;
            }
        }else{error(p,0x16d);break;}
    }
    if(!D(p+28) && !lengths.literal[256])error(p,0x173);
    if(!D(p+28))kernel_png_tree_build_83818d(p,lit,P(lengths.literal));
    if(!D(p+28))kernel_png_tree_build_83818d(p,dist,P(lengths.distance));
}
void kernel_png_compressed_838bc3(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t position,
    volatile uint32_t written,volatile uint32_t size,volatile uint32_t type)
{
    uint32_t literal_nodes[576],distance_nodes[64],code_nodes[38];
    uint32_t literal[3],distance[3],code[3];
    if(type==1){
        kernel_png_tree_init_83816d(P(literal),0x83c28f,288,15);
        kernel_png_tree_init_83816d(P(distance),0x83cb8f,32,15);
    }else if(type==2){
        kernel_png_tree_init_83816d(P(literal),P(literal_nodes),288,15);
        kernel_png_tree_init_83816d(P(distance),P(distance_nodes),32,15);
        kernel_png_tree_init_83816d(P(code),P(code_nodes),19,7);
        kernel_png_dynamic_8385df(p,P(literal),P(distance),P(code),input,position,size);
    }
    for(;;){
        uint32_t symbol=kernel_png_symbol_838519(p,input,position,P(literal),size);
        if(D(p+28))return;
        if(symbol==256)return;
        if(symbol<=255){
            if(D(written)>=capacity){error(p,0x1a6);return;}
            uint32_t where=D(written)+output;
            B(where)=symbol;D(written)=D(written)+1u;
        }else if(symbol<=285){
            uint32_t length=D(0x83bc2bu+symbol*4u),extra=D(0x83bcabu+symbol*4u);
            if((D(position)>>3)>=size){error(p,0x1b7);return;}
            length+=kernel_png_bits_838123(position,input,extra);
            uint32_t d=kernel_png_symbol_838519(p,input,position,P(distance),size);
            if(D(p+28))return;
            if(d>29){error(p,0x1c4);return;}
            uint32_t distance_base=D(0x83c12fu+d*4u),distance_extra=D(0x83c1afu+d*4u);
            if((D(position)>>3)>=size){error(p,0x1cf);return;}
            distance_base+=kernel_png_bits_838123(position,input,distance_extra);
            uint32_t start=D(written),back=start-distance_base;
            if(D(written)+length>=capacity){error(p,0x1da);return;}
            for(uint32_t i=0;i<length;++i){
                uint32_t where=D(written)+output;
                B(where)=B(output+back);D(written)=D(written)+1u;++back;
                if(back>=start)back=start-distance_base;
            }
        }
    }
}
void kernel_png_stored_838faf(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t position,
    volatile uint32_t written,volatile uint32_t size)
{
    while(D(position)&7u)D(position)=D(position)+1u;
    uint32_t byte=D(position)>>3;
    if(byte>=size-4u){error(p,0x1f8);return;}
    uint32_t length=B(input+byte)+((uint32_t)B(input+byte+1u)<<8);byte+=2;
    uint32_t inverse=B(input+byte)+((uint32_t)B(input+byte+1u)<<8);byte+=2;
    if(length+inverse!=65535){error(p,0x203);return;}
    if(D(written)+length>=capacity){error(p,0x208);return;}
    if(byte+length>size){error(p,0x20e);return;}
    for(uint32_t i=0;i<length;++i){
        uint32_t dest=D(written)+output,source=input+byte;++byte;
        B(dest)=B(source);D(written)=D(written)+1u;
    }
    D(position)=byte*8u;
}
uint32_t kernel_png_inflate_8390ed(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t size,
    volatile uint32_t offset)
{
    uint32_t position=0,written=0,final=0;
    while(!final){
        if((position>>3)>=size)return error(p,0x227);
        final=kernel_png_bit_8380ef(P(&position),input+offset);
        uint32_t type=kernel_png_bit_8380ef(P(&position),input+offset);
        type|=kernel_png_bit_8380ef(P(&position),input+offset)*2u;
        if(type==3)return error(p,0x231);
        if(!type)kernel_png_stored_838faf(p,output,capacity,input+offset,P(&position),P(&written),size);
        else kernel_png_compressed_838bc3(p,output,capacity,input+offset,P(&position),P(&written),size,type);
        if(D(p+28))return D(p+28);
    }
    return D(p+28);
}
uint32_t kernel_png_zlib_839263(volatile uint32_t p,volatile uint32_t output,
    volatile uint32_t capacity,volatile uint32_t input,volatile uint32_t size)
{
    if(size<=1)return error(p,0x246);
    if((((uint32_t)B(input)<<8)+B(input+1))%31u)return error(p,0x24c);
    if((B(input)&15u)!=8 || (B(input)>>4)>7)return error(p,0x252);
    if((B(input+1)>>5)&1u)return error(p,0x258);
    kernel_png_inflate_8390ed(p,output,capacity,input,size,2);return D(p+28);
}
