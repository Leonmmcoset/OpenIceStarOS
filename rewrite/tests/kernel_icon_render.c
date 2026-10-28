/* Complete DIB -> real quantizer -> real clipped surface write execution.
 * No decoder or helper CALL is redirected. A bytewise oracle observes live
 * resource changes when output aliases headers, palettes, pixels or masks.
 */
#include <stdint.h>
#include "../kernel/icon_dib.h"
#define BASE 0x600000u
#define REGION 65536u
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static uint8_t initial[REGION],expected[REGION];
static uint32_t args[4],dib,surface,stage,scenario,implementation,cases,branches[4],skipped,writes;
static const uint32_t dimensions[]={0,1,7,8,9,16,24,31,32,33,47,48,49,64,65};
static const uint32_t coordinates[]={0,1,3,68,69,0x7ffffffe,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
static const uint32_t bounds[]={0,1,2,16,31,69,0x7fffffff,0x80000000,0xffffffff};
#include "../build/icon_dib_resources.h"
extern uint32_t icon_dib_checked_call(const void *,const uint32_t *);

static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{
    int r;
    __asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");
    return r;
}
static void number(uint32_t v)
{
    char d[10];uint32_t i=sizeof(d);
    do{d[--i]='0'+v%10u;v/=10u;}while(v);
    syscall3(4,1,(uint32_t)(d+i),sizeof(d)-i);
}
static void fail(const char *s,uint32_t size)
{
    PRINT("FAIL: ");syscall3(4,1,(uint32_t)s,size);
    PRINT(" stage/scenario/implementation: ");number(stage);PRINT(" ");
    number(scenario);PRINT(" ");number(implementation);PRINT("\n");syscall3(1,1,0,0);
}
#define REQUIRE(c,s) do{if(!(c))fail(s,sizeof(s)-1);}while(0)
static int map(uint32_t a,uint32_t n)
{
    uint32_t p[]={a,n,7,0x32,0xffffffff,0};
    return (uint32_t)syscall3(90,(uint32_t)p,0,0)==a;
}
static int load(const char *path,uint32_t address)
{
    int fd=syscall3(5,(uint32_t)path,0,0),n;
    if(fd<0)return 0;
    n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);
    return n==250099;
}
static uint32_t get8(uint32_t a)
{REQUIRE(a-BASE<REGION,"oracle read outside mapping");return expected[a-BASE];}
static uint32_t get16(uint32_t a){return get8(a)|(get8(a+1u)<<8);}
static uint32_t get32(uint32_t a)
{return get16(a)|(get16(a+2u)<<16);}
static void put(uint8_t *memory,uint32_t address,uint32_t value,uint32_t bytes)
{
    for(uint32_t i=0;i<bytes;++i){
        REQUIRE(address+i-BASE<REGION,"oracle write outside mapping");
        memory[address+i-BASE]=(uint8_t)(value>>(i*8u));
    }
}
static void setup(uint32_t seed,uint32_t width,uint32_t height,uint32_t source,
                   uint32_t target,uint32_t pattern)
{
    for(uint32_t i=0;i<REGION;++i)initial[i]=(uint8_t)(i*37u+(i/32u)*11u+seed*83u);
    dib=BASE+0x101u+seed%4u;surface=BASE+0x7001u+(seed/4u)%4u;
    args[0]=surface;args[1]=0;args[2]=0;args[3]=dib;
    put(initial,dib+4u,width,4);put(initial,dib+8u,height*2u+seed%2u,4);
    put(initial,dib+14u,source,2);
    put(initial,surface,BASE+0x8001u+seed%4u,4);
    put(initial,surface+12u,69,4);put(initial,surface+16u,69,4);
    put(initial,surface+36u,target,4);
    uint32_t mask=dib+(source==32u?40u:1064u)+width*height*(source==32u?4u:1u);
    uint32_t stride=height<48u?4u:8u;
    for(uint32_t i=0;i<height*stride+width+8u;++i)put(initial,mask+i,pattern,1);
}
static void plan(void)
{
    uint32_t width=get32(dib+4u),height=get32(dib+8u)/2u,source=get16(dib+14u);
    if(source!=8u&&source!=32u){++skipped;return;}
    uint32_t target=get32(surface+36u);
    if(target!=8u&&target!=32u){++skipped;return;}
    ++branches[(source==32u?2u:0u)+(target==32u?1u:0u)];
    uint32_t bytes=source==32u?4u:1u,stride=height<48u?4u:8u;
    uint32_t pixels=dib+(source==32u?40u:1064u),mask=pixels+width*height*bytes;
    static const uint32_t matrix[2][2]={{3,1},{0,2}};
    for(uint32_t row=0;row<height;++row)for(uint32_t col=0;col<width;++col){
        uint32_t m=mask+(height-row-1u)*stride+col/8u;
        if(source==32u&&target==8u)m+=col;
        if(get8(m)&(128u>>(col%8u)))continue;
        uint32_t p=pixels+((height-row-1u)*width+col)*bytes;
        if(source==8u)p=dib+40u+get8(p)*4u;
        uint32_t color;
        if(target==32u){color=get32(p);if(source==32u)color&=0xffffffu;}
        else{
            uint32_t threshold=matrix[row%2u][col%2u];
            uint32_t blue=(get8(p)*21u/256u+threshold)/4u;
            uint32_t green=(get8(p+1u)*21u/256u+threshold)/4u;
            uint32_t red=(get8(p+2u)*21u/256u+threshold)/4u;
            color=(red+6u*green+36u*blue+16u)&255u;
        }
        uint32_t x=args[1]+col,y=args[2]+row;
        if((int32_t)x<0||(int32_t)get32(surface+12u)<=(int32_t)x)continue;
        if((int32_t)y<0||(int32_t)get32(surface+16u)<=(int32_t)y)continue;
        uint32_t address=get32(surface)+(get32(surface+12u)*y+x)*(target==32u?4u:1u);
        put(expected,address,color,target==32u?4u:1u);++writes;
    }
}
static void run(void)
{
    scenario=cases;
    for(uint32_t i=0;i<REGION;++i)expected[i]=initial[i];
    plan();
    const void *functions[]={(void *)0x92b20c,kernel_icon_dib_82b20c,(void *)0x82b20c};
    for(implementation=0;implementation<3;++implementation){
        for(uint32_t i=0;i<REGION;++i)*(volatile uint8_t *)(BASE+i)=initial[i];
        REQUIRE(!icon_dib_checked_call(functions[implementation],args),"decoder ABI mismatch");
        for(uint32_t i=0;i<REGION;++i)
            REQUIRE(*(volatile uint8_t *)(BASE+i)==expected[i],"complete-render 64 KiB oracle mismatch");
    }
    ++cases;
}
void _start(void)
{
    REQUIRE(map(BASE,REGION)&&map(0x800000,0x40000)&&map(0x900000,0x40000),"mmap");
    REQUIRE(load("build/icon_render_original.x",0x900000),"original load");
#ifdef TEST_INSTALLED
    REQUIRE(load("build/openicestaros_kernel.x",0x800000),"installed load");
#else
    REQUIRE(load("build/icon_render_preview.x",0x800000),"preview load");
#endif
    stage=1;
    for(uint32_t branch=0;branch<4;++branch)for(uint32_t align=0;align<4;++align)
      for(uint32_t mask=0;mask<256;++mask){
        setup(mask*4u+align,dimensions[mask%15u],dimensions[(mask/15u)%15u],
              branch&2u?32u:8u,branch&1u?32u:8u,mask);run();
      }
    stage=2;
    for(uint32_t branch=0;branch<4;++branch)for(uint32_t w=0;w<15;++w)for(uint32_t h=0;h<15;++h){
        setup(w*31u+h,dimensions[w],dimensions[h],branch&2u?32u:8u,branch&1u?32u:8u,0);run();
    }
    stage=3;
    for(uint32_t branch=0;branch<4;++branch)for(uint32_t a=0;a<4;++a)
      for(uint32_t x=0;x<10;++x)for(uint32_t y=0;y<10;++y){
        setup(a,16,16,branch&2u?32u:8u,branch&1u?32u:8u,0);
        args[1]=coordinates[x];args[2]=coordinates[y];run();
      }
    stage=4;
    for(uint32_t branch=0;branch<4;++branch)for(uint32_t w=0;w<9;++w)for(uint32_t h=0;h<9;++h){
        setup(w*4u+h,16,16,branch&2u?32u:8u,branch&1u?32u:8u,0);
        put(initial,surface+12u,bounds[w],4);put(initial,surface+16u,bounds[h],4);
        /* Only small accepted widths produce mapped effective writes. */
        if(bounds[w]>69u&&bounds[w]<0x80000000u)args[2]=0x80000000u;
        run();
    }
    stage=5;
    for(uint32_t frame=0;frame<sizeof(resource_fixtures)/sizeof(resource_fixtures[0]);++frame)
      for(uint32_t target=0;target<2;++target)for(uint32_t align=0;align<4;++align)
       for(uint32_t pos=0;pos<4;++pos){
        setup(align,16,16,8,target?32u:8u,0);
        for(uint32_t i=0;i<resource_fixtures[frame].size;++i)
            put(initial,dib+i,resource_fixtures[frame].bytes[i],1);
        args[1]=pos&1u?0xfffffffeu:3u;args[2]=pos&2u?68u:1u;run();
       }
    stage=6;
    for(uint32_t branch=0;branch<4;++branch)for(uint32_t a=0;a<4;++a)for(uint32_t alias=0;alias<5;++alias){
        setup(a,16,16,branch&2u?32u:8u,branch&1u?32u:8u,0);
        uint32_t destinations[]={dib,dib+40u,dib+1064u,dib+2200u,surface+36u};
        put(initial,surface,destinations[alias],4);run();
    }
    stage=7;
    for(uint32_t source=0;source<4;++source)for(uint32_t target=0;target<4;++target){
        static const uint32_t depths[]={4,8,32,0x10020};
        setup(0,0,0,depths[source],depths[target],0);run();
    }
    PRINT("Complete DIB chain cases: ");number(cases);PRINT("; executions: ");number(cases*3u);
    PRINT("; 8->8/8->32/32->8/32->32: ");
    for(uint32_t i=0;i<4;++i){number(branches[i]);PRINT(" ");}
    PRINT("; rejected depths: ");number(skipped);PRINT("; oracle pixel writes: ");number(writes);
    PRINT("; real decoder/pixel/quantizer chain verified against 64 KiB memory/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
