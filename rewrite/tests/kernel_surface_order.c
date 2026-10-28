/* Original instructions / recovered C / independent bounded byte-memory
 * oracle. Only the two flags service CALL sites are redirected. */
#include "../kernel/surface_order.h"
#include "../kernel/surface_refresh.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define ARGS 0x630800u
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static const uint32_t regions[][2] = {
    {0x1b000,0x1000},{0x500000,0x1000},{0x510000,0x1000},
    {0x600000,0x4000},{ARGS,0x400}
};
static const uint32_t values[] = {
    0,1,2,3,5,6,7,8,0xffffffff,0x80000000,0x7fffffff,0x40000001
};
static const uint32_t counts[] = {0,1,3,6,8,0x40000000,0x7fffffff,0x80000000,0xffffffff};
static uint8_t memory[0x7400],expected_memory[0x7400];
static uint32_t events[128],expected_events[128],event_count,expected_count;
static uint32_t path,seed,mode,layout,scenario,position,target,cases,skipped;
static uint32_t modeling,unsafe,steps,coverage,case_coverage,expected_return,mismatch;
static uint32_t native_cases,both_directions,layout_cases[12];
__attribute__((used)) static uint32_t native_flags,saved_stack,returned_flags,pop_flags;

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
static int offset(uint32_t address)
{
    uint32_t r,base=0;
    for(r=0;r<COUNT(regions);++r){
        if(address>=regions[r][0] && address-regions[r][0]<=regions[r][1]-4)
            return (int)(base+address-regions[r][0]);
        base+=regions[r][1];
    }
    unsafe=1;return -1;
}
static uint32_t read_word(uint32_t address)
{
    int o=offset(address);uint32_t i,result=0;
    if(o<0)return 0;
    if(!modeling)return WORD(address);
    for(i=0;i<4;++i)result|=(uint32_t)memory[o+i]<<(8*i);
    return result;
}
static void write_word(uint32_t address,uint32_t value)
{
    int o=offset(address);uint32_t i;
    if(o<0)return;
    if(!modeling){WORD(address)=value;return;}
    for(i=0;i<4;++i)memory[o+i]=(uint8_t)(value>>(8*i));
}
static uint32_t desc_position(uint32_t layer)
{return read_word(read_word(0x500000u+4u*layer)+44u);}
static void set_position(uint32_t layer,uint32_t value)
{write_word(read_word(0x500000u+4u*layer)+44u,value);}
static uint32_t flags_value(void)
{return 0x202u|(seed&1?0x400u:0)|(seed&2?0x895u:0)|(seed&4?0x44u:0);}
static uint32_t boundary(uint32_t kind,uint32_t argument)
{
    uint32_t i,result;
    static const uint32_t watched[]={0x1b064,0x510000,0x510004,0x51000c,
        0x50000c,0x500014,ARGS,ARGS+4};
    if(event_count+10>COUNT(events))finish(90);
    events[event_count++]=kind;events[event_count++]=argument;
    for(i=0;i<COUNT(watched);++i)events[event_count++]=read_word(watched[i]);
    if(kind==1){
        result=flags_value();
        if(mode&1)write_word(ARGS,5u+(seed&3u)*0x40000000u);
        if(mode&2)write_word(ARGS+4,values[(seed+2)%COUNT(values)]);
        if(mode&4){write_word(0x510000,counts[(seed+3)%COUNT(counts)]);write_word(0x1b064,values[(seed+5)%COUNT(values)]);}
        pop_flags=result;
    }else{
        result=0xface0000u+seed*0x101u+path;
        pop_flags=argument;
        if(mode&8){write_word(ARGS,0xface3456);write_word(ARGS+4,0xbeef1234);write_word(0x510000,0xdeadabcd);}
    }
    return result;
}
__attribute__((used,noinline))
static uint32_t boundary_body(uint32_t kind,volatile uint32_t *arguments)
{
    uint32_t result=boundary(kind,kind==2?arguments[0]:0);
    if(kind==2 && mode&16)arguments[0]=0xdeadc001;
    return result;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
__attribute__((naked,noinline)) uint32_t kernel_read_flags(void)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;pushl $1;call boundary_body;"
        "leal 8(%esp),%esp;cmpl $0,native_flags;je 1f;pushl pop_flags;popfl;1:ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) void kernel_write_flags(uint32_t flags)
{
    __asm__("leal 4(%esp),%eax;pushl %eax;pushl $2;call boundary_body;"
        "leal 8(%esp),%esp;cmpl $0,native_flags;je 1f;pushl pop_flags;popfl;1:ret");
    __builtin_unreachable();
}
__attribute__((naked,noinline)) static uint32_t invoke(uint32_t function)
{
    __asm__("movl %esp,saved_stack;movl 4(%esp),%eax;movl $0x630800,%esp;call *%eax;"
        "pushfl;popl returned_flags;cld;movl saved_stack,%esp;ret");
    __builtin_unreachable();
}
#pragma GCC diagnostic pop
static uint32_t oracle(void)
{
    uint32_t flags=boundary(1,0),i,address,layer,value;
    if(path==1){
        if(read_word(0x510000)<read_word(0x1b064)){
            address=0x510004u+4u*read_word(0x510000);
            write_word(address,read_word(ARGS));
            write_word(0x510000,read_word(0x510000)+1u);
            address=read_word(0x500000u+4u*read_word(ARGS))+44u;
            write_word(address,read_word(0x510000));case_coverage|=1;
        }else case_coverage|=2;
    }else if(path==0 || path==2){
        if(desc_position(read_word(ARGS)) && desc_position(read_word(ARGS))<=read_word(0x510000)){
            i=desc_position(read_word(ARGS));
            while(i<read_word(0x510000)){
                if(++steps>40||unsafe){unsafe=1;return 0;}
                write_word(0x510000u+4u*i,read_word(0x510004u+4u*i));
                layer=read_word(0x510000u+4u*i);set_position(layer,i);++i;case_coverage|=4;
            }
            if(!path){
                write_word(0x510000,read_word(0x510000)-1u);
                set_position(read_word(ARGS),0);case_coverage|=8;
            }else{
                address=0x510000u+4u*read_word(0x510000);
                write_word(address,read_word(ARGS));
                address=read_word(0x500000u+4u*read_word(ARGS))+44u;
                write_word(address,read_word(0x510000));case_coverage|=16;
            }
        }else case_coverage|=32;
    }else if(desc_position(read_word(ARGS)) && signed_word(read_word(ARGS+4))>0 &&
             read_word(ARGS+4)<=read_word(0x510000)){
        value=desc_position(read_word(ARGS));
        if(value==read_word(ARGS+4)){case_coverage|=64;return value;}
        if(desc_position(read_word(ARGS))<read_word(ARGS+4)){
            i=desc_position(read_word(ARGS));
            while(signed_word(i)<signed_word(read_word(ARGS+4))){
                if(++steps>40||unsafe){unsafe=1;return 0;}
                write_word(0x510000u+4u*i,read_word(0x510004u+4u*i));
                layer=read_word(0x510000u+4u*i);set_position(layer,i);++i;case_coverage|=128;
            }
        }
        if(desc_position(read_word(ARGS))>read_word(ARGS+4)){
            i=desc_position(read_word(ARGS));
            while(signed_word(i)>signed_word(read_word(ARGS+4))){
                if(++steps>40||unsafe){unsafe=1;return 0;}
                write_word(0x510000u+4u*i,read_word(0x50fffcu+4u*i));
                layer=read_word(0x510000u+4u*i);set_position(layer,i);--i;case_coverage|=256;
            }
        }
        address=0x510000u+4u*read_word(ARGS+4);write_word(address,read_word(ARGS));
        address=read_word(0x500000u+4u*read_word(ARGS))+44u;
        write_word(address,read_word(ARGS+4));case_coverage|=512;
    }else case_coverage|=1024;
    return boundary(2,flags);
}
static void initialize(void)
{
    uint32_t r,i,k,d,alignment=seed&3;
    modeling=unsafe=steps=event_count=0;
    for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)
        BYTE(regions[r][0]+i)=(uint8_t)(i*29u+seed*41u+r*17u);
    for(i=0;i<64;++i){
        d=0x600100u+64u*i+alignment;
        WORD(0x500000u+4u*i)=d;WORD(d+44u)=i;
        if(i)WORD(0x510000u+4u*i)=i;
    }
    WORD(ARGS)=3u+(seed&3u)*0x40000000u;WORD(ARGS+4)=values[target];
    WORD(0x510000)=counts[scenario];WORD(0x1b064)=8;
    WORD(WORD(0x50000c)+44u)=values[position];
    k=1u+(seed%6u);if(k==3)k=4;
    switch(layout){
    case 1:WORD(0x50000c)=0x510000u-44u;break;
    case 2:WORD(0x50000c)=0x510000u+4u*k-44u;WORD(0x510000u+4u*k)=values[position];break;
    case 3:WORD(0x500000u+4u*k)=0x510000u-44u;break;
    case 4:WORD(0x500000u+4u*k)=ARGS-44u;break;
    case 5:WORD(0x500000u+4u*k)=ARGS+4u-44u;break;
    case 6:WORD(0x50000c)=ARGS+4u-44u;break;
    case 7:WORD(0x50000c)=0x500000u+4u*k-44u;break;
    case 8:WORD(0x50000c)=0x1b064u-44u;WORD(0x1b064)=values[position];break;
    case 9:for(i=1;i<9;++i)WORD(0x500000u+4u*i)=WORD(0x50000c);break;
    case 10:WORD(0x500000u+4u*k)=0x510000u+4u*(k+1u)+alignment-44u;break;
    case 11:
        WORD(0x50000c)=0x510004u-44u;WORD(0x510004)=values[position];
        WORD(0x500008)=ARGS+4u-44u;break;
    default:break;
    }
}
static void capture(void)
{uint32_t r,i,n=0;for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)memory[n++]=BYTE(regions[r][0]+i);}
static void redirect_flags(uint32_t base)
{
    uint32_t p,address,destination,i,patched=0;const uint32_t targets[]={0x81a24a,0x81a24d};
    for(p=base+0x178f0;p<base+0x17cd0;++p)if(BYTE(p)==0xe8){
        address=p+5+WORD(p+1);
        for(i=0;i<2;++i)if(address==base+(targets[i]-0x800000u)){
            destination=i?(uint32_t)kernel_write_flags:(uint32_t)kernel_read_flags;
            WORD(p+1)=destination-p-5;++patched;break;
        }
    }
    if(patched!=8)finish(92);
}
static uint32_t candidate(void)
{
#ifdef KERNEL_LINKED_TEST
    const uint32_t targets[]={0x8178f0,0x8179d6,0x817a3c,0x817b32};
#else
    const uint32_t targets[]={(uint32_t)kernel_surface_order_remove,(uint32_t)kernel_surface_order_append,
        (uint32_t)kernel_surface_order_front,(uint32_t)kernel_surface_order_move};
#endif
    return targets[path];
}
static uint32_t exercise(void)
{
    uint32_t p,r,i,n,result,original_flags=0;
    static const uint32_t original[]={0x9178f0,0x9179d6,0x917a3c,0x917b32};
    initialize();capture();modeling=1;case_coverage=0;expected_return=oracle();
    if(unsafe){++skipped;return 0;}
    expected_count=event_count;
    for(i=0;i<event_count;++i)expected_events[i]=events[i];
    for(i=0;i<sizeof(memory);++i)expected_memory[i]=memory[i];
    for(p=0;p<2;++p){
        initialize();result=invoke(p?candidate():original[path]);
        if(unsafe||result!=expected_return||event_count!=expected_count)return 2+p;
        for(i=0;i<event_count;++i)if(events[i]!=expected_events[i]){mismatch=i;return 4+p;}
        n=0;
        for(r=0;r<COUNT(regions);++r)for(i=0;i<regions[r][1];++i)
            if(expected_memory[n++]!=BYTE(regions[r][0]+i)){mismatch=regions[r][0]+i;return 6+p;}
        if(native_flags){
            if(!p)original_flags=returned_flags;
            else if((returned_flags^original_flags)&0xcd5)return 8;
            if(expected_count==20 && ((returned_flags^flags_value())&0xcd5))return 9+p;
            if(expected_count==10 && ((returned_flags^(0x44u|(flags_value()&0x400u)))&0xcd5))return 11+p;
        }
    }
    coverage|=case_coverage;
    if((case_coverage&384)==384)++both_directions;
    ++layout_cases[layout];if(native_flags)++native_cases;
    ++cases;return 0;
}
static void check(void)
{
    uint32_t error=exercise();
    if(error){PRINT("Surface order failure ");number(error);PRINT(" path/seed/mode/layout/count/position/target/address: ");
        number(path);PRINT("/");number(seed);PRINT("/");number(mode);PRINT("/");number(layout);
        PRINT("/");number(scenario);PRINT("/");number(position);PRINT("/");number(target);PRINT("/");number(mismatch);PRINT("\n");finish(10+error);}
}
static int run(void)
{
    uint32_t r,start;
    for(r=0;r<COUNT(regions);++r){uint32_t a=regions[r][0]&~0xfffu;
        if(!map(a,(regions[r][0]+regions[r][1]-a+0xfffu)&~0xfffu))return 1;}
    if(!map(0x900000,0x40000)||!load("build/icestar.x",0x900000))return 2;
#ifdef KERNEL_LINKED_TEST
    if(!map(0x800000,0x40000)||!load("build/openicestaros_kernel.x",0x800000))return 3;
#endif
    redirect_flags(0x900000);
#ifdef KERNEL_LINKED_TEST
    redirect_flags(0x800000);
#endif
    for(path=0;path<4;++path){
        start=cases;
        for(seed=0;seed<4;++seed)for(layout=0;layout<12;++layout)
        for(scenario=0;scenario<COUNT(counts);++scenario)
        for(position=0;position<COUNT(values);++position)
        for(target=0;target<(path==3?COUNT(values):1u);++target)check();
        PRINT("Surface order path/cases: ");number(path);PRINT("/");number(cases-start);PRINT("\n");
    }
    native_flags=1;
    for(path=0;path<4;++path)for(seed=0;seed<8;++seed)
    for(layout=0;layout<12;++layout)for(scenario=0;scenario<COUNT(counts);++scenario)
    for(mode=0;mode<32;++mode){position=(seed+layout)%COUNT(values);target=(seed+scenario)%COUNT(values);check();}
    if(!both_directions)return 6;
    if(coverage!=2047){PRINT("Missing surface order coverage: ");number(coverage);PRINT("\n");return 4;}
    PRINT("Native flag cases: ");number(native_cases);PRINT("; both signed move loops in one call: ");number(both_directions);PRINT("\n");
    for(r=0;r<COUNT(layout_cases);++r){if(!layout_cases[r])return 5;PRINT("Alias layout/cases: ");number(r);PRINT("/");number(layout_cases[r]);PRINT("\n");}
    PRINT("Surface order original/C/oracle total: ");number(cases);PRINT("; unsafe scenarios skipped: ");number(skipped);
    PRINT("\nAll mapped bytes, raw returns, ordered flag callbacks, wrapped/unaligned descriptor/list/caller aliases, live count/target mutation and native EFLAGS/DF verified\n");
    return 0;
}
void _start(void){finish(run());}
