/* Actual display initializer -> slot three -> six-argument border forwarder. */
#define _start raster_outline_unit_start
#include "kernel_raster_outline.c"
#undef _start
#define SERVICES 0x620000u
#define DISPLAY 0x610000u
static uint8_t saved_services[4096];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(DISPLAY,4096)&&map(SERVICES,4096),"chain mappings");
    uint32_t total=0,excluded=0;
    for(implementation=0;implementation<2;++implementation){
        REQUIRE(load(implementation?"build/raster_outline_candidate.x":"build/raster_outline_original.x",0x800000),"chain load");
        cases=0;random_state=0x79268314u;
        for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t w=0;w<16;++w)
          for(uint32_t o=0;o<3;++o)for(uint32_t dx=0;dx<5;++dx)for(uint32_t dy=0;dy<5;++dy){
            static const uint32_t origins[][2]={{0,0},{3,4},{0xfffffffe,0xffffffff}};
            stage=10+format;scenario=total+excluded;uint32_t x=origins[o][0],y=origins[o][1];
            setup(ids[(w+o+dx+dy)%16u],values[w],x,y,x+dx-2u,y+dy-2u,DATA+0x2801u+align,random32());
            begin_oracle();uint32_t value;
            if(!oracle(&value)){++excluded;continue;}
            reset();*(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=DISPLAY;
            *(volatile uint32_t *)0x1a868=0;*(volatile uint32_t *)(DISPLAY+36u)=format?32u:8u;
            for(uint32_t i=0;i<4096;++i)*(volatile uint8_t *)(SERVICES+i)=(uint8_t)(i*37u+total);
            uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)0x8005aa,args,actual),"chain initializer ABI");
            REQUIRE(*(volatile uint32_t *)(SERVICES+12u)==(format?0x81c1e4u:0x81c058u),"mode-selected border");
            for(uint32_t i=0;i<4096;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
            REQUIRE(!raster_checked_call((void *)0x837589,args,actual),"outline forwarder ABI");
            compare(actual,value);
            for(uint32_t i=0;i<4096;++i)REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"service table mutation");
            ++cases;++total;
          }
    }
    PRINT("Outline service-chain original/candidate executions: ");number(total);
    PRINT("; excluded large/unmapped executions: ");number(excluded);
    PRINT("; rectangle outlines verified through real initializer/mode selection/forwarder; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
