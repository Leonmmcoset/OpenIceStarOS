/* Run the actual original x86 and recovered C with observable renderer and
 * interrupt-helper boundaries. POPFL is real: the invocation shim captures
 * EFLAGS immediately after return, then restores its caller's flags. */
#include "../kernel/surface_refresh.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t starts[]={0x1b000,0x500000,0x600000,0x610000};
static const uint32_t lengths[]={0x1000,0x2000,0x1000,0x1000};
static const uint32_t actives[]={0,0xffffffff,1,2,0x40000001,0x80000001};
static const uint32_t orders[][2]={{0,0},{0,1},{1,0},{1,1},{1,2},{2,1},
    {0x80000000,0x7fffffff},{0xffffffff,0x80000000}};
static const uint32_t args[]={0x80000001,0xffffffff,0x7fffffff,0x13579bdf};
static uint8_t expected[0x5000];
static uint32_t events[5][6],expected_events[5][6],calls,failure;
static uint32_t pattern,alignment,active_case,order_case,style,mutation,wrap;
static uint32_t layer,object,alternate,active1,active2,flags_word,active_after;
static uint32_t cases,branches,expected_return;
uint32_t surface_flags_after,surface_arguments_after[6];

static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void jump(uint32_t address,uint32_t target)
{*(uint8_t *)address=0xe9;WORD(address+1)=target-address-5;}
static void record(uint32_t kind,const uint32_t *arguments,uint32_t count)
{
    uint32_t i;if(calls>=5){failure=1;return;}
    events[calls][0]=kind;
    for(i=0;i<count;++i)events[calls][i+1]=arguments[i];
    ++calls;
}
uint32_t kernel_read_flags(void)
{
    record(1,0,0);
    if(mutation&1){WORD(0x1b0e4)=0x610100;WORD(0x500000+4*layer)=alternate;}
    return flags_word;
}
void kernel_disable_interrupts(void)
{
    record(2,0,0);
    if(mutation&2)WORD(0x1b138)=active_after;
}
__attribute__((used,noinline)) static void write_flags_observe(uint32_t flags)
{record(5,&flags,1);WORD(0x1b200)=flags;}
__attribute__((naked,noinline)) void kernel_write_flags(uint32_t flags __attribute__((unused)))
{
    __asm__("pushl 4(%esp);call write_flags_observe;addl $4,%esp;"
        "movl 4(%esp),%eax;pushl %eax;popfl;ret");
}
__attribute__((used,noinline)) static uint32_t draw_observe(uint32_t kind,volatile uint32_t *a)
{
    uint32_t i;record(kind,(const uint32_t *)a,kind==3?5:4);
    for(i=0;i<(kind==3?5u:4u);++i)a[i]=0xa5a50000|i;
    if(mutation&4){WORD(0x1b0e4)=0xdeadbeef;WORD(0x1b138)=0xdeadbeef;
        WORD(0x500000+4*layer)=0xdeadbeef;WORD(object+40)^=0x80000000;}
    WORD(0x1b204)^=kind*0x132457;
    return 0xcdef0000|kind;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked,noinline)) uint32_t kernel_surface_target_817cd0(
    uint32_t layer,uint32_t x,uint32_t y,uint32_t width,uint32_t height)
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $3;call draw_observe;addl $8,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t refresh0(uint32_t x,uint32_t y,uint32_t width,uint32_t height)
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $4;call draw_observe;addl $8,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t refresh1(uint32_t x,uint32_t y,uint32_t width,uint32_t height)
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $6;call draw_observe;addl $8,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target,uint32_t layer,
    uint32_t x,uint32_t y,uint32_t width,uint32_t height)
{
    __asm__("pushl %ebp;movl %esp,%ebp;pushfl;subl $40,%esp;"
        "movl 28(%ebp),%eax;movl %eax,16(%esp);"
        "movl 24(%ebp),%eax;movl %eax,12(%esp);"
        "movl 20(%ebp),%eax;movl %eax,8(%esp);"
        "movl 16(%ebp),%eax;movl %eax,4(%esp);"
        "movl 12(%ebp),%eax;movl %eax,0(%esp);"
        "movl $0x93fedc71,20(%esp);call *8(%ebp);"
        "pushfl;popl surface_flags_after;cld;"
        "movl 0(%esp),%ecx;movl %ecx,surface_arguments_after;"
        "movl 4(%esp),%ecx;movl %ecx,surface_arguments_after+4;"
        "movl 8(%esp),%ecx;movl %ecx,surface_arguments_after+8;"
        "movl 12(%esp),%ecx;movl %ecx,surface_arguments_after+12;"
        "movl 16(%esp),%ecx;movl %ecx,surface_arguments_after+16;"
        "movl 20(%esp),%ecx;movl %ecx,surface_arguments_after+20;"
        "pushl -4(%ebp);popfl;leave;ret");
}
#pragma GCC diagnostic pop
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<lengths[r];++i)
        ((volatile uint8_t *)starts[r])[i]=(uint8_t)(i*37+pattern*83+r*59);
    WORD(0x500000+4*layer)=object;WORD(0x500004)=active1;WORD(0x500008)=active2;
    WORD(object+44)=orders[order_case][0];WORD(object+40)=style?0x80000001:0x7fffffff;
    WORD(alternate+44)=orders[order_case][0]^0x80000000;
    WORD(alternate+40)=style?0x7fffffff:0x80000000;
    WORD(active1+44)=orders[order_case][1];WORD(active2+44)=orders[order_case][1];
    WORD(0x1b138)=actives[active_case];WORD(0x1b0e4)=0x610001;
    WORD(0x610001+44)=(uint32_t)refresh0;WORD(0x610100+44)=(uint32_t)refresh1;
    calls=failure=0;for(i=0;i<30;++i)((uint32_t *)events)[i]=0;
}
static void save(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<lengths[r];++i)
        expected[n++]=((volatile uint8_t *)starts[r])[i];
    for(i=0;i<30;++i)((uint32_t *)expected_events)[i]=((uint32_t *)events)[i];
}
static int compare(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<lengths[r];++i)
        if(expected[n++]!=((volatile uint8_t *)starts[r])[i])return 0;
    for(i=0;i<30;++i)if(((uint32_t *)expected_events)[i]!=((uint32_t *)events)[i])return 0;
    return 1;
}
static void oracle(void)
{
    uint32_t selected,order=orders[order_case][0],current=actives[active_case];
    uint32_t negative=style,a[5],i;
    if(!order){expected_return=object;branches|=1;return;}
    kernel_read_flags();kernel_disable_interrupts();
    if(mutation&1){order^=0x80000000;negative=!negative;}
    if(mutation&2)current=active_after;
    selected=(int32_t)current>0?current:2;
    if(negative && ((int32_t)current<=0 || order>orders[order_case][1]))selected=layer;
    branches|=selected==layer?2:((int32_t)current>0?4:8);
    a[0]=selected;for(i=0;i<4;++i)a[i+1]=args[i];draw_observe(3,a);
    for(i=0;i<4;++i)a[i]=args[i];
    draw_observe(4,a);
    write_flags_observe(flags_word);expected_return=flags_word;
}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    return 0x81905e;
#else
    return (uint32_t)kernel_syscall_target_81905e;
#endif
}
static int exercise(void)
{
    uint32_t phase,i,result,expected_calls;
    initialize();oracle();save();expected_calls=calls;
    for(phase=0;phase<2;++phase){
        initialize();result=invoke(phase?candidate():0x91905e,layer,args[0],args[1],args[2],args[3]);
        if(failure||result!=expected_return||calls!=expected_calls)return 1;
        if(calls && ((surface_flags_after^flags_word)&0xcd5))return 2;
        if(surface_arguments_after[0]!=layer||surface_arguments_after[5]!=0x93fedc71)return 3;
        for(i=0;i<4;++i)if(surface_arguments_after[i+1]!=args[i])return 4;
        if(!compare())return 5;
    }
    ++cases;return 0;
}
static void redirect(uint32_t base)
{
    uint32_t p,target,replacement;
    jump(base+0x17cd0,(uint32_t)kernel_surface_target_817cd0);
    /* The original CLI and flags-read leaves are only 2/3 bytes long.
     * Redirect their CALL sites rather than overwriting adjacent leaves. */
    for(p=base+0x1905e;p+5<=base+0x191d8;++p){
        if(*(volatile uint8_t *)p!=0xe8)continue;
        target=p+5+WORD(p+1);replacement=0;
        if(target==base+0x1a24a)replacement=(uint32_t)kernel_read_flags;
        if(target==base+0x1a242)replacement=(uint32_t)kernel_disable_interrupts;
        if(target==base+0x1a24d)replacement=(uint32_t)kernel_write_flags;
        if(replacement){WORD(p+1)=replacement-p-5;p+=4;}
    }
}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int run(void)
{
    uint32_t r,error;
    for(r=0;r<COUNT(starts);++r)if(!map(starts[r],lengths[r]))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
    redirect(0x900000);
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
    redirect(0x800000);
#endif
    for(pattern=0;pattern<2;++pattern)for(alignment=0;alignment<4;++alignment)
    for(active_case=0;active_case<COUNT(actives);++active_case)
    for(order_case=0;order_case<COUNT(orders);++order_case)for(style=0;style<2;++style)
    for(mutation=0;mutation<8;++mutation)for(wrap=0;wrap<4;++wrap){
        layer=5+wrap*0x40000000u;object=0x600000+alignment;alternate=0x600100+((alignment+1)&3);
        active1=0x600200+((alignment+2)&3);active2=0x600300+((alignment+3)&3);
        active_after=active_case&1?0:0x40000001;
        flags_word=0x202|((order_case*177+mutation*257+alignment*53+pattern*997)&0xcd5);
        error=exercise();if(error){PRINT("Surface refresh failure: ");number(error);PRINT(" at case ");number(cases);PRINT("\n");return 10+error;}
    }
    if(branches!=15)return 20;
    PRINT("Surface refresh original/C/oracle cases: ");number(cases);
    PRINT("; all selection routes, cached callback, mutations, stack isolation and full memory verified\n");
    PRINT("Native POPFL return checks: arithmetic flags and DF restored before RET\n");return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
