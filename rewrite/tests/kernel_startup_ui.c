/* Startup orchestration: independent bytewise oracle and fixture-only CALL
 * redirection, including persistent writable metadata across callbacks. */
#include <stdint.h>
#include "../kernel/startup_ui.h"
#define BASE 0x1a000u
#define SIZE 0xa000u
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t expected[SIZE];
struct event { uint32_t kind, count, args[10]; };
static struct event trace[160];
static uint32_t trace_count, cursor, failure, cases, lookup_counter;
static uint32_t pattern, layout, geometry, boot_index, mask_index, mutation_index;
static uint32_t display, colors, model_metadata[8], model_valid;
static uint32_t *live_metadata;
static const uint32_t dimensions[] = {0,1,0xffffffff,2,0xfffffffe,641,0x7fffffff,0x80000000,0xfffffffd};
static const uint32_t boots[] = {0,1,0x7fffffff,0x80000000,0xfffffffe,0xffffffff,2,3};
static const uint32_t masks[] = {0,0x7f,0x55,0x2a,1,0x40,8,0x10};
static const uint32_t mutations[] = {0,1,3,15};

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static int map(uint32_t a, uint32_t size)
{
    uint32_t args[] = {a,size,7,0x32,0xffffffff,0};
    return (uint32_t)syscall3(90,(uint32_t)args,0,0) == a;
}
static int load(const char *path, uint32_t a)
{
    int fd = syscall3(5,(uint32_t)path,0,0), size;
    if (fd < 0) return 0;
    size = syscall3(3,(uint32_t)fd,a,0x40000); syscall3(6,(uint32_t)fd,0,0);
    return size == 250099;
}
static uint32_t get(uint32_t a)
{
    uint32_t i,v=0;
    if (a < BASE || a > BASE+SIZE-4) { failure=90; return 0; }
    for (i=0;i<4;++i) v |= (uint32_t)expected[a-BASE+i] << (i*8);
    return v;
}
static void put(uint32_t a, uint32_t v, uint32_t model)
{
    uint32_t i;
    if (model) {
        if (a < BASE || a > BASE+SIZE-4) { failure=91; return; }
        for (i=0;i<4;++i) expected[a-BASE+i]=(uint8_t)(v>>(i*8));
    } else DWORD(a)=v;
}
static uint32_t value(uint32_t e)
{ return dimensions[geometry] ^ (0x13579bdu*(e+1u)) ^ (pattern*0x1010101u); }
static uint32_t lookup_result(uint32_t stage)
{
    static const uint32_t nonpositive[] = {0,0xffffffff,0x80000000,0xfffffffe};
    return masks[mask_index] & (1u<<stage) ? ((stage+geometry)&1u ? 1u : 0x7fffffffu) :
           nonpositive[(stage+geometry)%COUNT(nonpositive)];
}
static void mutate(uint32_t e, uint32_t model)
{
    uint32_t v=value(e), m=mutations[mutation_index], i;
    if (m&1u) {
        put(0x1b034,0x1e000+e*4u,model); put(0x1b0e0,0x1f000+e*4u,model);
        put(display+12u,v,model); put(display+16u,~v,model);
    }
    if (m&2u) {
        const uint32_t offsets[] = {0,4,12,16,28,40,60};
        for (i=0;i<COUNT(offsets);++i) put(colors+offsets[i],v^(i*0x10203u),model);
    }
    if (m&4u) {
        const uint32_t addresses[] = {0x1b154,0x1b15c,0x1b1bc,0x1b100,0x1b158,0x1b1a8,0x1b1ac,0x1b1b8};
        for (i=0;i<COUNT(addresses);++i) put(addresses[i],v+i,model);
        put(0x1a80a,~boots[boot_index],model);
    }
    if (m&8u) {
        if (model && model_valid) model_metadata[e%8u]^=v;
        if (!model && live_metadata) live_metadata[e%8u]^=v;
    }
    put(0x1ba01+e*7u,v,model);
}
static void record(uint32_t kind, uint32_t count, const uint32_t *args)
{
    uint32_t i,e=trace_count++;
    if (e>=COUNT(trace)) { failure=92; return; }
    trace[e].kind=kind; trace[e].count=count;
    for (i=0;i<count;++i) trace[e].args[i]=args[i];
    mutate(e,1);
}
static uint32_t callback(uint32_t kind, uint32_t count, const uint32_t *args)
{
    uint32_t i,e=cursor++;
    if (e>=trace_count) { failure=1; return 0; }
    if (trace[e].kind!=kind || trace[e].count!=count) failure=2;
    for (i=0;i<count;++i) if (trace[e].args[i]!=args[i]) failure=3;
    mutate(e,0); return value(e);
}
uint32_t kernel_startup_logo_821624(uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{ uint32_t args[]={a,b,c,d}; return callback(0,4,args); }
uint32_t kernel_startup_panel_81d6c2(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f,uint32_t g)
{ uint32_t args[]={a,b,c,d,e,f,g}; return callback(1,7,args); }
uint32_t kernel_startup_fill_837545(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f)
{ uint32_t args[]={a,b,c,d,e,f}; return callback(2,6,args); }
uint32_t kernel_startup_outline_837589(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f)
{ uint32_t args[]={a,b,c,d,e,f}; return callback(3,6,args); }
uint32_t kernel_startup_text_8376d3(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e,uint32_t f)
{ uint32_t args[]={a,b,c,d,e,f}; return callback(4,6,args); }
uint32_t kernel_startup_lookup_8263c0(uint32_t name,uint32_t *metadata)
{
    uint32_t i,e=cursor,stage=lookup_counter++;
    (void)callback(5,1,&name); live_metadata=metadata;
    for (i=0;i<8;++i) metadata[i]=value(e+stage*8u+i);
    return lookup_result(stage);
}
uint32_t kernel_startup_read_alloc_827454(uint32_t *metadata)
{
    uint32_t i,args[8];
    if (metadata!=live_metadata) failure=4;
    for (i=0;i<8;++i) args[i]=metadata[i];
    return callback(6,8,args);
}
uint32_t kernel_startup_read_into_82760c(uint32_t *metadata,uint32_t target)
{
    uint32_t i,args[9];
    if (metadata!=live_metadata) failure=5;
    for (i=0;i<8;++i) args[i]=metadata[i];
    args[8]=target; return callback(7,9,args);
}
uint32_t kernel_startup_load_image_80214c(uint32_t name) { return callback(8,1,&name); }
uint32_t kernel_wait_ticks(uint32_t ticks) { return callback(9,1,&ticks); }

struct call_patch { uint32_t offset,target; };
#include "../build/startup_ui_calls.h"
static void redirect(uint32_t base,const struct call_patch *patches,uint32_t count)
{
    uint32_t i;
    for (i=0;i<count;++i) {
        uint32_t a=base+patches[i].offset; DWORD(a+1)=patches[i].target-a-5;
    }
}
static void initialize(void)
{
    uint32_t i;
    static const uint32_t displays[]={0x1c000,0x1c001,0x1cffa,0x1b034,0x1b0d4,0x1b148,0x1b14c,0x1b1ac};
    static const uint32_t tables[]={0x1d000,0x1d001,0x1dffa,0x1b000,0x1b0dc,0x1b154,0x1b15c,0x1b1b8};
    static const uint32_t offsets[]={0,4,12,16,28,40,60};
    display=displays[layout]; colors=tables[layout];
    cursor=failure=lookup_counter=0; live_metadata=0;
    for (i=0;i<SIZE;++i) ((volatile uint8_t *)BASE)[i]=(uint8_t)(i*37+pattern*83);
    for (i=0;i<COUNT(offsets);++i) DWORD(colors+offsets[i])=value(i+200);
    DWORD(display+12u)=dimensions[geometry];
    DWORD(display+16u)=dimensions[(geometry+3u)%COUNT(dimensions)];
    DWORD(0x1b154)=value(220); DWORD(0x1b15c)=value(221); DWORD(0x1b1bc)=value(222);
    DWORD(0x1b034)=display; DWORD(0x1b0e0)=colors; DWORD(0x1a80a)=boots[boot_index];
}
/* Unsigned sign/magnitude calculation, independent of the C implementation's
 * signed /2. Offsets then use x86 wrapping arithmetic. */
static uint32_t model_half(uint32_t a)
{
    uint32_t v=get(a);
    return v&0x80000000u ? 0u-((0u-v)>>1) : v>>1;
}
static void rectangle(uint32_t kind,uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,uint32_t color_offset)
{
    uint32_t args[6]; args[0]=0;
    args[5]=get(colors+color_offset);
    args[4]=model_half(display+16u)+y1; args[3]=model_half(display+12u)+x1;
    args[2]=model_half(display+16u)+y0; args[1]=model_half(display+12u)+x0;
    record(kind,6,args);
}
static void oracle(void)
{
    uint32_t i,j,args[10],stage=0,e,name,destination,v;
    initialize(); trace_count=model_valid=0;
    for (i=0;i<SIZE;++i) expected[i]=((volatile uint8_t *)BASE)[i];
    display=get(0x1b034); colors=get(0x1b0e0);
    v=get(0x1a80a);
    if (v && !(v&0x80000000u)) {
        args[0]=0; args[3]=0x83ccc7;
        args[2]=model_half(display+16u)-150u; args[1]=model_half(display+12u)-130u;
        record(0,4,args);
        args[6]=get(colors+28u); args[5]=3; args[4]=22; args[3]=300;
        args[2]=model_half(display+16u)+160u; args[1]=model_half(display+12u)-151u;
        record(1,7,args);
    } else {
        rectangle(2,-60u,-40u,-5u,-5u,4); rectangle(2,5,-40u,60,-5u,40);
        rectangle(2,-60u,5,-5u,40,16); rectangle(2,5,5,60,40,60);
        args[0]=0; args[5]=0x83cce3; args[4]=get(colors); args[3]=get(colors+12u);
        args[2]=model_half(display+16u)+50u; args[1]=model_half(display+12u)-72u;
        record(4,6,args);
        rectangle(3,-150u,75,150,95,28); rectangle(3,-151u,76,149,94,28);
    }
    for (i=0;i<69;++i) {
        rectangle(2,i*4u-148u,163,i*4u-129u,177,16);
        if (i<=30 && i%5u==0) {
            static const uint32_t names[]={0x1b154,0x1b15c,0x83ccf6,0x83cd08,0x83cd1d,0x83cd33,0x1b1bc};
            static const uint32_t destinations[]={0x1b100,0x1b158,0x1b1a8,0x20200,0x62200,0x1b1ac,0x1b1b8};
            name=names[stage]; destination=destinations[stage];
            args[0]=stage==0 || stage==1 || stage==6 ? get(name) : name;
            e=trace_count; record(5,1,args);
            for (j=0;j<8;++j) model_metadata[j]=value(e+stage*8u+j);
            model_valid=1; v=lookup_result(stage);
            if (v && !(v&0x80000000u)) {
                e=trace_count;
                if (stage==1 || stage==6) {
                    args[0]=get(name); record(8,1,args); put(destination,value(e),1);
                } else {
                    for (j=0;j<8;++j) args[j]=model_metadata[j];
                    if (stage==3 || stage==4) { args[8]=destination; record(7,9,args); }
                    else { record(6,8,args); put(destination,value(e),1); }
                }
            }
            ++stage;
        } else { args[0]=1; record(9,1,args); }
    }
}
uint32_t entry_checked_call(void (*function)(void));
static int exercise(void)
{
    uint32_t implementation,i;
    oracle(); if (failure) return 4;
    for (implementation=0;implementation<3;++implementation) {
        void (*function)(void)=implementation==1 ? kernel_startup_ui :
            (void (*)(void))(0x81067a+(implementation==0 ? 0x100000 : 0));
        initialize();
        if (entry_checked_call(function)) return 10+implementation;
        if (failure || cursor!=trace_count || lookup_counter!=7) return 20+implementation;
        for (i=0;i<SIZE;++i) if (((volatile uint8_t *)BASE)[i]!=expected[i]) return 30+implementation;
    }
    ++cases; return 0;
}
static void number(uint32_t v)
{
    char d[10]; uint32_t i=sizeof(d);
    do { d[--i]='0'+v%10; v/=10; } while (v);
    syscall3(4,1,(uint32_t)(d+i),sizeof(d)-i);
}
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static int run(void)
{
    uint32_t error;
    if (!map(BASE,SIZE) || !map(0x900000,0x40000) || !map(0x800000,0x40000)) return 1;
    if (!load("build/startup_ui_original.x",0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/openicestaros_kernel.x",0x800000)) return 3;
#else
    if (!load("build/startup_ui_preview.x",0x800000)) return 3;
#endif
    redirect(0x900000,original_calls,COUNT(original_calls));
    redirect(0x800000,candidate_calls,COUNT(candidate_calls));
    for (pattern=0;pattern<2;++pattern)
    for (layout=0;layout<8;++layout)
    for (geometry=0;geometry<COUNT(dimensions);++geometry)
    for (boot_index=0;boot_index<COUNT(boots);++boot_index)
    for (mask_index=0;mask_index<COUNT(masks);++mask_index)
    for (mutation_index=0;mutation_index<COUNT(mutations);++mutation_index) {
        if ((error=exercise())) {
            PRINT("FAIL "); number(error); PRINT(" case "); number(cases);
            PRINT(" pattern/layout/geometry/boot/mask/mutation ");
            number(pattern); PRINT("/"); number(layout); PRINT("/"); number(geometry);
            PRINT("/"); number(boot_index); PRINT("/"); number(mask_index);
            PRINT("/"); number(mutation_index); PRINT("\n"); return error;
        }
    }
    if (cases!=36864) return 40;
    PRINT("Startup UI original/C/fixed-address/oracle cases: "); number(cases);
    PRINT("; 110592 executions; signed centering, both drawings, 69 progress steps, seven resource branches, 62 waits, metadata mutations, 40 KiB memory and ABI verified\n");
    return 0;
}
void _start(void) { syscall3(1,run(),0,0); __builtin_unreachable(); }
