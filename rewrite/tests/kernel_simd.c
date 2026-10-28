/* Independent lane oracle plus original / ordinary / fixed-address machine
 * execution. No helper calls or CPU instructions are substituted. */
#include <stdint.h>
#include "../kernel/simd.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))

uint32_t simd_entry,simd_stack,simd_flags,simd_mxcsr,simd_arguments[5];
uint32_t simd_xmm_seed[32],simd_machine[14];
uint8_t simd_fxstate[512] __attribute__((aligned(16)));
extern void simd_capture(void);
static uint32_t function,stage,cases[4],executions,service_cases,implementation;
static uint32_t data_base=0x600100,random_state=0xfedc1234;
static uint8_t initial[256],expected[256],reference_fxstate[512];
static uint32_t reference_machine[14],reference_table[320];
static const uint32_t entries[]={0x81ab34,0x81ab58,0x81ab7c,0x81abc6};
static const uint32_t ordinary[]={
    (uint32_t)kernel_simd_add_81ab34,(uint32_t)kernel_simd_subtract_81ab58,
    (uint32_t)kernel_simd_multiply_81ab7c,(uint32_t)kernel_simd_blend_81abc6};
static int sys3(unsigned n,unsigned a,unsigned b,unsigned c)
{
    int r;
    __asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");
    return r;
}
static void print(const char *text)
{
    unsigned n=0;while(text[n])++n;sys3(4,1,(uint32_t)text,n);
}
static void number(uint32_t n)
{
    char s[12];unsigned i=sizeof(s);s[--i]=0;
    do{s[--i]=(char)('0'+n%10);n/=10;}while(n);print(s+i);
}
static void fail(const char *text,uint32_t index)
{
    print(text);print(" stage/function/case/implementation/index: ");
    number(stage);print("/");number(function);print("/");number(cases[function]);
    print("/");number(implementation);print("/");number(index);print("\n");
    sys3(1,200,0,0);__builtin_unreachable();
}
#define REQUIRE(p,msg) do{if(!(p))fail(msg,0);}while(0)
static int map(uint32_t address,unsigned size)
{
    uint32_t args[]={address,size,7,0x32,0xffffffff,0};
    return sys3(90,(uint32_t)args,0,0)==(int)address;
}
static int load(const char *path,uint32_t base)
{
    int fd=sys3(5,(uint32_t)path,0,0),n;
    if(fd<0)return 0;
    n=sys3(3,(unsigned)fd,base,250099);sys3(6,(unsigned)fd,0,0);return n==250099;
}
static uint32_t random32(void)
{
    random_state^=random_state<<13;random_state^=random_state>>17;
    random_state^=random_state<<5;return random_state;
}
static void prepare(uint32_t first,uint32_t second,uint32_t third)
{
    for(unsigned i=0;i<sizeof(initial);++i)initial[i]=(uint8_t)random32();
    for(unsigned i=0;i<COUNT(simd_xmm_seed);++i)simd_xmm_seed[i]=random32();
    simd_arguments[0]=data_base+first;simd_arguments[1]=data_base+second;
    simd_arguments[2]=data_base+third;
    simd_arguments[3]=0x13579bdf;simd_arguments[4]=0x2468ace0;
    simd_stack=0x50f040u+(cases[function]%16u);
    simd_flags=0x202u|((cases[function]&1u)?0xcd5u:0x44u);
    simd_mxcsr=0x1f80u|((cases[function]%4u)<<13)|(cases[function]&63u);
}
static void oracle(void)
{
    unsigned first=simd_arguments[0]-data_base,second=simd_arguments[1]-data_base;
    unsigned third=simd_arguments[2]-data_base;
    for(unsigned i=0;i<sizeof(expected);++i)expected[i]=initial[i];
    for(unsigned i=0;i<16;++i){
        unsigned left=initial[first+i],right=initial[second+i],value;
        if(function==0)value=left+right;
        else if(function==1)value=left-right;
        else if(function==2){
            unsigned product=left*right;
            value=product>=32768u?0u:product>255u?255u:product;
        }else{
            /* Each DWORD's shifted OR is derived independently by channel,
             * using only pre-store bytes, including overlapping inputs. */
            unsigned at=third+(i&~3u),channel=i%4u;
            unsigned weight=0,begin=channel==3u?1u:0u;
            for(unsigned j=begin;j<=channel;++j)weight|=initial[at+j];
            unsigned constant=channel==3u?0u:128u;
            unsigned inverse=(constant-weight)&255u;
            unsigned sum=(left*weight+right*inverse)&65535u;
            value=sum>>7;if(value>255u)value=255u;
        }
        expected[(function==3?first:third)+i]=(uint8_t)value;
    }
}
static void reset(void)
{
    for(unsigned i=0;i<sizeof(initial);++i)BYTE(data_base+i)=initial[i];
    for(unsigned i=0;i<sizeof(simd_fxstate);++i)simd_fxstate[i]=0;
}
static void check_memory(void)
{
    for(unsigned i=0;i<sizeof(expected);++i)
        if(BYTE(data_base+i)!=expected[i])fail("byte oracle mismatch",i);
}
static void check(void)
{
    oracle();
    for(implementation=0;implementation<3;++implementation){
        reset();
        simd_entry=implementation==0?entries[function]+0x100000u:
                   implementation==1?ordinary[function]:entries[function];
        simd_capture();++executions;check_memory();
        if(implementation==0){
            for(unsigned i=0;i<COUNT(reference_machine);++i)reference_machine[i]=simd_machine[i];
            for(unsigned i=0;i<sizeof(reference_fxstate);++i)reference_fxstate[i]=simd_fxstate[i];
            /* Explicitly assert the interface, in addition to differential
             * snapshots, so equal errors cannot erase the ABI expectation. */
            const uint32_t registers[]={0x768d9ca7,0x657c8b97,0x546b7a87,
                simd_stack-4u,0x435a6977,0x32495867,0x21384957,0x10293847};
            for(unsigned i=0;i<8;++i)
                if(simd_machine[i]!=registers[i])fail("GPR/stack preservation",i);
            REQUIRE((simd_machine[8]&0xcd5u)==(simd_flags&0xcd5u),"EFLAGS preservation");
            for(unsigned i=0;i<5;++i)
                if(simd_machine[9+i]!=simd_arguments[i])fail("arguments/canaries",i);
            REQUIRE(*(uint32_t *)(simd_fxstate+24)==simd_mxcsr,"MXCSR preservation");
            REQUIRE(*(uint16_t *)(simd_fxstate)==0x37fu,"x87 control word");
            REQUIRE((*(uint16_t *)(simd_fxstate+2)&0x3800u)==0x3000u,"x87 stack depth");
        }else{
            for(unsigned i=0;i<COUNT(reference_machine);++i)
                if(reference_machine[i]!=simd_machine[i])fail("machine ABI mismatch",i);
            for(unsigned i=0;i<sizeof(reference_fxstate);++i)
                if(reference_fxstate[i]!=simd_fxstate[i])fail("XMM/x87/MXCSR mismatch",i);
        }
    }
    ++cases[function];
}
static void real_services(void)
{
    function=3;stage=6;
    for(unsigned mode=0;mode<3;++mode)for(unsigned scenario=0;scenario<64;++scenario){
        prepare(64u+scenario%16u,112u+scenario%16u,160u+scenario%16u);oracle();
        for(implementation=0;implementation<2;++implementation){
            uint32_t source=implementation?0xa00000u:0x900000u;
            for(unsigned i=0;i<250099u;++i)BYTE(0x800000u+i)=BYTE(source+i);
            for(unsigned i=0;i<320;++i)WORD(0x610000u+i*4u)=scenario*0x1234567u+i;
            WORD(0x1b0e4)=0x610000;WORD(0x1b034)=0x620000;
            WORD(0x620024)=mode==0?8u:mode==1?32u:24u;
            WORD(0x1a868)=1u<<25;
            ((void (*)(void))0x8005aa)();
            REQUIRE(WORD(0x610048)==0x81abc6u,"real initialized blend slot");
            for(unsigned i=0;i<320;++i){
                if(implementation==0)reference_table[i]=WORD(0x610000u+i*4u);
                else if(reference_table[i]!=WORD(0x610000u+i*4u))fail("service table mismatch",i);
            }
            reset();
            /* Actual table -> vector leaf. No slot mutation or CALL patch. */
            ((void (*)(uint32_t,uint32_t,uint32_t))(uintptr_t)WORD(0x610048))(
                simd_arguments[0],simd_arguments[1],simd_arguments[2]);
            check_memory();++executions;
        }
        ++service_cases;
    }
}
void _start(void)
{
    REQUIRE(map(0x10000,0x20000)&&map(0x500000,0x10000)&&map(0x600000,0x2000)&&
        map(0x610000,0x1000)&&map(0x620000,0x1000)&&map(0x800000,0x40000)&&
        map(0x900000,0x40000)&&map(0xa00000,0x40000),"mmap");
    REQUIRE(load("build/simd_original.x",0x900000),"original load");
    REQUIRE(load("build/simd_candidate.x",0x800000)&&load("build/simd_candidate.x",0xa00000),"candidate load");
    stage=1;
    for(function=0;function<3;++function)for(unsigned pair=0;pair<65536u;++pair){
        prepare(32u+pair%16u,80u+pair%16u,128u+pair%16u);
        for(unsigned i=0;i<16;++i){initial[simd_arguments[0]-data_base+i]=(uint8_t)pair;
            initial[simd_arguments[1]-data_base+i]=(uint8_t)(pair>>8);}
        check();
    }
    stage=2;function=3;
    for(unsigned weight=0;weight<256u;++weight)for(unsigned byte=0;byte<256u;++byte){
        prepare(32u+byte%16u,80u+byte%16u,128u+byte%16u);
        for(unsigned i=0;i<16;++i){
            initial[simd_arguments[0]-data_base+i]=(uint8_t)byte;
            initial[simd_arguments[1]-data_base+i]=(uint8_t)(255u-byte);
            initial[simd_arguments[2]-data_base+i]=i%4u?0u:(uint8_t)weight;
        }
        check();
    }
    stage=3;
    for(function=0;function<4;++function)for(unsigned i=0;i<8192u;++i){
        prepare(32u+i%16u,80u+(i/16u)%16u,128u+(i/256u)%16u);check();
    }
    stage=4;
    /* All 3-way overlaps: no output may affect a not-yet-loaded input. */
    for(function=0;function<4;++function)for(unsigned a=0;a<32;++a)
      for(unsigned b=0;b<32;++b)for(unsigned c=0;c<32;++c){
        prepare(64u+a,64u+b,64u+c);check();
      }
    stage=5;
    /* Valid 16-byte vectors exactly against the unmapped page on either
     * side. No wider access or alignment assumption is permitted. */
    for(function=0;function<4;++function)for(unsigned edge=0;edge<2;++edge)
      for(unsigned argument=0;argument<3;++argument)for(unsigned a=0;a<16;++a){
        data_base=edge?0x601f00u:0x600000u;
        prepare(64u+a,112u+a,160u+a);
        simd_arguments[argument]=data_base+(edge?240u:0u);check();
      }
    data_base=0x600100;real_services();
    for(function=0;function<4;++function){print("SIMD function/cases: ");
        number(function);print("/");number(cases[function]);print("\n");}
    print("SIMD complete initialized service scenarios: ");number(service_cases);
    print("; machine executions: ");number(executions);print("\n");
    print("SIMD byte oracle, exact GPR/flags/XMM/x87/MXCSR and guarded boundaries verified\n");
    sys3(1,0,0,0);__builtin_unreachable();
}
