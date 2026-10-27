/* Freeze the emulated clock, then execute both real IN/OUT implementations.
 * Check raw CMOS bytes and the otherwise non-C EAX/GS/EFLAGS contract. */
#include <stdint.h>
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define USED __attribute__((used))
static USED uint32_t call_entry, call_flags, call_gs, registers[10];
static uint32_t saved[10], count, phase, scenario;
static uint8_t expected[0x400], guard[0x400];
static const uint8_t indices[]={0,2,4,6,7,8,9,0x32};
static const uint32_t selectors[]={0,8,24,32};
static void put(const char *s){while(*s)__asm__ volatile("outb %0,$0xe9"::"a"(*s++));}
static void number(uint32_t n){char s[11];uint32_t i=10;s[i]=0;do{s[--i]='0'+n%10;n/=10;}while(n);put(s+i);}
__attribute__((noreturn)) static void done(uint32_t result)
{__asm__ volatile("outl %0,$0xf4"::"a"(result));for(;;)__asm__ volatile("hlt");}
static void require(int condition)
{
    if(condition)return;
    put("FAIL CMOS scenario/phase/checks: ");number(scenario);put("/");number(phase);
    put("/");number(count);put("\n");done(0x11);
}
static uint8_t in(uint16_t port){uint8_t v;__asm__ volatile("inb %w1,%0":"=a"(v):"d"(port));return v;}
static void out(uint16_t port,uint8_t value){__asm__ volatile("outb %0,%w1"::"a"(value),"d"(port));}
static uint8_t read_cmos(uint8_t index){out(0x70,index);return in(0x71);}
static void write_cmos(uint8_t index,uint8_t value){out(0x70,index);out(0x71,value);}
__attribute__((naked,noinline)) static void invoke(void)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;pushl %gs;"
        "movl call_gs,%eax;movl %eax,%gs;pushl call_flags;popfl;"
        "movl $0x10293847,%eax;movl $0x21384957,%ecx;movl $0x32495867,%edx;"
        "movl $0x435a6977,%ebx;movl $0x546b7a87,%ebp;movl $0x657c8b97,%esi;movl $0x768d9ca7,%edi;"
        "call *call_entry;pushfl;pushal;"
        "movl %esp,%esi;movl $registers,%edi;movl $9,%ecx;cld;rep movsl;"
        "xorl %eax,%eax;movw %gs,%ax;movl %eax,registers+36;"
        "addl $36,%esp;popl %gs;popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
void cmos_clock_qemu_main(void)
{
    uint32_t p,s,i;uint8_t sampled[8];
    /* Freeze time (SET), keep the hardware's raw BCD/binary behavior. */
    for(p=0;p<64;++p)for(s=0;s<4;++s){
        scenario=p*4+s;
        write_cmos(0xb,0x80|((p&1)?6:2));
        for(i=0;i<8;++i)write_cmos(indices[i],(uint8_t)(p*19+i*37));
        for(i=0;i<8;++i)sampled[i]=read_cmos(indices[i]);
        for(phase=0;phase<2;++phase){
            for(i=0;i<sizeof(expected);++i){
                uint8_t byte=(uint8_t)(i*37+p*83);
                expected[i]=byte;((volatile uint8_t *)0x1b000)[i]=byte;
                guard[i]=(uint8_t)~byte;((volatile uint8_t *)0x11b000)[i]=(uint8_t)~byte;
            }
            for(i=0;i<8;++i)expected[4+i]=sampled[i];
            call_entry=phase?0x82ae84:0x92ae84;
            call_flags=2|((p&2)?0x8d5:0)|((p&4)?0x400:0);
            call_gs=selectors[s];invoke();
            require(registers[0]==0x768d9ca7 && registers[1]==0x657c8b97 && registers[2]==0x546b7a87);
            require(registers[4]==0x435a6977 && registers[5]==0x32495867 && registers[6]==0x21384957);
            require(registers[7]==sampled[7] && registers[8]==call_flags && registers[9]==call_gs);
            for(i=0;i<10;++i){if(phase)require(registers[i]==saved[i]);else saved[i]=registers[i];}
            for(i=0;i<sizeof(expected);++i){
                require(((volatile uint8_t *)0x1b000)[i]==expected[i]);
                require(((volatile uint8_t *)0x11b000)[i]==guard[i]);
            }
            require(in(0x71)==sampled[7]);
        }
        ++count;
    }
    require(count==256);
    put("PASS: 256 CPL0 CMOS original/rebuilt/oracle comparisons\n");
    put("Raw bytes, GS restore/base override, flags/DF, registers and selected CMOS index verified\n");
    done(0x10);
}
