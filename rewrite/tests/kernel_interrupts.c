/* Original i386/C control flow; only selector access and gate installation
 * use common callbacks here. Actual segment/LIDT/I/O execution has its own
 * CPL0 fixture, and the five hardware leaves require byte identity. */
#include "../kernel/interrupts.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static uint8_t expected[0x5000];
static uint32_t events[265][4], saved_events[265][4];
static uint32_t calls, selector_reads, mutation, selector, failure;
static const uint32_t selectors[]={0,1,8,16,0xffff,0x10000,0x80000000,0xffffffff};
static const uint32_t overrides[][3]={
    {7,0x8371e6,0x8e00},{32,0x8371e9,0x8e00},{33,0x837209,0x8e00},
    {37,0x837222,0x8e00},{40,0x83723b,0x8e00},{41,0x83725a,0x8e00},
    {42,0x837273,0x8e00},{43,0x83728c,0x8e00},{44,0x8372a5,0x8e00},
    {64,0x8372be,0xee00},{65,0x8372d1,0xee00}};
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t args[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)args,0,0)==address;}
static int load(const char *path,uint32_t address)
{
    int fd=syscall3(5,(uint32_t)path,0,0),n;
    if(fd<0)return 0;
    n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);
    return n==250099;
}
static void jump(uint32_t address,uint32_t target)
{*(uint8_t *)address=0xe9;WORD(address+1)=target-address-5;}
uint32_t kernel_boot_code_selector(void)
{
    uint32_t value=WORD(0x70014);
    ++selector_reads;
    if(mutation&1)WORD(0x70014)=value^0xffffffff;
    return value;
}
void kernel_install_interrupt_gate(uint32_t handler,uint32_t code,uint32_t attributes,uint32_t vector)
{
    if(calls>=265){failure=1;return;}
    uint32_t expected_vector=calls<32?calls:calls<254?calls+1:overrides[calls-254][0];
    uint32_t expected_handler=calls<32?0x8371a0:calls<254?0x8371bc:overrides[calls-254][1];
    uint32_t expected_attributes=calls<254?0x8e00:overrides[calls-254][2];
    if(vector!=expected_vector||handler!=expected_handler||attributes!=expected_attributes||code!=selector)
        failure=2;
    events[calls][0]=handler;events[calls][1]=code;events[calls][2]=attributes;events[calls][3]=vector;
    WORD(0x17e00+vector*8)=(handler&0xffff)|(code<<16);
    WORD(0x17e04+vector*8)=(handler&0xffff0000)|(attributes&0xffff);
    *(volatile uint16_t *)0x1a82c=0x7ff;WORD(0x1a82e)=0x17e00;
    if(mutation&2)WORD(0x18600+4*(calls&255))=0xa5a50000|calls;
    ++calls;
}
static void initialize(uint32_t pattern)
{
    uint32_t i;for(i=0;i<sizeof(expected);++i)((volatile uint8_t *)0x17000)[i]=(uint8_t)(i*37+pattern*83);
    WORD(0x70014)=selector;calls=selector_reads=failure=0;
    for(i=0;i<265*4;++i)((uint32_t *)events)[i]=0;
}
static uint32_t run(void)
{
    uint32_t pattern,s,i,cases=0;
    if(!map(0x17000,0x5000)||!map(0x70000,0x1000)||!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 1;
    jump(0x937131,(uint32_t)kernel_boot_code_selector);jump(0x9370d2,(uint32_t)kernel_install_interrupt_gate);
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/icestar_c.x",0x800000))return 2;
    jump(0x837131,(uint32_t)kernel_boot_code_selector);jump(0x8370d2,(uint32_t)kernel_install_interrupt_gate);
#endif
    for(pattern=0;pattern<3;++pattern)for(s=0;s<COUNT(selectors);++s)for(mutation=0;mutation<4;++mutation){
        selector=selectors[s];initialize(pattern);((void (*)(void))0x913320)();
        if(failure||calls!=265||selector_reads!=1)return 3;
        for(i=0;i<sizeof(expected);++i)expected[i]=((volatile uint8_t *)0x17000)[i];
        for(i=0;i<265*4;++i)((uint32_t *)saved_events)[i]=((uint32_t *)events)[i];
        initialize(pattern);
#ifdef KERNEL_LINKED_TEST
        ((void (*)(void))0x813320)();
#else
        kernel_init_interrupts();
#endif
        if(failure||calls!=265||selector_reads!=1)return 4;
        for(i=0;i<sizeof(expected);++i)if(expected[i]!=((volatile uint8_t *)0x17000)[i])return 5;
        for(i=0;i<265*4;++i)if(((uint32_t *)saved_events)[i]!=((uint32_t *)events)[i])return 6;
        ++cases;
    }
    if(cases!=96)return 7;
    {static const char message[]="interrupt initialization original/C/oracle cases: 96; gate events: 25440\n";
     syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
