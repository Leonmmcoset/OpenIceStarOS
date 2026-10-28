/* Actual display initializer -> mode-selected slot five -> five-arg SDK. */
#define _start raster_disc_unit_start
#include "kernel_raster_disc.c"
#undef _start
static uint32_t saved_table[320];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(0x620000,4096),"disc chain maps");
    uint32_t service_cases=0;
    stage=10;
    for(format=0;format<2;++format)for(precision=0;precision<3;++precision)
      for(rounding=0;rounding<4;++rounding)for(depth=0;depth<2;++depth)
      for(unsigned radius=0;radius<=8;++radius){
        if(!precision&&radius>1u){++skipped;continue;}
        scenario=service_cases;alignment=radius%4u;
        setup(ids[radius%16u],48,48,12,12,radius,random32());begin_oracle();
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/raster_disc_candidate.x":"build/raster_disc_original.x",0x800000),"chain kernel");
            *(volatile uint32_t *)0x1b0e4=0x620000;
            *(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=format?32u:8u;
            *(volatile uint32_t *)0x1a868=0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(0x620000u+i*4u)=i*0x1234567u+cases;
            ((void (*)(void))0x8005aa)();
            REQUIRE(*(volatile uint32_t *)0x620014==(format?0x81cd50u:0x81ca82u),"initialized disc slot");
            for(unsigned i=0;i<320;++i){
                uint32_t value=*(volatile uint32_t *)(0x620000u+i*4u);
                if(implementation==0)saved_table[i]=value;
                else REQUIRE(value==saved_table[i],"entire service table mismatch");
            }
            check_execution((void *)0x837609);
            for(unsigned i=0;i<320;++i)
                REQUIRE(*(volatile uint32_t *)(0x620000u+i*4u)==saved_table[i],"disc mutated service table");
        }
        ++cases;++service_cases;
    }
    PRINT("Disc initialized-service scenarios: ");number(service_cases);
    PRINT("; executions: ");number(service_cases*2u);
    PRINT("; filled circles verified through real initializer/mode selection/forwarder/x87 state; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
