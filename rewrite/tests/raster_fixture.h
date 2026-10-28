/* Shared mapped-memory and ABI machinery for direct raster execution. */
#include <stdint.h>
#ifndef RASTER_ARGUMENT_WORDS
#define RASTER_ARGUMENT_WORDS 6
#endif
#define TABLE 0x4ff000u
#define ORDER 0x50f000u
#define DATA 0x600000u
#define ARGUMENTS 0x640f60u
#define REGION 32768u
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static uint8_t initial[REGION+RASTER_ARGUMENT_WORDS*4],expected[REGION+RASTER_ARGUMENT_WORDS*4];
static uint32_t args[RASTER_ARGUMENT_WORDS],stage,scenario,implementation,cases,skipped;
static uint32_t random_state=0x79268314u;
static const uint32_t values[]={0,1,2,3,7,31,255,256,0x10000,0x20000000,
    0x40000001,0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff};
static const uint32_t ids[]={0,1,63,1023,0x3fffffff,0x40000000,0x40000001,
    0x400003ff,0x7fffffff,0x80000000,0x80000001,0xbfffffff,0xc0000000,
    0xc00003ff,0xffffffff,0x40040000};
extern uint32_t raster_checked_call(const void *,const uint32_t *,uint32_t *);
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static void number(uint32_t v)
{char d[10];uint32_t i=10;do{d[--i]='0'+v%10u;v/=10u;}while(v);syscall3(4,1,(uint32_t)(d+i),10-i);}
static void fail(const char *why,uint32_t n)
{
    PRINT("FAIL: ");syscall3(4,1,(uint32_t)why,n);
    PRINT(" stage/scenario/implementation: ");number(stage);PRINT(" ");number(scenario);PRINT(" ");number(implementation);
    PRINT("\n");syscall3(1,1,0,0);
}
#define REQUIRE(c,s) do{if(!(c))fail(s,sizeof(s)-1);}while(0)
static int map(uint32_t a,uint32_t n)
{uint32_t p[]={a,n,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)p,0,0)==a;}
static int load(const char *path,uint32_t a)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,a,0x40000);syscall3(6,fd,0,0);return n==250099;}
static uint32_t random32(void)
{random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static int64_t signed32(uint32_t v)
{return v<0x80000000u?(int64_t)v:(int64_t)v-4294967296LL;}
static uint32_t index_of(uint32_t a)
{
    if(a-TABLE<8192u)return a-TABLE;
    if(a-ORDER<8192u)return 8192u+a-ORDER;
    if(a-DATA<16384u)return 16384u+a-DATA;
    if(a-ARGUMENTS<RASTER_ARGUMENT_WORDS*4u)return REGION+a-ARGUMENTS;
    return UINT32_MAX;
}
static uint32_t get32(const uint8_t *m,uint32_t a)
{
    uint32_t v=0;
    for(uint32_t i=0;i<4;++i){uint32_t n=index_of(a+i);REQUIRE(n!=UINT32_MAX,"oracle read outside mappings");v|=(uint32_t)m[n]<<(i*8u);}
    return v;
}
static int put(uint8_t *m,uint32_t a,uint32_t v,uint32_t bytes)
{
    for(uint32_t i=0;i<bytes;++i)if(index_of(a+i)==UINT32_MAX)return 0;
    for(uint32_t i=0;i<bytes;++i)m[index_of(a+i)]=(uint8_t)(v>>(i*8u));
    return 1;
}
static void pattern(void)
{for(uint32_t i=0;i<REGION;++i)initial[i]=(uint8_t)(i*37u+(i/32u)*11u+cases*83u);}
static void begin_oracle(void)
{
    for(uint32_t i=0;i<RASTER_ARGUMENT_WORDS;++i)REQUIRE(put(initial,ARGUMENTS+i*4u,args[i],4),"fixture arguments");
    for(uint32_t i=0;i<REGION+RASTER_ARGUMENT_WORDS*4u;++i)expected[i]=initial[i];
}
static void reset(void)
{
    for(uint32_t i=0;i<REGION;++i){
        uint32_t a=i<8192u?TABLE+i:i<16384u?ORDER+i-8192u:DATA+i-16384u;
        *(volatile uint8_t *)a=initial[i];
    }
}
static void compare(uint32_t actual[RASTER_ARGUMENT_WORDS+1],uint32_t value)
{
    REQUIRE(actual[0]==value,"raw return mismatch");
    for(uint32_t i=0;i<REGION;++i){
        uint32_t a=i<8192u?TABLE+i:i<16384u?ORDER+i-8192u:DATA+i-16384u;
        REQUIRE(*(volatile uint8_t *)a==expected[i],"32 KiB memory oracle mismatch");
    }
    for(uint32_t i=0;i<RASTER_ARGUMENT_WORDS;++i)REQUIRE(actual[i+1]==get32(expected,ARGUMENTS+i*4u),"live incoming word mismatch");
}
static void mappings(void)
{REQUIRE(map(TABLE,8192)&&map(ORDER,8192)&&map(DATA,16384)&&map(0x640000,4096)&&map(0x800000,0x40000)&&map(0x900000,0x40000),"mmap");}
