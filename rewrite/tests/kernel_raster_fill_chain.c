/* Real initializer -> slot two -> six-argument forwarder -> fill -> copy. */
#define _start raster_fill_unit_start
#include "kernel_raster_fill.c"
#undef _start
#define DISPLAY 0x610000u
static uint8_t saved_services[4096];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(DISPLAY,4096)&&map(SERVICES,4096),"chain mappings");
    uint32_t total=0,excluded=0;
    for(implementation=0;implementation<2;++implementation){
        REQUIRE(load(implementation?"build/raster_fill_candidate.x":"build/raster_fill_original.x",0x800000),"chain image");
        random_state=0x79268314u;cases=0;
        for(format=0;format<2;++format)for(accelerated=0;accelerated<2;++accelerated)
          for(align=0;align<4;++align)for(uint32_t w=0;w<16;++w)for(uint32_t h=0;h<16;++h)
          for(uint32_t reverse=0;reverse<4;++reverse){
            stage=10+format;scenario=total+excluded;
            setup(ids[(w+h)%16u],values[w],values[h],reverse&1u?6u:0xfffffffeu,reverse&2u?3u:0u,
                reverse&1u?0xfffffffeu:6u,reverse&2u?0u:3u,DATA+0x2801u+align,random32());
            begin_oracle();uint32_t value;
            if(!oracle(&value)){++excluded;continue;}
            reset();services(0x800000);
            *(volatile uint32_t *)0x1b034=DISPLAY;
            *(volatile uint32_t *)0x1a868=accelerated?1u<<25:0u;
            *(volatile uint32_t *)(DISPLAY+36u)=format?32u:8u;
            uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)0x8005aa,args,actual),"initializer ABI");
            REQUIRE(*(volatile uint32_t *)(SERVICES+8u)==(format?0x81bd26u:0x81ba8au),"mode-selected fill");
            REQUIRE(*(volatile uint32_t *)(SERVICES+68u)==(accelerated?0x8173dau:0x817322u),"feature-selected copy");
            for(uint32_t i=0;i<4096;++i)saved_services[i]=*(volatile uint8_t *)(SERVICES+i);
            REQUIRE(!raster_checked_call((void *)0x837545,args,actual),"fill forwarder ABI");
            /* The forwarder copies six arguments 48 bytes below its caller.
             * Both original and recovered forwarders retain that call layout. */
            compare(actual,format?value-48u:value);
            for(uint32_t i=0;i<4096;++i)REQUIRE(saved_services[i]==*(volatile uint8_t *)(SERVICES+i),"dispatch table mutation");
            ++cases;++total;
          }
    }
    PRINT("Fill service-chain original/candidate executions: ");number(total);
    PRINT("; excluded large/unmapped executions: ");number(excluded);
    PRINT("; rectangle fills verified through real initializer/mode/feature selection/forwarder/copy services; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
