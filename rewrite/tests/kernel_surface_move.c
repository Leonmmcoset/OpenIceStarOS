/* Compare original x86, recovered C and a separate damage-rectangle oracle.
 * Compositor/predicate/presentation boundaries use observable callbacks. */
#include "../kernel/surface_move.h"
#include "../kernel/surface_composite.h"
#include "../kernel/surface_present.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t starts[]={0x1b000,0x500000,0x600000,0x610000};
static const uint32_t active_values[]={0,1,0xffffffff,0x80000000,0x7fffffff};
static const uint32_t predicate_results[]={0,0,1,2,0xffffffff};
static const uint32_t geometries[][6]={
    {20,30,40,50,25,37},{20,30,40,50,15,37},
    {20,30,40,50,25,23},{20,30,40,50,15,23},
    {20,30,40,50,20,30},{20,30,40,50,20,37},
    {20,30,40,50,25,30},{20,30,40,50,60,80},
    {20,30,40,50,200,300},{20,30,40,50,200,20},
    {0,0,0,0,0,0},{0,0,0xffffffff,0x80000000,1,1},
    {0x7fffffff,0x7fffffff,1,1,0x80000000,0x80000000},
    {0x80000000,0x80000000,0x7fffffff,0xffffffff,0x7fffffff,0x7fffffff},
    {1,2,0x80000000,0x7fffffff,0x80000001,0x80000002},
    {0xffffffff,0xffffffff,0xffffffff,0xffffffff,0,0}};
static uint32_t input_words[7],geometry[6],events[8][9],expected_events[8][9];
static uint32_t expected_arguments[8],expected_return,expected_calls;
static uint8_t expected[0x4000];
static uint32_t calls,failure,cases,coverage,layout,style,active_case,mode,geo;
static uint32_t layer,object;
volatile uint32_t *move_caller_arguments;
uint32_t move_arguments_after[8];
static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *path,uint32_t address)
{int fd=syscall3(5,(uint32_t)path,0,0),n;if(fd<0)return 0;n=syscall3(3,(uint32_t)fd,address,0x40000);syscall3(6,(uint32_t)fd,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int64_t signed_word(uint32_t n)
{return n<0x80000000u?(int64_t)n:(int64_t)n-INT64_C(4294967296);}
static uint32_t natural_predicate(const uint32_t *a)
{
    return (signed_word(a[0]+a[2])>signed_word(a[4]) ||
            signed_word(a[1]+a[3])>signed_word(a[5])) &&
           (signed_word(a[4]+a[6])>signed_word(a[0]) ||
            signed_word(a[5]+a[7])>signed_word(a[1]));
}
static void effects(void)
{
    if(mode&2){
        move_caller_arguments[0]^=0x40000000;
        move_caller_arguments[1]+=calls*7;
        move_caller_arguments[2]^=0x80000000|calls;
        move_caller_arguments[3]-=calls*3;
        move_caller_arguments[4]+=calls*11;
        move_caller_arguments[5]^=0x80000001;
        move_caller_arguments[6]-=calls*5;
    }
    if((mode&4)&&calls==1){
        WORD(0x1b0e4)=0xdeadbeef;WORD(0x1b138)=0x76543210;
        WORD(0x610001+44)=0xdeadbeef;
        WORD(0x500000+4*layer)=0xdeadbeef;
        WORD(object+40)^=0x80000000;
    }
    WORD(0x1b300)^=calls*0x132457;
}
__attribute__((used,noinline)) static uint32_t observe(uint32_t kind,volatile uint32_t *a)
{
    uint32_t i,n=kind==1?5:(kind==2?8:4),result;
    if(calls>=8){failure=1;return 0;}
    events[calls][0]=kind;
    for(i=0;i<n;++i)events[calls][i+1]=a[i];
    result=kind==2?(mode<8?natural_predicate(events[calls]+1):
        predicate_results[mode>>3]):(0xcdef0000|(kind<<8)|(calls+1));
    if(mode&1)for(i=0;i<n;++i)a[i]=0xa5a50000|i;
    ++calls;effects();return result;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked,noinline)) uint32_t kernel_surface_target_817cd0(
    uint32_t l,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $1;call observe;addl $8,%esp;ret");}
__attribute__((naked,noinline)) uint32_t kernel_surface_regions_test(
    uint32_t ax,uint32_t ay,uint32_t aw,uint32_t ah,
    uint32_t bx,uint32_t by,uint32_t bw,uint32_t bh)
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $2;call observe;addl $8,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t present(uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{__asm__("leal 4(%esp),%eax;pushl %eax;pushl $3;call observe;addl $8,%esp;ret");}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target)
{
    __asm__("pushl %ebp;movl %esp,%ebp;subl $40,%esp;xorl %ecx,%ecx;"
        "1:movl input_words(,%ecx,4),%edx;movl %edx,(%esp,%ecx,4);"
        "incl %ecx;cmpl $7,%ecx;jne 1b;"
        "movl $0x93fedc71,28(%esp);movl %esp,move_caller_arguments;"
        "call *8(%ebp);xorl %ecx,%ecx;"
        "2:movl (%esp,%ecx,4),%edx;movl %edx,move_arguments_after(,%ecx,4);"
        "incl %ecx;cmpl $8,%ecx;jne 2b;leave;ret");
}
#pragma GCC diagnostic pop
static void initialize(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)
        ((volatile uint8_t *)starts[r])[i]=(uint8_t)(i*37+geo*83+r*59);
    WORD(0x1b0e4)=0x610001;WORD(0x610001+44)=(uint32_t)present;
    WORD(0x1b138)=active_values[active_case];
    WORD(0x500000+4*layer)=object;
    WORD(object+40)=style?0x80000001:0x7fffffff;
    input_words[0]=layer;for(i=0;i<6;++i)input_words[i+1]=geometry[i];
    calls=failure=0;for(i=0;i<72;++i)((uint32_t *)events)[i]=0;
}
static uint32_t emit(uint32_t kind,uint32_t a,uint32_t b,uint32_t c,
    uint32_t d,uint32_t e,uint32_t f,uint32_t g,uint32_t h)
{uint32_t words[]={a,b,c,d,e,f,g,h};return observe(kind,words);}
static uint32_t draw(uint32_t l,uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{return emit(1,l,x,y,w,h,0,0,0);}
static uint32_t show(uint32_t x,uint32_t y,uint32_t w,uint32_t h)
{return emit(3,x,y,w,h,0,0,0,0);}
static void oracle(void)
{
    uint32_t a[8],i,dx,dy,lower,positive_x,positive_y,x,y,w,h,result;
    for(i=0;i<7;++i)a[i]=input_words[i];
    a[7]=0x93fedc71;
    move_caller_arguments=a;
    dx=geometry[4]-geometry[0];dy=geometry[5]-geometry[1];
    positive_x=signed_word(dx)>0;positive_y=signed_word(dy)>0;
    lower=signed_word(active_values[active_case])>0?active_values[active_case]:2;
    coverage|=style?1:2;
    draw(style?layer:lower,a[5],a[6],a[3],a[4]);
    result=emit(2,a[1],a[2],a[3],a[4],a[5],a[6],a[3],a[4]);
    if(result==1){
        coverage|=4u<<(positive_x+2*positive_y);
        draw(lower,a[1],positive_y?a[2]:a[6]+a[4],a[3],positive_y?dy:0u-dy);
        y=positive_y?a[2]:a[6];h=positive_y?a[4]+dy:a[4]-dy;
        draw(lower,positive_x?a[1]:a[5]+a[3],positive_y?a[6]:a[2],
            positive_x?dx:0u-dx,positive_y?a[4]-dy:a[4]+dy);
        x=positive_x?a[1]:a[5];w=positive_x?a[3]+dx:a[3]-dx;
        result=show(x,y,w,h);
    }else{
        coverage|=64;
        draw(lower,a[1],a[2],a[3],a[4]);
        show(a[5],a[6],a[3],a[4]);result=show(a[1],a[2],a[3],a[4]);
    }
    expected_return=result;expected_calls=calls;
    for(i=0;i<8;++i)expected_arguments[i]=a[i];
}
static void save(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)
        expected[r*0x1000+i]=((volatile uint8_t *)starts[r])[i];
    for(i=0;i<72;++i)((uint32_t *)expected_events)[i]=((uint32_t *)events)[i];
}
static int compare(void)
{
    uint32_t r,i;
    for(r=0;r<COUNT(starts);++r)for(i=0;i<0x1000;++i)
        if(expected[r*0x1000+i]!=((volatile uint8_t *)starts[r])[i])return 0;
    for(i=0;i<72;++i)if(((uint32_t *)expected_events)[i]!=((uint32_t *)events)[i])return 0;
    for(i=0;i<8;++i)if(expected_arguments[i]!=move_arguments_after[i])return 0;
    return 1;
}
static void trampoline(uint32_t address,uint32_t target)
{*(volatile uint8_t *)(uintptr_t)address=0xe9;WORD(address+1)=target-address-5;}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    return 0x8191d8;
#else
    return (uint32_t)kernel_surface_move_refresh;
#endif
}
static int exercise(void)
{
    uint32_t phase,result;
    initialize();oracle();save();
    for(phase=0;phase<2;++phase){
        initialize();result=invoke(phase?candidate():0x9191d8);
        if(failure||result!=expected_return||calls!=expected_calls)return 1+phase;
        if(!compare())return 3+phase;
    }
    ++cases;return 0;
}
static int run(void)
{
    uint32_t r,i,error,random=0x13245768;
    for(r=0;r<COUNT(starts);++r)if(!map(starts[r],0x1000))return 1;
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
    trampoline(0x917cd0,(uint32_t)kernel_surface_target_817cd0);
    trampoline(0x918d14,(uint32_t)kernel_surface_regions_test);
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/icestar_c.x",0x800000))return 3;
    trampoline(0x817cd0,(uint32_t)kernel_surface_target_817cd0);
    trampoline(0x818d14,(uint32_t)kernel_surface_regions_test);
#endif
    for(geo=0;geo<COUNT(geometries)+16;++geo){
        for(i=0;i<6;++i){
            random=random*1664525u+1013904223u;
            geometry[i]=geo<COUNT(geometries)?geometries[geo][i]:
                ((geo&1)?random:(random%257u)-128u);
        }
        for(layout=0;layout<4;++layout)for(style=0;style<2;++style)
        for(active_case=0;active_case<COUNT(active_values);++active_case)
        for(mode=0;mode<40;++mode){
            layer=5+layout*0x40000000u;object=0x600000+layout;
            error=exercise();if(error){PRINT("Surface move failure: ");number(error);
                PRINT(" at case ");number(cases);PRINT("\n");return 10+error;}
        }
    }
    if(coverage!=127)return 20;
    PRINT("Surface move original/C/oracle cases: ");number(cases);
    PRINT("; signed/wrapped geometry, four damage quadrants, exact predicate return, cached selection/callback, live caller mutation, outgoing isolation, raw returns and full memory verified\n");
    return 0;
}
void _start(void){syscall3(1,run(),0,0);__builtin_unreachable();}
