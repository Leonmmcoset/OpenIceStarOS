/* Actual mode initializer -> slot four -> original five-argument forwarder. */
#define _start raster_circle_unit_start
#include "kernel_raster_circle.c"
#undef _start
#define SERVICES 0x620000u
#define DISPLAY 0x610000u
static uint8_t saved_services[4096];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(DISPLAY,4096)&&map(SERVICES,4096),"chain maps");
    uint32_t total=0,excluded=0;
    for(implementation=0;implementation<2;++implementation){
        REQUIRE(load(implementation?"build/raster_circle_candidate.x":"build/raster_circle_original.x",0x800000),"chain load");
        cases=0;random_state=0x79268314u;
        for(format=0;format<2;++format)for(alignment=0;alignment<4;++alignment)
          for(unsigned w=0;w<16;++w)for(unsigned origin=0;origin<3;++origin)for(unsigned radius=0;radius<5;++radius){
            stage=10+format;scenario=total+excluded;
            setup(ids[(w+origin+radius)%16u],values[w],17,origin*4u-2u,origin*3u-1u,radius,
                DATA+0x2801u+alignment,random32());
            begin_oracle();uint32_t value;
            if(!oracle(&value)){++excluded;continue;}
            reset();*(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=DISPLAY;
            *(volatile uint32_t *)0x1a868=0;*(volatile uint32_t *)(DISPLAY+36u)=format?32u:8u;
            for(unsigned i=0;i<4096;++i)*(volatile uint8_t *)(SERVICES+i)=(uint8_t)(i*37u+total);
            uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)0x8005aa,args,actual),"initializer ABI");
            REQUIRE(*(volatile uint32_t *)(SERVICES+16u)==(format?0x81c6f2u:0x81c396u),"mode circle slot");
            for(unsigned i=0;i<4096;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
            REQUIRE(!raster_checked_call((void *)0x8375cd,args,actual),"circle forwarder ABI");
            compare(actual,value);
            for(unsigned i=0;i<4096;++i)REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"service mutation");
            ++cases;++total;
          }
    }
    PRINT("Circle service-chain original/candidate executions: ");number(total);
    PRINT("; excluded unmapped executions: ");number(excluded);
    PRINT("; circle outlines verified through real initializer/mode selection/forwarder; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
