/* Bytewise live-memory oracle, with widened signed arithmetic and explicit
 * modulo reduction. Large or unmapped generated fixtures are counted/skipped
 * before either original x86 or C is executed; the routines add no clipping.
 */
#include "../kernel/raster_line.h"
#include "raster_fixture.h"
static uint32_t align,format,counts[2],writes[2],branches[2],no_draw,aliases,caller_aliases;
static const uint32_t origins[][2]={{0,0},{4,3},{0xfffffffc,0xfffffffe},
    {0x3ffffcu,0x200001u},{0x7ffffffcu,0x80000003u},{0x80000000u,0x7fffffffu}};
static uint32_t fixed_integer(uint32_t value)
{
    int64_t v=signed32(value),q=v/1024;
    if(v<0&&v%1024)--q;
    return (uint32_t)q;
}
static uint32_t quotient(uint32_t numerator,uint32_t denominator)
{
    int64_t sn=signed32(numerator),sd=signed32(denominator);
    uint64_t n=sn<0?-sn:sn,d=sd<0?-sd:sd,result=0;
    for(int bit=31;bit>=0;--bit)if((d<<bit)<=n){n-=d<<bit;result|=1ULL<<bit;}
    return (uint32_t)((sn<0)!=(sd<0)?0ULL-result:result);
}
static int valid(uint32_t a,uint32_t bytes)
{for(uint32_t i=0;i<bytes;++i)if(index_of(a+i)==UINT32_MAX)return 0;return 1;}
static void setup(uint32_t id,uint32_t width,uint32_t x0,uint32_t y0,
                  uint32_t x1,uint32_t y1,uint32_t destination,uint32_t color)
{
    pattern();args[0]=id;args[1]=x0;args[2]=y0;args[3]=x1;args[4]=y1;args[5]=color;
    uint32_t surface=DATA+0x101u+align,other=DATA+0x201u+align;
    uint64_t index=(uint64_t)fixed_integer(y0<<10)*width+fixed_integer(x0<<10);
    uint32_t pixels=destination-(uint32_t)(index*(format?4u:1u));
    REQUIRE(put(initial,0x500000u+id*4u,surface,4)&&put(initial,surface,pixels,4)&&
            put(initial,surface+12u,width,4)&&put(initial,surface+16u,values[cases%16u],4),"fixture surface");
    if(id!=1u)REQUIRE(put(initial,0x500004u,other,4),"fixture second slot");
    REQUIRE(put(initial,other,DATA+0x2401u+align,4)&&put(initial,other+12u,7,4),"fixture second surface");
}
static int oracle(uint32_t *result)
{
    uint32_t endpoints[4]={args[1],args[2],args[3],args[4]};
    uint32_t magnitude[2],increment[2];
    for(uint32_t a=0;a<2;++a){
        uint32_t difference=(uint32_t)((uint64_t)endpoints[a+2u]-endpoints[a]);
        magnitude[a]=signed32(difference)<0?(uint32_t)(0x100000000ULL-difference):difference;
    }
    uint32_t major=signed32(magnitude[0])>=signed32(magnitude[1])?0u:1u,minor=major^1u;
    uint32_t count=(uint32_t)((uint64_t)magnitude[major]+1u);
    increment[major]=signed32(endpoints[major])>signed32(endpoints[major+2u])?0xfffffc00u:1024u;
    int64_t adjustment=signed32(endpoints[minor])<=signed32(endpoints[minor+2u])?1:-1;
    uint32_t delta=(uint32_t)((uint64_t)endpoints[minor+2u]-endpoints[minor]+adjustment);
    uint32_t numerator=(uint32_t)((uint64_t)delta*1024u);
    REQUIRE(count!=0&&!(numerator==0x80000000u&&count==UINT32_MAX),"unexpected generated division fault");
    increment[minor]=quotient(numerator,count);
    if(signed32(count)>96)return 0;
    ++branches[major];
    if(signed32(count)<=0){++no_draw;*result=0;return 1;}
    uint32_t fixed[2]={(uint32_t)((uint64_t)endpoints[0]*1024u),
                       (uint32_t)((uint64_t)endpoints[1]*1024u)};
    for(uint32_t i=0;i<count;++i){
        uint32_t id=get32(expected,ARGUMENTS),color=get32(expected,ARGUMENTS+20u);
        uint32_t slot=(uint32_t)(0x500000ULL+(uint64_t)id*4u);
        if(!valid(slot,4))return 0;
        uint32_t surface=get32(expected,slot);
        if(!valid(surface+12u,4)||!valid(surface,4))return 0;
        uint64_t index=(uint64_t)fixed_integer(fixed[1])*get32(expected,surface+12u)+fixed_integer(fixed[0]);
        uint32_t address=(uint32_t)((uint64_t)get32(expected,surface)+index*(format?4u:1u));
        if(!put(expected,address,color,format?4u:1u))return 0;
        if(address-ARGUMENTS<24u)++caller_aliases;
        if(address-DATA<0x1000u||address-0x500000u<4096u)++aliases;
        ++writes[format];
        for(uint32_t a=0;a<2;++a)fixed[a]=(uint32_t)((uint64_t)fixed[a]+increment[a]);
    }
    *result=count;return 1;
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();uint32_t value;
    uint32_t old_writes=writes[format],old_aliases=aliases,old_caller=caller_aliases;
    if(!oracle(&value)){
        ++skipped;writes[format]=old_writes;aliases=old_aliases;caller_aliases=old_caller;return;
    }
    const void *functions[]={format?(void *)0x91b8dc:(void *)0x91b734,
        format?(void *)kernel_raster_line32_81b8dc:(void *)kernel_raster_line8_81b734,
        format?(void *)0x81b8dc:(void *)0x81b734};
    for(implementation=0;implementation<3;++implementation){
        reset();uint32_t actual[7];
        REQUIRE(!raster_checked_call(functions[implementation],args,actual),"line ABI mismatch");
        compare(actual,value);
    }
    ++cases;++counts[format];
}
void _start(void)
{
    mappings();REQUIRE(load("build/raster_original.x",0x900000),"original load");
#ifdef TEST_INSTALLED
    REQUIRE(load("build/icestar_c.x",0x800000),"installed load");
#else
    REQUIRE(load("build/raster_preview.x",0x800000),"preview load");
#endif
    stage=1;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t origin=0;origin<6;++origin)
      for(uint32_t dx=0;dx<17;++dx)for(uint32_t dy=0;dy<17;++dy){
        uint32_t x=origins[origin][0],y=origins[origin][1];
        setup(ids[(origin+dx+dy)%16u],31,x,y,x+dx-8u,y+dy-8u,
              DATA+0x2801u+align,random32());run();
      }
    stage=2;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t w=0;w<16;++w)
      for(uint32_t x=0;x<16;++x)for(uint32_t y=0;y<16;++y){
        setup(ids[(w+x+y)%16u],values[w],values[x],values[y],values[x],values[y],
              DATA+0x2801u+align,random32());run();
      }
    stage=3;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t color=0;color<256;++color)
      for(uint32_t direction=0;direction<8;++direction){
        uint32_t x=4,y=4,x1=4,y1=4;
        if(direction&1u)x1+=direction&2u?3u:0xfffffffdu;
        else y1+=direction&2u?3u:0xfffffffdu;
        if(direction&4u){x1+=2u;y1-=2u;}
        setup(0,31,x,y,x1,y1,DATA+0x2801u+align,
              color|(color^255u)<<8|color<<16|(color^255u)<<24);run();
      }
    stage=4;
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t alias=0;alias<10;++alias){
        static const uint32_t offsets[]={0,4,8,12,16,36,40};
        uint32_t destination=alias<7u?DATA+0x101u+align+offsets[alias]:0x500000u+(alias-7u)*4u;
        uint32_t color=alias==0?DATA+0x2401u+align:alias>=7?DATA+0x201u+align:4u;
        setup(0,3,0,0,3,3,destination,color);run();
    }
    for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t offset=0;offset<24;++offset){
        uint32_t x=offset<4u?0u:1u,x1=offset<4u?1u:0u;
        setup(0,3,x,0,x1,0,ARGUMENTS+offset,1);run();
    }
    stage=5;
    for(format=0;format<2;++format)for(uint32_t dx=0;dx<16;++dx)for(uint32_t dy=0;dy<16;++dy){
        setup(0,7,0,0,values[dx],values[dy],DATA+0x2801u,random32());run();
    }
    /* Negative loop count must bypass the entirely unmapped surface table. */
    for(format=0;format<2;++format)for(uint32_t axis=0;axis<2;++axis){
        setup(0,7,0,0,axis?0u:0x7fffffffu,axis?0x7fffffffu:0u,DATA+0x2801u,random32());
        args[0]=0x17283940u;run();
    }
    /* Positive one-pixel lines read width/pixels but never an unmapped height. */
    for(format=0;format<2;++format){
        setup(0,7,0,0,0,0,DATA+0x2801u,random32());
        uint32_t s=DATA+16384u-16u;
        REQUIRE(put(initial,0x500000u,s,4)&&put(initial,s,DATA+0x2801u,4)&&put(initial,s+12u,7,4),"guard line");run();
    }
    PRINT("Line cases8/32: ");number(counts[0]);PRINT(" ");number(counts[1]);PRINT("; executions: ");number(cases*3u);
    PRINT("; oracle writes8/32: ");number(writes[0]);PRINT(" ");number(writes[1]);
    PRINT("; no-draw counts: ");number(no_draw);PRINT("; X/Y-major plans: ");number(branches[0]);PRINT(" ");number(branches[1]);
    PRINT("; metadata/table writes: ");number(aliases);PRINT("; caller writes: ");number(caller_aliases);
    PRINT("; excluded large/unmapped scenarios: ");number(skipped);
    PRINT("; lines verified against 32 KiB memory/return/live-argument/ABI oracle; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
