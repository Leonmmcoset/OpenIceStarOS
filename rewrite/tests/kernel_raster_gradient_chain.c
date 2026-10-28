/* Real service initializer, followed by its common slot at offset 0x208.
 * The original table exposes this eight-argument function directly.
 */
#define _start raster_gradient_unit_start
#include "kernel_raster_gradient.c"
#undef _start
static uint32_t saved_table[320];
void _start(void)
{
    mappings();REQUIRE(map(0x1a000,8192)&&map(0x610000,4096)&&map(0x620000,4096),"gradient chain maps");
    uint32_t service_cases=0;stage=10;
    static const uint32_t depths[]={0,8,16,24,32,0xffffffffu};
    for(unsigned d=0;d<6;++d)for(unsigned feature=0;feature<2;++feature)
      for(alignment=0;alignment<4;++alignment)for(unsigned width=0;width<4;++width)
      for(unsigned p=0;p<8;++p){
        static const uint32_t periods[]={0,1,2,3,5,7,31,0xffffffff};
        scenario=service_cases;
        setup(ids[(width+p)%16u],31,0,0,width,2,DATA+0x2801u+alignment,random32(),random32(),periods[p]);
        begin_oracle();uint32_t value;REQUIRE(oracle(&value),"gradient service oracle");
        for(implementation=0;implementation<2;++implementation){
            REQUIRE(load(implementation?"build/raster_gradient_candidate.x":"build/raster_gradient_original.x",0x800000),"gradient chain kernel");
            *(volatile uint32_t *)0x1b0e4=0x620000;
            *(volatile uint32_t *)0x1b034=0x610000;
            *(volatile uint32_t *)0x610024=depths[d];
            *(volatile uint32_t *)0x1a868=feature?1u<<25:0;
            for(unsigned i=0;i<320;++i)*(volatile uint32_t *)(0x620000u+i*4u)=i*0x1234567u+cases;
            ((void (*)(void))0x8005aa)();
            REQUIRE(*(volatile uint32_t *)0x620208==0x81bf50u,"initialized gradient slot");
            for(unsigned i=0;i<320;++i){
                uint32_t word=*(volatile uint32_t *)(0x620000u+i*4u);
                if(!implementation)saved_table[i]=word;
                else REQUIRE(word==saved_table[i],"gradient entire service table");
            }
            reset();uint32_t actual[9];
            REQUIRE(!raster_checked_call((void *)(uintptr_t)*(volatile uint32_t *)0x620208,args,actual),"gradient service ABI");
            compare(actual,value);
            for(unsigned i=0;i<320;++i)REQUIRE(*(volatile uint32_t *)(0x620000u+i*4u)==saved_table[i],"gradient service mutated table");
        }
        ++service_cases;++cases;
    }
    PRINT("Gradient initialized-service scenarios: ");number(service_cases);
    PRINT("; executions: ");number(service_cases*2u);
    PRINT("; gradient rectangles verified through real initializer/display depths/CPU features/eight-argument table call; no CALL substitutions\n");
    syscall3(1,0,0,0);
}
