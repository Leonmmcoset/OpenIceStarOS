#include <stdint.h>
#include "../apps/color.h"
#include "color_bindings.h"

enum { RECORD0 = 0x700003, RECORD1 = 0x701005, PALETTE0 = 0x720003,
       PALETTE1 = 0x721005, TABLE0 = 0x730003, TABLE1 = 0x731005, MAX_CALLS = 620 };
struct call { uint32_t slot, args[8], state[4]; char title[8]; };
static struct call calls[MAX_CALLS], expected_calls[MAX_CALLS];
static unsigned call_count, expected_count, rectangles, polls, mode, seed, cases;
static uint32_t active_base, active_close, window_result, surface_result, close_result;
static volatile struct color_state *state;
static volatile uint32_t *captured_palette;
static uint8_t expected_memory[0x4018];
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
    count = sys3(3, (unsigned)fd, base, 0x38b4);
    sys3(6, (unsigned)fd, 0, 0);
    if (count != 0x38b4 || h[0] != 10) return 0;
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

static struct call *record(unsigned slot, unsigned count, const uint32_t *args)
{
    unsigned i;
    struct call *call;
    if (call_count == MAX_CALLS) { print("COLOR trace overflow\n"); sys3(1, 200, 0, 0); }
    call = &calls[call_count++];
    for (i = 0; i < sizeof(*call); ++i) ((uint8_t *)call)[i] = 0;
    call->slot = slot;
    for (i = 0; i < count; ++i) call->args[i] = args[i];
    for (i = 0; i < 4; ++i) call->state[i] = ((volatile uint32_t *)state)[i];
    return call;
}

static uint32_t rectangle(uint32_t s, uint32_t x1, uint32_t y1,
                           uint32_t x2, uint32_t y2, uint32_t color)
{
    const uint32_t args[] = {s, x1, y1, x2, y2, color};
    record(2, 6, args);
    ++rectangles;
    if (mode & 1) {
        state->surface ^= 0x01020304;
        WORD(0x1b0e0) = PALETTE1;
        WORD(0x1b0e4) = rectangles & 1 ? TABLE1 : TABLE0;
        if (rectangles < 600) captured_palette[rectangles] ^= rectangles * 0x1234567u;
    }
    return color ^ 0xdeadbeef;
}

static uint32_t create(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        uint32_t flags, uint32_t color, uint32_t style, const char *title)
{
    const uint32_t args[] = {x, y, w, h, flags, color, style, 0};
    struct call *call = record(100, 8, args);
    unsigned i;
    for (i = 0; i < 5; ++i) call->title[i] = title[i];
    if (mode & 2) WORD(0x1b0e4) = TABLE1;
    return window_result;
}

static uint32_t surface(uint32_t window)
{
    record(111, 1, &window);
    if (mode & 2) {
        state->window_record = RECORD1;
        WORD(RECORD1 + 4) ^= 0xfedcba98;
        WORD(0x1b0e4) = TABLE0;
    }
    return surface_result;
}

static uint32_t show(uint32_t window)
{
    record(105, 1, &window);
    return 0xcafeface;
}

static uint32_t close_handle(uint32_t handle)
{
    record(107, 1, &handle);
    if (mode & 2) state->window_record = RECORD1;
    return close_result;
}

static uint32_t poll(void)
{
    record(108, 0, 0);
    ++polls;
    /* Let the original event mechanism invoke the registered close callback
       once, including cases where a service changed the active record. */
    if ((mode & 4) && polls == 1 && WORD(RECORD0 + 0x5b7) == active_base + active_close)
        ((uint32_t (*)(void))(active_base + active_close))();
    if (polls == 3) finish();
    return 0x87654321;
}

static void initialize(uint32_t base, unsigned close_offset)
{
    static const unsigned slots[] = {2, 100, 105, 107, 108, 111};
    const uint32_t functions[] = {(uint32_t)rectangle, (uint32_t)create, (uint32_t)show,
                                  (uint32_t)close_handle, (uint32_t)poll, (uint32_t)surface};
    unsigned i;
    active_base = base;
    active_close = close_offset;
    state = (volatile struct color_state *)(base + 0x3898);
    for (i = 0; i < 16; ++i) ((volatile uint8_t *)state)[i] = (uint8_t)(seed * 41 + i * 17);
    state->window_record = RECORD0;
    state->surface = surface_result ^ 0xa55aa55a;
    for (i = 0; i < 0x2000; ++i) ((volatile uint8_t *)0x700000)[i] = (uint8_t)(i * 37 + seed * 59);
    for (i = 0; i < 0x2000; ++i) ((volatile uint8_t *)0x720000)[i] = (uint8_t)(i * 43 + seed * 71);
    WORD(RECORD0 + 4) = close_result ^ 0x31415927;
    WORD(RECORD1 + 4) = close_result ^ 0xabcdef98;
    for (i = 0; i < 600; ++i) {
        WORD(PALETTE0 + i * 4) = (i * 0xabcdefu) ^ (seed * 0xdeadbeefu);
        WORD(PALETTE1 + i * 4) = (i * 0x1234567u) ^ (seed * 0xfedcba98u);
    }
    for (i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        WORD(TABLE0 + slots[i] * 4) = functions[i];
        WORD(TABLE1 + slots[i] * 4) = functions[i];
    }
    WORD(0x6a0000u + window_result * 4u) = RECORD0;
    WORD(0x1b0e4) = TABLE0;
    WORD(0x1b0e0) = seed & 1 ? PALETTE1 : PALETTE0;
    captured_palette = (volatile uint32_t *)(uintptr_t)WORD(0x1b0e0);
    call_count = rectangles = polls = 0;
}

static int snapshot(unsigned save)
{
    const uint32_t starts[] = {active_base + 0x3898, 0x700000, 0x720000, 0x1b0e0};
    const unsigned sizes[] = {16, 0x2000, 0x2000, 8};
    unsigned region, i, offset = 0;
    /* Callback addresses differ between the two compiled programs. Validate
       them explicitly before comparing the remainder of the window records. */
    if (WORD(RECORD0 + 0x5b7) == active_base + active_close) WORD(RECORD0 + 0x5b7) = 0xc010c010;
    if (save) {
        expected_count = call_count;
        for (i = 0; i < call_count; ++i) expected_calls[i] = calls[i];
    } else {
        if (expected_count != call_count) return 0;
        for (i = 0; i < call_count * sizeof(struct call); ++i)
            if (((uint8_t *)expected_calls)[i] != ((uint8_t *)calls)[i]) {
                print("trace byte "); number(i); print("\n"); return 0;
            }
    }
    for (region = 0; region < 4; ++region)
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
        else if (function == 1) result = ((uint32_t (*)(void))(base + offsets[1]))();
        else ((void (*)(void))(base + offsets[2]))();
        if (!copy) { snapshot(1); expected_result = result; }
        else if (!snapshot(0) || result != expected_result) return 0;
    }
    ++cases;
    return 1;
}

static int run(void)
{
    static const uint32_t handles[] = {0, 1, 2, 7, 0x40000000, 0x7fffffff, 0x80000000, 0xffffffff};
    static const uint32_t values[] = {0, 1, 0x12345678, 0x7fffffff, 0x80000000, 0xffffffff};
    unsigned pass, h, value, function;
    if (!map(0x10000, 0x20000) || !map(0x690000, 0x20000) || !map(0x700000, 0x40000) ||
        !map(0x1000000, 0x20000) || !map(0x1200000, 0x20000)) return 1;
    for (pass = 0; pass < 2; ++pass) {
        uint32_t a = 0x1000000 + (pass ? 0x113 : 0), b = 0x1200000 + (pass ? 0x513 : 0);
        if (!load("build/color.original.x", a) || !load("build/color.x", b)) return 2;
        for (mode = 0; mode < 8; ++mode)
            for (seed = 0; seed < 6; ++seed)
                for (h = 0; h < 8; ++h)
                    for (function = 0; function < 3; ++function) {
                        value = (h + seed) % 6;
                        window_result = handles[h];
                        surface_result = values[value];
                        close_result = values[(value + mode) % 6];
                        if (!exercise(function, a, b)) {
                            print("COLOR mismatch function "); number(function);
                            print(" mode "); number(mode); print(" seed "); number(seed);
                            print(" handle "); number(window_result); print("\n"); return 10;
                        }
                    }
    }
    print("COLOR original/C differential cases: "); number(cases); print("\n");
    return 0;
}

void _start(void) { sys3(1, (unsigned)run(), 0, 0); __builtin_unreachable(); }
