/* Real service initializer -> rectangle slots 155/156; third entry is direct. */
#define ROUNDED_CHAIN
#include "kernel_raster_rounded.c"
#undef ROUNDED_CHAIN
static uint32_t reference_table[320];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(SERVICES,4096)&&map(0x610000,4096),"rounded chain maps");
    uint32_t total=0,excluded=0;stage=10;
    static const uint32_t depths[]={0,8,15,16,24,32};
    for(kind=0;kind<3;++kind)for(unsigned depth=0;depth<6;++depth)
      for(accelerated=0;accelerated<2;++accelerated)for(alignment=0;alignment<4;++alignment)
      for(unsigned r=0;r<5;++r)for(unsigned mode=0;mode<4;++mode){
        format=depths[depth]==32;
        args[0]=ids[(r+mode)%16u];args[1]=mode;args[2]=mode;
        args[3]=8;args[4]=7;args[5]=r;args[6]=random32();
        setup(16,16,DATA+0x1001u+alignment);
        scenario=total+excluded;begin_oracle();uint32_t value;
        if(!rounded_oracle(&value)){++excluded;continue;}
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/raster_rounded_candidate.x":"build/raster_rounded_original.x",0x800000),"rounded chain image");
            *(volatile uint32_t *)0x1b0e4=SERVICES;*(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[depth];
            *(volatile uint32_t *)0x1a868=accelerated?1u<<25:0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(SERVICES+i*4u)=i*0x1234567u+cases;
            /* Other depths deliberately retain existing display callbacks.
             * Give that state real 8-bit callees before the initializer. */
            *(volatile uint32_t *)(SERVICES+0)=0x81b26e;
            *(volatile uint32_t *)(SERVICES+4)=0x81b734;
            *(volatile uint32_t *)(SERVICES+8)=0x81ba8a;
            ((void (*)(void))0x8005aa)();
            REQUIRE(*(volatile uint32_t *)(SERVICES+0)==(format?0x81b2f4u:0x81b26eu)&&
                *(volatile uint32_t *)(SERVICES+4)==(format?0x81b8dcu:0x81b734u)&&
                *(volatile uint32_t *)(SERVICES+8)==(format?0x81bd26u:0x81ba8au),"rounded mode slots");
            if(kind<2)REQUIRE(*(volatile uint32_t *)(SERVICES+(155u+kind)*4u)==rounded_entries[kind],"rounded initialized slot");
            for(unsigned i=0;i<320;++i){
                uint32_t word=*(volatile uint32_t *)(SERVICES+i*4u);
                if(!implementation)reference_table[i]=word;
                else REQUIRE(word==reference_table[i],"rounded entire initialized table");
            }
            reset();uint32_t actual[9];
            uint32_t entry=kind<2?*(volatile uint32_t *)(SERVICES+(155u+kind)*4u):rounded_entries[kind];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)entry,args,actual),"rounded chain ABI");
            compare(actual,value);
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(SERVICES+i*4u)==reference_table[i],"rounded chain table mutation");
        }
        ++total;++cases;
    }
    PRINT("Rounded initialized-service scenarios: ");number(total);PRINT("; executions: ");number(total*2u);
    PRINT("; excluded large/unmapped: ");number(excluded);
    PRINT("; rounded rectangles verified through real initializer/six depths/two CPU copy selections/slots 155..156 and direct chamfer/entire drawing chain; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
