#include <stdint.h>
#include "../kernel/math_extra.h"
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
static uint32_t functions[3][7]={
    {0x81a3f6,0x81a4fc,0x81a604,0x81a9ee,0x81aa52,0x81aab8,0x81ab2a},
    {(uint32_t)kernel_acos_81a3f6,(uint32_t)kernel_round_81a4fc,
     (uint32_t)kernel_absolute_81a604,(uint32_t)kernel_floor_81a9ee,
     (uint32_t)kernel_ceil_81aa52,(uint32_t)kernel_remainder_81aab8,
     (uint32_t)kernel_sqrt_81ab2a},
    {0x81a3f6,0x81a4fc,0x81a604,0x81a9ee,0x81aa52,0x81aab8,0x81ab2a}
};
static const unsigned service_slots[]={246,247,249,250,251,253,252};
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
        if(copy==0&&!read_file("build/math_extra_original.x",0x800000,250099))fail("original kernel read");
        if(copy==2&&!read_file("build/math_extra_candidate.x",0x800000,250099))fail("candidate kernel read");
        uint32_t target=functions[copy][fn];
        if(chain) {
            /* Actual service initializer, including all original target
               addresses. Invoke the requested slot with no rewritten CALL. */
            WORD(0x1b0e4)=0x8300000;
            WORD(0x1b034)=0x1c000;WORD(0x1c024)=args[0]&1?8:32;
            WORD(0x1a868)=args[0]&2?1u<<25:0;
            for(i=0;i<320;++i)WORD(0x8300000+i*4)=i*0x87654321u;
            ((void (*)(void))0x8005aa)();
            unsigned slot=service_slots[fn];
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
    static const uint32_t values[][2]={
        {0,0},{0,0x80000000},{1,0},{1,0x80000000},{0xffffffff,0xfffff},
        {0,0x100000},{0xffffffff,0x3fefffff},{0,0x3ff00000},{1,0x3ff00000},
        {0,0xbff00000},{0,0x3fe00000},{0,0xbfe00000},{0x54442d18,0x400921fb},
        {0x54442d18,0x401921fb},{0,0x40240000},{0,0xc0240000},{0,0x40590000},
        {0,0xc0590000},{0xffffffff,0x7fefffff},{0xffffffff,0xffefffff},
        {0,0x7ff00000},{0,0xfff00000},{0x12345678,0x7ff81234},
        {0x87654321,0xfff85678},{1,0x7ff00000},{1,0xfff00000},
        {0x80000000,0x41dfffff},{0,0x41e00000},{1,0x41e00000},
        {0,0xc1e00000},{1,0xc1e00000},{0xffffffff,0xc1dfffff},
        {0xffffffff,0xbfefffff},{1,0xbff00000},{0,0x3ff80000},{0,0xbff80000},
        {0,0x43300000},{0,0xc3300000}
    };
    static const double edges[]={-2147483649.0,-2147483648.5,-2147483648.0,
        -2147483647.5,-2.5,-2.0,-1.5,-1.0,-0.5,0,0.5,1.0,1.5,2.0,2.5,
        2147483646.5,2147483647.0,2147483647.5,2147483648.0,2147483648.5};
    static const unsigned offsets[]={0,1,5,12};
    unsigned i,j,fn,alignment;
    uint32_t args[4]={0};
    if(!map(0x10000,0x20000)||!map(0x670000,0x10000)||!map(0x800000,0x40000)||
       !map(0x8300000,0x10000))return 1;
    for(alignment=0;alignment<4;++alignment) {
        math_argument_stack=0x67ff80+offsets[alignment];
        for(precision=0;precision<3;++precision)for(rounding=0;rounding<4;++rounding) {
            for(i=0;i<sizeof(values)/sizeof(values[0]);++i) {
                args[0]=values[i][0];args[1]=values[i][1];args[2]=args[3]=0;
                for(fn=0;fn<7;++fn)if(!execute(fn,args,0))goto mismatch;
                for(j=0;j<sizeof(values)/sizeof(values[0]);++j) {
                    args[2]=values[j][0];args[3]=values[j][1];
                    if(!execute(5,args,0))goto mismatch;
                }
            }
            for(i=0;i<sizeof(edges)/sizeof(edges[0]);++i) {
                union {double d;uint64_t bits;} u={edges[i]};
                for(j=0;j<3;++j) {
                    uint64_t bits=u.bits+j-1;
                    args[0]=(uint32_t)bits;args[1]=(uint32_t)(bits>>32);args[2]=0;args[3]=0x3ff80000;
                    for(fn=0;fn<7;++fn)if(!execute(fn,args,0))goto mismatch;
                }
            }
            for(i=0;i<512;++i) {
                for(j=0;j<4;++j)args[j]=random_word();
                for(fn=0;fn<7;++fn)if(!execute(fn,args,0))goto mismatch;
            }
            /* In-domain random acos values; most unconstrained binary64
               values would take only its invalid-domain branch. */
            for(i=0;i<128;++i) {
                union {double d;uint32_t w[2];} u={(int32_t)random_word()/2147483648.0};
                args[0]=u.w[0];args[1]=u.w[1];args[2]=0;args[3]=0x3ff00000;
                if(!execute(0,args,0))goto mismatch;
                for(fn=0;fn<7;++fn)if(!execute(fn,args,1))goto mismatch;
            }
        }
    }
    print("x87 extra math verified: ");number(cases);print(" scenarios / ");number(executions);
    print(" executions, ");number(real_service_cases);print(" service scenarios\n");return 0;
mismatch:
    print("\nmath mismatch function ");number(function_index);print(" precision ");number(precision);
    print(" rounding ");number(rounding);print(" args ");
    for(i=0;i<4;++i){number(current_args[i]);print(" ");}print("\n");return 10;
}
void _start(void){sys3(1,(unsigned)run(),0,0);__builtin_unreachable();}
