/* Execute the original x87 decimal rasterizer, ordinary C and fixed-address
 * C against identical memory and FP state. Literal radius-zero/one oracles
 * independently check stores; larger circles use original machine execution.
 */
#include "../kernel/raster_disc.h"
#include "raster_fixture.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
static uint32_t format,alignment,precision,rounding,depth,counts[2],fp_cases,
    literal_cases,alias_cases,changed_color_cases,stalled_cases;
static uint32_t reference_result[7];
static uint8_t reference_memory[REGION],reference_fp[512] __attribute__((aligned(16)));
static uint8_t actual_fp[512] __attribute__((aligned(16)));
static const uint32_t precision_bits[]={0,0x200,0x300};
static uint16_t reference_cw,reference_sw;

static void fp_start(void)
{
    uint16_t cw=(uint16_t)(0x7f|precision_bits[precision]|rounding*0x400u);
    __asm__ volatile("fninit; fldcw %0"::"m"(cw):"memory");
    if(cases&1u)__asm__ volatile("fldz; fldz; fdivp; fstp %%st(0)":::"memory");
    if(depth)__asm__ volatile("fld1; fldpi":::"memory");
}
static void fp_read(uint8_t *state,uint16_t *cw,uint16_t *sw)
{
    __asm__ volatile("fnstcw %0; fnstsw %1":"=m"(*cw),"=m"(*sw)::"memory");
    __asm__ volatile("fxsave (%0)"::"r"(state):"memory");
    __asm__ volatile("fninit":::"memory");
}
static void setup(uint32_t id,uint32_t width,uint32_t height,uint32_t x,
                  uint32_t y,uint32_t radius,uint32_t color)
{
    pattern();args[0]=id;args[1]=x;args[2]=y;args[3]=radius;args[4]=color;args[5]=0x8badf00d;
    uint32_t record=DATA+0x101u+alignment,other=DATA+0x201u+alignment;
    REQUIRE(put(initial,0x500000u+id*4u,record,4)&&
        put(initial,record,DATA+0x1001u+alignment,4)&&
        put(initial,record+12u,width,4)&&put(initial,record+16u,height,4),"disc fixture");
    if((id&0x3fffffffu)!=1u)REQUIRE(put(initial,0x500004u,other,4),"secondary ID");
    REQUIRE(put(initial,other,DATA+0x2801u+alignment,4)&&put(initial,other+12u,16,4)&&
        put(initial,other+16u,16,4),"secondary record");
}
static uint32_t address_at(unsigned i)
{
    return i<8192u?TABLE+i:i<16384u?ORDER+i-8192u:DATA+i-16384u;
}
static void literal_oracle(void)
{
    if(stage!=1)return;
    uint32_t record=get32(initial,0x500000u+args[0]*4u);
    uint32_t base=get32(initial,record),pitch=get32(initial,record+12u),bytes=format?4u:1u;
    uint32_t radius=args[3],x=args[1],y=args[2];
    for(unsigned i=0;i<REGION+24;++i)expected[i]=initial[i];
    if(radius<=1u){
        static const int8_t points[5][2]={{0,0},{-1,0},{1,0},{0,-1},{0,1}};
        for(unsigned p=0;p<(radius?5u:1u);++p){
            uint32_t at=base+((y+(uint32_t)points[p][1])*pitch+x+(uint32_t)points[p][0])*bytes;
            REQUIRE(put(expected,at,args[4],bytes),"literal pixels in mapped memory");
        }
    }
    compare(reference_result,radius<=1u?radius+1u:1u);++literal_cases;
}
static void check_execution(const void *target)
{
        reset();fp_start();uint32_t result[7];uint16_t cw,sw;
        REQUIRE(!raster_checked_call(target,args,result),"disc ABI mismatch");
        fp_read(actual_fp,&cw,&sw);
        if(implementation==0){
            for(unsigned i=0;i<7;++i)reference_result[i]=result[i];
            for(unsigned i=0;i<REGION;++i)reference_memory[i]=BYTE(address_at(i));
            for(unsigned i=0;i<512;++i)reference_fp[i]=actual_fp[i];
            reference_cw=cw;reference_sw=sw;literal_oracle();
            if(stage==4){++alias_cases;if(reference_result[5]!=args[4])++changed_color_cases;}
        }else{
            for(unsigned i=0;i<7;++i)REQUIRE(result[i]==reference_result[i],"disc raw result/live words");
            for(unsigned i=0;i<REGION;++i)
                REQUIRE(BYTE(address_at(i))==reference_memory[i],"disc full memory mismatch");
            REQUIRE(cw==reference_cw&&cw==(uint16_t)(0x7f|precision_bits[precision]|rounding*0x400u),"disc control word");
            REQUIRE((sw&0x383fu)==(reference_sw&0x383fu),"disc x87 flags/stack mismatch");
            if(depth)for(unsigned s=0;s<2;++s)for(unsigned i=0;i<10;++i)
                REQUIRE(actual_fp[32u+s*16u+i]==reference_fp[32u+s*16u+i],"incoming x87 data");
        }
}
static void run(void)
{
    scenario=cases+skipped;begin_oracle();
    REQUIRE(signed32(args[3])<=16,"unbounded disc fixture radius");
    /* Original decimal additions can stagnate at 24-bit precision. Keep
     * those inputs outside the terminating matrix; a separate child-process
     * watchdog below observes an actual stalled original and both C builds.
     */
    if(!precision&&args[3]>1u&&args[3]<=16u){++skipped;return;}
    const void *functions[]={format?(void *)0x91cd50:(void *)0x91ca82,
        format?(void *)kernel_raster_disc32_81cd50:(void *)kernel_raster_disc8_81ca82,
        format?(void *)0x81cd50:(void *)0x81ca82};
    for(implementation=0;implementation<3;++implementation)check_execution(functions[implementation]);
    ++cases;++counts[format];if(args[3]&&args[3]<0x80000000u)++fp_cases;
}
static void check_stagnation(void)
{
    stage=5;alignment=precision=rounding=depth=0;
    for(format=0;format<2;++format)for(unsigned kind=0;kind<2;++kind){
        /* A center exactly at the right edge also stalls the original
         * central span because each iteration clamps x back to width-1. */
        setup(0,48,48,kind?48u:12u,12,kind?0u:2u,0x12345678);begin_oracle();
        const void *functions[]={format?(void *)0x91cd50:(void *)0x91ca82,
            format?(void *)kernel_raster_disc32_81cd50:(void *)kernel_raster_disc8_81ca82,
            format?(void *)0x81cd50:(void *)0x81ca82};
        for(implementation=0;implementation<3;++implementation){
            int child=syscall3(2,0,0,0);REQUIRE(child>=0,"fork stagnation probe");
            if(!child){
                /* SIGALRM after 100 ms; default disposition terminates. */
                uint32_t timer[]={0,0,0,100000},result[7];
                REQUIRE(!syscall3(104,0,(uint32_t)timer,0),"stagnation timer");
                reset();fp_start();raster_checked_call(functions[implementation],args,result);
                syscall3(1,77,0,0);
            }
            uint32_t status=0;
            REQUIRE(syscall3(7,(uint32_t)child,(uint32_t)&status,0)==child&&
                (status&0x7fu)==14u,"expected original/C single-precision stagnation");
            ++stalled_cases;
        }
    }
}
void _start(void)
{
    mappings();REQUIRE(load("build/raster_disc_original.x",0x900000)&&
        load("build/raster_disc_candidate.x",0x800000),"disc image load");
    stage=1;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(precision=0;precision<3;++precision)for(rounding=0;rounding<4;++rounding)
      for(depth=0;depth<2;++depth)for(unsigned radius=0;radius<3;++radius){
        setup(ids[cases%16u],32,32,5,6,radius==2?0xffffffffu:radius,random32());run();
      }
    stage=2;depth=0;
    static const uint32_t origins[]={0,1,4,12,24};
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(precision=0;precision<3;++precision)for(rounding=0;rounding<4;++rounding)
      for(unsigned origin=0;origin<5;++origin)for(unsigned radius=1;radius<=16;++radius){
        setup(ids[(radius+origin)%16u],48,48,origins[origin],12,radius,random32());run();
      }
    stage=3;depth=1;
    for(format=0;format<2;++format)for(precision=0;precision<3;++precision)
      for(rounding=0;rounding<4;++rounding)for(unsigned h=0;h<16;++h)
      for(unsigned y=0;y<6;++y)for(unsigned radius=0;radius<5;++radius){
        static const uint32_t ys[]={0xfffffffdu,0xffffffffu,0,1,8,16};
        alignment=(h+y)%4u;setup(ids[(h+y+radius)%16u],48,values[h],4,ys[y],radius,random32());run();
      }
    stage=4;depth=0;
    for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
      for(precision=0;precision<3;++precision)for(rounding=0;rounding<4;++rounding){
        uint32_t record=DATA+0x101u+alignment;
        /* Change the active record after the first central store. */
        setup(0,32,32,4,0,1,format?DATA+0x201u+alignment:2u);
        uint32_t destination=0x500000u+(format?0u:1u);
        REQUIRE(put(initial,record,destination-3u*(format?4u:1u),4),"record alias base");run();
        /* Narrow live width, preserving a strictly progressing span. */
        setup(0,32,32,4,0,format?1u:0u,16);
        REQUIRE(put(initial,record,record+12u-(format?3u*4u:4u),4),"width alias base");run();
        /* Radius changes from zero to -1, ending the live loops. */
        setup(0,32,32,4,0,0,format?0xffffffffu:0x80u);
        REQUIRE(put(initial,record,ARGUMENTS+(format?12u:15u)-4u*(format?4u:1u),4),"radius alias base");run();
        /* Wrapped negative radius yields exactly two central stores. The
         * unaligned output changes the next store's live color. */
        setup(0,16,16,0x80000000u,0,0x80000001u,format?0x12345680u:0x12345678u);
        REQUIRE(put(initial,record,ARGUMENTS+(format?15u:17u),4),"color alias base");run();
      }
    check_stagnation();
    REQUIRE(alias_cases&&changed_color_cases&&fp_cases&&literal_cases,"missing disc coverage");
    PRINT("Disc cases8/32: ");number(counts[0]);PRINT(" ");number(counts[1]);
    PRINT("; executions: ");number(cases*3u);PRINT("; x87 scenarios: ");number(fp_cases);
    PRINT("; literal oracles: ");number(literal_cases);PRINT("; aliases/changed colors: ");
    number(alias_cases);PRINT(" ");number(changed_color_cases);
    PRINT("; excluded single-precision scenarios: ");number(skipped);
    PRINT("; watchdog-confirmed stalls: ");number(stalled_cases);
    PRINT("; filled circles verified against original memory/return/live-argument/x87/ABI evidence; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
