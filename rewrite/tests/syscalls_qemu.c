/* Actual 0x40/0x41 -> PUSHAD -> dispatcher -> service -> IRET chains. */
#include <stdint.h>
#include "../kernel/syscalls.h"
#include "../kernel/service_forward.h"
#include "../kernel/runtime.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
struct route {uint32_t target,count,sign;};
static const struct route routes40[]={
    {0x8374cf,4,0},{0x837501,6,0},{0x837545,6,0},{0x837589,6,0},
    {0x8375cd,5,0},{0x837609,5,0},{0x837645,6,1},{0x83768f,6,0},
    {0x837861,5,0},{0x8376d3,6,0},{0x837717,7,0},{0x8377d3,6,1},
    {0x83781d,6,0},{0x8082c6,1,0},{0x808378,1,0},{0x81021c,1,0},
    {0x81017c,1,0},{0x80129e,2,0},{0x81041a,2,0},{0x81434a,0,0}};
static const struct route routes41[]={
    {0x807a0a,8,0},{0x810478,1,0},{0x810448,1,0},{0x81b714,1,0},{0x82a9bb,2,0}};
static const uint32_t invalid40[]={20,21,255,256,65535,0x80000000,0xffffffff};
static const uint32_t invalid41[]={6,7,8,9,10,11,12,13,14,15,16,17,18,19,
    20,21,255,256,65535,0x80000000,0xffffffff};
static const uint32_t values[]={0,1,0x7f,0x80,0xff,0x100,0x80000000,0xffffffff};
static uint64_t idt[256] __attribute__((aligned(8)));
static uint32_t tss[26],count,ring,vector,operation,pattern,phase,gate_type;
static uint32_t parameters[8],trace[11],saved_trace[11],saved_result[14];
static uint8_t saved_memory[0x4040];
static uint32_t seed_hits;
#ifdef KERNEL_SURFACE_CHAIN_TEST
static uint32_t surface_variant,surface_count,surface_total;
static uint32_t surface_events[8][10];
static uint8_t surface_memory[0x2000];
static const uint32_t surface_rectangles[][4]={
    {0,0,1,1},{0xffffffff,0xffffffff,17,19},{120,90,20,30},
    {0x7fffffff,0x7fffffff,1,1},{1,2,0xffffffff,0x80000000},
    {128,96,0,0},{200,300,40,50},{0x80000000,5,0x7fffffff,100}};
#endif
uint32_t syscalls_input[8],syscalls_result[14],syscalls_flags,syscalls_segment;
uint32_t syscalls_stack,syscalls_kernel_stack,syscalls_operation;
uint32_t syscalls_callback_flags,syscalls_callback_cs,syscalls_callback_ds,syscalls_callback_ss;
uint32_t syscalls_return;
extern uint64_t entries_gdt[];
extern void entries_fault(void),entries_int40(void),entries_int41(void);
static void put(const char *s){while(*s)__asm__ volatile("outb %0,$0xe9"::"a"(*s++));}
static void number(uint32_t n){char s[11];uint32_t i=10;s[i]=0;do{s[--i]='0'+n%10;n/=10;}while(n);put(s+i);}
__attribute__((noreturn)) static void done(uint32_t n)
{__asm__ volatile("outl %0,$0xf4"::"a"(n));for(;;)__asm__ volatile("hlt");}
static void require_at(int condition,uint32_t line)
{
    if(condition)return;
    put("FAIL case/ring/vector/op/pattern/phase/gate: ");number(count);put("/");number(ring);put("/");
    number(vector);put("/");number(operation);put("/");number(pattern);put("/");number(phase);put("/");
    number(gate_type);put(" line=");number(line);put("\n");done(0x11);
}
#define require(c) require_at((c),__LINE__)
static const struct route *route(void)
{return vector==0x40?(operation<20?routes40+operation:0):(operation<5?routes41+operation:0);}
static int real_service(void)
{
#ifdef KERNEL_SERVICE_CHAIN_TEST
    return vector==0x40?operation==18:(operation==1||operation==2||operation==4);
#else
    return 0;
#endif
}
static int real_surface(void)
{
#ifdef KERNEL_SURFACE_CHAIN_TEST
    return vector==0x40 && operation==16;
#else
    return 0;
#endif
}
__attribute__((unused)) static uint32_t random_seed(void)
{++seed_hits;return values[(pattern+5)%COUNT(values)];}
static uint32_t service_result(void)
{
    if(operation==1)return values[pattern%COUNT(values)];
    if(operation==2)return values[(pattern+1)%COUNT(values)];
    uint32_t n=values[(pattern+5)%COUNT(values)];n^=n>>6;n^=n<<12;n^=n>>14;
    return n%(parameters[1]-parameters[0]+1)+parameters[0];
}

__attribute__((used,noinline)) static uint32_t observe(volatile uint32_t *args,uint32_t target)
{
    const struct route *r=route();uint32_t i;uint16_t cs,ds,ss;
    __asm__ volatile("movw %%cs,%0;movw %%ds,%1;movw %%ss,%2":"=r"(cs),"=r"(ds),"=r"(ss));
    syscalls_callback_cs=cs;syscalls_callback_ds=ds;syscalls_callback_ss=ss;
    ++trace[0];trace[1]=target;trace[2]=r?r->count:0;
    if(r)for(i=0;i<r->count;++i){trace[3+i]=args[i];if(pattern&1)args[i]=0xa5a50000|i;}
    if(pattern&2)for(i=0;i<8;++i)WORD(0x60000c+4*i)=0xface0000|i;
    WORD(0x1b088)^=target;return syscalls_return;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define STUB(type,name,address,parameters_) \
    __attribute__((naked,noinline)) type name parameters_ { \
        __asm__("pushl %ebp;movl %esp,%ebp;pushfl;popl syscalls_callback_flags;cld;" \
            "pushl $" #address ";leal 8(%ebp),%eax;pushl %eax;call observe;addl $8,%esp;leave;" \
            "movl $0x9357ace1,%ecx;movl $0x12348765,%edx;pushl syscalls_callback_flags;popfl;ret"); \
        __builtin_unreachable(); }
#define SERVICE(suffix,slot,form,start,end) STUB(uint32_t,kernel_service_##suffix,start,(SERVICE_PARAMETERS_##form))
#include "../kernel/service_forward.def"
#undef SERVICE
STUB(uint32_t,kernel_syscall_target_8082c6,0x8082c6,(uint32_t a0))
STUB(uint32_t,kernel_syscall_target_808378,0x808378,(uint32_t a0))
STUB(uint32_t,kernel_syscall_target_81021c,0x81021c,(uint32_t a0))
STUB(uint32_t,kernel_syscall_target_81017c,0x81017c,(uint32_t a0))
STUB(uint32_t,kernel_syscall_target_81041a,0x81041a,(uint32_t a0,uint32_t a1))
STUB(uint32_t,kernel_syscall_target_807a0a,0x807a0a,(uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,
    uint32_t a4,uint32_t a5,uint32_t a6,uint32_t a7))
STUB(uint32_t,kernel_syscall_target_810478,0x810478,(uint32_t a0))
STUB(uint32_t,kernel_syscall_target_810448,0x810448,(uint32_t a0))
STUB(uint32_t,kernel_syscall_target_82a9bb,0x82a9bb,(uint32_t a0,uint32_t a1))
STUB(uint32_t,kernel_palette_color,0x81b714,(uint32_t a0))
STUB(void,kernel_queue_api_request,0x80129e,(uint32_t a0,uint32_t a1))
STUB(void,kernel_yield_task,0x81434a,(void))
#ifdef KERNEL_SURFACE_CHAIN_TEST
__attribute__((used,noinline)) static uint32_t surface_observe(volatile uint32_t *args,uint32_t kind)
{
    uint32_t i,*event;uint16_t cs,ds,ss;
    __asm__ volatile("movw %%cs,%0;movw %%ds,%1;movw %%ss,%2":"=r"(cs),"=r"(ds),"=r"(ss));
    require(surface_count<8);event=surface_events[surface_count++];event[0]=kind;
    for(i=0;i<(kind==1?5u:4u);++i){event[i+1]=args[i];args[i]=0xa5a50000|i;}
    event[6]=syscalls_callback_flags&0x600;event[7]=cs;event[8]=ds;event[9]=ss;
    if(kind==1 && surface_count==1){
        if(pattern&1)WORD(0x510000)=0;
        if(pattern&2){
            WORD(0x51000c)=0xdecafbad;WORD(0x1b0e4)=0xdeadbeef;
            WORD(0x500000)=0xdeadbeef;WORD(0x6a001c)=0xdeadbeef;
        }
    }
    WORD(0x1b300)^=kind*0x132457;return 0xcdef0000|kind;
}
#define SURFACE_STUB(name,kind) \
    __attribute__((naked,noinline)) static uint32_t name(void) { \
        __asm__("pushl %ebp;movl %esp,%ebp;pushfl;popl syscalls_callback_flags;cld;" \
            "pushl $" #kind ";leal 8(%ebp),%eax;pushl %eax;call surface_observe;addl $8,%esp;leave;" \
            "pushl syscalls_callback_flags;popfl;ret"); }
SURFACE_STUB(surface_draw,1)
SURFACE_STUB(surface_present,2)
#undef SURFACE_STUB
static uint32_t requested_order(void)
{return surface_variant&1?0:(surface_variant&16?6:2);}
static uint32_t active_layer(void)
{return surface_variant&2?1:(surface_variant&8?0x80000001:0);}
static uint32_t active_order(void)
{return surface_variant&8?1:3;}
static void surface_setup(void)
{
    uint32_t i,a=pattern&3;
    for(i=0;i<0x1000;++i){((volatile uint8_t *)0x500000)[i]=(uint8_t)(i*31+pattern*59);
        ((volatile uint8_t *)0x510000)[i]=(uint8_t)(i*17+pattern*23);}
    parameters[0]=0xc0000007;WORD(0x60000c)=parameters[0];
    WORD(0x6a001c)=0x610500+a;WORD(0x610500+a+12)=0xc0000005;
    WORD(0x500000)=0x610100+a;WORD(0x500004)=0x610300+a;
    WORD(0x500008)=0x610400+a;WORD(0x500014)=0x610200+a;
    WORD(0x610100+a+12)=128;WORD(0x610100+a+16)=96;
    for(i=0;i<4;++i)WORD(0x610200+a+4+4*i)=surface_rectangles[pattern&7][i];
    WORD(0x610200+a+40)=surface_variant&4?0x80000000:0;
    WORD(0x610200+a+44)=requested_order();WORD(0x610300+a+44)=active_order();
    WORD(0x610400+a+44)=1;WORD(0x1b138)=active_layer();
    WORD(0x1b0e4)=0x610601;WORD(0x610601+40)=(uint32_t)surface_draw;
    WORD(0x610601+44)=(uint32_t)surface_present;
    WORD(0x510000)=4;for(i=1;i<=4;++i)WORD(0x510000+4*i)=0x11110000+i;
    surface_count=0;for(i=0;i<80;++i)((uint32_t *)surface_events)[i]=0;
}
static void surface_event_check(uint32_t row,uint32_t kind,const uint32_t *args)
{
    uint32_t i;require(row<surface_count);require(surface_events[row][0]==kind);
    for(i=0;i<(kind==1?5u:4u);++i)require(surface_events[row][i+1]==args[i]);
    if(kind==2)require(surface_events[row][5]==0);
    require(surface_events[row][6]==(syscalls_flags&0x400));
    require(surface_events[row][7]==16 && surface_events[row][8]==8 && surface_events[row][9]==8);
}
static void surface_check(void)
{
    uint32_t i,order,n=0,limit=4,a[5],rect_[4],request=requested_order(),active=active_layer();
    if(request){
        order=(int32_t)active>0?active_order():1;
        if((surface_variant&4) && ((int32_t)active<=0 || request>active_order()))order=request;
        for(i=0;i<4;++i)rect_[i]=surface_rectangles[pattern&7][i];
        if(rect_[0]&0x80000000)rect_[0]=0;
        if(rect_[1]&0x80000000)rect_[1]=0;
        if((int32_t)(rect_[0]+rect_[2])>128)rect_[2]=128-rect_[0];
        if((int32_t)(rect_[1]+rect_[3])>96)rect_[3]=96-rect_[1];
        for(;order<=limit;++order){
            a[0]=(n && (pattern&2) && order==3)?0xdecafbad:0x11110000+order;
            for(i=0;i<4;++i)a[i+1]=rect_[i];
            surface_event_check(n++,1,a);
            if(pattern&1)limit=0;
        }
        for(i=0;i<4;++i)a[i]=surface_rectangles[pattern&7][i];
        surface_event_check(n++,2,a);
    }
    require(surface_count==n);
    for(i=0;i<sizeof(surface_memory);++i){
        uint8_t byte=((volatile uint8_t *)(i<0x1000?0x500000:0x510000))[i&0xfff];
        if(!phase)surface_memory[i]=byte;else require(surface_memory[i]==byte);
    }
}
#endif
#pragma GCC diagnostic pop
#define SNAPSHOT \
    "pushfl;pushal;movl %esp,%esi;movl $syscalls_result,%edi;movl $9,%ecx;cld;rep movsl;" \
    "xorl %eax,%eax;movw %ds,%ax;movl %eax,syscalls_result+36;" \
    "movw %es,%ax;movl %eax,syscalls_result+40;movw %ss,%ax;movl %eax,syscalls_result+44;" \
    "movw %fs,%ax;movl %eax,syscalls_result+48;movw %gs,%ax;movl %eax,syscalls_result+52;addl $36,%esp;"
#define INPUT \
    "movl syscalls_input,%edi;movl syscalls_input+4,%esi;movl syscalls_input+8,%ebp;" \
    "movl syscalls_input+16,%ebx;movl syscalls_input+20,%edx;movl syscalls_input+24,%ecx;movl syscalls_input+28,%eax;"
#define SEGMENTS \
    "movl syscalls_segment,%eax;movw %ax,%ds;movw %ax,%es;movw %ax,%fs;movw %ax,%gs;"
#define RESET_SEGMENTS "movl $8,%eax;movw %ax,%ds;movw %ax,%es;movw %ax,%fs;movw %ax,%gs;"
__attribute__((naked,noinline)) static void invoke(void)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;" SEGMENTS
        "movl %esp,syscalls_stack;pushl syscalls_flags;popfl;" INPUT "call *syscalls_operation;"
        SNAPSHOT "cli;cld;" RESET_SEGMENTS "popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static void user_return(void)
{
    __asm__("cli;cld;" RESET_SEGMENTS "movl syscalls_kernel_stack,%esp;popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline,used)) static void user_code(void)
{__asm__(INPUT "call *syscalls_operation;" SNAPSHOT "int $0xf1;ud2");__builtin_unreachable();}
__attribute__((naked,noinline)) static void invoke_user(void)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;movl %esp,syscalls_kernel_stack;"
        SEGMENTS "movl $0x520000,syscalls_stack;"
        "pushl $75;pushl $0x520000;pushl syscalls_flags;pushl $67;pushl $user_code;iret");
    __builtin_unreachable();
}
static void jump(uint32_t address,uint32_t target)
{*(uint8_t *)address=0xe9;WORD(address+1)=target-address-5;}
static void redirect(uint32_t base)
{
#define SERVICE(suffix,slot,form,start,end) jump(base+start-0x800000,(uint32_t)kernel_service_##suffix);
#include "../kernel/service_forward.def"
#undef SERVICE
#define TARGET(address) jump(base+0x##address-0x800000,(uint32_t)kernel_syscall_target_##address)
    TARGET(8082c6);TARGET(808378);TARGET(81021c);TARGET(807a0a);
#ifndef KERNEL_SURFACE_CHAIN_TEST
    TARGET(81017c);
#endif
#ifndef KERNEL_SERVICE_CHAIN_TEST
    TARGET(81041a);TARGET(810478);TARGET(810448);TARGET(82a9bb);
#else
    jump(base+0x3734a,(uint32_t)random_seed);
#endif
#undef TARGET
    jump(base+0x1b714,(uint32_t)kernel_palette_color);jump(base+0x129e,(uint32_t)kernel_queue_api_request);
    jump(base+0x1434a,(uint32_t)kernel_yield_task);
}
static void gate(uint32_t vector_,uint32_t entry,uint32_t attributes)
{idt[vector_]=(entry&0xffffu)|((uint64_t)16<<16)|((uint64_t)attributes<<32)|((uint64_t)(entry&0xffff0000u)<<32);}
static void setup(void)
{
    const struct route *r=route();uint32_t i,dispatch;
    for(i=0;i<0x1000;++i)((volatile uint8_t *)0x1b000)[i]=(uint8_t)(i*37+pattern*83);
    for(i=0;i<0x40;++i)((volatile uint8_t *)0x600000)[i]=(uint8_t)(i*17+pattern*59);
    for(i=0;i<0x1000;++i)((volatile uint8_t *)0x610000)[i]=(uint8_t)(i*23+pattern*31);
    for(i=0;i<0x2000;++i)((volatile uint8_t *)0x6a0000)[i]=(uint8_t)(i*29+pattern*47);
    for(i=0;i<8;++i){
        syscalls_input[i]=0x10293847u*(i+1)^(pattern*0x10204081u);
        parameters[i]=values[(pattern+i)%COUNT(values)];WORD(0x60000c+4*i)=parameters[i];
    }
    if(real_service()){
        if(vector==0x41 && operation==4){parameters[0]=values[pattern%COUNT(values)];parameters[1]=parameters[0]+0x10000;}
        else {
            uint32_t object=0x610000+(pattern&3);
            parameters[0]=0xc0000007;WORD(0x6a001c)=object;
            WORD(object+4)=values[pattern%COUNT(values)];WORD(object+12)=values[(pattern+1)%COUNT(values)];
        }
        WORD(0x60000c)=parameters[0];WORD(0x600010)=parameters[1];
    }
#ifdef KERNEL_SURFACE_CHAIN_TEST
    if(real_surface())surface_setup();
#endif
    for(i=0;i<COUNT(syscalls_result);++i)syscalls_result[i]=0;
    for(i=0;i<COUNT(trace);++i)trace[i]=0;
    syscalls_callback_flags=syscalls_callback_cs=syscalls_callback_ds=syscalls_callback_ss=0;
    seed_hits=0;
    syscalls_return=values[(pattern+3)%COUNT(values)];WORD(0x1b084)=syscalls_return;
    syscalls_input[4]=r && r->count?0x600000:(pattern&1?0:0xfffffff4);
    syscalls_input[5]=operation;
    syscalls_flags=2|(pattern&1?0x8d5:0)|(pattern&2?0x400:0)|(pattern&4?0x200:0);
    syscalls_segment=ring?75:(pattern&8?24:8);
    if(!phase)dispatch=vector==0x40?0x913b70:0x91406c;
    else {
#ifdef KERNEL_LINKED_TEST
        dispatch=vector==0x40?0x813b70:0x81406c;
#else
        dispatch=vector==0x40?(uint32_t)kernel_dispatch_syscall40:(uint32_t)kernel_dispatch_syscall41;
#endif
    }
    WORD(0x18600+4*vector)=dispatch;
    gate(vector,(vector==0x40?0x8372be:0x8372d1)+(phase?0:0x100000),gate_type?0xef00:0xee00);
    syscalls_operation=(uint32_t)(vector==0x40?entries_int40:entries_int41);
}
static void check(void)
{
    const struct route *r=route();uint32_t i;
    require(trace[0]==(r&&!real_service()&&!real_surface()?1u:0u));
    require(seed_hits==(real_service()&&vector==0x41&&operation==4?1u:0u));
    if(real_service()&&vector==0x40)require(WORD(0x610000+(pattern&3)+0x55f)==parameters[1]);
    if(r&&!real_service()&&!real_surface()){
        require(trace[1]==r->target && trace[2]==r->count);
        require(syscalls_callback_cs==16 && syscalls_callback_ds==8 && syscalls_callback_ss==8);
        require((syscalls_callback_flags&0x600)==((syscalls_flags&0x400)|0x200));
        for(i=0;i<r->count;++i){
            uint32_t expected=parameters[i];
            if(vector==0x40 && operation==17)expected=i?parameters[0]:13;
            if(r->sign && i==5)expected=(uint32_t)(int32_t)(int8_t)expected;
            require(trace[3+i]==expected);
        }
    }
#ifdef KERNEL_SURFACE_CHAIN_TEST
    if(real_surface())surface_check();
#endif
    for(i=0;i<8;++i){
        uint32_t expected=i==3?syscalls_stack-4:syscalls_input[i];
        if(i==7 && vector==0x41)expected=real_service()?service_result():(r||operation==5?syscalls_return:0);
        require(syscalls_result[i]==expected);
    }
    require(syscalls_result[8]==syscalls_flags);
    for(i=9;i<14;++i)require(syscalls_result[i]==(i==11?(ring?75u:8u):syscalls_segment));
    for(i=0;i<COUNT(syscalls_result);++i){if(!phase)saved_result[i]=syscalls_result[i];else require(saved_result[i]==syscalls_result[i]);}
    for(i=0;i<COUNT(trace);++i){if(!phase)saved_trace[i]=trace[i];else require(saved_trace[i]==trace[i]);}
    for(i=0;i<COUNT(saved_memory);++i){
        uint8_t byte=i<0x1000?((volatile uint8_t *)0x1b000)[i]:
            i<0x1040?((volatile uint8_t *)0x600000)[i-0x1000]:
            i<0x2040?((volatile uint8_t *)0x610000)[i-0x1040]:((volatile uint8_t *)0x6a0000)[i-0x2040];
        if(!phase)saved_memory[i]=byte;else require(saved_memory[i]==byte);
    }
}
void syscalls_qemu_main(void)
{
    uint32_t i,operation_index;
    struct __attribute__((packed)){uint16_t limit;uint32_t base;} descriptor={sizeof(idt)-1,(uint32_t)idt};
    __asm__ volatile("outb %0,$0x21;outb %0,$0xa1"::"a"((uint8_t)0xff));
    for(i=0;i<COUNT(idt);++i)gate(i,(uint32_t)entries_fault,0x8e00);
    gate(0xf1,(uint32_t)user_return,0xee00);__asm__ volatile("lidt %0"::"m"(descriptor));
    for(i=0;i<COUNT(tss);++i)tss[i]=0;
    tss[1]=0x510000;tss[2]=8;tss[25]=sizeof(tss)<<16;
    {uint32_t base=(uint32_t)tss;entries_gdt[10]=(sizeof(tss)-1)|((uint64_t)(base&0xffffff)<<16)|((uint64_t)0x89<<40)|((uint64_t)(base>>24)<<56);}
    __asm__ volatile("ltr %w0"::"r"((uint16_t)80));
    redirect(0x800000);redirect(0x900000);
    put("QEMU native system-call dispatch, original/rebuilt/oracle\n");
    for(ring=0;ring<2;++ring)for(vector=0x40;vector<=0x41;++vector)
    for(operation_index=0;operation_index<27;++operation_index)for(gate_type=0;gate_type<2;++gate_type)
    for(pattern=0;pattern<16;++pattern){
        operation=operation_index;
        if(operation>=(vector==0x40?20u:6u))operation=vector==0x40?invalid40[operation-20]:invalid41[operation-6];
        for(phase=0;phase<2;++phase){setup();if(ring)invoke_user();else invoke();check();}
        ++count;
    }
    require(count==3456);put("PASS: 3456 complete syscall-chain comparisons; CPL0/CPL3, PUSHAD, routes, arguments, EAX and IRET\n");
#ifdef KERNEL_SERVICE_CHAIN_TEST
    put("PASS: recovered object fields and xorshift services used inside real INT/IRET chains\n");
#endif
#ifdef KERNEL_SURFACE_CHAIN_TEST
    vector=0x40;operation=16;
    for(ring=0;ring<2;++ring)for(gate_type=0;gate_type<2;++gate_type)
    for(surface_variant=0;surface_variant<32;++surface_variant)for(pattern=0;pattern<16;++pattern){
        for(phase=0;phase<2;++phase){setup();if(ring)invoke_user();else invoke();check();}
        ++count;++surface_total;
    }
    require(surface_total==2048);
    put("PASS: 2048 additional native surface-chain comparisons; real CLI/POPFL, clipping, live list and cached callbacks\n");
#endif
    done(0x10);
}
