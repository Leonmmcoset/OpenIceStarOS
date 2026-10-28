/* Compare the actual original x86 and C against a clipping/list oracle.
 * Only the per-layer drawing callback is replaced by an observer. */
#include "../kernel/surface_composite.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t starts[]={0x1b000,0x500000,0x50f000,0x600000,0x610000};
static const uint32_t sizes[]={0x1000,0x1000,0x3000,0x1000,0x1000};
static const uint32_t rectangles[][4]={
    {0,0,1,1},{0xffffffff,0xffffffff,1,1},{0x80000000,0x7fffffff,1,1},
    {1,1,0xffffffff,0xffffffff},{50,60,100,200},{0x7fffffff,0x7fffffff,1,1},
    {0x7ffffffe,0x7ffffffe,2,2},{1,2,0x80000000,0x80000000},
    {0,0,0xffffffff,0x7fffffff},{5,8,0,0},{300,400,20,30},
    {0x80000001,0x80000001,0xffffffff,0xffffffff}};
static const uint32_t screens[][2]={{100,100},{0,0},{0x80000000,0xffffffff}};
static const uint32_t lists[][2]={{0,0},{1,3},{3,2},{7,7},{0xffffffff,4},
    {0x80000000,2},{0xfffffffe,0xfffffffd},{0xfffffffe,0xfffffffe},{0xffffffff,0xffffffff}};
static uint8_t expected[0x7000];
static uint32_t events[8][6],expected_events[8][6],expected_arguments[6];
static uint32_t calls,failure,pattern,alignment,wrap,rect,screen_case,list_case,mode;
static uint32_t object,screen,layer,cases,coverage;
volatile uint32_t *composite_caller_arguments;
uint32_t composite_arguments_after[6];
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static void effects(void)
{
    if(calls==1){
        if(list_case>=7 || (mode&1))WORD(0x510000)=0;
        else if(mode&2)WORD(0x510000)=lists[list_case][1]+1;
        if(mode&2){
            WORD(0x1b0e4)=0xdeadbeef;WORD(0x610001+40)=0xdeadbeef;
            WORD(0x500000)=0xdeadbeef;WORD(object+44)=0xdeadbeef;
            WORD(0x51000c)=0x13579bdf;
        }
        if(mode&4){
            composite_caller_arguments[1]+=5;composite_caller_arguments[2]^=0x80000000;
            composite_caller_arguments[3]-=3;composite_caller_arguments[4]+=7;
        }
    }
    WORD(0x1b300)^=calls*0x132457;
}
__attribute__((used,noinline)) static uint32_t observe(volatile uint32_t *a)
{
    uint32_t i;
    if(calls>=8){failure=1;return 0;}
    events[calls][0]=0x817dac;
    for(i=0;i<5;++i){events[calls][i+1]=a[i];a[i]=0xa5a50000|i;}
    ++calls;effects();return 0xcdef0000|calls;
}
__attribute__((naked,noinline)) static uint32_t draw(void)
{__asm__("leal 4(%esp),%eax;pushl %eax;call observe;addl $4,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target __attribute__((unused)))
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $40,%esp;"
        "movl layer,%eax;movl %eax,0(%esp);"
        "movl rect,%eax;shll $4,%eax;addl $rectangles,%eax;"
        "movl 0(%eax),%edx;movl %edx,4(%esp);"
        "movl 4(%eax),%edx;movl %edx,8(%esp);"
        "movl 8(%eax),%edx;movl %edx,12(%esp);"
        "movl 12(%eax),%edx;movl %edx,16(%esp);"
        "movl $0x93fedc71,20(%esp);movl %esp,composite_caller_arguments;"
        "call *8(%ebp);"
        "movl 0(%esp),%edx;movl %edx,composite_arguments_after;"
        "movl 4(%esp),%edx;movl %edx,composite_arguments_after+4;"
        "movl 8(%esp),%edx;movl %edx,composite_arguments_after+8;"
        "movl 12(%esp),%edx;movl %edx,composite_arguments_after+12;"
        "movl 16(%esp),%edx;movl %edx,composite_arguments_after+16;"
        "movl 20(%esp),%edx;movl %edx,composite_arguments_after+20;leave;ret");
}
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<sizes[r];++i)
        ((volatile uint8_t *)starts[r])[i]=(uint8_t)(i*37+pattern*83+r*59);
    WORD(0x1b0e4)=0x610001;WORD(0x610001+40)=(uint32_t)draw;
    WORD(0x500000)=screen;WORD(0x500000+4*layer)=object;
    WORD(screen+12)=screens[screen_case][0];WORD(screen+16)=screens[screen_case][1];
    WORD(object+44)=lists[list_case][0];
    WORD(0x50fff8)=0xdeadbeef;WORD(0x50fffc)=0xface1234;
    for(i=1;i<=8;++i)WORD(0x510000+4*i)=0x31415926*i;
    WORD(0x510000)=lists[list_case][1];
    calls=failure=0;for(i=0;i<48;++i)((uint32_t *)events)[i]=0;
}
static int64_t signed_word(uint32_t n)
{return n<0x80000000u?(int64_t)n:(int64_t)n-INT64_C(4294967296);}
static uint32_t oracle(void)
{
    uint32_t a[6],i,order=lists[list_case][0],entry[5];
    a[0]=layer;for(i=0;i<4;++i)a[i+1]=rectangles[rect][i];a[5]=0x93fedc71;
    if(signed_word(a[1])<0){a[1]=0;coverage|=1;}
    if(signed_word(a[2])<0){a[2]=0;coverage|=2;}
    if(signed_word(a[1]+a[3])>signed_word(screens[screen_case][0])){a[3]=screens[screen_case][0]-a[1];coverage|=4;}
    if(signed_word(a[2]+a[4])>signed_word(screens[screen_case][1])){a[4]=screens[screen_case][1]-a[2];coverage|=8;}
    composite_caller_arguments=a;
    for(;;){
        uint32_t limit=WORD(0x510000);
        if((uint64_t)order>(uint64_t)limit)break;
        entry[0]=WORD(0x510000+4*order);for(i=1;i<5;++i)entry[i]=a[i];
        observe(entry);order=(uint32_t)((uint64_t)order+1);
    }
    if(!calls)coverage|=16;else if(calls>1)coverage|=32;
    if(order<lists[list_case][0])coverage|=64;
    for(i=0;i<6;++i)expected_arguments[i]=a[i];
    return order;
}
static void save(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<sizes[r];++i)
        expected[n++]=((volatile uint8_t *)starts[r])[i];
    for(i=0;i<48;++i)((uint32_t *)expected_events)[i]=((uint32_t *)events)[i];
}
static int compare(void)
{
    uint32_t r,i,n=0;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<sizes[r];++i)
        if(expected[n++]!=((volatile uint8_t *)starts[r])[i])return 0;
    for(i=0;i<48;++i)if(((uint32_t *)expected_events)[i]!=((uint32_t *)events)[i])return 0;
    for(i=0;i<6;++i)if(expected_arguments[i]!=composite_arguments_after[i])return 0;
    return 1;
}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    return 0x817cd0;
#else
    return (uint32_t)kernel_surface_target_817cd0;
#endif
}
static int exercise(void)
{
    uint32_t phase,result,expected_return,expected_calls;
    initialize();expected_return=oracle();save();expected_calls=calls;
    for(phase=0;phase<2;++phase){
        initialize();result=invoke(phase?candidate():0x917cd0);
        if(failure||result!=expected_return||calls!=expected_calls)return 1+phase;
        if(!compare())return 3+phase;
    }
    ++cases;return 0;
}
static int run(void)
{
    uint32_t r,error;
    for(r=0;r<COUNT(starts);++r)if(!map(starts[r],sizes[r]))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
#endif
    for(pattern=0;pattern<2;++pattern)for(alignment=0;alignment<4;++alignment)
    for(wrap=0;wrap<4;++wrap)for(rect=0;rect<COUNT(rectangles);++rect)
    for(screen_case=0;screen_case<COUNT(screens);++screen_case)
    for(list_case=0;list_case<COUNT(lists);++list_case)for(mode=0;mode<8;++mode){
        layer=5+wrap*0x40000000u;screen=0x600000+alignment;object=0x600200+((alignment+1)&3);
        error=exercise();if(error){PRINT("Surface composite failure: ");number(error);PRINT(" at case ");number(cases);PRINT("\n");return 10+error;}
    }
    if(coverage!=127)return 20;
    PRINT("Surface composite original/C/oracle cases: ");number(cases);
    PRINT("; signed clipping, caller stores, wrapped order/list indices, live bounds, cached callback and full memory verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
