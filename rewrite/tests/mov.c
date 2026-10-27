#include <stdint.h>
#include "mov_bindings.h"

enum { RECORD0 = 0x700003, RECORD1 = 0x701005, DISPLAY0 = 0x710003,
       DISPLAY1 = 0x711005, PALETTE0 = 0x720003, PALETTE1 = 0x721005,
       TABLE0 = 0x730003, TABLE1 = 0x731005, MAX_CALLS = 160, MAX_POLLS = 24 };
struct call { uint32_t slot, table, tick, args[8], frames[4]; char text[32]; };
static struct call calls[MAX_CALLS], expected_calls[MAX_CALLS];
static unsigned call_count, expected_count, polls, images, releases, mode, seed, cases;
static uint32_t active_base, active_close, window_result, initial_tick, display_value;
static volatile uint32_t *frames;
static uint8_t expected_memory[0x6028];
static volatile uint32_t saved_sp __attribute__((used));
static volatile uint32_t saved_bp __attribute__((used));
static volatile uint32_t resume_pc __attribute__((used));
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

static int sys3(unsigned n, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}

static void print(const char *text)
{
    unsigned length = 0;
    while (text[length]) ++length;
    sys3(4, 1, (unsigned)text, length);
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

static int load(const char *path, uint32_t base)
{
    uint32_t *h = (uint32_t *)base;
    unsigned i;
    int fd = sys3(5, (unsigned)path, 0, 0), count;
    if (fd < 0) return 0;
    count = sys3(3, (unsigned)fd, base, 0x3a54);
    sys3(6, (unsigned)fd, 0, 0);
    if (count != 0x3a54 || h[0] != 10) return 0;
    for (i = 0; i < h[8]; ++i) WORD(base + WORD(base + h[9] + i * 4)) += base;
    h[7] = base;
    for (i = h[9]; i < h[9] + h[8] * 4 + h[11] + h[3] * 4096; ++i)
        ((uint8_t *)base)[i] = 0;
    return 1;
}

__attribute__((naked, noinline)) static void enter(uint32_t function __attribute__((unused)))
{
    __asm__ volatile ("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
                      "movl %esp,saved_sp\n\tmovl %ebp,saved_bp\n\tmovl $1f,resume_pc\n\t"
                      "call *8(%ebp)\n\t1: popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret");
}

__attribute__((naked, noreturn)) static void finish(void)
{
    __asm__ volatile ("movl saved_sp,%esp\n\tmovl saved_bp,%ebp\n\t"
                      "movl resume_pc,%edx\n\tjmp *%edx");
    __builtin_unreachable();
}

static struct call *record(unsigned slot, unsigned table, unsigned count, const uint32_t *args)
{
    unsigned i;
    struct call *call;
    if (call_count == MAX_CALLS) fail("MOV trace overflow");
    call = &calls[call_count++];
    for (i = 0; i < sizeof(*call); ++i) ((uint8_t *)call)[i] = 0;
    call->slot = slot; call->table = table; call->tick = WORD(0x1b084);
    for (i = 0; i < count; ++i) call->args[i] = args[i];
    for (i = 0; i < 4; ++i) call->frames[i] = frames[i];
    if (mode & 2) WORD(0x1b0e4) = call_count & 1 ? TABLE1 : TABLE0;
    if (mode & 4) WORD(0x1b084) += slot * 0x1020304u + call_count * 13u;
    return call;
}

static uint32_t create(unsigned table, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        uint32_t flags, uint32_t color, uint32_t style, const char *title)
{
    const uint32_t args[] = {x, y, w, h, flags, color, style, (uint32_t)title - active_base};
    struct call *call = record(100, table, 8, args);
    unsigned i;
    for (i = 0; i < 9; ++i) call->text[i] = title[i];
    if (mode & 1) {
        WORD(0x6a0000u + window_result * 4u) = RECORD1;
        WORD(0x1b034) = DISPLAY1;
        WORD(0x1b0e0) = PALETTE1;
    }
    return window_result;
}

static uint32_t image_create(unsigned table, uint32_t window, uint32_t x, uint32_t y,
                              uint32_t w, uint32_t h, uint32_t flags, const char *path)
{
    const uint32_t args[] = {window, x, y, w, h, flags, (uint32_t)path - active_base};
    struct call *call = record(61, table, 7, args);
    unsigned i;
    uint32_t rec = WORD(0x6a0000u + window * 4u);
    if (WORD(rec + 0x5b7) != active_base + active_close) fail("MOV callback registration");
    for (i = 0; i < 30; ++i) call->text[i] = path[i];
    ++images;
    if (mode & 1) frames[(images + 1) % 4] ^= images * 0x1234567u;
    return (seed * 0xabcdef01u) ^ (images * 0x76543210u);
}

static uint32_t image_show(unsigned table, uint32_t value)
{
    record(65, table, 1, &value);
    if (mode & 1) frames[call_count % 4] ^= call_count * 0x76543u;
    return value ^ 0xfefefefe;
}

static uint32_t release(unsigned table, uint32_t value)
{
    record(62, table, 1, &value);
    ++releases;
    if (mode & 1) frames[releases % 4] ^= releases * 0xfedcba9u;
    return value ^ (releases * 0x98765432u) ^ table;
}

static uint32_t terminate(unsigned table, uint32_t value)
{
    record(107, table, 1, &value);
    if (mode & 1) {
        frames[0] ^= 0xcafebabe;
        WORD(0x6a0000u + window_result * 4u) = RECORD0;
        WORD(RECORD0 + 4) ^= 0x12345678;
    }
    return 0x12345678;
}

static uint32_t show(unsigned table, uint32_t value)
{
    record(105, table, 1, &value);
    return 0x23456789;
}

static uint32_t refresh(unsigned table, uint32_t value)
{
    record(110, table, 1, &value);
    if (mode & 1) frames[(polls + 2) % 4] ^= 0x10203040;
    return 0x3456789a;
}

static uint32_t poll(unsigned table)
{
    static const uint32_t offsets[MAX_POLLS] = {
        9, 10, 19, 20, 29, 30, 39, 40, 49, 50, 0x7fffffff, 0x80000000,
        0xfffffff5, 0xfffffff6, 0xfffffffe, 0xffffffff, 0, 10, 20, 30, 40, 50, 60, 70};
    record(108, table, 0, 0);
    ++polls;
    if ((mode & 8) && polls == 5 && (int32_t)window_result > 0) {
        uint32_t rec = WORD(0x6a0000u + window_result * 4u);
        uint32_t callback = WORD(rec + 0x5b7);
        if (callback != active_base + active_close) fail("MOV poll callback target");
        ((uint32_t (*)(uint32_t))callback)(window_result);
    }
    if (polls == MAX_POLLS) finish();
    WORD(0x1b084) = initial_tick + offsets[polls - 1];
    return 0x456789ab;
}

#define CREATE(t) \
static uint32_t create##t(uint32_t x,uint32_t y,uint32_t w,uint32_t h, \
                           uint32_t f,uint32_t c,uint32_t s,const char *title) \
{ return create(t,x,y,w,h,f,c,s,title); } \
static uint32_t image_create##t(uint32_t w,uint32_t x,uint32_t y,uint32_t a, \
                                 uint32_t b,uint32_t f,const char *path) \
{ return image_create(t,w,x,y,a,b,f,path); } \
static uint32_t poll##t(void) { return poll(t); }
CREATE(0)
CREATE(1)
#define ONE(name, t) static uint32_t name##t(uint32_t value) { return name(t, value); }
ONE(image_show, 0) ONE(image_show, 1)
ONE(release, 0) ONE(release, 1)
ONE(terminate, 0) ONE(terminate, 1)
ONE(show, 0) ONE(show, 1)
ONE(refresh, 0) ONE(refresh, 1)

static void initialize(uint32_t base, unsigned close_offset)
{
    static const unsigned slots[] = {61, 62, 65, 100, 105, 107, 108, 110};
    const uint32_t functions[2][8] = {
        {(uint32_t)image_create0,(uint32_t)release0,(uint32_t)image_show0,(uint32_t)create0,
         (uint32_t)show0,(uint32_t)terminate0,(uint32_t)poll0,(uint32_t)refresh0},
        {(uint32_t)image_create1,(uint32_t)release1,(uint32_t)image_show1,(uint32_t)create1,
         (uint32_t)show1,(uint32_t)terminate1,(uint32_t)poll1,(uint32_t)refresh1}};
    unsigned i;
    active_base = base; active_close = close_offset;
    frames = (volatile uint32_t *)(base + 0x3a14);
    for (i = 0; i < 4; ++i) frames[i] = (seed * 0x1234567u) ^ (i * 0x87654321u);
    for (i = 0; i < 0x2000; ++i) {
        ((volatile uint8_t *)0x700000)[i] = (uint8_t)(i * 37 + seed * 59);
        ((volatile uint8_t *)0x710000)[i] = (uint8_t)(i * 41 + seed * 61);
        ((volatile uint8_t *)0x720000)[i] = (uint8_t)(i * 43 + seed * 71);
    }
    WORD(RECORD0 + 4) = seed * 0x31415927u;
    WORD(RECORD1 + 4) = seed * 0xabcdef98u;
    WORD(DISPLAY0 + 0x10) = display_value;
    WORD(DISPLAY1 + 0x10) = ~display_value;
    for (i = 0; i < 8; ++i) {
        WORD(TABLE0 + slots[i] * 4) = functions[0][i];
        WORD(TABLE1 + slots[i] * 4) = functions[1][i];
    }
    WORD(0x6a0000u + window_result * 4u) = RECORD0;
    WORD(0x1b0e4) = TABLE0;
    WORD(0x1b0e0) = PALETTE0;
    WORD(0x1b034) = DISPLAY0;
    WORD(0x1b084) = initial_tick;
    call_count = polls = images = releases = 0;
}

static int snapshot(unsigned save)
{
    const uint32_t starts[] = {active_base + 0x3a14, 0x700000, 0x710000, 0x720000,
        0x1b0e0, 0x1b034, 0x1b084, 0x6a0000u + window_result * 4u};
    const unsigned sizes[] = {16, 0x2000, 0x2000, 0x2000, 8, 4, 4, 4};
    unsigned region, i, offset = 0;
    if (WORD(RECORD0 + 0x5b7) == active_base + active_close) WORD(RECORD0 + 0x5b7) = 0xc010c010;
    if (WORD(RECORD1 + 0x5b7) == active_base + active_close) WORD(RECORD1 + 0x5b7) = 0xc010c010;
    if (save) {
        expected_count = call_count;
        for (i = 0; i < call_count; ++i) expected_calls[i] = calls[i];
    } else {
        if (expected_count != call_count) { print("trace count\n"); return 0; }
        for (i = 0; i < call_count * sizeof(struct call); ++i)
            if (((uint8_t *)expected_calls)[i] != ((uint8_t *)calls)[i]) {
                print("trace call "); number(i / sizeof(struct call));
                print(" byte "); number(i % sizeof(struct call)); print("\n"); return 0;
            }
    }
    for (region = 0; region < sizeof(starts) / sizeof(starts[0]); ++region)
        for (i = 0; i < sizes[region]; ++i, ++offset) {
            uint8_t actual = *(volatile uint8_t *)(starts[region] + i);
            if (save) expected_memory[offset] = actual;
            else if (expected_memory[offset] != actual) {
                print("memory byte "); number(starts[region] + i); print("\n"); return 0;
            }
        }
    return 1;
}

static int exercise(unsigned function, uint32_t original_base, uint32_t rewritten_base)
{
    uint32_t expected_result = 0, result;
    unsigned copy;
    for (copy = 0; copy < 2; ++copy) {
        const unsigned *offsets = copy ? rewritten_functions : original_functions;
        uint32_t base = copy ? rewritten_base : original_base;
        initialize(base, offsets[1]);
        result = 0;
        if (function == 0) enter(base + offsets[0]);
        else result = ((uint32_t (*)(uint32_t))(base + offsets[1]))(window_result);
        if (!copy) { snapshot(1); expected_result = result; }
        else if (!snapshot(0) || result != expected_result) return 0;
    }
    ++cases;
    return 1;
}

static int run(void)
{
    static const uint32_t handles[] = {0, 1, 2, 7, 0x40000000, 0x7fffffff, 0x80000000, 0xffffffff};
    static const uint32_t sizes[] = {0, 1, 3, 1024, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffd, 0xfffffffe, 0xffffffff};
    static const uint32_t ticks[] = {0, 1, 0x7fffffff, 0x80000000, 0xfffffff0, 0xfffffff5, 0xfffffff6, 0xffffffff};
    unsigned pass, h, t, function;
    if (!map(0x10000, 0x20000) || !map(0x690000, 0x20000) || !map(0x700000, 0x40000) ||
        !map(0x1000000, 0x20000) || !map(0x1200000, 0x20000)) return 1;
    for (pass = 0; pass < 2; ++pass) {
        uint32_t a = 0x1000000 + (pass ? 0x113 : 0), b = 0x1200000 + (pass ? 0x513 : 0);
        if (!load("build/mov.original.x", a) || !load("build/mov.x", b)) return 2;
        for (mode = 0; mode < 16; ++mode)
            for (seed = 0; seed < 10; ++seed)
                for (t = 0; t < 8; ++t)
                    for (h = 0; h < 8; ++h)
                        for (function = 0; function < 2; ++function) {
                            window_result = handles[h]; initial_tick = ticks[t]; display_value = sizes[seed];
                            if (!exercise(function, a, b)) {
                                print("MOV mismatch function "); number(function);
                                print(" mode "); number(mode); print(" seed "); number(seed);
                                print(" tick "); number(initial_tick); print(" handle "); number(window_result);
                                print("\n"); return 10;
                            }
                        }
    }
    print("MOV original/C differential cases: "); number(cases); print("\n");
    return 0;
}

void _start(void) { sys3(1, (unsigned)run(), 0, 0); __builtin_unreachable(); }
