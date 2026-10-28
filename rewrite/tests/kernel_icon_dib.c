/* Real decoder execution with only pixel/quantizer CALL boundaries replaced.
 * Original x86, ordinary C and fixed-address C are compared with a bytewise
 * model, including ordered pixel arguments, callback mutations and the ABI.
 */
#include <stdint.h>
#include "../kernel/icon_dib.h"
#define BASE 0x600000u
#define REGION 32768u
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
static uint8_t initial[REGION], expected[REGION];
static struct event { uint32_t kind, a[5]; } events[16384];
static uint32_t arguments[4], dib, surface, pixels, mask, stride, height;
static uint32_t mutation, planned, observed, failure, bounds, cases, stage;
static uint32_t implementation, scenario, branch[4], skipped, event_totals[4];
static const uint32_t dimensions[] = {0, 1, 7, 31, 255, 256, 0x7fffffff,
    0x80000000, 0xfffffffe, 0xffffffff};
#include "../build/icon_dib_resources.h"

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0'+value%10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(digits+i), sizeof(digits)-i);
}
static int map(uint32_t address, uint32_t size)
{
    uint32_t args[] = {address, size, 7, 0x32, 0xffffffff, 0};
    return (uint32_t)syscall3(90, (uint32_t)args, 0, 0) == address;
}
static int load(const char *path, uint32_t address)
{
    int fd = syscall3(5, (uint32_t)path, 0, 0), size;
    if (fd < 0) return 0;
    size = syscall3(3, (uint32_t)fd, address, 0x40000);
    syscall3(6, (uint32_t)fd, 0, 0);
    return size == 250099;
}
static uint32_t get8(uint32_t address)
{
    if (address-BASE >= REGION) { bounds = 1; return 0; }
    return expected[address-BASE];
}
static uint32_t get16(uint32_t address)
{ return get8(address) + 256u*get8(address+1u); }
static uint32_t get32(uint32_t address)
{
    uint32_t value = 0, i;
    for (i = 0; i < 4; ++i) value |= get8(address+i) << (8*i);
    return value;
}
static void set8(uint32_t address, uint32_t value)
{ initial[address-BASE] = (uint8_t)value; }
static void set16(uint32_t address, uint32_t value)
{ set8(address, value); set8(address+1, value/256); }
static void set32(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) set8(address+i, value >> (8*i));
}
static void effect(uint32_t address, uint32_t value, uint32_t bytes, int model)
{
    uint32_t i;
    for (i = 0; i < bytes; ++i) {
        if (address+i-BASE >= REGION) { bounds = 1; return; }
        if (model) expected[address+i-BASE] = (uint8_t)(value >> (8*i));
        else *(volatile uint8_t *)(uintptr_t)(address+i) = (uint8_t)(value >> (8*i));
    }
}
static uint32_t quantized(const uint32_t *a)
{
    /* Upper result bits are deliberately nonzero: the caller must use AL. */
    return 0xa5000000u | ((a[0]*0x1f123u+a[1]*0x25101u+a[2]*0x10901u+
                          a[3]*0xabc3u+a[4]*0x1973u) & 0xffffffu);
}
static void mutate(const struct event *e, uint32_t index, int model)
{
    uint32_t value = e->kind*0x291173u+e->a[0]*17u+e->a[1]*19u+
                     e->a[2]*31u+e->a[3]*37u+e->a[4]*41u+index*0x10203u;
    if (mutation & 1u) {
        effect(dib+4u, 0xdeadbeefu, 4, model);
        effect(dib+8u, 0xffffffffu, 4, model);
        effect(dib+14u, index%2 ? 8u : 32u, 2, model);
        effect(surface+36u, 0x87654321u, 4, model);
    }
    if (mutation & 2u) {
        effect(pixels+(index*13u)%96u, value, 4, model);
        effect(dib+40u+4u*((index*17u)%256u), value^0x9112deadu, 4, model);
        effect(mask-(index%height)*stride+index%8u, value >> 24, 1, model);
    }
    /* The pixel boundary writes a small deterministic output area. This is
     * compared along with the entire input and surface metadata region. */
    if (e->kind != 3u)
        effect(BASE+0x7100u+4u*(index%32u), value, 4, model);
}
static uint32_t event(uint32_t kind, uint32_t a, uint32_t b, uint32_t c,
                      uint32_t d, uint32_t e, int model)
{
    struct event item = {kind, {a,b,c,d,e}};
    uint32_t i, index = model ? planned++ : observed++;
    if (index >= 16384u) { failure = 1; return 0; }
    if (model) { events[index] = item; ++event_totals[kind]; }
    else {
        if (index >= planned || events[index].kind != kind) failure = 1;
        for (i = 0; i < 5; ++i) if (events[index].a[i] != item.a[i]) failure = 1;
    }
    mutate(&item, index, model);
    return kind == 3u ? quantized(item.a) : 0;
}
void kernel_icon_put32_81b3c0(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{ event(1, a,b,c,d,0,0); }
void kernel_icon_put8_81b37c(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{ event(2, a,b,c,d,0,0); }
uint32_t kernel_icon_quantize_81fd90(uint32_t a, uint32_t b, uint32_t c,
                                    uint32_t d, uint32_t e)
{ return event(3, a,b,c,d,e,0); }

static void prepare(uint32_t seed, uint32_t w, uint32_t h,
                     uint32_t source, uint32_t target, uint32_t mut, uint32_t pattern)
{
    uint32_t i;
    scenario = seed;
    for (i = 0; i < REGION; ++i) initial[i] = (uint8_t)(i*37u+(i/32u)*11u+seed*83u);
    dib = BASE+0x101u+seed%4u;
    surface = BASE+0x7001u+(seed/4u)%4u;
    arguments[0] = surface; arguments[1] = dimensions[seed%10u];
    arguments[2] = dimensions[(seed/10u)%10u]; arguments[3] = dib;
    mutation = mut;
    set32(dib+4u, w); set32(dib+8u, h*2u); set16(dib+14u, source);
    set32(surface+36u, target);
    pixels = dib+(source == 32u ? 40u : 1064u);
    stride = h < 48u ? 4u : 8u; height = h;
    mask = pixels+w*h*(source == 32u ? 4u : 1u)+(h-1u)*stride;
    if (pattern < 256u && w <= 65u && h && h <= 64u)
        for (i = 0; i < h*stride+w+8u; ++i)
            set8(pixels+w*h*(source == 32u ? 4u : 1u)+i, pattern);
}

/* Independent row/column model: it indexes each mask bit directly and uses
 * bytewise little-endian loads rather than the implementation's pointer walk.
 */
static void plan(void)
{
    uint32_t w = get32(dib+4u), h = get32(dib+8u)/2u;
    uint32_t source = get16(dib+14u), target, row, col, bytes, mask_start, palette;
    if (source != 8u && source != 32u) { ++skipped; return; }
    target = get32(arguments[0]+36u);
    if (target != 8u && target != 32u) { ++skipped; return; }
    ++branch[(source == 32u ? 2u : 0u)+(target == 32u ? 1u : 0u)];
    bytes = source/8u;
    palette = dib+40u;
    mask_start = dib+40u+(source == 8u ? 1024u : 0u)+w*h*bytes;
    for (row = 0; row < h; ++row) for (col = 0; col < w; ++col) {
        uint32_t mask_byte = col/8u+(source == 32u && target == 8u ? col : 0u);
        uint32_t mask_address = mask_start+(h-row-1u)*(h < 48u ? 4u : 8u)+mask_byte;
        uint32_t pixel, color;
        if (get8(mask_address) & (128u/(1u << (col%8u)))) continue;
        pixel = dib+40u+(source == 8u ? 1024u : 0u)+((h-row-1u)*w+col)*bytes;
        if (source == 8u) pixel = palette+get8(pixel)*4u;
        if (target == 32u) {
            color = get32(pixel);
            if (source == 32u) color %= 0x1000000u;
            event(1, arguments[0], arguments[1]+col, arguments[2]+row, color, 0, 1);
        } else {
            uint32_t blue = get8(pixel), green = get8(pixel+1u), red = get8(pixel+2u);
            color = event(3, red,green,blue,col,row,1)%256u;
            event(2, arguments[0], arguments[1]+col, arguments[2]+row, color,0,1);
        }
    }
}
uint32_t icon_dib_checked_call(uint32_t target, const uint32_t *args);
static int exercise(void)
{
    uint32_t i, target;
    bounds = failure = planned = 0;
    for (i = 0; i < REGION; ++i) expected[i] = initial[i];
    plan();
    if (bounds || failure) return 3;
    for (implementation = 0; implementation < 3; ++implementation) {
        target = implementation == 0 ? 0x92b20cu : implementation == 1 ?
                 (uint32_t)kernel_icon_dib_82b20c : 0x82b20cu;
        for (i = 0; i < REGION; ++i) ((volatile uint8_t *)BASE)[i] = initial[i];
        observed = failure = 0;
        if (icon_dib_checked_call(target, arguments)) return 10;
        if (failure || bounds || observed != planned) return 20;
        for (i = 0; i < REGION; ++i)
            if (((volatile uint8_t *)BASE)[i] != expected[i]) return 30;
    }
    ++cases;
    return 0;
}
#define EXERCISE() do { error = exercise(); if (error) return error; } while (0)
static int run(void)
{
    uint32_t a,b,c,d,m,i,error;
    const uint32_t widths[] = {0,1,2,7,8,9,15,16,17,23,24,31,32,33,47,48,49,63,64,65};
    const uint32_t heights[] = {0,1,2,3,7,8,15,16,31,32,46,47,48,49,63,64};
    const uint32_t targets[] = {0,1,7,8,9,15,16,24,31,32,33,64,255,256,
        0x10008,0x10020,0xffff0008,0xffff0020,0x7fffffff,0x80000000,0xffffffff};
    if (!map(BASE,REGION) || !map(0x800000,0x40000) || !map(0x900000,0x40000) ||
        !load("build/icon_dib_original.x",0x900000)) return 1;
#ifdef TEST_INSTALLED
    if (!load("build/openicestaros_kernel.x",0x800000)) return 2;
#else
    if (!load("build/icon_dib_preview.x",0x800000)) return 2;
#endif
    {
#include "../build/icon_dib_calls.h"
    }
    /* Every WORD source depth, with valid depths actually drawing. */
    stage = 1;
    for (a = 0; a < 65536u; ++a) {
        prepare(a,1,1,a,32,0,0); EXERCISE();
    }
    /* Target depth is a DWORD, and cannot be truncated to its low WORD. */
    stage = 2;
    for (a = 0; a < sizeof(targets)/sizeof(*targets); ++a)
    for (b = 0; b < 2; ++b) for (c = 0; c < 4; ++c) {
        prepare(a*8+b*4+c,3,2,b ? 32 : 8,targets[a],c,0); EXERCISE();
    }
    /* All mask bytes in four source/target branches and four alignments. */
    stage = 3;
    for (a = 0; a < 256u; ++a) for (b = 0; b < 4; ++b)
    for (c = 0; c < 4; ++c) {
        prepare(a*16+b*4+c,17,3,b/2u ? 32 : 8,b%2u ? 32 : 8,0,a); EXERCISE();
    }
    /* A single set bit at each traversed mask address distinguishes the
     * 32 -> 8 extra-byte walk from ordinary packed bit addressing. */
    for (a = 0; a < 65u; ++a) for (b = 0; b < 4; ++b)
    for (c = 0; c < 4; ++c) {
        prepare(a*16+b*4+c,65,2,b/2u ? 32 : 8,b%2u ? 32 : 8,0,0);
        set8(mask+a/8u+(b == 2u ? a : 0u), 128u/(1u << (a%8u)));
        EXERCISE();
    }
    /* Rectangular images test packed rows and height-selected mask stride,
     * including the exact 47/48 boundary, odd doubled heights and mutations. */
    stage = 4;
    for (a = 0; a < sizeof(widths)/sizeof(*widths); ++a)
    for (b = 0; b < sizeof(heights)/sizeof(*heights); ++b)
    for (c = 0; c < 4; ++c) for (m = 0; m < 4; ++m) {
        uint32_t seed = ((a*16+b)*4+c)*4+m;
        prepare(seed,widths[a],heights[b],c/2u ? 32 : 8,c%2u ? 32 : 8,m,256);
        set32(dib+8u, heights[b]*2u+(seed%2u)); EXERCISE();
    }
    /* All palette indices and alpha bytes; high palette bytes are retained
     * for 8 -> 32, while 32 -> 32 drops alpha. B/G/R stay in order. */
    stage = 5;
    for (a = 0; a < 256u; ++a) for (b = 0; b < 4; ++b) {
        prepare(a*4+b,1,1,b/2u ? 32 : 8,b%2u ? 32 : 8,0,0);
        if (b/2u) set32(pixels, a*0x1000000u+0x123456u);
        else { set8(pixels,a); set32(dib+40+4*a,a*0x1000000u+0x123456u); }
        EXERCISE();
    }
    /* Wrapped destination coordinates, local conversion coordinates and
     * repeated unaligned headers/surfaces across all four rendering paths. */
    stage = 6;
    for (a = 0; a < 10; ++a) for (b = 0; b < 10; ++b)
    for (c = 0; c < 4; ++c) for (d = 0; d < 4; ++d) {
        prepare(((a*10+b)*4+c)*4+d,9,2,c/2u ? 32 : 8,c%2u ? 32 : 8,d,0);
        arguments[1] = dimensions[a]; arguments[2] = dimensions[b]; EXERCISE();
    }
    /* Huge widths with zero height and huge dimensions with invalid target
     * depth must perform no pixel/mask accesses. Invalid source depth does
     * not dereference even an unmapped surface argument. */
    stage = 7;
    for (a = 0; a < 10; ++a) for (b = 0; b < 4; ++b) {
        prepare(a*4+b,dimensions[a],0,b/2u ? 32 : 8,b%2u ? 32 : 8,0,256);
        EXERCISE();
        prepare(a*4+b,dimensions[a],0,b/2u ? 32 : 8,0x10020,0,256);
        set32(dib+8u,0xffffffffu); EXERCISE();
    }
    prepare(0,1,1,24,32,0,256); arguments[0] = 0xdead0000; EXERCISE();
    /* Every original non-PNG frame in four standalone icons and four X
     * embedded ICO packages. Bytes beyond each payload retain a defined
     * pattern because original mask addressing may read past that payload. */
    stage = 8;
    for (a = 0; a < sizeof(resource_fixtures)/sizeof(*resource_fixtures); ++a)
    for (b = 0; b < 2; ++b) for (m = 0; m < 4; ++m) {
        prepare(a*8+b*4+m,1,1,32,b ? 32 : 8,m,256);
        for (i = 0; i < resource_fixtures[a].size; ++i)
            set8(dib+i,resource_fixtures[a].bytes[i]);
        for (i = 0; i < REGION; ++i) expected[i] = initial[i];
        c = get16(dib+14u); height = get32(dib+8u)/2u;
        stride = height < 48u ? 4u : 8u;
        pixels = dib+(c == 32u ? 40u : 1064u);
        mask = pixels+get32(dib+4u)*height*(c == 32u ? 4u : 1u)+(height-1u)*stride;
        EXERCISE();
    }
    PRINT("Total cases: "); number(cases); PRINT("; executions: "); number(cases*3);
    PRINT("; 8->8/8->32/32->8/32->32: ");
    for (i = 0; i < 4; ++i) { number(branch[i]); PRINT(" "); }
    PRINT("; rejected depths: "); number(skipped);
    PRINT("; put32/put8/quantizer events: ");
    for (i = 1; i < 4; ++i) { number(event_totals[i]); PRINT(" "); }
    PRINT("; DIB decoder verified against pixel/event/32 KiB memory/ABI oracle\n");
    return 0;
}
void _start(void)
{
    int result = run();
    if (result) {
        PRINT("DIB error "); number((uint32_t)result); PRINT(" stage "); number(stage);
        PRINT(" scenario "); number(scenario); PRINT(" implementation "); number(implementation);
        PRINT(" calls "); number(observed); PRINT("/"); number(planned); PRINT("\n");
    }
    syscall3(1,(uint32_t)result,0,0);
    __builtin_unreachable();
}
