/* Unhooked privileged paging execution, plus construction-only fixture runs
 * that suppress CR writes to check every emitted entry without CPU A/D bits. */
#include <stdint.h>
#include "../kernel/paging.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static uint32_t initial_directory[1024] __attribute__((aligned(4096)));
static uint32_t initial_tables[16][1024] __attribute__((aligned(4096)));
uint32_t paging_entry, paging_input_flags, paging_call_stack, paging_stack_after;
uint32_t paging_eax_after, paging_flags_after, paging_registers[6];
uint32_t paging_cr0_after, paging_cr3_after;
uint16_t paging_ds, paging_gs, paging_segments[5];
uint8_t paging_canaries[2];
void paging_invoke(void);
static uint32_t scenario, phase, count, pattern, construction, seed_cr0;
static uint32_t cr0_input, cr3_input, incoming_pg;
static const uint16_t ds_values[]={8,24}, gs_values[]={0,8,32,40};
static const uint32_t cr0_variants[]={0,0x10000,0x40002};

static void put(const char *s)
{ while(*s) __asm__ volatile("outb %0,$0xe9" :: "a"(*s++)); }
static void number(uint32_t n)
{
    char s[11]; uint32_t i=10; s[i]=0;
    do { s[--i]='0'+n%10; n/=10; } while(n);
    put(s+i);
}
__attribute__((noreturn)) static void done(uint32_t value)
{ __asm__ volatile("cli;outl %0,$0xf4" :: "a"(value)); for(;;) __asm__ volatile("hlt"); }
static void require(int condition,const char *message)
{
    if(condition) return;
    put("FAIL paging scenario/phase: "); number(scenario); put("/"); number(phase);
    put(" "); put(message); put("\n"); done(0x11);
}
static void setup_initial_map(void)
{
    uint32_t i;
    for(i=0; i<16384; ++i) initial_tables[i/1024][i%1024]=i*4096u|3u;
    for(i=0; i<16; ++i) initial_directory[i]=(uint32_t)initial_tables[i]|3u;
}
static void prepare(void)
{
    uint32_t i,fill=pattern?0xa5c39e71:0x5a3c618e;
    for(i=0; i<1024; ++i) WORD(0x19000+i*4)=fill;
    for(i=0; i<1048576; ++i) WORD(0x100000+i*4)=fill;
    for(i=0; i<64; ++i) {
        WORD(0x18f00+i*4)=fill; WORD(0x1a000+i*4)=fill; WORD(0x500000+i*4)=fill;
        WORD(0x1019000+i*4)=fill; WORD(0x1100000+i*4)=fill;
    }
    cr3_input=(uint32_t)initial_directory;
    cr0_input=(seed_cr0&~(0x80050002u))|cr0_variants[cr0_input];
    if(incoming_pg) cr0_input|=0x80000000u;
    __asm__ volatile("movl %0,%%cr3;movl %1,%%cr0" :: "r"(cr3_input),"r"(cr0_input) : "memory");
}
static void check(void)
{
    uint32_t i,mask=construction?0xffffffffu:~0x60u;
    uint32_t fill=pattern?0xa5c39e71:0x5a3c618e;
    uint32_t output_cr0=cr0_input|0x80000000u;
    require(paging_cr0_after==(construction?cr0_input:output_cr0),"CR0 preservation/PG");
    require(paging_cr3_after==(construction?cr3_input:0x19000u),"CR3 directory");
    require(paging_eax_after==output_cr0,"original EAX return");
    require(paging_stack_after==paging_call_stack && paging_canaries[0] && paging_canaries[1],"stack balance/canaries");
    for(i=0; i<6; ++i) require(paging_registers[i]==0x11223344u+i*0x11111111u,"register preservation");
    require(paging_segments[0]==paging_ds && paging_segments[1]==8 && paging_segments[2]==8 &&
            paging_segments[3]==paging_gs && paging_segments[4]==8,"segment preservation");
    /* OR clears CF/OF/ZF, sets SF, and sets PF from the low result byte.
     * AF is architecturally undefined, so it is deliberately excluded. */
    {
        uint32_t parity=output_cr0&255u;
        parity^=parity>>4; parity^=parity>>2; parity^=parity>>1;
        require((paging_flags_after&0xec5u)==
                (0x80u|((parity&1u)?0u:4u)|(paging_input_flags&0x600u)),"final OR flags/IF/DF");
    }
    for(i=0; i<1024; ++i)
        require((WORD(0x19000+i*4)&mask)==((0x100000u+i*4096u)|0x107u),"full PDE contents");
    for(i=0; i<1048576; ++i)
        require((WORD(0x100000+i*4)&mask)==((i*4096u)|0x107u),"full PTE contents/wrap");
    for(i=0; i<64; ++i) {
        require(WORD(0x18f00+i*4)==fill && WORD(0x1a000+i*4)==fill &&
                WORD(0x500000+i*4)==fill,"table boundary guard");
        require(WORD(0x1019000+i*4)==fill && WORD(0x1100000+i*4)==fill,"nonflat DS shadow guard");
    }
}
struct cr_patch { uint32_t offset; uint8_t bytes[3]; };
#include "../build/paging_cr_patches.h"
static void control_writes(uint32_t suppress)
{
    const struct cr_patch *patches;
    uint32_t i,j;
    patches=!phase?original_cr_patches:phase==1?standalone_cr_patches:candidate_cr_patches;
    for(i=0; i<2; ++i) for(j=0; j<3; ++j)
        BYTE(paging_entry+patches[i].offset+j)=suppress?0x90:patches[i].bytes[j];
}
void paging_qemu_main(void)
{
    uint32_t d,g,v;
    __asm__ volatile("movl %%cr0,%0" : "=r"(seed_cr0));
    __asm__ volatile("outb %0,$0x21;outb %0,$0xa1" :: "a"((uint8_t)255));
    setup_initial_map();
    for(construction=0; construction<2; ++construction)
    for(pattern=0; pattern<2; ++pattern)
    for(d=0; d<COUNT(ds_values); ++d)
    for(g=0; g<COUNT(gs_values); ++g)
    for(v=0; v<COUNT(cr0_variants); ++v)
    for(incoming_pg=0; incoming_pg<2; ++incoming_pg) {
        ++scenario;
        for(phase=0; phase<3; ++phase) {
            paging_ds=ds_values[d]; paging_gs=gs_values[g]; cr0_input=v;
            paging_input_flags=2u|(pattern?0x600u:0u);
            paging_entry=!phase?0x937448:phase==1?(uint32_t)kernel_init_identity_paging:0x837448;
            control_writes(construction);
            prepare(); paging_invoke(); check();
            control_writes(0); ++count;
        }
    }
    put("PASS: "); number(scenario); put(" paging scenarios, "); number(count);
    put(" original/C/fixed-address executions\n");
    put("All 1024 PDEs and 1048576 PTEs, GS stores, nonflat DS, null/nonflat GS restore, CR0/CR3, registers, stack and flags verified\n");
    put("Construction-only runs check exact bytes; unhooked paging runs allow CPU-generated A/D bits\n");
    done(0x10);
}
