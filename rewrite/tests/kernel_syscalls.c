#include "../kernel/syscalls.h"
#include "../kernel/service_forward.h"
#include "../kernel/runtime.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
struct route {uint32_t address,count,sign;};
/* Independent transcription of the original dispatcher call sites. */
static const struct route routes40[]={
    {0x8374cf,4,0},{0x837501,6,0},{0x837545,6,0},{0x837589,6,0},
    {0x8375cd,5,0},{0x837609,5,0},{0x837645,6,1},{0x83768f,6,0},
    {0x837861,5,0},{0x8376d3,6,0},{0x837717,7,0},{0x8377d3,6,1},
    {0x83781d,6,0},{0x8082c6,1,0},{0x808378,1,0},{0x81021c,1,0},
    {0x81017c,1,0},{0x80129e,2,0},{0x81041a,2,0},{0x81434a,0,0}};
static const struct route routes41[]={
    {0x807a0a,8,0},{0x810478,1,0},{0x810448,1,0},{0x81b714,1,0},{0x82a9bb,2,0}};
static const uint32_t values[]={0,1,0x7f,0x80,0xff,0x100,0x7fffffff,0x80000000,0xffffffff};
static const uint32_t invalid[]={20,21,255,256,65535,0x80000000,0xffffffff};
static const uint32_t invalid41[]={6,7,8,9,10,11,12,13,14,15,16,17,18,19,
    20,21,255,256,65535,0x80000000,0xffffffff};
static uint32_t mode,group,op,return_word,parameters[8],trace[11],saved_trace[11],cases;
static uint32_t frame_after[8] __attribute__((used));
static uint8_t expected[0x3000];
static uint8_t expected_guard[0x1000];
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t args[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)args,0,0)==address;}
static int load(const char *path,uint32_t address)
{
    int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;
    n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;
}
static void jump(uint32_t address,uint32_t target)
{*(uint8_t *)address=0xe9;WORD(address+1)=target-address-5;}
static const struct route *route(void)
{return group==0?(op<20?routes40+op:0):(op<5?routes41+op:0);}
static uint32_t input_count(void)
{const struct route *r=route();return !r?0:group==0 && op==17?1:r->count;}
/* The assembly boundaries pass their real outgoing stack to this observer.
 * Deliberate argument writes expose illegal tail-call reuse of PUSHAD. */
__attribute__((used,noinline)) static uint32_t observe(volatile uint32_t *args,uint32_t target)
{
    const struct route *r=route();uint32_t i;
    ++trace[0];trace[1]=target;trace[2]=r?r->count:0;
    if(r)for(i=0;i<r->count;++i){trace[3+i]=args[i];if(mode&1)args[i]=0xa5a50000|i;}
    if(mode&2)for(i=0;i<8;++i)WORD(0x60000c+4*i)=0xface0000|i;
    WORD(0x1b088)^=target;
    return return_word;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define STUB(type,name,address,parameters_) \
    __attribute__((naked,noinline)) type name parameters_ { \
        __asm__("pushl %ebp;movl %esp,%ebp;pushl $" #address ";" \
                "leal 8(%ebp),%eax;pushl %eax;call observe;addl $8,%esp;leave;ret"); \
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
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t entry,uint32_t *frame)
{
    __asm__("pushl %ebp;pushl %ebx;pushl %esi;pushl %edi;movl 24(%esp),%esi;"
        "pushl 28(%esi);pushl 24(%esi);pushl 20(%esi);pushl 16(%esi);"
        "pushl 12(%esi);pushl 8(%esi);pushl 4(%esi);pushl (%esi);call *52(%esp);"
        "movl %esp,%esi;movl $frame_after,%edi;movl $8,%ecx;cld;rep movsl;"
        "addl $32,%esp;popl %edi;popl %esi;popl %ebx;popl %ebp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
static void redirect(uint32_t base)
{
#define SERVICE(suffix,slot,form,start,end) jump(base+start-0x800000,(uint32_t)kernel_service_##suffix);
#include "../kernel/service_forward.def"
#undef SERVICE
#define TARGET(address) jump(base+0x##address-0x800000,(uint32_t)kernel_syscall_target_##address)
    TARGET(8082c6);TARGET(808378);TARGET(81021c);TARGET(81017c);TARGET(81041a);
    TARGET(807a0a);TARGET(810478);TARGET(810448);TARGET(82a9bb);
#undef TARGET
    jump(base+0x1b714,(uint32_t)kernel_palette_color);jump(base+0x129e,(uint32_t)kernel_queue_api_request);
    jump(base+0x1434a,(uint32_t)kernel_yield_task);
}
static void initialize(uint32_t pattern)
{
    uint32_t i;for(i=0;i<0x1000;++i)((volatile uint8_t *)0x1b000)[i]=(uint8_t)(i*37+pattern*83);
    for(i=0;i<0x2000;++i)((volatile uint8_t *)0x600000)[i]=(uint8_t)(i*37+pattern*83);
    for(i=0;i<8;++i)WORD(0x60000c+4*i)=parameters[i];
    for(i=0;i<11;++i)trace[i]=0;
    WORD(0x1b084)=return_word;
}
static void initialize_record(uint32_t pattern,uint32_t pointer)
{
    uint32_t i;
    initialize(pattern);
    for(i=0;i<0x1000;++i)((volatile uint8_t *)0x604000)[i]=(uint8_t)(i*19+pattern*43);
    for(i=0;i<input_count();++i)WORD(pointer+12+4*i)=parameters[i];
}
static int oracle(uint32_t result)
{
    const struct route *r=route();uint32_t i;
    if(trace[0]!=(r?1u:0u))return 0;
    if(group && result!=(r||op==5?return_word:0))return 0;
    if(!r)return 1;
    if(trace[1]!=r->address || trace[2]!=r->count)return 0;
    for(i=0;i<r->count;++i){
        uint32_t value=parameters[i];
        if(group==0 && op==17)value=i==0?13:parameters[0];
        if(r->sign && i==5)value=(uint32_t)(int32_t)(int8_t)value;
        if(trace[3+i]!=value)return 0;
    }
    return 1;
}
static uint32_t candidate_entry(void)
{
#ifdef KERNEL_LINKED_TEST
    return group?0x81406c:0x813b70;
#else
    return group?(uint32_t)kernel_dispatch_syscall41:(uint32_t)kernel_dispatch_syscall40;
#endif
}
static int exercise(uint32_t pattern,uint32_t pointer)
{
    uint32_t frame[8]={0xfedcba98,0x76543210,0x89abcdef,0x13579bdf,pointer,op,0x2468ace0,0xdeadbeef};
    uint32_t i,result;
    initialize_record(pattern,pointer);result=invoke(group?0x91406c:0x913b70,frame);
    if(!oracle(result))return 1;
    for(i=0;i<8;++i)if(frame_after[i]!=frame[i])return 2;
    for(i=0;i<0x1000;++i)expected[i]=((volatile uint8_t *)0x1b000)[i];
    for(i=0;i<0x2000;++i)expected[0x1000+i]=((volatile uint8_t *)0x600000)[i];
    for(i=0;i<11;++i)saved_trace[i]=trace[i];
    for(i=0;i<0x1000;++i)expected_guard[i]=((volatile uint8_t *)0x604000)[i];
    initialize_record(pattern,pointer);result=invoke(candidate_entry(),frame);
    if(!oracle(result))return 3;
    for(i=0;i<8;++i)if(frame_after[i]!=frame[i])return 4;
    for(i=0;i<0x1000;++i)if(expected[i]!=((volatile uint8_t *)0x1b000)[i])return 5;
    for(i=0;i<0x2000;++i)if(expected[0x1000+i]!=((volatile uint8_t *)0x600000)[i])return 6;
    for(i=0;i<11;++i)if(saved_trace[i]!=trace[i])return 7;
    for(i=0;i<0x1000;++i)if(expected_guard[i]!=((volatile uint8_t *)0x604000)[i])return 8;
    ++cases;return 0;
}
static void number(uint32_t n)
{char s[11];uint32_t i=10;s[i]=0;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}

/* Linux i386 debug-register ABI, also used by kernel_service_optional.c.
 * Each adjacent pair remains watched throughout the call: the higher DWORD
 * must be read first, both exactly once. Poison each word after its trap;
 * the callback must still receive the value captured before that mutation. */
struct registers {
    uint32_t ebx,ecx,edx,esi,edi,ebp,eax;
    uint32_t ds,es,fs,gs,original_eax,eip,cs,flags,esp,ss;
};
static int trace_call(uint32_t request,uint32_t child,uint32_t address,uint32_t data)
{
    int result;
    __asm__ volatile("int $0x80":"=a"(result):"a"(26),"b"(request),"c"(child),"d"(address),"S"(data):"memory");
    return result;
}
static int wait_child(int child,int *status)
{return syscall3(7,(uint32_t)child,(uint32_t)status,0)==child;}
extern const char after_syscall_watch;
__attribute__((noinline)) static void watched_child(uint32_t entry,uint32_t *frame)
{
    uint32_t value=invoke(entry,frame);
    if(!oracle(value))syscall3(1,82,0,0);
    for(uint32_t i=0;i<8;++i)if(frame_after[i]!=frame[i])syscall3(1,83,0,0);
    __asm__ volatile(".global after_syscall_watch\nafter_syscall_watch: int3":::"memory");
    syscall3(1,0,0,0);__builtin_unreachable();
}
static int watch_pair(uint32_t high,int original)
{
    uint32_t frame[8]={0x76543210,0x89abcdef,0x13579bdf,0x2468ace0,0x600000,op,0xdeadbeef,0x55aa55aa};
    uint32_t addresses[2]={0x60000c+4*high,0x600008+4*high},reads=0,traps=0;
    uint32_t count=high?2:1,entry=original?(group?0x91406c:0x913b70):candidate_entry();
    int child,status=0;
    mode=0;initialize_record(0,0x600000);
    child=syscall3(2,0,0,0);if(child<0)return 0;
    if(!child){
        if(trace_call(0,0,0,0)<0)syscall3(1,81,0,0);
        syscall3(37,(uint32_t)syscall3(20,0,0,0),19,0);
        watched_child(entry,frame);
    }
    if(!wait_child(child,&status)||status!=0x137f ||
       trace_call(6,(uint32_t)child,252,addresses[0])<0 ||
       (count==2 && trace_call(6,(uint32_t)child,256,addresses[1])<0) ||
       trace_call(6,(uint32_t)child,280,count==2?0xff0005:0xf0001)<0 ||
       trace_call(7,(uint32_t)child,0,0)<0)goto failed;
    while(wait_child(child,&status)){
        struct registers regs;uint32_t debug_status=0;
        if(++traps>count+1 || status!=0x57f ||
           trace_call(12,(uint32_t)child,0,(uint32_t)&regs)<0)goto failed;
        if(regs.eip==(uint32_t)&after_syscall_watch+1){
            if(reads!=count || trace_call(7,(uint32_t)child,0,0)<0 ||
               !wait_child(child,&status)||status!=0)goto failed;
            return 1;
        }
        if(reads>=count || trace_call(3,(uint32_t)child,276,(uint32_t)&debug_status)<0 ||
           (debug_status&15)!=(1u<<reads) ||
           trace_call(5,(uint32_t)child,addresses[reads],parameters[high-reads]^0x97a35ce1)<0 ||
           trace_call(6,(uint32_t)child,276,0)<0)goto failed;
        ++reads;if(trace_call(7,(uint32_t)child,0,0)<0)goto failed;
    }
failed:
    syscall3(37,(uint32_t)child,9,0);(void)wait_child(child,&status);return 0;
}
static int guard_fault(uint32_t bytes,int original)
{
    uint32_t frame[8]={0,0,0,0,0x605000-12-4*input_count()+bytes,op,0,0};
    int child=syscall3(2,0,0,0),status=0;
    if(child<0)return 0;
    if(!child){invoke(original?(group?0x91406c:0x913b70):candidate_entry(),frame);syscall3(1,84,0,0);}
    return wait_child(child,&status) && (status&0x7f)==11;
}
static int run(void)
{
    uint32_t p,v,i,error,watched=0,faults=0;
    if(!map(0x1b000,0x1000)||!map(0x600000,0x2000)||!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 1;
    if(!map(0x603000,0x3000)||syscall3(125,0x603000,0x1000,0)||syscall3(125,0x605000,0x1000,0))return 3;
    redirect(0x900000);
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 2;
    redirect(0x800000);
#endif
    for(group=0;group<2;++group)for(op=0;op<(group?6+COUNT(invalid41):20+COUNT(invalid));++op){
        uint32_t saved_op=op;
        if(op>=(group?6u:20u))op=group?invalid41[op-6]:invalid[op-20];
        for(p=0;p<3;++p)for(mode=0;mode<4;++mode)for(v=0;v<COUNT(values);++v){
            for(i=0;i<8;++i)parameters[i]=values[(v+i)%COUNT(values)];
            return_word=values[(v+3)%COUNT(values)];
            error=exercise(p,0x600000);if(error)return 10+error;
        }
        if(!route() || (group==0 && op==19)){
            error=exercise(0,0);if(error)return 20+error;
            error=exercise(0,0xfffffff4);if(error)return 30+error;
        }
        op=saved_op;
    }
    for(group=0;group<2;++group)for(op=0;op<(group?5u:20u);++op)
    for(v=0;v<256;++v){
        for(i=0;i<8;++i)parameters[i]=0x93a50000u^(i<<20)^((v*(2*i+1)+257*i)&255);
        return_word=values[v%COUNT(values)];mode=v&3;
        error=exercise(v%3,0x600000);if(error)return 40+error;
    }
    for(group=0;group<2;++group)for(op=0;op<(group?5u:19u);++op){
        for(i=0;i<8;++i)parameters[i]=0x83b70180u^(i*0x32459617u);
        mode=0;return_word=0xfedcba98;
        error=exercise(0,0x603ff4);if(error)return 50+error;
        error=exercise(0,0x605000-12-4*input_count());if(error)return 60+error;
        for(p=0;p<2;++p){
            for(v=0;v<3;++v){if(!guard_fault(v+1,p))return 70;++faults;}
            for(v=0;v<(input_count()>1?input_count()-1:1);++v){
                if(!watch_pair(input_count()>1?v+1:0,p))return 71;
                ++watched;
            }
        }
    }
    {static const char message[]="Syscall dispatch original/C/oracle cases: ";syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    number(cases);{static const char message[]="; ordered routes, raw returns, signed bytes and PUSHAD preservation verified\n";syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    {static const char message[]="Syscall hardware-watchpoint original/C comparisons: ";syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    number(watched);{static const char message[]="; descending DWORD reads exactly once and captured values verified\n";syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    {static const char message[]="Syscall guard-page original/C faults: ";syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    number(faults);{static const char message[]="; full-width reads and exact record boundaries verified\n";syscall3(4,1,(uint32_t)message,sizeof(message)-1);}
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
