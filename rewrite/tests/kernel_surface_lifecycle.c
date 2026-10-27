/* Execute original/C/independent byte-memory state machine. Shared stubs
 * model external allocation, order-list and flag-control boundaries only. */
#include "../kernel/surface_lifecycle.h"
#include "../kernel/surface_refresh.h"
#include "../kernel/memory.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define ARGS 0x630800u
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t regions[][2] = {
    {0x1a000,0x2000},{0x500000,0x2000},{0x510000,0x1000},
    {0x600000,0x2000},{0x610000,0x1000},{ARGS,0x800}
};
static const uint32_t scenarios[][4] = {
    /* high-water, capacity, free count, first hole (or absent). */
    {2,16,0,0xffffffff},{3,16,1,1},{8,8,3,3},{8,8,1,0xffffffff},
    {3,3,0,0xffffffff},{0,16,1,0xffffffff},{3,16,0xffffffff,1},
    {3,0x80000000,1,1},{0x80000000,0,1,0xffffffff},
    {0xffffffff,0x80000000,1,0xffffffff},{0x7fffffff,0,1,3},
    {7,16,0x7fffffff,6},{7,16,0x80000000,1},{0,0,0,0xffffffff},
    {0x7fffffff,0x7fffffff,0,0xffffffff},{3,16,2,0}
};
static const uint32_t values[] = {
    0,1,7,8,32,0xffffffff,0xfffffff9,0xfffffff8,
    0x80000000,0x7fffffff,0x10000000,0xabcde123
};
static uint8_t model_memory[0x8800],expected_memory[0x8800];
static uint32_t events[256],expected_events[256],event_count,expected_count;
static uint32_t path,seed,mode,layout,scenario,failure_case,descriptor;
static uint32_t modeling,allocations,frees,cases,coverage,unsafe,native_flags;
static uint32_t expected_return,mismatch_address;
__attribute__((used)) static uint32_t saved_stack,returned_flags;

static int syscall3(int n,uint32_t a,uint32_t b,uint32_t c)
{int r;__asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory");return r;}
static void finish(uint32_t code){syscall3(1,code,0,0);__builtin_unreachable();}
static int map(uint32_t address,uint32_t size)
{uint32_t a[]={address,size,7,0x32,0xffffffff,0};return (uint32_t)syscall3(90,(uint32_t)a,0,0)==address;}
static int load(const char *name,uint32_t address)
{int f=syscall3(5,(uint32_t)name,0,0),n;if(f<0)return 0;n=syscall3(3,(uint32_t)f,address,0x40000);syscall3(6,(uint32_t)f,0,0);return n==250099;}
static void number(uint32_t n)
{char s[10];uint32_t i=10;do{s[--i]='0'+n%10;n/=10;}while(n);syscall3(4,1,(uint32_t)(s+i),10-i);}
static int64_t signed_word(uint32_t n)
{return n<0x80000000u?(int64_t)n:(int64_t)n-INT64_C(4294967296);}
static int offset(uint32_t address,uint32_t size)
{
    uint32_t r,base=0;
    for(r=0;r<COUNT(regions);++r){
        if(size<=regions[r][1] && address>=regions[r][0] &&
           address-regions[r][0]<=regions[r][1]-size)
            return (int)(base+address-regions[r][0]);
        base+=regions[r][1];
    }
    unsafe=1;return -1;
}
static uint32_t read_word(uint32_t address)
{
    int o=offset(address,4);uint32_t i,result=0;
    if(o<0)return 0;
    if(!modeling)return WORD(address);
    for(i=0;i<4;++i)result|=(uint32_t)model_memory[o+i]<<(8*i);
    return result;
}
static void write_word(uint32_t address,uint32_t value)
{
    int o=offset(address,4);uint32_t i;
    if(o<0)return;
    if(!modeling){WORD(address)=value;return;}
    for(i=0;i<4;++i)model_memory[o+i]=(uint8_t)(value>>(8*i));
}
static void event(uint32_t kind,uint32_t a,uint32_t b)
{
    uint32_t i;
    static const uint32_t watched[]={0x1b064,0x1b068,0x1b06c,0x1b0d8,
        0x1a816,0x50000c,0x500014,0x50001c,ARGS,ARGS+4,ARGS+8,ARGS+12,ARGS+16};
    if(event_count+18>COUNT(events))finish(90);
    events[event_count++]=kind;events[event_count++]=a;events[event_count++]=b;
    for(i=0;i<COUNT(watched);++i)events[event_count++]=read_word(watched[i]);
    events[event_count++]=read_word(descriptor);
    events[event_count++]=read_word(descriptor+44u);
}
static uint32_t boundary(uint32_t kind,uint32_t a,uint32_t b)
{
    uint32_t result=0x81fe0000u+kind*0x101u+seed;
    event(kind,a,b);
    switch(kind){
    case 1:
        result=0x202u|(seed&1?0x400u:0)|(seed&2?0x895u:0)|(seed&4?0x44u:0);
        if(mode&1){write_word(0x1b0d8,values[(seed+3)%COUNT(values)]);write_word(ARGS,values[(seed+4)%COUNT(values)]);}
        break;
    case 2:
        if(mode&2){write_word(0x1b0d8,values[(seed+5)%COUNT(values)]);write_word(0x1a816,values[(seed+6)%COUNT(values)]);}
        break;
    case 3:
        ++allocations;
        if(allocations==1){
            result=failure_case==1?0:descriptor;
            if(mode&4){write_word(ARGS+8,values[(seed+7)%COUNT(values)]);write_word(ARGS+12,values[(seed+8)%COUNT(values)]);}
        }else{
            result=failure_case==2?0:0x610100u+seed;
            if(mode&8){write_word(0x1b068,2);write_word(0x1b06c,0x80000000);write_word(0x1b0d8,0xface0011);}
        }
        break;
    case 4:
        ++frees;
        if(native_flags)result=0x202u|(seed&1?0x400u:0)|(seed&2?0x895u:0)|(seed&4?0x44u:0);
        if(path==3 && mode&4){
            write_word(ARGS,5u+(frees&1u)*2u+(seed&3u)*0x40000000u);
            write_word(0x500014,0x600900u+(seed&3));
            write_word(0x50001c,0x600a00u+(seed&3));
        }
        if(mode&8){write_word(descriptor,0x610600u+seed);write_word(0x1b06c,0x7fffffff);}
        break;
    case 5:
        if(mode&4){write_word(ARGS,5u+(seed&3u)*0x40000000u);write_word(0x500014,0x600800u+(seed&3));}
        if(mode&8){write_word(descriptor,seed&1?0:0x610500u+seed);write_word(0x1b06c,0xffffffff);}
        break;
    case 6:
        if(native_flags)result=a;
        if(mode&8){write_word(0x1b068,0xdeadbeef);write_word(ARGS+4,0x13579bdf);}
        break;
    default:finish(91);
    }
    return result;
}
__attribute__((used,noinline))
static uint32_t boundary_body(uint32_t kind,volatile uint32_t *arguments)
{
    uint32_t a=0,b=0,result;
    if(kind>=3)a=arguments[0];
    if(kind==3)b=arguments[1];
    result=boundary(kind,a,b);
    if(mode&16 && kind>=3){arguments[0]=0xdeadc001u;if(kind==3)arguments[1]=0xdeadc002u;}
    return result;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define STUB(type,name,params,kind) \
__attribute__((naked,noinline)) type name params { \
    __asm__("leal 4(%esp),%eax;pushl %eax;pushl $" #kind ";call boundary_body;" \
        "leal 8(%esp),%esp;ret");__builtin_unreachable();}
STUB(uint32_t,kernel_read_flags,(void),1)
STUB(void,kernel_disable_interrupts,(void),2)
STUB(uint32_t,kernel_allocate_for_task,(uint32_t size,uint32_t owner),3)
STUB(uint32_t,kernel_surface_order_remove,(uint32_t entry),5)
__attribute__((naked,noinline)) void kernel_free_allocation(uint32_t address)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;pushl $4;call boundary_body;"
        "leal 8(%esp),%esp;cmpl $0,native_flags;je 1f;pushl %eax;popfl;1:ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) void kernel_write_flags(uint32_t flags)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;pushl $6;call boundary_body;"
        "leal 8(%esp),%esp;cmpl $0,native_flags;je 1f;pushl %eax;popfl;1:ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t target)
{
    __asm__("movl %esp,saved_stack;movl 4(%esp),%eax;movl $0x630800,%esp;call *%eax;"
        "pushfl;popl returned_flags;cld;movl saved_stack,%esp;ret");
}
#pragma GCC diagnostic pop

static uint32_t search(void)
{
    uint32_t i=0;
    if(signed_word(read_word(0x1b06c))>0){
        while(signed_word(i)<signed_word(read_word(0x1b068))){
            if(i>=64){unsafe=1;return 0xffffffff;}
            if(!read_word(0x500000+4*i))return i;
            ++i;
        }
    }
    return signed_word(read_word(0x1b068))<signed_word(read_word(0x1b064))?
        read_word(0x1b068):0xffffffff;
}
static uint32_t oracle(void)
{
    uint32_t flags,d,index,owner,area,pixels,j;
    static const uint32_t fields[][2]={{4,0},{8,1},{12,2},{16,3},{20,2},{24,3}};
    if(path==0){
        write_word(0x500000,read_word(0x1b034));write_word(0x500004,read_word(0x1b04c));
        write_word(0x1b070,0x900000);write_word(0x1b074,0x510000);
        write_word(0x510000,0);write_word(0x1b068,2);return 0x1b068;
    }
    if(path==1)return search();
    flags=boundary(1,0,0);boundary(2,0,0);
    if(path==2){
        d=boundary(3,56,read_word(0x1b0d8));index=search();
        if(!d){coverage|=1;return 0xffffffff;}
        if(signed_word(index)<0){boundary(4,d,0);coverage|=2;return 0xffffffff;}
        for(j=0;j<COUNT(fields);++j)write_word(d+fields[j][0],read_word(ARGS+4*fields[j][1]));
        write_word(d+28,100);write_word(d+32,100);write_word(d+36,read_word(0x1a816));
        write_word(d+40,0xffffffff);write_word(d+44,0);write_word(d+48,0);write_word(d+52,0);
        owner=read_word(0x1b0d8);area=read_word(ARGS+8)*read_word(ARGS+12);
        pixels=boundary(3,area*(uint32_t)(signed_word(read_word(d+36))/8),owner);
        write_word(d,pixels);
        if(!read_word(d)){boundary(4,d,0);coverage|=4;return 0xffffffff;}
        write_word(0x500000+4*index,d);
        if(signed_word(read_word(0x1b068))>signed_word(index)){
            write_word(0x1b06c,read_word(0x1b06c)-1);coverage|=8;
        }else{write_word(0x1b068,read_word(0x1b068)+1);coverage|=16;}
        boundary(6,flags,0);return index;
    }
    index=read_word(ARGS);
    if(read_word(0x500000+4*index)){
        d=read_word(0x500000+4*read_word(ARGS));
        if(read_word(d+44)){boundary(5,read_word(d+44),0);coverage|=32;}
        if(read_word(d)){boundary(4,read_word(d),0);coverage|=64;}
        boundary(4,read_word(0x500000+4*read_word(ARGS)),0);
        write_word(0x500000+4*read_word(ARGS),0);
        write_word(0x1b06c,read_word(0x1b06c)+1);coverage|=128;
    }else coverage|=256;
    return boundary(6,flags,0);
}

static void initialize(void)
{
    uint32_t r,i;
    modeling=0;event_count=allocations=frees=unsafe=0;
    for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)
        BYTE(regions[r][0]+i)=(uint8_t)(i*37+seed*17+r*53);
    descriptor=0x600200+(layout&3);
    if(layout==4)descriptor=ARGS+4;
    if(layout==5)descriptor=0x1b040;
    if(layout==6)descriptor=0x1b0b4;
    if(layout==7)descriptor=0x500004;
    if(layout==8)descriptor=0x510000;
    WORD(0x1b068)=scenarios[scenario][0];WORD(0x1b064)=scenarios[scenario][1];
    WORD(0x1b06c)=scenarios[scenario][2];WORD(0x1b0d8)=values[(seed+1)%COUNT(values)];
    WORD(0x1a816)=values[seed%COUNT(values)];
    for(i=0;i<64;++i)WORD(0x500000+4*i)=0x600700+(seed&3);
    if(scenarios[scenario][3]!=0xffffffff)WORD(0x500000+4*scenarios[scenario][3])=0;
    for(i=0;i<4;++i)WORD(ARGS+4*i)=values[(seed+i)%COUNT(values)];
    WORD(ARGS+16)=0x93fedc71;
    if(path==3){
        WORD(ARGS)=3u+(seed&3u)*0x40000000u;
        WORD(0x50000c)=failure_case==1?0:descriptor;
        WORD(descriptor)=failure_case==2?0:0x610200+seed;
        WORD(descriptor+44)=failure_case==3?0:values[(seed+2)%COUNT(values)];
        /* Read-flags mutation must keep the delete index safely mapped. */
        if(mode&1)WORD(ARGS)=3u+(seed&3u)*0x40000000u;
    }
}
static void capture_model(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)model_memory[n++]=BYTE(regions[r][0]+i);}
static void trampoline(uint32_t address,uint32_t target)
{BYTE(address)=0xe9;WORD(address+1)=target-address-5;}
static void redirect_flags(uint32_t base)
{
    uint32_t p,target,replacement;
    /* CLI/read-flags leaves occupy only two/three bytes. Redirect calls
     * within these four functions instead of overwriting adjacent entries. */
    for(p=base+0x175c0;p+5<=base+0x178f0;++p){
        if(BYTE(p)!=0xe8)continue;
        target=p+5+WORD(p+1);replacement=0;
        if(target==base+0x1a24a)replacement=(uint32_t)kernel_read_flags;
        if(target==base+0x1a242)replacement=(uint32_t)kernel_disable_interrupts;
        if(target==base+0x1a24d)replacement=(uint32_t)kernel_write_flags;
        if(replacement){WORD(p+1)=replacement-p-5;p+=4;}
    }
}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    static const uint32_t targets[]={0x8175c0,0x8177c4,0x817626,0x81783e};
#else
    const uint32_t targets[]={(uint32_t)kernel_surfaces_init,(uint32_t)kernel_surface_find_free,
        (uint32_t)kernel_surface_create,(uint32_t)kernel_surface_delete};
#endif
    return targets[path];
}
static int exercise(void)
{
    uint32_t p,r,i,n,result,original_flags=0;
    static const uint32_t original[]={0x9175c0,0x9177c4,0x917626,0x91783e};
    initialize();capture_model();modeling=1;expected_return=oracle();
    if(unsafe)return 1;
    expected_count=event_count;
    for(i=0;i<event_count;++i)expected_events[i]=events[i];
    for(i=0;i<sizeof(expected_memory);++i)expected_memory[i]=model_memory[i];
    for(p=0;p<2;++p){
        initialize();result=invoke(p?candidate():original[path]);
        if(unsafe||result!=expected_return||event_count!=expected_count)return 2+p;
        for(i=0;i<event_count;++i)if(events[i]!=expected_events[i]){mismatch_address=i;return 4+p;}
        n=0;
        for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)
            if(expected_memory[n++]!=BYTE(regions[r][0]+i)){mismatch_address=regions[r][0]+i;return 6+p;}
        if(native_flags && (path==3||(path==2&&result!=0xffffffff))){
            uint32_t flags=0x202u|(seed&1?0x400u:0)|(seed&2?0x895u:0)|(seed&4?0x44u:0);
            if((returned_flags&0xcd5)!=(flags&0xcd5))return 8+p;
        }
        if(native_flags && path==2 && result==0xffffffff){
            if(!p)original_flags=returned_flags;
            else if((returned_flags^original_flags)&0xcd5)return 10;
        }
    }
    ++cases;return 0;
}
static void check(void)
{
    uint32_t error=exercise();
    if(error){PRINT("Surface lifecycle failure ");number(error);PRINT(" path/seed/mode/layout/scenario/failure/address: ");
        number(path);PRINT("/");number(seed);PRINT("/");number(mode);PRINT("/");number(layout);
        PRINT("/");number(scenario);PRINT("/");number(failure_case);PRINT("/");number(mismatch_address);PRINT("\n");finish(10+error);}
}
static int run(void)
{
    uint32_t i,r,start;
    for(r=0;r<COUNT(regions);++r){uint32_t a=regions[r][0]&~0xfffu;
        if(!map(a,(regions[r][0]+regions[r][1]-a+0xfffu)&~0xfffu))return 1;}
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/icestar_c.x",0x800000))return 3;
#endif
    for(i=0;i<2;++i){uint32_t b=i?0x800000:0x900000;
#ifndef KERNEL_LINKED_TEST
        if(i)break;
#endif
        redirect_flags(b);
        trampoline(b+0x155ca,(uint32_t)kernel_allocate_for_task);
        trampoline(b+0x15978,(uint32_t)kernel_free_allocation);
        trampoline(b+0x178f0,(uint32_t)kernel_surface_order_remove);
    }
#ifndef LIFECYCLE_FLAGS_ONLY
    for(path=0;path<4;++path){
        start=cases;
        for(seed=0;seed<COUNT(values);++seed)for(layout=0;layout<(path<2?1u:path==2?9u:4u);++layout)
        for(scenario=0;scenario<(path==0?1u:COUNT(scenarios));++scenario)
        for(failure_case=0;failure_case<(path==2?3u:path==3?4u:1u);++failure_case)
        for(mode=0;mode<(path<2?1u:32u);++mode){
            if(path==3 && mode&1)continue; /* Separate delete caller mutation below. */
            if(path==3 && scenario>0)continue;
            check();
        }
        PRINT("Surface lifecycle path/cases: ");number(path);PRINT("/");number(cases-start);PRINT("\n");
    }
    if(coverage!=511)return 4;
#else
    (void)start;
#endif
    native_flags=1;scenario=failure_case=0;layout=0;
    for(path=2;path<4;++path)for(seed=0;seed<8;++seed)
    for(scenario=0;scenario<(path==2?COUNT(scenarios):1u);++scenario)
    for(failure_case=0;failure_case<3;++failure_case)
    for(mode=0;mode<32;mode+=2)check();
    PRINT("Surface lifecycle original/C/oracle total: ");number(cases);
    PRINT("; signed counters, wrapped/unaligned aliases, allocation failures, live caller/callback mutation, outgoing isolation, ordered boundaries, raw returns and complete memory verified\n");
    PRINT("Native POPFL checks: successful create/delete restore arithmetic flags and DF before RET; create failures preserve original failure flags without a flags-write callback\n");
    return 0;
}
void _start(void){finish(run());}
