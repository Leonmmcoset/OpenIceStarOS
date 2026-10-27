/* CPL0 original/C/fixed-address comparison with an independent sequential
 * memory/I/O oracle. Only fixture CALL operands are redirected. CLI, POPFL,
 * IF/DF return behavior and VGA DAC port writes/readback are real hardware. */
#include <stdint.h>
#include "../kernel/display_dac.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define USED __attribute__((used, noinline))
#define DATA 0x1b000u
#define DATA_SIZE 0x3000u
#define MEMORY_SIZE (DATA_SIZE + 8192u)
static uint32_t directory[1024] __attribute__((aligned(4096)));
static uint32_t tables[5][1024] __attribute__((aligned(4096)));
static uint8_t expected[MEMORY_SIZE], hardware[768];
struct event { uint32_t kind, port, value; };
static struct event wanted[780];
struct range { uint32_t first, last, triples, prefix; };
static const struct range ranges[] = {
    {0,0,1,0}, {0,15,16,0}, {16,231,216,0}, {0,255,256,0},
    {255,255,1,0}, {256,257,2,0}, {0x12345678,0x1234567a,3,0},
    {0xfffffffd,0xffffffff,3,0}, {0xfffffffe,1,4,0},
    {0x80000000,0x80000003,4,0}, {1,0,0,0},
    {0x80000001,0x80000000,0,0}, {0,0xffffffff,0,0},
    {0x7fffffff,0x80000000,0,0}, {0x7ffffffe,0x7ffffffe,1,0},
    {0x7fffffff,0x7fffffff,4,1},
    {0x7ffffffe,0x7fffffff,4,1},
    {0xffffffff,0x7fffffff,4,1}
};
static const uint32_t records[] = {0x1c000,0x1c001,0x1bfff,0x1cfff,0xfffffffc,0xffffffff};
static const uint32_t flag_seeds[] = {2,0x203,0xcd6,0xed7};
uint32_t dac_entry, dac_first, dac_last, dac_rgb, dac_flags;
uint32_t dac_flags_after, dac_stack_after, dac_call_stack;
uint32_t dac_registers[4], dac_arguments_after[5], dac_aborted;
void dac_invoke(void);
__attribute__((noreturn)) void dac_abort(void);
static uint32_t wanted_count, seen_count, case_number, phase, pattern, mutation;
static uint32_t prefix, triples, hardware_test, hardware_checks, executions;
static uint32_t hardware_cursor;

static void out(uint32_t port, uint32_t value)
{ __asm__ volatile("outb %b0,%w1" :: "a"(value), "d"(port)); }
static uint32_t in(uint32_t port)
{ uint8_t value; __asm__ volatile("inb %w1,%0" : "=a"(value) : "d"(port)); return value; }
static uint32_t flags(void)
{ uint32_t value; __asm__ volatile("pushfl;popl %0" : "=r"(value)); return value; }
static void put(const char *s) { while (*s) out(0xe9, *s++); }
static void number(uint32_t n)
{
    char s[11]; uint32_t i=10; s[i]=0;
    do { s[--i]='0'+n%10; n/=10; } while(n);
    put(s+i);
}
__attribute__((noreturn)) static void done(uint32_t value)
{ __asm__ volatile("cli;outl %0,$0xf4" :: "a"(value)); for(;;) __asm__ volatile("hlt"); }
static void require(int condition, const char *message)
{
    if (condition) return;
    __asm__ volatile("cld;cli" ::: "memory");
    put("FAIL DAC case/phase: "); number(case_number); put("/"); number(phase);
    put(" "); put(message); put("\n"); done(0x11);
}
static uint32_t memory_address(uint32_t offset)
{
    if (offset < DATA_SIZE) return DATA + offset;
    if (offset < DATA_SIZE + 4096) return offset - DATA_SIZE;
    return 0xfffff000u + offset - DATA_SIZE - 4096;
}
static uint32_t memory_offset(uint32_t address)
{
    if (address >= DATA && address < DATA + DATA_SIZE) return address - DATA;
    if (address < 4096) return DATA_SIZE + address;
    require(address >= 0xfffff000u, "oracle memory bounds");
    return DATA_SIZE + 4096 + address - 0xfffff000u;
}
static void initialize(void)
{
    uint32_t i;
    for (i=0; i<MEMORY_SIZE; ++i) BYTE(memory_address(i))=(uint8_t)(i*37+pattern*83);
    seen_count=0;
}
static void mutate(uint32_t event, uint32_t model)
{
    uint32_t addresses[3], i;
    addresses[0]=dac_rgb+event;        /* Change the very next component. */
    addresses[1]=dac_rgb+event+1;    /* And a later component in the triplet. */
    addresses[2]=DATA+0x203+(event*7)%0x800;
    for (i=0; i<3; ++i) if (mutation & (1u << i)) {
        uint8_t value=(uint8_t)(0xa5+event*13+i*61+pattern);
        if (model) expected[memory_offset(addresses[i])]=value;
        else BYTE(addresses[i])=value;
    }
}
static void expect(uint32_t kind, uint32_t port, uint32_t value)
{
    require(wanted_count<COUNT(wanted), "oracle event capacity");
    wanted[wanted_count++]=(struct event){kind,port,value};
}
static void oracle(void)
{
    uint32_t i, components=triples*3;
    initialize(); wanted_count=0;
    for(i=0; i<MEMORY_SIZE; ++i) expected[i]=BYTE(memory_address(i));
    expect(1,0,0); expect(2,0,0); expect(3,0x3c8,dac_first);
    mutate(0,1);
    for(i=0; i<components; ++i) {
        expect(3,0x3c9,expected[memory_offset(dac_rgb+i)]/4);
        mutate(i+1,1);
    }
    if(!prefix) expect(4,0,dac_flags);
}
static void record(uint32_t kind, uint32_t port, uint32_t value)
{
    const struct event *e;
    require(seen_count<wanted_count, "unexpected event");
    e=&wanted[seen_count++];
    require(e->kind==kind && e->port==port && e->value==value, "event order/arguments");
}
uint32_t kernel_read_flags(void)
{
    require((flags()&0x200)==(dac_flags&0x200), "IF before CLI");
    record(1,0,0); return dac_flags;
}
void kernel_disable_interrupts(void)
{
    require((flags()&0x200)==(dac_flags&0x200), "CLI call ordering");
    __asm__ volatile("cli" ::: "memory");
    record(2,0,0);
}
void kernel_write_port_byte(uint32_t port, uint32_t value)
{
    uint32_t event=seen_count-2;
    require(!(flags()&0x200), "interrupts enabled during DAC write");
    record(3,port,value);
    if(hardware_test) {
        out(port,value);
        if(port==0x3c8) hardware_cursor=(value&255)*3;
        else { hardware[hardware_cursor]=(uint8_t)value; hardware_cursor=(hardware_cursor+1)%768; }
    }
    mutate(event,0);
    if(prefix && seen_count==wanted_count) dac_abort();
}
static USED void observe_write_flags(uint32_t value)
{
    require(!(flags()&0x200), "flags restored before last event");
    record(4,0,value);
}
__attribute__((naked,noinline)) void kernel_write_flags(uint32_t value __attribute__((unused)))
{
    __asm__("pushl 4(%esp);call observe_write_flags;addl $4,%esp;"
            "pushl 4(%esp);popfl;ret");
}
struct call_patch { uint32_t offset, target; };
#include "../build/display_dac_calls.h"
static void redirect(uint32_t base, const struct call_patch *patches, uint32_t count)
{
    uint32_t i;
    for(i=0; i<count; ++i) {
        uint32_t a=base+patches[i].offset;
        require(BYTE(a)==0xe8, "fixture CALL opcode");
        WORD(a+1)=patches[i].target-a-5;
    }
}
static void reset_hardware(void)
{
    uint32_t i;
    out(0x3c6,255); out(0x3c8,0);
    for(i=0; i<768; ++i) { hardware[i]=(uint8_t)(i%64); out(0x3c9,hardware[i]); }
}
static void check_hardware(void)
{
    uint32_t i;
    out(0x3c7,0);
    for(i=0; i<768; ++i) require(in(0x3c9)==hardware[i], "VGA DAC readback");
    ++hardware_checks;
}
static void exercise(void)
{
    uint32_t i;
    ++case_number; oracle();
    hardware_test=(case_number%64)==1;
    for(phase=0; phase<3; ++phase) {
        initialize();
        if(hardware_test) reset_hardware();
        dac_entry=!phase?0x91ad6e:phase==1?(uint32_t)kernel_display_write_palette:0x81ad6e;
        dac_invoke();
        require(dac_aborted==prefix, "bounded wrap escape");
        require(seen_count==wanted_count, "missing event");
        require(dac_arguments_after[0]==dac_first && dac_arguments_after[1]==dac_last &&
                dac_arguments_after[3]==0x13579bdf && dac_arguments_after[4]==0x2468ace0,
                "incoming arguments/canaries");
        if(!prefix) {
            require(dac_stack_after==dac_call_stack, "stack balance");
            require(dac_registers[0]==0x11223344 && dac_registers[1]==0x55667788 &&
                    dac_registers[2]==0x99aabbcc && dac_registers[3]==0xddeeff00,
                    "callee-saved registers");
            require(dac_arguments_after[2]==dac_rgb+triples*3, "advanced RGB argument slot");
            require(((dac_flags_after^dac_flags)&0xed5)==0, "native EFLAGS/IF/DF restoration");
        }
        for(i=0; i<MEMORY_SIZE; ++i)
            require(BYTE(memory_address(i))==expected[i], "unexpected memory change");
        if(hardware_test) check_hardware();
        ++executions;
    }
}
static void paging(void)
{
    uint32_t i;
    for(i=0; i<4096; ++i) tables[i/1024][i%1024]=i*4096u|3u;
    for(i=0; i<4; ++i) directory[i]=(uint32_t)tables[i]|3u;
    for(i=0; i<256; ++i) tables[0][i]=(0x200000u+i*4096u)|3u;
    tables[4][1023]=0x320003;
    directory[1023]=(uint32_t)tables[4]|3u;
    __asm__ volatile("movl %0,%%cr3" :: "r"(directory) : "memory");
    __asm__ volatile("movl %%cr0,%%eax;orl $0x80000000,%%eax;movl %%eax,%%cr0"
                     ::: "eax","memory");
}
void display_dac_qemu_main(void)
{
    uint32_t r,s,f;
    out(0x21,255); out(0xa1,255); /* IF can be tested without asynchronous PIC IRQs. */
    paging();
    redirect(0x900000,original_calls,COUNT(original_calls));
    redirect(0x800000,candidate_calls,COUNT(candidate_calls));
    for(pattern=0; pattern<2; ++pattern)
    for(r=0; r<COUNT(records); ++r)
    for(s=0; s<COUNT(ranges); ++s)
    for(mutation=0; mutation<8; ++mutation)
    for(f=0; f<COUNT(flag_seeds); ++f) {
        dac_first=ranges[s].first; dac_last=ranges[s].last; dac_rgb=records[r];
        triples=ranges[s].triples; prefix=ranges[s].prefix; dac_flags=flag_seeds[f];
        exercise();
    }
    put("PASS: "); number(case_number); put(" DAC scenarios, "); number(executions);
    put(" original/C/fixed-address executions; "); number(hardware_checks);
    put(" VGA hardware readbacks\n");
    put("Signed/wrapping indices, RGB address wrap, component mutation, full memory, ABI and native IF/DF/EFLAGS verified\n");
    put("INT_MAX ranges use a bounded four-triplet prefix; full nonterminating ranges are not completed\n");
    done(0x10);
}
