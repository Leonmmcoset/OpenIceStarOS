/* Execute the original i386 callbacks and the recovered C against identical
 * observable boundaries. Only port I/O, CMOS sampling and queue writes are
 * redirected; the callback instructions themselves run unmodified. */
#include "../kernel/interrupt_callbacks.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BASE 0x1b000
#define SIZE 0x3000
#define QA 0x1c000
#define QB 0x1c100
#define FA 0x1c200
#define FB 0x1c300
static uint8_t expected[SIZE];
static uint32_t events[4][3], saved_events[4][3];
static uint32_t calls, failure, mutation, value, pattern, which;
static const uint32_t addresses[]={0x813a8c,0x813aa6,0x813ae6,0x813b2a,0x813b30};
#ifndef KERNEL_LINKED_TEST
static void (*const recovered[])(void)={kernel_rtc_callback,kernel_keyboard_callback,
    kernel_audio_callback,kernel_unused_irq_callback,kernel_mouse_callback};
#endif
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
static void record(uint32_t kind,uint32_t a,uint32_t b)
{
    if(calls>=4){failure=1;return;}
    events[calls][0]=kind;events[calls][1]=a;events[calls][2]=b;++calls;
    /* No callback may publish its final event before its dependencies. */
    if(which==0 && WORD(FA+4)!=0x11111111)failure=2;
    if(which==1 && WORD(FA+8)!=0x11111111)failure=3;
    if(which==2 && WORD(0x1b17c)!=0x11111111)failure=4;
    if(which==4 && WORD(FA+12)!=0x11111111)failure=5;
}
static void change_pointers(void)
{WORD(0x1b038)=QB;WORD(0x1b044)=QB;WORD(0x1b040)=FB;}
void kernel_read_cmos_clock(void)
{
    uint32_t i;record(0,0,0);
    for(i=0;i<8;++i)((volatile uint8_t *)BASE)[4+i]=(uint8_t)(value+i*19);
    if(mutation&1)change_pointers();
}
uint32_t kernel_read_port_byte(uint32_t port)
{
    record(1,port,0);
    if(mutation&1)change_pointers();
    WORD(0x1b180)=port;
    return (value&255)|((pattern+1)*0x35000000);
}
void kernel_write_port_byte(uint32_t port,uint32_t byte)
{
    record(2,port,byte);
    if(mutation&4)change_pointers();
    WORD(0x1b184)=byte;
}
static uint32_t queue_write(uint32_t kind,volatile uint32_t *queue,uint32_t byte)
{
    record(kind,(uint32_t)queue,byte);
    if((uint32_t)queue!=QA || byte!=value)failure=6;
    queue[0]=byte;queue[1]^=0x55aa55aa;
    if(mutation&2)change_pointers();
    return mutation&4 ? 0 : 0xffffffff;
}
uint32_t kernel_input_write(volatile uint32_t *queue,uint32_t byte)
{return queue_write(3,queue,byte);}
uint32_t kernel_queue_write(volatile uint32_t *queue,uint32_t byte)
{return queue_write(4,queue,byte);}
static void initialize(void)
{
    uint32_t i;for(i=0;i<SIZE;++i)((volatile uint8_t *)BASE)[i]=(uint8_t)(i*37+pattern*83);
    WORD(0x1b038)=QA;WORD(0x1b044)=QA;WORD(0x1b040)=FA;
    for(i=0;i<4;++i){WORD(FA+4*i)=0x11111111;WORD(FB+4*i)=0x22222222;}
    WORD(0x1b17c)=0x11111111;calls=failure=0;
    for(i=0;i<12;++i)((uint32_t *)events)[i]=0;
}
static int oracle(void)
{
    uint32_t target=which==0 && (mutation&1)?FB:FA;
    if(failure)return 0;
    if(which==0)return calls==1 && events[0][0]==0 && WORD(target+4)==1;
    if(which==1 || which==4)return calls==2 && events[0][0]==1 && events[0][1]==0x60 &&
        events[1][0]==(which==1?3u:4u) && events[1][1]==QA && events[1][2]==value &&
        WORD(FA+(which==1?8:12))==1 && WORD(FB+(which==1?8:12))==0x22222222;
    if(which==2)return calls==3 && events[0][0]==2 && events[0][1]==0xa && events[0][2]==5 &&
        events[1][0]==1 && events[1][1]==0x22e && events[2][0]==1 && events[2][1]==0x22f &&
        WORD(0x1b17c)==1;
    return calls==0;
}
static void redirect(uint32_t base)
{
    jump(base+0x2ae84,(uint32_t)kernel_read_cmos_clock);
    jump(base+0x1a254,(uint32_t)kernel_read_port_byte);
    jump(base+0x1a261,(uint32_t)kernel_write_port_byte);
    jump(base+0x19d96,(uint32_t)kernel_input_write);
    jump(base+0x12e7e,(uint32_t)kernel_queue_write);
}
static uint32_t run(void)
{
    uint32_t i,cases=0;
    if(!map(BASE,SIZE)||!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 1;
    redirect(0x900000);
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/icestar_c.x",0x800000))return 2;
    redirect(0x800000);
#endif
    for(pattern=0;pattern<3;++pattern)for(value=0;value<256;++value)
    for(mutation=0;mutation<8;++mutation)for(which=0;which<5;++which){
        initialize();((void (*)(void))(addresses[which]+0x100000))();
        if(!oracle())return 10+which;
        for(i=0;i<SIZE;++i)expected[i]=((volatile uint8_t *)BASE)[i];
        for(i=0;i<12;++i)((uint32_t *)saved_events)[i]=((uint32_t *)events)[i];
        initialize();
#ifdef KERNEL_LINKED_TEST
        ((void (*)(void))addresses[which])();
#else
        recovered[which]();
#endif
        if(!oracle())return 20+which;
        for(i=0;i<SIZE;++i)if(expected[i]!=((volatile uint8_t *)BASE)[i])return 30+which;
        for(i=0;i<12;++i)if(((uint32_t *)saved_events)[i]!=((uint32_t *)events)[i])return 40+which;
        ++cases;
    }
    if(cases!=30720)return 50;
    {static const char message[]="IRQ callbacks original/C/oracle cases: 30720; full shared-memory and ordered boundary checks\n";
     syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
