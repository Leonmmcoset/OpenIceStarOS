/* The setters reject negative coordinates; the getters intentionally do not.
 * A bytewise oracle covers wrapped table/pixel addressing and the complete
 * mapped table/data regions. No CALL or memory boundary is substituted.
 */
#include <stdint.h>
#include "../kernel/pixel_access.h"
#define TABLE 0x4ff000u
#define TABLE_BYTES 8192u
#define DATA 0x600000u
#define DATA_BYTES 4096u
#define REGION (TABLE_BYTES+DATA_BYTES)
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static uint8_t initial[REGION],expected[REGION];
static uint32_t args[5],cases[4],rejected[4],negative_reads[2],stage,scenario,implementation;
static uint32_t random_state=0x19283746u;
static const uint32_t values[]={0,1,2,3,7,31,255,256,0x10000,0x20000000,
    0x40000001,0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff};
static const uint32_t ids[]={0,1,63,1023,0x3fffffff,0x40000000,0x40000001,
    0x400003ff,0x7fffffff,0x80000000,0x80000001,0xbfffffff,0xc0000000,
    0xc00003ff,0xffffffff,0x40040000};
static const uint32_t entries[]={0x81b26e,0x81b2f4,0x81b406,0x81b486};
static const void *ordinary[]={kernel_pixel_put8_81b26e,kernel_pixel_put32_81b2f4,
                               kernel_pixel_get8_81b406,kernel_pixel_get32_81b486};
extern uint32_t icon_pixels_checked_call(const void *,const uint32_t *,uint32_t *);

static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80":"=a"(result):"a"(n),"b"(a),"c"(b),"d"(c):"memory");
    return result;
}
static void number(uint32_t value)
{
    char digits[10];uint32_t i=sizeof(digits);
    do{digits[--i]='0'+value%10u;value/=10u;}while(value);
    syscall3(4,1,(uint32_t)(digits+i),sizeof(digits)-i);
}
static void fail(const char *why,uint32_t bytes)
{
    PRINT("FAIL: ");syscall3(4,1,(uint32_t)why,bytes);
    PRINT(" stage/implementation/scenario: ");number(stage);PRINT(" ");
    number(implementation);PRINT(" ");number(scenario);PRINT("\n");syscall3(1,1,0,0);
}
#define REQUIRE(c,s) do{if(!(c))fail(s,sizeof(s)-1);}while(0)
static int map(uint32_t address,uint32_t size)
{
    uint32_t a[]={address,size,7,0x32,0xffffffff,0};
    return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;
}
static int load(const char *path,uint32_t address)
{
    int fd=syscall3(5,(uint32_t)path,0,0),size;
    if(fd<0)return 0;
    size=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);
    return size==250099;
}
static uint32_t random32(void)
{random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static uint32_t index_of(uint32_t address)
{
    if(address-TABLE<TABLE_BYTES)return address-TABLE;
    REQUIRE(address-DATA<DATA_BYTES,"oracle access outside mappings");
    return TABLE_BYTES+address-DATA;
}
static uint32_t get32(uint32_t address)
{
    uint32_t value=0;
    for(uint32_t i=0;i<4;++i)value|=(uint32_t)initial[index_of(address+i)]<<(8u*i);
    return value;
}
static void put(uint8_t *memory,uint32_t address,uint32_t value,uint32_t bytes)
{for(uint32_t i=0;i<bytes;++i)memory[index_of(address+i)]=(uint8_t)(value>>(i*8u));}
static int64_t signed32(uint32_t value)
{return value<0x80000000u?(int64_t)value:(int64_t)value-4294967296LL;}
static void reset(void)
{
    for(uint32_t i=0;i<REGION;++i){
        uint32_t address=i<TABLE_BYTES?TABLE+i:DATA+i-TABLE_BYTES;
        *(volatile uint8_t *)address=initial[i];
    }
}
static void check_memory(void)
{
    for(uint32_t i=0;i<REGION;++i){
        uint32_t address=i<TABLE_BYTES?TABLE+i:DATA+i-TABLE_BYTES;
        REQUIRE(*(volatile uint8_t *)address==expected[i],"12 KiB memory oracle mismatch");
    }
}
static void check(uint32_t f,int guard_rejection)
{
    scenario=cases[f];
    for(uint32_t i=0;i<REGION;++i)expected[i]=initial[i];
    uint32_t result=UINT32_MAX,accepted=0;
    if(!guard_rejection){
        uint32_t surface=get32(0x500000u+args[0]*4u),width=get32(surface+12u),height=get32(surface+16u);
        accepted=signed32(width)>signed32(args[1])&&signed32(height)>signed32(args[2]);
        if(f<2)accepted=accepted&&signed32(args[1])>=0&&signed32(args[2])>=0;
        if(accepted){
            uint32_t bytes=(f&1u)?4u:1u;
            uint64_t offset=((uint64_t)width*args[2]+args[1])*bytes;
            uint32_t address=(uint32_t)((uint64_t)get32(surface)+offset);
            if(f<2)put(expected,address,args[3],bytes);
            else{
                result=0;
                for(uint32_t i=0;i<bytes;++i)result|=(uint32_t)initial[index_of(address+i)]<<(8u*i);
                if(signed32(args[1])<0||signed32(args[2])<0)++negative_reads[f-2u];
            }
        }
    }
    if(!accepted)++rejected[f];
    for(implementation=0;implementation<3;++implementation){
        reset();
        const void *fn=implementation==0?(const void *)(entries[f]+0x100000u):
                       implementation==1?ordinary[f]:(const void *)entries[f];
        uint32_t actual;
        REQUIRE(!icon_pixels_checked_call(fn,args,&actual),"pixel service ABI mismatch");
        if(f>=2)REQUIRE(actual==result,"pixel read/sentinel mismatch");
        check_memory();
    }
    ++cases[f];
}
static void prepare(uint32_t f,uint32_t id,uint32_t surface,uint32_t width,uint32_t height,
                    uint32_t x,uint32_t y,uint32_t destination,uint32_t color)
{
    for(uint32_t i=0;i<REGION;++i)initial[i]=(uint8_t)(i*37u+(i/32u)*11u+cases[f]);
    args[0]=id;args[1]=x;args[2]=y;args[3]=color;args[4]=0x17283940;
    uint32_t address=0x500000u+id*4u;
    put(initial,address,surface,4);
    put(initial,surface,destination-(width*y+x)*((f&1u)?4u:1u),4);
    put(initial,surface+12u,width,4);put(initial,surface+16u,height,4);
}
void _start(void)
{
    REQUIRE(map(TABLE,TABLE_BYTES)&&map(DATA,DATA_BYTES)&&map(0x800000,0x40000)&&map(0x900000,0x40000),"mmap");
    REQUIRE(load("build/pixel_access_original.x",0x900000),"original load");
#ifdef TEST_INSTALLED
    REQUIRE(load("build/openicestaros_kernel.x",0x800000),"installed load");
#else
    REQUIRE(load("build/pixel_access_preview.x",0x800000),"preview load");
#endif
    stage=1;
    for(uint32_t f=0;f<4;++f)for(uint32_t w=0;w<16;++w)for(uint32_t h=0;h<16;++h)
      for(uint32_t x=0;x<16;++x)for(uint32_t y=0;y<16;++y){
        prepare(f,ids[(w+h+x+y)%16u],DATA+101u+(x+y)%4u,values[w],values[h],
                values[x],values[y],DATA+2049u+(w+h)%4u,random32());check(f,0);
      }
    stage=2;
    for(uint32_t f=0;f<4;++f)for(uint32_t color=0;color<65536u;++color){
        uint32_t value=color|((color^0xffffu)<<16),destination=DATA+2049u+color%4u;
        prepare(f,ids[color%16u],DATA+101u+color%4u,7,7,3,5,destination,value);
        if(f>=2)put(initial,destination,value,(f&1u)?4u:1u);
        check(f,0);
    }
    stage=3;
    for(uint32_t f=0;f<4;++f)for(uint32_t i=0;i<8192u;++i){
        uint32_t w=random32(),h=random32(),x=random32(),y=random32();
        prepare(f,ids[i%16u],DATA+101u+i%4u,w,h,x,y,DATA+2049u+i%4u,random32());check(f,0);
    }
    stage=4;
    for(uint32_t f=0;f<4;++f)for(uint32_t a=0;a<4;++a)for(uint32_t i=0;i<8;++i){
        uint32_t surface=DATA+101u+a,id=ids[(i+a)%16u];
        uint32_t destination=i<6?surface+i*4u:0x500000u+id*4u-(i-6u)*4u;
        prepare(f,id,surface,8,8,3,4,destination,random32());check(f,0);
    }
    stage=5;
    /* Width rejects before an unmapped height field, for reads and writes. */
    for(uint32_t f=0;f<4;++f)for(uint32_t i=0;i<16;++i){
        uint32_t surface=DATA+DATA_BYTES-16u;
        for(uint32_t j=0;j<REGION;++j)initial[j]=(uint8_t)j;
        put(initial,0x500000u,surface,4);put(initial,surface+12u,values[i],4);
        args[0]=0;args[1]=0x7fffffffu;args[2]=0;args[3]=random32();check(f,1);
    }
    for(uint32_t f=0;f<2;++f)for(uint32_t i=0;i<16;++i){
        /* Negative x skips even the unmapped table lookup. */
        args[0]=0x17283940u;args[1]=0x80000000u+i;args[2]=0;check(f,1);
        /* Negative y skips height; width alone is readable. */
        put(initial,0x500000u,DATA+DATA_BYTES-16u,4);
        put(initial,DATA+DATA_BYTES-4u,1,4);
        args[0]=0;args[1]=0;args[2]=0x80000000u+i;check(f,1);
    }
    PRINT("Cases put8/put32/get8/get32: ");uint32_t total=0;
    for(uint32_t f=0;f<4;++f){number(cases[f]);PRINT(" ");total+=cases[f];}
    PRINT("; executions: ");number(total*3u);PRINT("; rejected: ");
    for(uint32_t f=0;f<4;++f){number(rejected[f]);PRINT(" ");}
    PRINT("; accepted negative-coordinate reads8/32: ");number(negative_reads[0]);PRINT(" ");number(negative_reads[1]);
    PRINT("; surface-ID services verified against 12 KiB memory/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
