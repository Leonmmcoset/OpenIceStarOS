/* Independent byte-address oracle, including live argument/metadata aliases.
 * Widened signed loop bounds keep hazardous original inputs out of the test.
 */
#include "../kernel/raster_gradient.h"
#include "raster_fixture.h"
static uint32_t alignment,stores,metadata_stores,caller_stores,counter_crossings,
    recaptured_rows,period_changes;
static uint32_t lower(uint32_t a,uint32_t b){return signed32(a)<signed32(b)?a:b;}
static uint32_t upper(uint32_t a,uint32_t b){return signed32(a)>signed32(b)?a:b;}
static int valid(uint32_t a,uint32_t n)
{for(uint32_t i=0;i<n;++i)if(index_of(a+i)==UINT32_MAX)return 0;return 1;}
static void setup(uint32_t id,uint32_t width,uint32_t x0,uint32_t y0,
    uint32_t x1,uint32_t y1,uint32_t destination,uint32_t color,uint32_t delta,uint32_t period)
{
    pattern();args[0]=id;args[1]=x0;args[2]=y0;args[3]=x1;args[4]=y1;
    args[5]=color;args[6]=delta;args[7]=period;
    uint32_t surface=DATA+0x101u+alignment,other=DATA+0x201u+alignment;
    uint32_t offset=(uint32_t)((uint64_t)lower(y0,y1)*width+lower(x0,x1))*4u;
    REQUIRE(put(initial,0x500000u+id*4u,surface,4)&&put(initial,surface,destination-offset,4)&&
        put(initial,surface+12u,width,4)&&put(initial,surface+16u,values[cases%16u],4),"gradient surface");
    if((id&0x3fffffffu)!=1u)REQUIRE(put(initial,0x500004u,other,4),"other gradient slot");
    REQUIRE(put(initial,other,DATA+0x2801u+alignment,4)&&put(initial,other+12u,8,4),"other gradient record");
}
static int oracle_body(uint32_t *result)
{
    uint32_t ymin=lower(args[2],args[4]),ymax=upper(args[2],args[4]),counter=0;
    if(signed32(ymax)-signed32(ymin)>16||ymax==0x7fffffffu)return 0;
    for(int64_t y=signed32(ymin);y<=signed32(ymax);++y){
        uint32_t x0=get32(expected,ARGUMENTS+4),x1=get32(expected,ARGUMENTS+12);
        uint32_t xmin=lower(x0,x1),xmax=upper(x0,x1),value=get32(expected,ARGUMENTS+20),delta=get32(expected,ARGUMENTS+24);
        if(signed32(xmax)-signed32(xmin)>32||xmax==0x7fffffffu)return 0;
        if(x0!=args[1]||x1!=args[3]||value!=args[5]||delta!=args[6])++recaptured_rows;
        if(counter)++counter_crossings;
        for(int64_t x=signed32(xmin);x<=signed32(xmax);++x){
            uint32_t id=get32(expected,ARGUMENTS),slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
            if(!valid(slot,4))return 0;
            uint32_t surface=get32(expected,slot);
            if(!valid(surface,4)||!valid(surface+12u,4))return 0;
            uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+
                ((uint64_t)(uint32_t)y*get32(expected,surface+12u)+(uint32_t)x)*4u);
            if(!put(expected,address,value,4))return 0;
            ++stores;
            if(address-ARGUMENTS<32u)++caller_stores;
            if(address-DATA<0x1000u||address-0x500000u<4096u)++metadata_stores;
            uint32_t period=get32(expected,ARGUMENTS+28);
            if(period!=args[7])++period_changes;
            if(++counter==period){value=(uint32_t)((uint64_t)value+delta);counter=0;}
        }
    }
    *result=ymax+1u;return 1;
}
static int oracle(uint32_t *result)
{
    uint32_t *counters[]={&stores,&metadata_stores,&caller_stores,&counter_crossings,&recaptured_rows,&period_changes},saved[6];
    for(unsigned i=0;i<6;++i)saved[i]=*counters[i];
    if(oracle_body(result))return 1;
    for(unsigned i=0;i<6;++i)*counters[i]=saved[i];
    return 0;
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    if(!oracle(&value)){++skipped;return;}
    const void *functions[]={(void *)0x91bf50,(void *)kernel_raster_gradient32_81bf50,(void *)0x81bf50};
    for(implementation=0;implementation<3;++implementation){
        reset();uint32_t actual[9];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"gradient ABI");compare(actual,value);
    }
    ++cases;
}
void _start(void)
{
    mappings();REQUIRE(load("build/raster_gradient_original.x",0x900000)&&
        load("build/raster_gradient_candidate.x",0x800000),"gradient image");
    stage=1;
    static const uint32_t origins[][2]={{0,0},{4,3},{0xfffffffc,0xfffffffe},
        {0x40000001,0x20000000},{0x7ffffff0,0x80000000}};
    static const uint32_t periods[]={0,1,2,3,5,7,31,0xffffffff};
    for(alignment=0;alignment<4;++alignment)for(unsigned o=0;o<5;++o)
      for(unsigned dx=0;dx<9;++dx)for(unsigned dy=0;dy<7;++dy)for(unsigned p=0;p<8;++p){
        uint32_t x=origins[o][0],y=origins[o][1];
        setup(ids[(o+dx+dy)%16u],31,x,y,x+dx-4u,y+dy-3u,DATA+0x2801u+alignment,random32(),random32(),periods[p]);run();
    }
    stage=2;
    for(alignment=0;alignment<4;++alignment)for(unsigned w=0;w<16;++w)
      for(unsigned x=0;x<16;++x)for(unsigned y=0;y<16;++y){
        setup(ids[(w+x+y)%16u],values[w],values[x],values[y],values[x],values[y],
            DATA+0x2801u+alignment,random32(),values[(x+y)%16u],periods[y%8u]);run();
    }
    stage=3;
    for(alignment=0;alignment<4;++alignment)for(unsigned offset=0;offset<32;++offset)
      for(unsigned width=0;width<4;++width)for(unsigned reverse=0;reverse<2;++reverse){
        setup(0,width,reverse?1u:0u,0,reverse?0u:1u,1,ARGUMENTS+offset,1,1,2);run();
    }
    for(alignment=0;alignment<4;++alignment)for(unsigned a=0;a<9;++a){
        static const uint32_t offsets[]={0,4,8,12,16,40};
        uint32_t dest=a<6u?DATA+0x101u+alignment+offsets[a]:0x500000u+(a-6u)*4u;
        setup(0,3,0,0,2,2,dest,a==0?DATA+0x2801u+alignment:a>=6?DATA+0x201u+alignment:4u,0,3);run();
    }
    stage=4;
    for(unsigned x=0;x<16;++x)for(unsigned y=0;y<16;++y){
        alignment=0;setup(0,7,0,0,values[x],values[y],DATA+0x2801u,0x12345678,1,3);run();
    }
    REQUIRE(stores&&metadata_stores&&caller_stores&&counter_crossings&&recaptured_rows&&period_changes,"gradient coverage");
    PRINT("Gradient scenarios: ");number(cases);PRINT("; executions: ");number(cases*3u);
    PRINT("; excluded large/unmapped scenarios: ");number(skipped);PRINT("; stores: ");number(stores);
    PRINT("; metadata/caller stores: ");number(metadata_stores);PRINT(" ");number(caller_stores);
    PRINT("; counter crossings/recaptured rows/live period changes: ");number(counter_crossings);PRINT(" ");
    number(recaptured_rows);PRINT(" ");number(period_changes);
    PRINT("; gradient rectangles verified against 32 KiB memory/return/eight live arguments/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
