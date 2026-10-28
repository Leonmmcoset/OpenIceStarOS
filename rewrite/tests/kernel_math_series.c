#include <stdint.h>
#include "../kernel/math_series.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
struct result { uint8_t extended[10],padding[2]; uint32_t bits[2]; uint16_t status,cw; };
extern unsigned math_checked_call(uint32_t,const uint32_t *,struct result *);
static unsigned cases,real_service_cases,executions;
static uint32_t expected_table[320];
static unsigned function_index,precision,rounding;
uint32_t math_argument_stack;
static uint32_t current_args[4];
static int sys3(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static void print(const char *text)
{
    unsigned n = 0;
    while (text[n]) ++n;
    sys3(4, 1, (unsigned)text, n);
}
static void number(uint32_t n)
{
    char text[12];
    unsigned i = sizeof(text);
    text[--i] = 0;
    do { text[--i] = (char)('0' + n % 10); n /= 10; } while (n);
    print(text + i);
}
static void fail(const char *text)
{
    print(text); print("\n"); sys3(1, 200, 0, 0);
}
static int map(uint32_t address, unsigned size)
{
    uint32_t args[] = {address, size, 7, 0x32, 0xffffffff, 0};
    return sys3(90, (unsigned)args, 0, 0) == (int)address;
}
static int read_file(const char *path,uint32_t base,unsigned size)
{
    int fd=sys3(5,(unsigned)path,0,0),n;
    if(fd<0) return 0;
    n=sys3(3,(unsigned)fd,base,size);
    sys3(6,(unsigned)fd,0,0);
    return n==(int)size;
}
static uint32_t functions[3][4]={
    {0x81a2aa,0x81a538,0x81a2dc,0x81a366},
    {(uint32_t)kernel_factorial_81a2aa,(uint32_t)kernel_power_81a538,
     (uint32_t)kernel_sine_81a2dc,(uint32_t)kernel_cosine_81a366},
    {0x81a2aa,0x81a538,0x81a2dc,0x81a366}
};
static int same(const struct result *a,const struct result *b)
{
    unsigned i;
    for(i=0;i<10;++i)if(a->extended[i]!=b->extended[i])return 0;
    return a->bits[0]==b->bits[0]&&a->bits[1]==b->bits[1]&&
        (a->status&0x3f)==(b->status&0x3f)&&a->cw==b->cw;
}
static void output(const struct result *r)
{
    number(r->bits[1]);print("/");number(r->bits[0]);print(" flags ");number(r->status&0x3f);
}
static int execute(unsigned fn,const uint32_t *args,unsigned chain)
{
    struct result expected={0},actual={0};
    static const unsigned pcs[]={0,0x200,0x300};
    uint16_t cw=(uint16_t)(0x7f|pcs[precision]|rounding*0x400);
    unsigned copy,i;
    function_index=fn;
    for(i=0;i<4;++i) current_args[i]=args[i];
    for(copy=0;copy<3;++copy) {
        if(chain&&copy==1)continue;
        if(copy==0&&!read_file("build/math_series_original.x",0x800000,250099))fail("original kernel read");
        if(copy==2&&!read_file("build/math_series_candidate.x",0x800000,250099))fail("candidate kernel read");
        uint32_t target=functions[copy][fn];
        if(chain) {
            /* Actual service initializer, including all original target
               addresses. Invoke slot 244/245 with no rewritten CALL. */
            WORD(0x1b0e4)=0x8300000;
            WORD(0x1b034)=0x1c000;WORD(0x1c024)=args[0]&1?8:32;
            WORD(0x1a868)=args[0]&2?1u<<25:0;
            for(i=0;i<320;++i)WORD(0x8300000+i*4)=i*0x87654321u;
            ((void (*)(void))0x8005aa)();
            unsigned slot=fn==2?244:245;
            if(WORD(WORD(0x1b0e4)+slot*4)!=functions[0][fn])fail("math service target");
            target=WORD(WORD(0x1b0e4)+slot*4);
            for(i=0;i<320;++i) {
                if(!copy)expected_table[i]=WORD(0x8300000+i*4);
                else if(expected_table[i]!=WORD(0x8300000+i*4))fail("math complete service table");
            }
        }
        __asm__ volatile("fninit; fldcw %0"::"m"(cw));
        ++executions;
        if(math_checked_call(target,args,&actual))fail("math cdecl/stack ABI");
        if(actual.cw!=cw||(actual.status&0x3800)!=0x3800)fail("math x87 CW/return stack");
        for(i=0;i<4;++i)if(WORD(math_argument_stack+i*4)!=args[i])fail("math incoming arguments changed");
        if(!copy)expected=actual;
        else if(!same(&expected,&actual)) {
            print("copy ");number(copy);print(" expected ");output(&expected);
            print(" actual ");output(&actual);return 0;
        }
    }
    ++cases;if(chain)++real_service_cases;return 1;
}
static uint32_t rng_state=0x12345678;
static uint32_t random_word(void)
{
    rng_state^=rng_state<<13;rng_state^=rng_state>>17;rng_state^=rng_state<<5;return rng_state;
}
static int run(void)
{
    static const uint32_t factorials[]={0,1,2,3,10,19,20,21,33,100,170,171,172,255,512,
                                      0x80000000,0xffffffff};
    static const uint32_t values[][2]={
        {0,0},{0,0x80000000},{1,0},{1,0x80000000},{0xffffffff,0xfffff},
        {0,0x100000},{0xffffffff,0x3fefffff},{0,0x3ff00000},{1,0x3ff00000},
        {0,0xbff00000},{0,0x3fe00000},{0,0xbfe00000},{0x54442d18,0x400921fb},
        {0x54442d18,0x401921fb},{0,0x40240000},{0,0xc0240000},{0,0x40590000},
        {0,0xc0590000},{0xffffffff,0x7fefffff},{0xffffffff,0xffefffff},
        {0,0x7ff00000},{0,0xfff00000},{0x12345678,0x7ff81234},
        {0x87654321,0xfff85678},{1,0x7ff00000},{1,0xfff00000}
    };
    static const double powers[]={-20,-19.5,-3,-2.5,-2,-1.5,-1,-0.5,-0.0,0.0,
                                   0.1,0.5,1,1.1,1.5,2,2.5,3,7,19,20,31};
    unsigned i,j,fn,alignment;
    static const unsigned offsets[]={0,1,5,12};
    uint32_t args[4]={0};
    if(!map(0x10000,0x20000)||!map(0x670000,0x10000)||!map(0x800000,0x40000)||
       !map(0x8300000,0x10000))return 1;
    /* The actual initializer writes its service table at 0x8300000. */
    for(alignment=0;alignment<4;++alignment) {
    math_argument_stack=0x67ff80+offsets[alignment];
    for(precision=0;precision<3;++precision)for(rounding=0;rounding<4;++rounding) {
        for(i=0;i<sizeof(factorials)/4;++i) {
            args[0]=factorials[i];args[1]=args[2]=args[3]=0;
            if(!execute(0,args,0))goto mismatch;
        }
        for(i=0;i<sizeof(values)/sizeof(values[0]);++i) {
            args[0]=values[i][0];args[1]=values[i][1];
            for(fn=2;fn<4;++fn)if(!execute(fn,args,0))goto mismatch;
            for(j=0;j<sizeof(powers)/sizeof(powers[0]);++j) {
                union {double d;uint32_t w[2];} e={powers[j]};
                args[2]=e.w[0];args[3]=e.w[1];
                if(!execute(1,args,0))goto mismatch;
            }
            /* QNaN and SNaN exponents terminate with the original -1 result.
               Infinite/huge exponents are deliberately not executed: their
               original loops cannot converge within the fixture's budget. */
            args[2]=0x12345678;args[3]=0x7ff81234;if(!execute(1,args,0))goto mismatch;
            args[2]=1;args[3]=0x7ff00000;if(!execute(1,args,0))goto mismatch;
        }
        for(i=0;i<512;++i) {
            args[0]=random_word();args[1]=random_word();args[2]=0;args[3]=0;
            for(fn=2;fn<4;++fn)if(!execute(fn,args,0))goto mismatch;
            union {double d;uint32_t w[2];} e={(int32_t)(random_word()%161)-80.25};
            args[2]=e.w[0];args[3]=e.w[1];
            if(!execute(1,args,0))goto mismatch;
        }
        for(i=0;i<120;++i) {
            union {double d;uint32_t w[2];} angle={i*0.05235987755982988};
            args[0]=angle.w[0];args[1]=angle.w[1];args[2]=args[3]=0;
            for(fn=2;fn<4;++fn)if(!execute(fn,args,1))goto mismatch;
        }
    }
    }
    print("x87 math series verified: ");number(cases);print(" scenarios / ");number(executions);
    print(" executions, ");number(real_service_cases);print(" service scenarios\n");return 0;
mismatch:
    print("\nmath mismatch function ");number(function_index);print(" precision ");number(precision);
    print(" rounding ");number(rounding);print(" args ");
    for(i=0;i<4;++i){number(current_args[i]);print(" ");}print("\n");return 10;
}
void _start(void){sys3(1,(unsigned)run(),0,0);__builtin_unreachable();}
