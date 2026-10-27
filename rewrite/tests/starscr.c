#include <stdint.h>
#include "../apps/starscr.h"
#include "starscr_bindings.h"

enum { RECORD0 = 0x700003, RECORD1 = 0x701005, DISPLAY0 = 0x710003,
       DISPLAY1 = 0x711005, TABLE0 = 0x730003, TABLE1 = 0x731005, MAX_CALLS = 160 };
struct call { uint32_t slot, table, args[8], state[4]; char title[12]; };
static struct call calls[MAX_CALLS], expected_calls[MAX_CALLS];
static unsigned call_count, expected_count, sleeps, mode, seed, cases;
static uint32_t active_base, active_key, active_close, window_result, dimension, key, pressed, phase;
static volatile struct starscr_state *state;
static uint8_t expected_memory[0x4020];
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
static int load(const char *path, uint32_t base)
{
    uint32_t *h = (uint32_t *)base;
    unsigned i;
    int fd = sys3(5, (unsigned)path, 0, 0), count;
    if (fd < 0) return 0;
    count = sys3(3, (unsigned)fd, base, 0x3eb4);
    sys3(6, (unsigned)fd, 0, 0);
    if (count != 0x3eb4 || h[0] != 10) return 0;
    for (i = 0; i < h[8]; ++i) WORD(base + WORD(base + h[9] + i * 4)) += base;
    h[7] = base;
    for (i = h[9]; i < h[9] + h[8] * 4 + h[11] + h[3] * 4096; ++i)
        ((uint8_t *)base)[i] = 0;
    return 1;
}

__attribute__((naked, noinline)) static uint32_t enter(uint32_t function __attribute__((unused)))
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
    struct call *c;
    if (call_count == MAX_CALLS) fail("STARSCR trace overflow");
    c = &calls[call_count++];
    for (i = 0; i < sizeof(*c); ++i) ((uint8_t *)c)[i] = 0;
    c->slot = slot; c->table = table;
    for (i = 0; i < count; ++i) c->args[i] = args[i];
    for (i = 0; i < 4; ++i) c->state[i] = ((volatile uint32_t *)state)[i];
    if (mode & 2) WORD(0x1b0e4) = call_count & 1 ? TABLE1 : TABLE0;
    if (mode & 1) {
        state->record = call_count & 1 ? RECORD1 : RECORD0;
        state->surface ^= call_count * 0x1234567u;
        state->window ^= slot * 0xabcdefu;
        WORD(state->record + 0x143) ^= call_count * 0x7654321u;
        WORD(state->record + 0x147) += call_count * 0x1234567u;
        WORD(0x1b034) = call_count & 1 ? DISPLAY1 : DISPLAY0;
    }
    return c;
}
static uint32_t create(unsigned table, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        uint32_t flags, uint32_t color, uint32_t style, const char *title)
{
    const uint32_t args[] = {x, y, w, h, flags, color, style, (uint32_t)title - active_base};
    struct call *c = record(100, table, 8, args);
    unsigned i;
    for (i = 0; i < 12; ++i) c->title[i] = title[i];
    if (mode & 1) WORD(0x6a0000u + window_result * 4u) = RECORD1;
    return window_result;
}
static uint32_t rectangle(unsigned table, uint32_t s, uint32_t x1, uint32_t y1,
                           uint32_t x2, uint32_t y2, uint32_t color)
{
    const uint32_t args[] = {s, x1, y1, x2, y2, color};
    record(2, table, 6, args);
    return color ^ 0x10203040;
}
static uint32_t circle(unsigned table, uint32_t s, uint32_t x, uint32_t y, uint32_t r, uint32_t color)
{
    const uint32_t args[] = {s, x, y, r, color};
    record(5, table, 5, args);
    return color ^ 0xfedcba98;
}
static uint32_t show(unsigned table, uint32_t value)
{
    uint32_t rec = WORD(0x6a0000u + window_result * 4u);
    if (WORD(rec + 0x593) != active_base + active_key ||
        WORD(rec + 0x5b7) != active_base + active_close || WORD(rec + 0x11f) != 4)
        fail("STARSCR callback/flags registration");
    record(105, table, 1, &value);
    return 0x23456789;
}
static uint32_t terminate(unsigned table, uint32_t value)
{
    record(107, table, 1, &value);
    return value ^ seed * 0xfedcba98u ^ table;
}
static uint32_t refresh(unsigned table, uint32_t value)
{
    record(110, table, 1, &value);
    return 0x3456789a;
}
static uint32_t wait_ticks(unsigned table, uint32_t value)
{
    record(113, table, 1, &value);
    ++sleeps;
    if ((mode & 8) && sleeps == 3) {
        uint32_t rec = WORD(0x6a0000u + window_result * 4u);
        uint32_t callback = WORD(rec + 0x593);
        if (callback != active_base + active_key) fail("STARSCR key dispatch");
        ((void (*)(uint32_t,uint32_t,uint32_t))callback)(window_result, 0x1b, 1);
    }
    if (sleeps == 12) finish();
    return 0x456789ab;
}
static uint32_t close_window(unsigned table, uint32_t value)
{
    record(128, table, 1, &value);
    if (mode & 8) ((uint32_t (*)(void))(active_base + active_close))();
    return 0x56789abc;
}
static uint32_t palette(unsigned table, uint32_t value)
{
    record(237, table, 1, &value);
    return value * 0xabcdef01u ^ seed * 0xfedcba98u ^ call_count;
}
static void alter(volatile uint32_t *value)
{
    if (mode & 4) *value ^= 0x87654321;
}
#define TABLE(t) \
static uint32_t create##t(uint32_t x,uint32_t y,uint32_t w,uint32_t h, \
                           uint32_t f,uint32_t c,uint32_t s,const char *title) \
{ uint32_t r = create(t,x,y,w,h,f,c,s,title); alter(&x); return r; } \
static uint32_t rectangle##t(uint32_t s,uint32_t x,uint32_t y,uint32_t a,uint32_t b,uint32_t c) \
{ uint32_t r = rectangle(t,s,x,y,a,b,c); alter(&s); return r; } \
static uint32_t circle##t(uint32_t s,uint32_t x,uint32_t y,uint32_t radius,uint32_t c) \
{ uint32_t r = circle(t,s,x,y,radius,c); alter(&s); return r; }
TABLE(0) TABLE(1)
#define ONE(name,t) \
static uint32_t name##t(uint32_t value) { uint32_t r = name(t,value); alter(&value); return r; }
ONE(show,0) ONE(show,1) ONE(terminate,0) ONE(terminate,1)
ONE(refresh,0) ONE(refresh,1) ONE(wait_ticks,0) ONE(wait_ticks,1)
ONE(close_window,0) ONE(close_window,1) ONE(palette,0) ONE(palette,1)

static void initialize(uint32_t base, const unsigned *offsets)
{
    static const unsigned slots[] = {2,5,100,105,107,110,113,128,237};
    const uint32_t functions[2][9] = {
        {(uint32_t)rectangle0,(uint32_t)circle0,(uint32_t)create0,(uint32_t)show0,
         (uint32_t)terminate0,(uint32_t)refresh0,(uint32_t)wait_ticks0,(uint32_t)close_window0,(uint32_t)palette0},
        {(uint32_t)rectangle1,(uint32_t)circle1,(uint32_t)create1,(uint32_t)show1,
         (uint32_t)terminate1,(uint32_t)refresh1,(uint32_t)wait_ticks1,(uint32_t)close_window1,(uint32_t)palette1}};
    unsigned i;
    active_base = base; active_key = offsets[1]; active_close = offsets[2];
    state = (volatile struct starscr_state *)(base + 0x3ddc);
    state->record = RECORD0; state->window = seed * 0x98765432u;
    state->surface = seed * 0x12345678u; state->unused = seed ^ 0xdeadbeef;
    for (i = 0; i < 0x2000; ++i) {
        ((volatile uint8_t *)0x700000)[i] = (uint8_t)(i * 37 + seed * 59);
        ((volatile uint8_t *)0x710000)[i] = (uint8_t)(i * 41 + seed * 61);
    }
    WORD(RECORD0) = window_result ^ 0x12345678;
    WORD(RECORD1) = window_result ^ 0xabcdef01;
    WORD(RECORD0 + 4) = seed * 0x31415927u;
    WORD(RECORD1 + 4) = seed * 0xabcdef98u;
    WORD(RECORD0 + 0x143) = dimension; WORD(RECORD0 + 0x147) = ~dimension;
    WORD(RECORD1 + 0x143) = ~dimension; WORD(RECORD1 + 0x147) = dimension;
    WORD(DISPLAY0 + 0xc) = dimension; WORD(DISPLAY0 + 0x10) = ~dimension;
    WORD(DISPLAY1 + 0xc) = ~dimension; WORD(DISPLAY1 + 0x10) = dimension;
    for (i = 0; i < 9; ++i) {
        WORD(TABLE0 + slots[i] * 4) = functions[0][i];
        WORD(TABLE1 + slots[i] * 4) = functions[1][i];
    }
    WORD(0x6a0000u + window_result * 4u) = RECORD0;
    WORD(0x1b0e4) = TABLE0; WORD(0x1b034) = DISPLAY0;
    call_count = sleeps = 0;
}
static int snapshot(unsigned save)
{
    const uint32_t starts[] = {active_base + 0x3ddc,0x700000,0x710000,
                               0x1b0e4,0x1b034,0x6a0000u + window_result * 4u};
    const unsigned sizes[] = {16,0x2000,0x2000,4,4,4};
    unsigned region, i, offset = 0;
    const uint32_t records[] = {RECORD0,RECORD1};
    for (i = 0; i < 2; ++i) {
        if (WORD(records[i] + 0x593) == active_base + active_key) WORD(records[i] + 0x593) = 0xc010c010;
        if (WORD(records[i] + 0x5b7) == active_base + active_close) WORD(records[i] + 0x5b7) = 0xc020c020;
    }
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
static int exercise(unsigned function, uint32_t a, uint32_t b)
{
    uint32_t expected_result = 0, result;
    unsigned copy;
    for (copy = 0; copy < 2; ++copy) {
        const unsigned *offsets = copy ? rewritten_functions : original_functions;
        uint32_t base = copy ? b : a;
        initialize(base, offsets);
        result = 0;
        if (function == 0) {
            result = enter(base + offsets[0]);
            if ((int32_t)window_result > 0) result = 0; /* Successful entry never returns. */
        } else if (function == 1) {
            ((void (*)(uint32_t,uint32_t,uint32_t))(base + offsets[1]))(window_result,key,pressed);
        } else if (function == 2) result = ((uint32_t (*)(void))(base + offsets[2]))();
        else ((void (*)(uint32_t))(base + offsets[3]))(phase);
        if (!copy) { snapshot(1); expected_result = result; }
        else if (!snapshot(0) || result != expected_result) return 0;
    }
    ++cases;
    return 1;
}
static int run(void)
{
    static const uint32_t handles[] = {0,1,2,7,0x40000000,0x7fffffff,0x80000000,0xffffffff};
    static const uint32_t sizes[] = {0,1,3,1024,0x7fffffff,0x80000000,0x80000001,0xfffffffd,0xfffffffe,0xffffffff};
    static const uint32_t phases[] = {0,1,2,3,4,0x7fffffff,0x80000000,0xffffffff};
    static const uint32_t extra_keys[] = {0x100001b,0x8000001b,0xffffff1b,0xffffffff};
    static const uint32_t presses[] = {0,1,2,0x101,0x80000001,0xffffffff};
    unsigned pass, h, f, p, k;
    if (!map(0x10000,0x20000) || !map(0x690000,0x20000) || !map(0x700000,0x40000) ||
        !map(0x1000000,0x20000) || !map(0x1200000,0x20000)) return 1;
    for (pass = 0; pass < 2; ++pass) {
        uint32_t a = 0x1000000 + (pass ? 0x113 : 0), b = 0x1200000 + (pass ? 0x513 : 0);
        if (!load("build/starscr.original.x",a) || !load("build/starscr.x",b)) return 2;
        for (mode = 0; mode < 16; ++mode) {
            for (seed = 0; seed < 10; ++seed)
                for (h = 0; h < 8; ++h) {
                    window_result = handles[h]; dimension = sizes[seed];
                    for (f = 0; f < 10; ++f) {
                        phase = phases[f < 2 ? 0 : f - 2];
                        if (!exercise(f == 0 ? 0 : f == 1 ? 2 : 3,a,b)) goto mismatch;
                    }
                }
            seed = 3; dimension = 1024;
            for (k = 0; k < 260; ++k)
                for (p = 0; p < 6; ++p) {
                    key = k < 256 ? k : extra_keys[k - 256]; pressed = presses[p];
                    window_result = handles[(k + p) % 8];
                    if (!exercise(1,a,b)) goto mismatch;
                }
        }
    }
    print("STARSCR original/C differential cases: "); number(cases); print("\n");
    return 0;
mismatch:
    print("STARSCR mismatch mode "); number(mode); print(" seed "); number(seed);
    print(" handle "); number(window_result); print(" phase "); number(phase);
    print(" key "); number(key); print(" pressed "); number(pressed); print("\n");
    return 10;
}
void _start(void) { sys3(1,(unsigned)run(),0,0); __builtin_unreachable(); }
