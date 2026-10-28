/* Direct execution of three original routines and both compiled C forms.
 * No function boundaries are substituted. The bytewise write model and
 * 64-bit signed color oracle avoid the candidate's pointer arithmetic and
 * wrapped multiplication/division implementation.
 */
#include <stdint.h>
#include "../kernel/icon_pixels.h"
#define BASE 0x600000u
#define REGION 4096u
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
static uint8_t initial[REGION], expected[REGION];
static uint32_t args[5], cases[3], stage, implementation, scenario, rejected[2];
static uint32_t random_state = 0x84269317u;
static const uint32_t values[] = {0,1,2,3,7,31,255,256,0x10000,0x20000000,
    0x40000001,0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff};
static const uint32_t entries[] = {0x81b37c,0x81b3c0,0x81fd90};
static const void *ordinary[] = {kernel_icon_put8_81b37c,kernel_icon_put32_81b3c0,
                               kernel_icon_quantize_81fd90};
extern uint32_t icon_pixels_checked_call(const void *, const uint32_t *, uint32_t *);

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "a"(n),"b"(a),"c"(b),"d"(c) : "memory");
    return result;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i=sizeof(digits);
    do {digits[--i]='0'+value%10u;value/=10u;} while(value);
    syscall3(4,1,(uint32_t)(digits+i),sizeof(digits)-i);
}
static void fail(const char *why, uint32_t bytes)
{
    PRINT("FAIL: ");syscall3(4,1,(uint32_t)why,bytes);
    PRINT(" stage/implementation/scenario: ");number(stage);PRINT(" ");
    number(implementation);PRINT(" ");number(scenario);PRINT("\n");
    syscall3(1,1,0,0);
}
#define REQUIRE(c,s) do {if(!(c)) fail(s,sizeof(s)-1);} while(0)
static int map(uint32_t address, uint32_t size)
{
    uint32_t a[]={address,size,7,0x32,0xffffffff,0};
    return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;
}
static int load(const char *path,uint32_t address)
{
    int fd=syscall3(5,(uint32_t)path,0,0),size;
    if(fd<0)return 0;
    size=syscall3(3,(uint32_t)fd,address,0x40000);
    syscall3(6,(uint32_t)fd,0,0);
    return size==250099;
}
static uint32_t random32(void)
{
    random_state^=random_state<<13;random_state^=random_state>>17;
    random_state^=random_state<<5;return random_state;
}
static void put32(uint32_t address,uint32_t value)
{
    for(uint32_t i=0;i<4;++i){
        REQUIRE(address+i-BASE<REGION,"fixture write outside mapping");
        initial[address+i-BASE]=(uint8_t)(value>>(8u*i));
    }
}
static uint32_t get32(uint32_t address)
{
    uint32_t result=0;
    for(uint32_t i=0;i<4;++i){
        REQUIRE(address+i-BASE<REGION,"oracle read outside mapping");
        result|=(uint32_t)initial[address+i-BASE]<<(8u*i);
    }
    return result;
}
static int64_t signed32(uint32_t value)
{return value<0x80000000u?(int64_t)value:(int64_t)value-4294967296LL;}
static uint32_t quantized(void)
{
    static const uint32_t matrix[2][2]={{3,1},{0,2}};
    int64_t q[3];
    for(uint32_t i=0;i<3;++i){
        /* Conversion via a 64-bit product followed by explicit modulo 2^32. */
        uint32_t product=(uint32_t)((uint64_t)args[i]*21u);
        q[i]=(signed32(product)/256+matrix[args[4]%2u][args[3]%2u])/4;
    }
    int64_t color=q[0]+6*q[1]+36*q[2]+16;
    return (uint32_t)color%256u;
}
static void call(uint32_t function,uint32_t expected_value,int check_return)
{
    for(implementation=0;implementation<3;++implementation){
        const void *fn=implementation==0?(const void *)(entries[function]+0x100000u):
                       implementation==1?ordinary[function]:(const void *)entries[function];
        uint32_t result;
        REQUIRE(!icon_pixels_checked_call(fn,args,&result),"stack/callee-saved/canary mismatch");
        if(check_return)REQUIRE(result==expected_value,"color oracle mismatch");
    }
    ++cases[function];
}
static void color_case(uint32_t red,uint32_t green,uint32_t blue,uint32_t x,uint32_t y)
{
    args[0]=red;args[1]=green;args[2]=blue;args[3]=x;args[4]=y;
    scenario=cases[2];call(2,quantized(),1);
}
static void pixel_case(uint32_t function,uint32_t surface,uint32_t x,uint32_t y,
                        uint32_t color)
{
    args[0]=surface;args[1]=x;args[2]=y;args[3]=color;args[4]=0x17283940u;
    scenario=cases[function];
    for(uint32_t i=0;i<REGION;++i)expected[i]=initial[i];
    uint32_t bytes=function?4u:1u;
    /* Widened signed comparisons are independent of candidate casts. */
    if(signed32(x)>=0 && signed32(get32(surface+12u))>signed32(x) &&
       signed32(y)>=0 && signed32(get32(surface+16u))>signed32(y)){
        uint64_t index=(uint64_t)get32(surface+12u)*y+x;
        uint32_t address=(uint32_t)((uint64_t)get32(surface)+index*bytes);
        for(uint32_t i=0;i<bytes;++i){
            REQUIRE(address+i-BASE<REGION,"oracle destination outside mapping");
            expected[address+i-BASE]=(uint8_t)(color>>(8u*i));
        }
    }else ++rejected[function];
    for(implementation=0;implementation<3;++implementation){
        for(uint32_t i=0;i<REGION;++i)*(volatile uint8_t *)(BASE+i)=initial[i];
        const void *fn=implementation==0?(const void *)(entries[function]+0x100000u):
                       implementation==1?ordinary[function]:(const void *)entries[function];
        uint32_t result;
        REQUIRE(!icon_pixels_checked_call(fn,args,&result),"pixel ABI mismatch");
        for(uint32_t i=0;i<REGION;++i)
            REQUIRE(*(volatile uint8_t *)(BASE+i)==expected[i],"4 KiB pixel oracle mismatch");
    }
    ++cases[function];
}
static void prepare(uint32_t surface,uint32_t width,uint32_t height,
                    uint32_t x,uint32_t y,uint32_t function,uint32_t destination)
{
    for(uint32_t i=0;i<REGION;++i)initial[i]=(uint8_t)(i*37u+(i/32u)*11u+scenario);
    put32(surface,destination-(width*y+x)*(function?4u:1u));
    put32(surface+12u,width);put32(surface+16u,height);
}
void _start(void)
{
    REQUIRE(map(BASE,REGION)&&map(0x800000,0x40000)&&map(0x900000,0x40000),"mmap");
    REQUIRE(load("build/icon_pixels_original.x",0x900000),"original load");
#ifdef TEST_INSTALLED
    REQUIRE(load("build/icestar_c.x",0x800000),"installed load");
#else
    REQUIRE(load("build/icon_pixels_preview.x",0x800000),"preview load");
#endif
    stage=1;
    for(uint32_t f=0;f<2;++f)
      for(uint32_t wi=0;wi<16;++wi)for(uint32_t hi=0;hi<16;++hi)
       for(uint32_t xi=0;xi<16;++xi)for(uint32_t yi=0;yi<16;++yi){
        uint32_t surface=BASE+101u+(xi+yi)%4u;
        prepare(surface,values[wi],values[hi],values[xi],values[yi],f,BASE+2049u+(wi+hi)%4u);
        pixel_case(f,surface,values[xi],values[yi],random32());
       }
    stage=2;
    for(uint32_t f=0;f<2;++f)for(uint32_t color=0;color<65536u;++color){
        uint32_t surface=BASE+101u+color%4u;
        uint32_t full=(color<<16)|(color^0x9183u);
        prepare(surface,31,17,color%31u,(color/31u)%17u,f,BASE+2048u+color%4u);
        pixel_case(f,surface,color%31u,(color/31u)%17u,full);
    }
    stage=3;
    for(uint32_t f=0;f<2;++f)for(uint32_t align=0;align<4;++align)
      for(uint32_t alias=0;alias<40;++alias){
        uint32_t surface=BASE+101u+align;
        prepare(surface,31,17,3,5,f,surface+alias);
        pixel_case(f,surface,3,5,random32());
      }
    /* Guarded rejection paths must not touch fields that follow the rejecting
     * comparison. BASE's adjacent pages and address zero remain unmapped. */
    stage=4;
    for(uint32_t f=0;f<2;++f){
        pixel_case(f,0,0x80000000u,0,0x12345678u);
        pixel_case(f,0xffffffffu,0xffffffffu,0xffffffffu,0x12345678u);
        put32(BASE+REGION-4u,0);
        pixel_case(f,BASE+REGION-16u,0,0,0x12345678u);
        put32(BASE+REGION-4u,1);
        pixel_case(f,BASE+REGION-16u,0,0xffffffffu,0x12345678u);
        put32(BASE,1);put32(BASE+4u,0);
        pixel_case(f,BASE-12u,0,0,0x12345678u);
    }
    PRINT("Pixel bounds, wrapping, aliases, guards and all 65,536 color pairs passed\n");
    stage=5;
    for(uint32_t rgb=0;rgb<0x1000000u;++rgb)
      for(uint32_t parity=0;parity<4;++parity)
        color_case(rgb&255u,(rgb>>8)&255u,rgb>>16,parity&1u,parity>>1);
    PRINT("All 16,777,216 RGB colors at all four dither positions passed\n");
    stage=6;
    for(uint32_t i=0;i<16;++i)for(uint32_t j=0;j<16;++j)
      for(uint32_t k=0;k<16;++k)for(uint32_t x=0;x<16;++x)for(uint32_t y=0;y<16;++y)
        color_case(values[i],values[j],values[k],values[x],values[y]);
    stage=7;
    for(uint32_t i=0;i<65536u;++i){
        color_case(i,0x80000000u+i,0xffff0000u+i,0x80000000u+i,0xffff0000u+i/2u);
        color_case(i<<16,(i<<16)^0x7fffff00u,0xffffffffu-i,random32(),random32());
    }
    stage=8;
    for(uint32_t i=0;i<131072u;++i){
        uint32_t a=random32(),b=random32(),c=random32(),x=random32(),y=random32();
        color_case(a,b,c,x,y);
    }
    PRINT("Cases put8/put32/quantizer: ");
    for(uint32_t i=0;i<3;++i){number(cases[i]);PRINT(" ");}
    PRINT("; executions: ");number(3u*(cases[0]+cases[1]+cases[2]));
    PRINT("; clipped put8/put32: ");number(rejected[0]);PRINT(" ");number(rejected[1]);
    PRINT("; direct pixel helpers and quantizer verified without boundary substitutions\n");
    syscall3(1,0,0,0);
}
