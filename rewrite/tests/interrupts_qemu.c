/* Actual segment changes, LIDT/SIDT, INT/IRET and device I/O under QEMU. */
#include <stdint.h>
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define USED __attribute__((used))
static uint8_t expected[0x5000];
static uint32_t original_registers[11];
static USED uint32_t call_entry,call_args[4],call_flags,registers[11];
static uint32_t count,phase,scenario,group;
uint32_t interrupts_qemu_hits;
extern void interrupts_qemu_handler(void);
static const uint32_t values[]={0,1,8,16,0xffff,0x10000,0x80000000,0xffffffff};
static const uint32_t vectors[]={0,1,7,32,254,255,256,511,0x1fffffff,0x20000000,0x20000001,0xffffffff};
static const uint32_t overrides[][3]={
    {7,0x8371e6,0x8e00},{32,0x8371e9,0x8e00},{33,0x837209,0x8e00},
    {37,0x837222,0x8e00},{40,0x83723b,0x8e00},{41,0x83725a,0x8e00},
    {42,0x837273,0x8e00},{43,0x83728c,0x8e00},{44,0x8372a5,0x8e00},
    {64,0x8372be,0xee00},{65,0x8372d1,0xee00}};
static const uint32_t callbacks[][2]={
    {0,0x81364c},{7,0x8380e1},{32,0x813a5e},{33,0x813aa6},{37,0x813ae6},{40,0x813a8c},
    {41,0x813b2a},{42,0x813b2a},{43,0x813b2a},{44,0x813b30},{64,0x813b70},{65,0x81406c}};
static void put(const char *s){while(*s)__asm__ volatile("outb %0,$0xe9"::"a"(*s++));}
static void number(uint32_t n){char s[11];uint32_t i=10;s[i]=0;do{s[--i]='0'+n%10;n/=10;}while(n);put(s+i);}
__attribute__((noreturn)) static void done(uint32_t result)
{__asm__ volatile("outl %0,$0xf4"::"a"(result));for(;;)__asm__ volatile("hlt");}
static void require(int condition)
{
    if(condition)return;
    put("FAIL count/group/scenario/phase: ");number(count);put("/");number(group);put("/");
    number(scenario);put("/");number(phase);put("\n");done(0x11);
}
static uint8_t in(uint16_t port){uint8_t value;__asm__ volatile("inb %w1,%0":"=a"(value):"d"(port));return value;}
static void out(uint16_t port,uint8_t value){__asm__ volatile("outb %0,%w1"::"a"(value),"d"(port));}
static void segments(uint32_t selector)
{__asm__ volatile("mov %w0,%%ds;mov %w0,%%gs"::"r"(selector):"memory");}
__attribute__((naked,noinline)) static void invoke(void)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;"
        "pushl call_args+12;pushl call_args+8;pushl call_args+4;pushl call_args;"
        "pushl call_flags;popfl;"
        "movl $0x10293847,%eax;movl $0x21384957,%ecx;movl $0x32495867,%edx;"
        "movl $0x435a6977,%ebx;movl $0x546b7a87,%ebp;movl $0x657c8b97,%esi;movl $0x768d9ca7,%edi;"
        "call *call_entry;pushfl;pushal;"
        "movl %esp,%esi;movl $registers,%edi;movl $9,%ecx;cld;rep movsl;"
        "xorl %eax,%eax;movw %ds,%ax;movl %eax,registers+36;movw %gs,%ax;movl %eax,registers+40;"
        "addl $36,%esp;addl $16,%esp;popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
static void initialize(uint32_t seed)
{
    uint32_t i;for(i=0;i<sizeof(expected);++i){uint8_t byte=(uint8_t)(i*37+seed*83);expected[i]=byte;((volatile uint8_t *)0x17000)[i]=byte;}
    call_flags=2|(seed&1?0x8d5:0);
    for(i=0;i<4;++i)call_args[i]=0;
    segments(seed&1?24:8);
}
static void expected_word(uint32_t address,uint32_t value,uint32_t size)
{uint32_t i;require(address>=0x17000 && address+size<=0x1c000);for(i=0;i<size;++i)expected[address-0x17000+i]=(uint8_t)(value>>(i*8));}
static void gate(uint32_t handler,uint32_t selector,uint32_t attributes,uint32_t vector)
{
    uint32_t address=0x17e00+vector*8u;
    expected_word(address,(handler&0xffff)|(selector<<16),4);
    expected_word(address+4,(handler&0xffff0000)|(attributes&0xffff),4);
    expected_word(0x1a82c,0x7ff,2);expected_word(0x1a82e,0x17e00,4);
}
static void memory_matches(void)
{uint32_t i;for(i=0;i<sizeof(expected);++i)if(expected[i]!=((volatile uint8_t *)0x17000)[i]){put("byte=");number(i);put(" ");require(0);}}
static void compare_registers(void)
{
    uint32_t i;for(i=0;i<11;++i){if(!phase)original_registers[i]=registers[i];else require(original_registers[i]==registers[i]);}
}
static void idtr_matches(void)
{
    struct __attribute__((packed)){uint16_t limit;uint32_t base;} idtr;
    __asm__ volatile("sidt %0":"=m"(idtr));require(idtr.limit==0x7ff && idtr.base==0x17e00);
}
static void gate_tests(void)
{
    uint32_t v,p;
    group=1;
    for(v=0;v<COUNT(vectors);++v)for(p=0;p<32;++p){
        scenario=v*32+p;
        for(phase=0;phase<2;++phase){
            initialize(scenario);call_entry=phase?0x8370d2:0x9370d2;
            call_args[0]=values[p%8];call_args[1]=values[(p/4)%8];call_args[2]=values[(p/2)%8];call_args[3]=vectors[v];
            gate(call_args[0],call_args[1],call_args[2],call_args[3]);invoke();
            memory_matches();idtr_matches();compare_registers();
            require(registers[9]==(scenario&1?24u:8u) && registers[10]==registers[9]);
        }
        ++count;
    }
    put("QEMU CPL0 gate/LIDT/segment/wrapped-index cases: 384\n");
}
static void selector_tests(void)
{
    uint32_t p;
    group=2;
    for(p=0;p<16;++p){
        scenario=p;
        for(phase=0;phase<2;++phase){
            initialize(p);WORD(0x70014)=values[p/2];call_entry=phase?0x837131:0x937131;invoke();
            require(registers[7]==values[p/2] && registers[8]==call_flags);
            require(registers[9]==(p&1?24u:8u) && registers[10]==registers[9]);
            memory_matches();compare_registers();
        }
        ++count;
    }
    put("QEMU CPL0 GS boot-selector cases: 16\n");
}
static void initialization_tests(void)
{
    uint32_t p,s,i;
    group=3;
    for(p=0;p<3;++p)for(s=0;s<COUNT(values);++s){
        scenario=p*8+s;
        for(phase=0;phase<2;++phase){
            initialize(p);WORD(0x70014)=values[s];call_entry=phase?0x813320:0x913320;
            for(i=0;i<32;++i)gate(0x8371a0,values[s],0x8e00,i);
            for(i=33;i<255;++i)gate(0x8371bc,values[s],0x8e00,i);
            for(i=0;i<COUNT(overrides);++i)gate(overrides[i][1],values[s],overrides[i][2],overrides[i][0]);
            for(i=0;i<COUNT(callbacks);++i)expected_word(0x18600+callbacks[i][0]*4,callbacks[i][1],4);
            invoke();memory_matches();idtr_matches();
            require(registers[0]==0x768d9ca7 && registers[1]==0x657c8b97 && registers[2]==0x546b7a87 && registers[4]==0x435a6977);
        }
        ++count;
    }
    put("QEMU CPL0 complete IDT/callback initialization cases: 24\n");
}
__attribute__((noinline)) static void trigger(void){__asm__ volatile("int $0xf0":::"memory");}
static void interrupt_test(void)
{
    group=4;scenario=0;
    for(phase=0;phase<2;++phase){
        initialize(0);call_entry=phase?0x8370d2:0x9370d2;
        call_args[0]=(uint32_t)interrupts_qemu_handler;call_args[1]=16;call_args[2]=0x8e00;call_args[3]=0xf0;invoke();
        interrupts_qemu_hits=0;call_entry=(uint32_t)trigger;invoke();require(interrupts_qemu_hits==1);compare_registers();
        require(registers[8]==call_flags);
    }
    ++count;put("QEMU CPL0 actual INT/IRET through original/C gates: 1\n");
}
static void device_tests(void)
{
    uint32_t p,f;
    static const uint32_t entries[]={0x837143,0x83717c,0x837189};
    group=5;
    for(f=0;f<3;++f)for(p=0;p<32;++p){
        scenario=f*32+p;
        for(phase=0;phase<2;++phase){
            uint8_t master=(uint8_t)(p*7),slave=(uint8_t)(p*17),status;
            ((void (*)(void))0x837143)();out(0x21,master);out(0xa1,slave);
            out(0x70,0x0b);out(0x71,2);out(0x70,0x0c);(void)in(0x71);
            call_flags=2|(p&1?0x8d5:0);call_entry=entries[f]+(phase?0:0x100000);invoke();
            if(f==0){require(in(0x21)==0xfb && in(0xa1)==0xff);require(registers[7]==0x102938ff && registers[8]==call_flags);}
            if(f==1){out(0x43,0xe2);status=in(0x40);require((status&0x3f)==0x34);require(registers[7]==0x1029382e && registers[8]==call_flags);}
            if(f==2){require(in(0x21)==master && in(0xa1)==(slave&0xfe));out(0x70,0x0b);require(in(0x71)==0x12);require((registers[7]&0xffffff00)==0x10293800);registers[7]&=0xffffff00;}
            compare_registers();
        }
        ++count;
    }
    put("QEMU CPL0 PIC/PIT/RTC device configuration cases: 96\n");
}
void interrupts_qemu_main(void)
{
    put("QEMU interrupt fixture: CPL0, flat OS data/code selectors, IF=0\n");
    gate_tests();selector_tests();initialization_tests();interrupt_test();device_tests();
    require(count==521);put("PASS: 521 CPL0 interrupt comparisons\n");done(0x10);
}
