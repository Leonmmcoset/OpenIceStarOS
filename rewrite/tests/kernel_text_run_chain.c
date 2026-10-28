#define RUN_CHAIN
#include "kernel_text_run.c"
#undef RUN_CHAIN
#define SERVICES 0x620000u
static uint32_t reference_table[320];
void _start(void)
{
    wide_mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(SERVICES,4096),"run chain maps");
    uint32_t total=0,excluded=0;stage=10;static const uint32_t depths[]={0,8,15,16,24,32};
    for(unsigned depth=0;depth<6;++depth)for(unsigned accelerated=0;accelerated<2;++accelerated)
      for(unsigned vertical=0;vertical<2;++vertical)for(alignment=0;alignment<4;++alignment)
      for(unsigned mode=0;mode<16;++mode){
        kind=(depths[depth]==32?2u:0u)+vertical;
        args[0]=ids[mode%16u];args[1]=2;args[2]=8;args[3]=random32();args[4]=random32();
        run_setup(24,24,DATA+0x1801u+alignment);
        for(unsigned i=0;i<mode;++i)text_byte(i,i%3u?161u:65u);
        text_byte(mode,0);text_byte(mode+1u,0);
        scenario=total+excluded;begin_oracle();uint32_t value;
        if(!run_oracle(&value)){++excluded;continue;}
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/text_run_candidate.x":"build/text_run_original.x",0x800000),"run chain image");
            *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[depth];*(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            *(volatile uint32_t *)0x1b020=ascii_font_base;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            *(volatile uint32_t *)(SERVICES+8u*4u)=run_entries[0];
            *(volatile uint32_t *)(SERVICES+13u*4u)=run_entries[1];
            ((void (*)(void))0x8005aa)();unsigned slot=vertical?13u:8u;
            REQUIRE(*(volatile uint32_t *)(SERVICES+slot*4u)==run_entries[kind],"run initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t word=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)reference_table[i]=word;else REQUIRE(word==reference_table[i],"run entire initialized table");
            }
            reset();font_reset();uint32_t actual[7];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)(SERVICES+slot*4u),args,actual),"run chain ABI");
            compare(actual,value);font_compare();
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==reference_table[i],"run chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Text-run initialized-service scenarios: ");number(total);PRINT("; executions: ");number(total*2u);
    PRINT("; excluded unsafe: ");number(excluded);
    PRINT("; text runs verified through real initializer/six depths/two CPU selections/slots 8 and 13 and real composed glyph calls; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
