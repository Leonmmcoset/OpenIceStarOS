/* Actual mode initializer -> service slot one -> real forwarder -> real line.
 * The host fixture and oracle are reused; no CALL is redirected. */
#define _start raster_line_unit_start
#include "kernel_raster_line.c"
#undef _start
#define SERVICES 0x620000u
#define DISPLAY 0x610000u
static uint8_t saved_services[4096];

void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(DISPLAY,4096)&&map(SERVICES,4096),"chain mappings");
    uint32_t total=0,excluded=0;
    for(implementation=0;implementation<2;++implementation){
        REQUIRE(load(implementation?"build/raster_chain_candidate.x":"build/raster_original.x",0x800000),"chain load");
        random_state=0x79268314u;cases=0;
        for(format=0;format<2;++format)for(align=0;align<4;++align)for(uint32_t origin=0;origin<6;++origin)
          for(uint32_t dx=0;dx<9;++dx)for(uint32_t dy=0;dy<9;++dy){
            stage=10+format;scenario=total+excluded;
            uint32_t x=origins[origin][0],y=origins[origin][1];
            setup(ids[(origin+dx+dy)%16u],31,x,y,x+dx-4u,y+dy-4u,
                  DATA+0x2801u+align,random32());begin_oracle();
            uint32_t value;
            if(!oracle(&value)){++excluded;continue;}
            reset();
            *(volatile uint32_t *)0x1b0e4=SERVICES;
            *(volatile uint32_t *)0x1b034=DISPLAY;
            *(volatile uint32_t *)0x1a868=0;
            *(volatile uint32_t *)(DISPLAY+36u)=format?32u:8u;
            for(uint32_t i=0;i<4096;++i)*(volatile uint8_t *)(SERVICES+i)=(uint8_t)(i*37u+total);
            uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)0x8005aa,args,actual),"chain initializer ABI");
            REQUIRE(*(volatile uint32_t *)(SERVICES+4u)==(format?0x81b8dcu:0x81b734u),"mode-selected line target");
            for(uint32_t i=0;i<4096;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
            REQUIRE(!raster_checked_call((void *)0x837501,args,actual),"chain line forwarder ABI");
            compare(actual,value);
            for(uint32_t i=0;i<4096;++i)
                REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"line mutated dispatch table");
            ++cases;++total;
          }
    }
    PRINT("Line service-chain original/candidate executions: ");number(total);
    PRINT("; excluded large/unmapped executions: ");number(excluded);
    PRINT("; real initializer/mode selection/forwarder/line chain verified; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
