#define NUMBER_CHAIN
#include "kernel_text_number.c"
#undef NUMBER_CHAIN
#define SERVICES 0x620000u
static uint32_t number_reference_table[320];
void _start(void)
{
    wide_mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(SERVICES,4096),"number chain maps");
    uint32_t total=0,excluded=0;stage=10;
    static const uint32_t depths[]={0,8,15,16,24,32},radices[]={2,10,16,36};
    for(unsigned depth=0;depth<6;++depth)for(unsigned accelerated=0;accelerated<2;++accelerated)
      for(unsigned role=0;role<2;++role)for(alignment=0;alignment<4;++alignment)
      for(unsigned mode=0;mode<16;++mode){
        kind=depths[depth]==32?2u:0u;
        args[0]=ids[mode];args[1]=1;args[2]=2;args[3]=random32();args[4]=random32();
        args[5]=radices[mode%4u];args[6]=values[mode];args[7]=random32();
        number_setup(24,24,DATA+0x1801u+alignment);
        if(role){kind=1;args[0]=radices[mode%4u];args[1]=values[mode];}
        scenario=total+excluded;begin_oracle();
        uint32_t value;if(!number_oracle(&value)){++excluded;continue;}
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/text_number_candidate.x":"build/text_number_original.x",0x800000),"number chain image");
            *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[depth];*(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            *(volatile uint32_t *)0x1b020=ascii_font_base;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            *(volatile uint32_t *)(SERVICES+9u*4u)=number_entries[0];
            ((void (*)(void))0x8005aa)();unsigned slot=role?242u:9u;
            unsigned entry=role?2u:kind/2u;
            REQUIRE(*(volatile uint32_t *)(SERVICES+slot*4u)==number_entries[entry],"number initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t value=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)number_reference_table[i]=value;
                else REQUIRE(value==number_reference_table[i],"number entire initialized table");
            }
            reset();font_reset();uint32_t actual[9];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)(SERVICES+slot*4u),args,actual),"number chain ABI");
            compare(actual,value);font_compare();
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==number_reference_table[i],"number chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Number initialized-service scenarios: ");number(total);PRINT("; executions: ");number(total*2u);
    PRINT("; excluded unsafe: ");number(excluded);
    PRINT("; numbers verified through real initializer/six depths/two CPU choices/slots 9 and 242/real glyph chains; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
