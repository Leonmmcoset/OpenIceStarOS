#define WIDE_CHAIN
#include "kernel_text_wide.c"
#undef WIDE_CHAIN
#define SERVICES 0x620000u
static uint32_t reference_table[320];
void _start(void)
{
    wide_mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(SERVICES,4096),"wide chain maps");
    uint32_t total=0;stage=10;
    static const uint32_t depths[]={0,8,15,16,24,32};
    for(unsigned depth=0;depth<6;++depth)for(unsigned accelerated=0;accelerated<2;++accelerated)
      for(unsigned masked=0;masked<2;++masked)for(alignment=0;alignment<4;++alignment)
      for(unsigned code=0;code<32;++code){
        kind=(depths[depth]==32?2u:0u)+masked;
        args[0]=ids[code%16u];args[1]=2;args[2]=3;args[3]=random32();args[4]=random32();
        setup(24,24,DATA+0x1801u+alignment,code<16?160u+code:255u,254u-code);
        scenario=total;begin_oracle();uint32_t value;REQUIRE(wide_oracle(&value),"wide chain oracle");
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/text_wide_candidate.x":"build/text_wide_original.x",0x800000),"wide chain image");
            *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[depth];*(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            *(volatile uint32_t *)(SERVICES+7u*4u)=entries[0];
            *(volatile uint32_t *)(SERVICES+14u*4u)=entries[1];
            ((void (*)(void))0x8005aa)();
            unsigned slot=masked?14u:7u;
            REQUIRE(*(volatile uint32_t *)(SERVICES+slot*4u)==entries[kind],"wide initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t value=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)reference_table[i]=value;
                else REQUIRE(value==reference_table[i],"wide entire initialized table");
            }
            reset();font_reset();uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)(SERVICES+slot*4u),args,actual),"wide chain ABI");
            compare(actual,value);font_compare();
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==reference_table[i],"wide chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Wide initialized-service scenarios: ");number(total);PRINT("; executions: ");number(total*2u);
    PRINT("; wide glyphs verified through real initializer/six depths/two CPU selections/slots 7 and 14; unsupported depths retain seeded real 8-bit callees; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
