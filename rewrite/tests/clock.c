#include <stdint.h>
#include "../apps/clock.h"
#include "clock_bindings.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
enum { RECORD0=0x700003, RECORD1=0x701005, TABLE0=0x730003,
       TABLE1=0x731005, PALETTE0=0x720003, PALETTE1=0x721005, MAX_CALLS=512 };
struct call { uint32_t slot,table,args[8],state[4],cw; };
static struct call calls[MAX_CALLS],expected_calls[MAX_CALLS];
static unsigned call_count,expected_count,polls,mode,seed,cases,function_index,format_index;
static uint32_t active_base,active_close,window_result,time_value,initial_cw;
static volatile struct clock_state *state;
static uint8_t expected_memory[0x4040];
extern uint32_t clock_invoke(uint32_t,const uint32_t *,uint32_t);
extern void clock_finish(void);
extern uint32_t clock_bad_abi,clock_result;
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
static int load(const char *path,uint32_t base)
{
    uint32_t *h=(uint32_t *)base;
    unsigned i;
    if(!read_file(path,base,0x3cd8)) return 0;
    for(i=0;i<h[8];++i) WORD(base+WORD(base+h[9]+i*4))+=base;
    h[7]=base;
    for(i=h[9];i<h[9]+h[8]*4+h[11]+h[3]*4096;++i) ((uint8_t *)base)[i]=0;
    return 1;
}
static uint32_t normalized(uint32_t value)
{
    return value>=active_base && value<active_base+0x3c70 ? value-active_base : value;
}
static void set_cw(uint32_t value)
{
    uint16_t cw=(uint16_t)value;
    __asm__ volatile("fldcw %0"::"m"(cw));
}
static struct call *record(unsigned slot,unsigned table,unsigned n,const uint32_t *args)
{
    struct call *c;
    uint16_t cw;
    unsigned i;
    if(call_count>=MAX_CALLS) fail("CLOCK trace overflow");
    c=&calls[call_count++];
    c->slot=slot;c->table=table;
    for(i=0;i<8;++i) c->args[i]=i<n?args[i]:0;
    if(slot==7||slot==8) c->args[5]=normalized(c->args[5]);
    if(slot==100) c->args[7]=normalized(c->args[7]);
    for(i=0;i<4;++i) c->state[i]=((volatile uint32_t *)state)[i];
    __asm__ volatile("fnstcw %0":"=m"(cw)); c->cw=cw;
    if(mode&2) WORD(0x1b0e4)=call_count&1?TABLE1:TABLE0;
    if(mode&1) {
        state->record=call_count&1?RECORD1:RECORD0;
        state->surface^=call_count*0x1234567u;
        state->window^=slot*0xabcdefu;
        WORD(0x1b0e0)=call_count&1?PALETTE1:PALETTE0;
        WORD(WORD(0x1b0e0)+4)^=call_count*0x87654321u;
        WORD(WORD(0x1b0e0)+32)+=slot*0x31415927u;
    }
    if((mode&8) && function_index==6) {
        WORD(0x66ff98)^=0x87654321; /* first live vararg */
        WORD(0x66ff8c)+=slot;      /* live foreground */
        WORD(0x66ff90)^=call_count;
    }
    if((mode&8) && (slot==244||slot==245)) set_cw(cw^0x400);
    return c;
}
static uint32_t six(unsigned slot,unsigned table,uint32_t a,uint32_t b,uint32_t c,
                    uint32_t d,uint32_t e,uint32_t f)
{
    const uint32_t args[]={a,b,c,d,e,f};
    record(slot,table,6,args);
    return slot^seed*0x98765432u^call_count;
}
static uint32_t seven(unsigned slot,unsigned table,uint32_t a,uint32_t b,uint32_t c,
                      uint32_t d,uint32_t e,uint32_t f,uint32_t g)
{
    const uint32_t args[]={a,b,c,d,e,f,g};
    record(slot,table,7,args);
    return slot^seed*0xfedcba98u^call_count;
}
static uint32_t create(unsigned table,uint32_t a,uint32_t b,uint32_t c,uint32_t d,
                       uint32_t e,uint32_t f,uint32_t g,const char *title)
{
    const uint32_t args[]={a,b,c,d,e,f,g,(uint32_t)title};
    record(100,table,8,args);
    for(unsigned i=0;i<5;++i)
        if((uint8_t)title[i] != ((const uint8_t *)"\xca\xb1\xd6\xd3")[i]) fail("CLOCK title");
    if(mode&1) WORD(0x6a0000u+window_result*4u)=RECORD1;
    WORD(0x1b004)=time_value^0x1234;
    return window_result;
}
static uint32_t one(unsigned slot,unsigned table,uint32_t value)
{
    if(slot==105) {
        uint32_t rec=WORD(0x6a0000u+window_result*4u);
        if(WORD(rec+0x5b7)!=active_base+active_close) fail("CLOCK close registration");
    }
    record(slot,table,1,&value);
    return value^seed*0xfedcba98u^table;
}
static uint32_t poll(unsigned table)
{
    static const uint32_t times[]={0,0,0x012359,0x012359,0x120000,0xff235959};
    record(108,table,0,0);
    if(++polls==6) clock_finish();
    WORD(0x1b004)=times[polls];
    return 0x87654321;
}
static uint32_t digits(unsigned table,uint32_t base,uint32_t value)
{
    const uint32_t args[]={base,value};
    record(242,table,2,args);
    if(function_index==6 && seed%3==0) return 0;
    unsigned n=1;while(value>=base){value/=base;++n;}
    return n;
}
static double trig(unsigned slot,unsigned table,double angle)
{
    union { double d; uint32_t w[2]; } u={angle};
    static const double values[]={-1.0,-0.5,-0.3333333333333333,-0.0,
                                  0.0,0.3333333333333333,0.5,1.0,0.9999999999999999};
    record(slot,table,2,u.w);
    if(mode&16) return ((double (*)(double))(slot==244?0x81a2dc:0x81a366))(angle);
    return values[(seed+call_count)%9];
}
static void alter(volatile uint32_t *p){if(mode&4)*p^=0x87654321;}
#define SIX(name,slot,t) \
static uint32_t name##t(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f) \
{uint32_t r=six(slot,t,a,b,c,d,e,f);alter(&a);return r;}
#define SEVEN(name,slot,t) \
static uint32_t name##t(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f,uint32_t g) \
{uint32_t r=seven(slot,t,a,b,c,d,e,f,g);alter(&a);return r;}
#define ONE(name,slot,t) \
static uint32_t name##t(uint32_t a){uint32_t r=one(slot,t,a);alter(&a);return r;}
#define TABLE(t) \
SIX(rectangle,2,t) SIX(character,6,t) SIX(chinese,7,t) SIX(string,8,t) SIX(circle,240,t) \
SEVEN(number_draw,9,t) SEVEN(line,239,t) \
ONE(show,105,t) ONE(terminate,107,t) ONE(refresh,110,t) ONE(surface,111,t) \
static uint32_t create##t(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f,uint32_t g,const char *title) \
{uint32_t r=create(t,a,b,c,d,e,f,g,title);alter(&a);return r;} \
static uint32_t poll##t(void){return poll(t);} \
static uint32_t digits##t(uint32_t a,uint32_t b){uint32_t r=digits(t,a,b);alter(&a);return r;} \
static double sine##t(double a){return trig(244,t,a);} \
static double cosine##t(double a){return trig(245,t,a);}
TABLE(0) TABLE(1)
static void initialize(uint32_t base,const unsigned *offsets)
{
    static const unsigned slots[]={2,6,7,8,9,100,105,107,108,110,111,239,240,242,244,245};
    const uint32_t funcs[2][16]={
        {(uint32_t)rectangle0,(uint32_t)character0,(uint32_t)chinese0,(uint32_t)string0,
         (uint32_t)number_draw0,(uint32_t)create0,(uint32_t)show0,(uint32_t)terminate0,
         (uint32_t)poll0,(uint32_t)refresh0,(uint32_t)surface0,(uint32_t)line0,
         (uint32_t)circle0,(uint32_t)digits0,(uint32_t)sine0,(uint32_t)cosine0},
        {(uint32_t)rectangle1,(uint32_t)character1,(uint32_t)chinese1,(uint32_t)string1,
         (uint32_t)number_draw1,(uint32_t)create1,(uint32_t)show1,(uint32_t)terminate1,
         (uint32_t)poll1,(uint32_t)refresh1,(uint32_t)surface1,(uint32_t)line1,
         (uint32_t)circle1,(uint32_t)digits1,(uint32_t)sine1,(uint32_t)cosine1}};
    unsigned i;
    active_base=base;active_close=offsets[1];
    state=(volatile struct clock_state *)(base+0x3c70);
    state->record=RECORD0;state->window=seed*0x98765432u;
    state->surface=seed*0x12345678u;state->unused=seed^0xdeadbeef;
    for(i=0;i<0x2000;++i) {
        ((volatile uint8_t *)0x700000)[i]=(uint8_t)(i*37+seed*59);
        ((volatile uint8_t *)0x720000)[i]=(uint8_t)(i*41+seed*61);
    }
    WORD(RECORD0)=window_result^0x12345678;WORD(RECORD1)=window_result^0xabcdef01;
    WORD(RECORD0+4)=seed*0x31415927u;WORD(RECORD1+4)=seed*0xabcdef98u;
    WORD(0x6a0000u+window_result*4u)=RECORD0;
    for(i=0;i<16;++i) {
        WORD(TABLE0+slots[i]*4)=funcs[0][i];WORD(TABLE1+slots[i]*4)=funcs[1][i];
    }
    WORD(0x1b0e4)=TABLE0;WORD(0x1b0e0)=PALETTE0;WORD(0x1b004)=time_value;
    call_count=polls=clock_bad_abi=clock_result=0;
    __asm__ volatile("fninit");set_cw(initial_cw);
}
static int snapshot(unsigned save)
{
    const uint32_t starts[]={active_base+0x3c70,0x700000,0x720000,
                              0x1b0e4,0x1b0e0,0x1b004,0x6a0000u+window_result*4u};
    const unsigned sizes[]={16,0x2000,0x2000,4,4,4,4};
    unsigned i,j,offset=0;
    for(i=0;i<2;++i) {
        uint32_t rec=i?RECORD1:RECORD0;
        if(WORD(rec+0x5b7)==active_base+active_close) WORD(rec+0x5b7)=0xc020c020;
    }
    if(save) {
        expected_count=call_count;
        for(i=0;i<call_count;++i) expected_calls[i]=calls[i];
    } else {
        if(expected_count!=call_count) {print("trace count ");number(expected_count);print("/");number(call_count);return 0;}
        for(i=0;i<call_count;++i)
            for(j=0;j<sizeof(struct call)/4;++j)
                if(((uint32_t *)&calls[i])[j]!=((uint32_t *)&expected_calls[i])[j]) {
                    print("trace ");number(i);print(" field ");number(j);print(" expected ");
                    number(((uint32_t *)&expected_calls[i])[j]);print(" actual ");number(((uint32_t *)&calls[i])[j]);return 0;
                }
    }
    for(i=0;i<7;++i)for(j=0;j<sizes[i];++j,++offset) {
        uint8_t byte=*(volatile uint8_t *)(starts[i]+j);
        if(save)expected_memory[offset]=byte;
        else if(expected_memory[offset]!=byte){print("memory ");number(starts[i]+j);return 0;}
    }
    return 1;
}
static const char *const formats[]={
    "", "12", "%d", "%D:%u:%U:%x:%X", "%c%C", "%s%S", "%2d %9D %3x %8X",
    "%1d %0d %2u %% %q %", "a\rB\nC", "\x80\x9f\xa0\xa1\xca\xb1", "%9dZ", "%9xZ"
};
static int exercise(unsigned fn,uint32_t a,uint32_t b,uint32_t value)
{
    uint32_t expected_result=0;
    uint16_t expected_cw=0;
    function_index=fn;
    for(unsigned copy=0;copy<2;++copy) {
        const unsigned *offsets=copy?rewritten_functions:original_functions;
        uint32_t base=copy?b:a,args[12]={0};
        initialize(base,offsets);
        args[0]=value;
        if(fn==4) {
            union {double d;uint32_t w[2];} angle={(int32_t)(value%1000u)*0.010471975511965977};
            args[0]=angle.w[0];args[1]=angle.w[1];args[2]=seed%2?0x80000000u:135;args[3]=seed*0x87654321u;
        }
        if(fn==6) {
            const char *fmt=formats[format_index];unsigned i=0;
            do{((char *)0x740000)[i]=fmt[i];}while(fmt[i++]);
            args[0]=0xfedcba98;args[1]=seed*0xfedcba98u;args[2]=seed*0x98765432u;
            args[3]=0x11223344;args[4]=0x55667788;args[5]=0x740000;
            for(i=6;i<12;++i)args[i]=format_index==5?0x740400:value;
        }
        uint32_t result=clock_invoke(base+offsets[fn],args,fn==0&&(int32_t)window_result>0);
        uint16_t cw,sw;__asm__ volatile("fnstcw %0; fnstsw %1":"=m"(cw),"=am"(sw));
        if(clock_bad_abi)fail("CLOCK cdecl callee-saved/stack contract");
        if(sw&0x3800)fail("CLOCK x87 stack imbalance");
        if(fn==0&&(int32_t)window_result>0&&polls!=6)fail("CLOCK loop returned early");
        if(fn>2 || (fn==0&&(int32_t)window_result>0)) result=0;
        if(!copy){snapshot(1);expected_result=result;expected_cw=cw;}
        else if(!snapshot(0)||result!=expected_result||cw!=expected_cw)return 0;
    }
    ++cases;return 1;
}
static int run(void)
{
    static const uint32_t handles[]={0,1,7,0x40000000,0x7fffffff,0x80000000,0xffffffff};
    static const uint32_t values[]={0,1,0x010159,0x115959,0x120000,0x235959,0xff235959,
                                   0x7fffffff,0x80000000,0xffffffff};
    static const uint32_t pcs[]={0,0x200,0x300};
    unsigned pass,h,f,v,rounding,pc;
    if(!map(0x10000,0x20000)||!map(0x660000,0x10000)||!map(0x690000,0x20000)||
       !map(0x700000,0x50000)||!map(0x800000,0x40000)||
       !map(0x1000000,0x20000)||!map(0x1200000,0x20000))return 1;
    if(!read_file("build/clock.test_kernel.x",0x800000,250099))return 2;
    for(pass=0;pass<2;++pass) {
        uint32_t a=0x1000000+(pass?0x113:0),b=0x1200000+(pass?0x513:0);
        if(!load("build/clock.original.x",a)||!load("build/clock.x",b))return 3;
        for(mode=0;mode<32;++mode)for(seed=0;seed<4;++seed) {
            initial_cw=0x37f;time_value=values[seed];
            for(h=0;h<7;++h) {window_result=handles[h];if(!exercise(0,a,b,0))goto mismatch;}
            window_result=7;
            for(f=1;f<=5;++f)for(v=0;v<10;++v) {
                if(f==2&&v>1)continue;
                if(!exercise(f,a,b,f==2?v*7:values[v]))goto mismatch;
            }
            for(format_index=0;format_index<12;++format_index)for(v=0;v<10;++v)
                if(!exercise(6,a,b,values[v]))goto mismatch;
        }
        mode=27;seed=2;window_result=7;
        for(pc=0;pc<3;++pc)for(rounding=0;rounding<4;++rounding) {
            initial_cw=0x7f|pcs[pc]|rounding*0x400;
            if(!exercise(3,a,b,0))goto mismatch;
            for(v=0;v<512;++v)if(!exercise(5,a,b,v*0x87654321u))goto mismatch;
        }
    }
    print("CLOCK original/C differential scenarios: ");number(cases);print("\n");return 0;
mismatch:
    print("\nCLOCK mismatch function ");number(function_index);print(" mode ");number(mode);
    print(" seed ");number(seed);print(" format ");number(format_index);print(" cw ");number(initial_cw);
    print("\n");return 10;
}
void _start(void){sys3(1,(unsigned)run(),0,0);__builtin_unreachable();}
