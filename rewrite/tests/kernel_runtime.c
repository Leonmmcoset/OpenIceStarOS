/* Execute original, host-linked C, and fixed-address C service routines.
 * Only the unrecovered task-yield boundary is replaced by a test callback. */
#include "../kernel/runtime.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t expected_low[0x1000], expected_palette[0x8000];
static uint32_t trace[32], expected_trace[32], mode, deadline, initial_tick;
static uint32_t palette_cases, wait_cases;
static const uint32_t edge_indices[] = {
    0, 1, 15, 255, 256, 599, 600, 0x3fffffff,
    0x40000000, 0x40000001, 0x7fffffff, 0x80000000,
    0xbfffffff, 0xc0000000, 0xfffffffe, 0xffffffff
};
static const uint32_t tick_values[] = {
    0, 1, 2, 10, 255, 0x10000, 0x7ffffffe, 0x7fffffff,
    0x80000000, 0x80000001, 0xfffffffe, 0xffffffff
};

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}

static void finish(unsigned result)
{
    syscall3(1, result, 0, 0);
    __builtin_unreachable();
}

static int map_memory(uint32_t address, uint32_t length, int protection)
{
    uint32_t args[] = {address, length, (uint32_t)protection, 0x32, 0xffffffff, 0};
    return syscall3(90, (unsigned)(uintptr_t)args, 0, 0) == (int)address;
}

static int load_kernel(const char *path, uint32_t address)
{
    int fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0), count;
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, address, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return count == 250099;
}

static void redirect_yield(uint32_t address)
{
    *(volatile uint8_t *)(uintptr_t)address = 0xe9;
    WORD(address + 1) = (uint32_t)(uintptr_t)kernel_yield_task - address - 5u;
}

void kernel_yield_task(void)
{
    uint32_t n = trace[0]++, tick = WORD(0x1b084);
    if (n >= 10) finish(90); /* A broken loop must fail instead of hanging. */
    trace[1 + n * 3] = tick;
    trace[2 + n * 3] = WORD(0x1b0e0);
    trace[3 + n * 3] = WORD(0x1b0e4);
    if (mode == 0) tick = deadline;
    else if (mode == 1) tick = n ? deadline : deadline - 1u;
    else if (mode == 2) tick = n < 3 ? initial_tick : deadline;
    else if (mode == 3) tick = n ? deadline : 0;
    else if (mode == 4) tick = 0xffffffff;
    else if (mode == 5) tick = n ? deadline : tick + 0x80000000u;
    else if (mode == 6) {
        tick = n ? deadline : deadline - 1u;
        WORD(0x1b0e0) = 0x607003u ^ n;
        WORD(0x1b0e4) = 0x603001u + n;
        WORD(0x1b0c8) ^= 0x12345678;
    } else tick = n < 4 ? (initial_tick ^ (0x80000000u >> n)) : deadline;
    WORD(0x1b084) = tick;
    WORD(0x1b200 + n * 4) = 0x5a7d0000u ^ tick;
}

static void initialize(unsigned pattern)
{
    volatile uint8_t *low = (volatile uint8_t *)0x1b000;
    volatile uint8_t *palette = (volatile uint8_t *)0x600000;
    unsigned i;
    for (i = 0; i < sizeof(expected_low); ++i) low[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < sizeof(expected_palette); ++i) palette[i] = (uint8_t)(i * 29 + pattern * 71);
    for (i = 0; i < COUNT(trace); ++i) trace[i] = 0;
}

static int compare(int save)
{
    volatile uint8_t *low = (volatile uint8_t *)0x1b000;
    volatile uint8_t *palette = (volatile uint8_t *)0x600000;
    unsigned i;
    for (i = 0; i < sizeof(expected_low); ++i) {
        if (save) expected_low[i] = low[i];
        else if (expected_low[i] != low[i]) return 0;
    }
    for (i = 0; i < sizeof(expected_palette); ++i) {
        if (save) expected_palette[i] = palette[i];
        else if (expected_palette[i] != palette[i]) return 0;
    }
    for (i = 0; i < COUNT(trace); ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static int test_palette(void)
{
    unsigned pattern, vector, alignment;
    for (pattern = 0; pattern < 2; ++pattern)
        for (alignment = 0; alignment < 4; ++alignment)
            for (vector = 0; vector < COUNT(edge_indices) + 512; ++vector) {
                uint32_t index = vector < COUNT(edge_indices) ? edge_indices[vector] :
                    vector * 0x9e3779b9u ^ 0xe731c42du;
                uint32_t target = 0x600000u + (vector * 59u % 0x1fffu) * 4u + alignment;
                uint32_t original, recovered, expected;
                initialize(pattern);
                WORD(0x1b0e0) = target - index * 4u;
                expected = WORD(target);
                compare(1);
                original = ((uint32_t (*)(uint32_t))0x91b714)(index);
                if (original != expected || !compare(0)) return 10;
#ifdef KERNEL_LINKED_TEST
                recovered = ((uint32_t (*)(uint32_t))0x81b714)(index);
#else
                recovered = kernel_palette_color(index);
#endif
                if (recovered != original || !compare(0)) return 11;
                ++palette_cases;
            }
    return 0;
}

static int test_wait(void)
{
    unsigned pattern, tick, duration;
    for (pattern = 0; pattern < 2; ++pattern)
        for (tick = 0; tick < COUNT(tick_values); ++tick)
            for (duration = 0; duration < COUNT(tick_values); ++duration)
                for (mode = 0; mode < 8; ++mode) {
                    uint32_t original, recovered, value = tick_values[duration];
                    initial_tick = tick_values[tick];
                    deadline = initial_tick + value;
                    initialize(pattern);
                    WORD(0x1b084) = initial_tick;
                    original = ((uint32_t (*)(uint32_t))0x92ad81)(value);
                    if (original < deadline || original != WORD(0x1b084)) return 20;
                    if ((initial_tick >= deadline) != (trace[0] == 0)) return 21;
                    compare(1);
                    initialize(pattern);
                    WORD(0x1b084) = initial_tick;
#ifdef KERNEL_LINKED_TEST
                    recovered = ((uint32_t (*)(uint32_t))0x82ad81)(value);
#else
                    recovered = kernel_wait_ticks(value);
#endif
                    if (recovered != original || !compare(0)) return 22;
                    ++wait_cases;
                }
    return 0;
}

static void print_count(uint32_t value)
{
    char digits[10];
    unsigned n = 0;
    do { digits[9 - n++] = (char)('0' + value % 10); value /= 10; } while (value);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - n), n);
}

static int run_test(void)
{
    int result;
    if (!map_memory(0x1b000, 0x1000, 3) || !map_memory(0x600000, 0x8000, 3) ||
        !map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7)) return 1;
    if (!load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/icestar_c.x", 0x800000)) return 2;
    redirect_yield(0x81434a);
    redirect_yield(0x91434a);
    result = test_palette();
    if (result) return result;
    result = test_wait();
    if (result) return result;
    syscall3(4, 1, (unsigned)(uintptr_t)"palette cases: ", 15);
    print_count(palette_cases);
    syscall3(4, 1, (unsigned)(uintptr_t)"; tick-wait cases: ", 19);
    print_count(wait_cases);
    syscall3(4, 1, (unsigned)(uintptr_t)"\n", 1);
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
