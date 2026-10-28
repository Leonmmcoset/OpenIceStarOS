#include "raster_fixture.h"
#include "../kernel/jpeg_bits.h"
static uint32_t field,budget,refills,skips_ff,eofs,updates,changed_words,stream_calls,guard_faults,guard_safe;
static uint8_t output_reference[0x100000],work_reference[65536];
static int read8(uint32_t a,uint32_t *v)
{
    uint32_t i=index_of(a);if(i==UINT32_MAX)return 0;
    *v=expected[i];return 1;
}
static int read32(uint32_t a,uint32_t *v)
{
    *v=0;
    for(unsigned i=0;i<4;++i){uint32_t b;if(!read8(a+i,&b))return 0;*v|=b<<(i*8u);}
    return 1;
}
static uint32_t arg(unsigned i){return get32(expected,ARGUMENTS+i*4u);}
static int write32(uint32_t a,uint32_t v)
{if(!put(expected,a,v,4))return 0;++updates;return 1;}
static int model(uint32_t *result)
{
    uint32_t reservoir,bits,current,end,byte;
    if(!read32(arg(0)+0x5be8u,&reservoir)||!read32(arg(0)+0x5becu,&bits))return 0;
    while(signed32(bits)<=16){
        if(++budget>128u)return 0;
        if(!read32(arg(0)+0x5be0u,&current)||!read32(arg(0)+0x5be4u,&end))return 0;
        if(current>=end){*result=0;++eofs;return 1;}
        if(!write32(arg(0)+0x5be0u,current+1u)||!read8(current,&byte))return 0;
        if(byte==255u){
            if(!read32(arg(0)+0x5be0u,&current)||!read32(arg(0)+0x5be4u,&end))return 0;
            if(current>=end){*result=0;++eofs;return 1;}
            if(!write32(arg(0)+0x5be0u,current+1u))return 0;
            ++skips_ff;
        }
        reservoir=(reservoir*256u)|byte;bits+=8u;++refills;
    }
    /* Reconstruct the selected bits individually, using masked x86 shifts. */
    uint32_t shift=(bits-arg(1))%32u,mask_bits=arg(1)%32u,value=0;
    for(unsigned i=0;i<mask_bits&&i<16u;++i)
        if(i+shift<32u && (reservoir&(1u<<(i+shift))))value|=1u<<i;
    bits-=arg(1);
    if(!write32(arg(0)+0x5becu,bits)||!write32(arg(0)+0x5be8u,reservoir))return 0;
    *result=value;return 1;
}
static void setup(unsigned alignment,uint32_t pointer,uint32_t end,uint32_t bits,uint32_t reservoir)
{
    pattern();field=DATA+0x101u+alignment;args[0]=field-0x5be0u;
    for(unsigned i=2;i<6;++i)args[i]=random32();
    REQUIRE(put(initial,field,pointer,4)&&put(initial,field+4u,end,4)&&
        put(initial,field+8u,reservoir,4)&&put(initial,field+12u,bits,4),"bit reader state");
}
static void exercise(unsigned sequence)
{
    scenario=cases+skipped;budget=0;begin_oracle();
    uint32_t result[8],saved[]={refills,skips_ff,eofs,updates};
    for(unsigned call=0;call<sequence;++call)if(!model(&result[call])){
        refills=saved[0];skips_ff=saved[1];eofs=saved[2];updates=saved[3];++skipped;return;
    }
    for(implementation=0;implementation<3;++implementation){
        REQUIRE(load(implementation?"build/jpeg_bits_candidate.x":"build/jpeg_bits_original.x",0x800000),"JPEG bit images");
        reset();uint32_t actual[7],incoming[6];
        for(unsigned i=0;i<6;++i)incoming[i]=args[i];
        for(unsigned call=0;call<sequence;++call){
            REQUIRE(!raster_checked_call(implementation==1u?(void *)kernel_jpeg_bits_83a835:(void *)0x83a835,incoming,actual),"JPEG bit ABI");
            REQUIRE(actual[0]==result[call],"JPEG bit sequential EAX");
            for(unsigned i=0;i<6;++i)incoming[i]=actual[i+1u];
        }
        compare(actual,result[sequence-1u]);
    }
    for(unsigned i=0;i<6;++i)if(arg(i)!=args[i])++changed_words;
    stream_calls+=sequence;++cases;
}
static void guards(void)
{
    stage=9;
    for(unsigned probe=0;probe<3;++probe){
        args[1]=8;setup(0,DATA+16384u,DATA+16385u,probe==2u?24u:0u,0x12345678u);
        if(probe==1u)REQUIRE(put(initial,field+4u,DATA+16384u,4),"EOF guard");
        for(implementation=0;implementation<3;++implementation){
            REQUIRE(load(implementation?"build/jpeg_bits_candidate.x":"build/jpeg_bits_original.x",0x800000),"JPEG bit guard image");reset();
            int pid=syscall3(2,0,0,0);REQUIRE(pid>=0,"JPEG guard fork");
            if(!pid){
                uint32_t actual[7];raster_checked_call(implementation==1u?(void *)kernel_jpeg_bits_83a835:(void *)0x83a835,args,actual);
                uint32_t result=probe==2u?0x34u:0u;
                syscall3(1,actual[0]==result?0u:1u,0,0);
            }
            uint32_t status=0;REQUIRE(syscall3(7,(uint32_t)pid,(uint32_t)&status,0)==pid,"JPEG guard wait");
            if(probe){REQUIRE(!status,"JPEG EOF/reservoir avoids source read");++guard_safe;}
            else{REQUIRE((status&127u)==11u,"JPEG source read remains unchecked");++guard_faults;}
        }
    }
}
static void jpeg_composition(void)
{
    stage=10;
    REQUIRE(map(0x1000000,65536)&&map(0x1100000,65536)&&map(0x1200000,0x100000),"JPEG composed mappings");
    for(implementation=0;implementation<2;++implementation){
        REQUIRE(load(implementation?"build/jpeg_bits_candidate.x":"build/jpeg_bits_original.x",0x800000),"JPEG composed image");
        int fd=syscall3(5,(uint32_t)"build/jpeg_bits_fixture.jpg",0,0);REQUIRE(fd>=0,"JPEG original file");
        REQUIRE(syscall3(3,fd,0x1100000,65536)==JPEG_FILE_BYTES,"JPEG file read");syscall3(6,fd,0,0);
        for(unsigned i=0;i<65536;++i)*(volatile uint8_t *)(0x1000000u+i)=(uint8_t)(i*19u+71u);
        for(unsigned i=0;i<0x100000;++i)*(volatile uint8_t *)(0x1200000u+i)=0x6d;
        args[0]=0x1000000;args[1]=DATA+0x3000u;args[2]=JPEG_FILE_BYTES;args[3]=0x1100000;args[4]=args[5]=0;
        uint32_t actual[7];REQUIRE(!raster_checked_call((void *)0x83a70f,args,actual)&&actual[0]==1u,"real JPEG info");
        REQUIRE(*(volatile uint32_t *)(DATA+0x3000u)==2u&&*(volatile uint32_t *)(DATA+0x3008u)==JPEG_WIDTH&&
            *(volatile uint32_t *)(DATA+0x300cu)==JPEG_HEIGHT,"real JPEG dimensions");
        args[1]=JPEG_FILE_BYTES;args[2]=0x1100000;args[3]=4;args[4]=0x1200000;args[5]=0;
        REQUIRE(!raster_checked_call((void *)0x83a7a1,args,actual)&&!actual[0],"real JPEG decode");
        uint32_t changed=0;
        for(unsigned i=0;i<0x100000;++i){
            uint8_t pixel=*(volatile uint8_t *)(0x1200000u+i);
            if(pixel!=0x6du)++changed;
            if(!implementation)output_reference[i]=pixel;else REQUIRE(pixel==output_reference[i],"composed JPEG pixels/slack");
        }
        REQUIRE(changed>JPEG_WIDTH*JPEG_HEIGHT,"JPEG decode wrote image");
        for(unsigned i=0;i<65536;++i){
            uint8_t byte=*(volatile uint8_t *)(0x1000000u+i);
            if(!implementation)work_reference[i]=byte;else REQUIRE(byte==work_reference[i],"composed JPEG workspace");
        }
    }
}
void _start(void)
{
    mappings();stage=1;
    static const uint32_t bit_counts[]={0,1,8,16,17,24,31,32,0x7fffffff,0x80000000,0xfffffff0,0xffffffff};
    static const uint32_t amounts[]={0,1,7,8,15,16,17,24,31,32,33,0x7fffffff,0x80000000,0xffffffff};
    for(unsigned alignment=0;alignment<4;++alignment)for(unsigned bits=0;bits<12;++bits)
      for(unsigned amount=0;amount<14;++amount)for(unsigned length=0;length<6;++length){
        args[1]=amounts[amount];setup(alignment,DATA+0x500u,DATA+0x500u+length,bit_counts[bits],random32());exercise(1);
    }
    stage=2;
    for(unsigned byte=0;byte<256;++byte)for(unsigned next=0;next<4;++next){
        args[1]=8;setup(byte&3u,DATA+0x500u,DATA+0x508u,0,random32());
        REQUIRE(put(initial,DATA+0x500u,byte,1)&&put(initial,DATA+0x501u,next?next==1u?255u:0xd9u:0u,1),"JPEG byte/stuffing");exercise(4);
    }
    stage=3;
    for(unsigned alignment=0;alignment<4;++alignment)for(unsigned offset=0;offset<24;++offset)
      for(unsigned amount=0;amount<14;++amount){
        uint32_t pointer=DATA+0x101u+alignment+offset;
        args[1]=amounts[amount];setup(alignment,pointer,pointer+7u,0,random32());exercise(2);
    }
    stage=4;
    for(unsigned bits=0;bits<12;++bits)for(unsigned amount=0;amount<14;++amount)
      for(unsigned length=0;length<6;++length){
        setup(0,DATA+0x500u,DATA+0x506u,0,random32());
        field=ARGUMENTS+8u;args[0]=field-0x5be0u;args[1]=amounts[amount];
        args[2]=DATA+0x500u;args[3]=args[2]+length;args[4]=random32();args[5]=bit_counts[bits];exercise(2);
    }
    guards();jpeg_composition();
    REQUIRE(refills&&skips_ff&&eofs&&updates,"JPEG bit coverage");
    PRINT("JPEG bit scenarios/executions/calls/excluded: ");number(cases);PRINT(" ");number(cases*3u);PRINT(" ");number(stream_calls*3u);PRINT(" ");number(skipped);
    PRINT("; refill/FF skip/EOF/state stores/changed caller: ");number(refills);PRINT(" ");number(skips_ff);PRINT(" ");number(eofs);PRINT(" ");number(updates);PRINT(" ");number(changed_words);
    PRINT("; fault/safe probes: ");number(guard_faults);PRINT(" ");number(guard_safe);
    PRINT("; real JPEG composed executions/dimensions: 2 ");number(JPEG_WIDTH);PRINT(" ");number(JPEG_HEIGHT);
    PRINT("; jpeg bits verified with bytewise state/source/caller/EAX/ABI, real info/decode and full pixel/workspace comparisons; no helper substitutions\n");
    syscall3(1,0,0,0);
}
