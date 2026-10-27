/* Original machine code, ordinary C and fixed-address C compared with an
 * independent bytewise model. Launch and wait are hooked only in fixtures. */
#include <stdint.h>
#include "../kernel/startup_apps.h"
#define BASE 0x1b000u
#define SIZE 0x3000u
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t expected[SIZE];
static uint32_t pattern, mutation, seed, events, failure, cases;
static uint32_t arguments[3][2];
static const uint32_t seeds[] = {0, 1, 0x7f, 0x80, 0xff, 0x100,
    0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
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
static uint32_t get(uint32_t address)
{
    uint32_t i, value = 0;
    for (i = 0; i < 4; ++i) value |= (uint32_t)expected[address - BASE + i] << (i * 8);
    return value;
}
static void put(uint32_t address, uint32_t value, uint32_t bytes)
{
    uint32_t i;
    for (i = 0; i < bytes; ++i) expected[address - BASE + i] = (uint8_t)(value >> (i * 8));
}
static uint32_t value(uint32_t event)
{
    return seed ^ (0x12345678u * (event + 1)) ^ (pattern * 0x1010101u);
}
static void mutate(uint32_t event, uint32_t model)
{
    uint32_t v = value(event), i;
    const uint32_t addresses[] = {0x1b158, 0x1b15c, 0x1b1b8, 0x1b1bc};
    for (i = 0; i < COUNT(addresses); ++i) {
        if (!(mutation & (1u << i))) continue;
        if (model) put(addresses[i], v ^ (i * 0x10203u), 4);
        else DWORD(addresses[i]) = v ^ (i * 0x10203u);
    }
    /* Exercise observable unaligned callback writes outside the parameters. */
    if (model) put(0x1b501 + event * 7, v, 4);
    else DWORD(0x1b501 + event * 7) = v;
}
static uint32_t callback(uint32_t wait, uint32_t first, uint32_t second)
{
    if (events >= 3) { failure = 1; return 0; }
    if (wait != (events == 1) || first != arguments[events][0] ||
        second != arguments[events][1]) failure = 2;
    mutate(events, 0);
    return value(events++);
}
void kernel_startup_launch_image(uint32_t first, uint32_t second)
{
    (void)callback(0, first, second);
}
uint32_t kernel_wait_ticks(uint32_t duration) { return callback(1, duration, 0); }

struct call_patch { uint32_t offset, target; };
#include "../build/startup_apps_calls.h"
static void redirect(uint32_t base, const struct call_patch *patches, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        uint32_t address = base + patches[i].offset;
        DWORD(address + 1) = patches[i].target - address - 5;
    }
}
static void initialize(void)
{
    uint32_t i;
    events = failure = 0;
    for (i = 0; i < SIZE; ++i)
        ((volatile uint8_t *)BASE)[i] = (uint8_t)(i * 37 + pattern * 83);
    DWORD(0x1b158) = seed;
    DWORD(0x1b15c) = ~seed;
    DWORD(0x1b1b8) = seed + pattern;
    DWORD(0x1b1bc) = seed - pattern;
}
static void oracle(void)
{
    uint32_t i;
    initialize();
    for (i = 0; i < SIZE; ++i) expected[i] = ((volatile uint8_t *)BASE)[i];
    arguments[0][1] = get(0x1b15c);
    arguments[0][0] = get(0x1b158);
    mutate(0, 1);
    arguments[1][0] = 20; arguments[1][1] = 0;
    mutate(1, 1);
    arguments[2][1] = get(0x1b1bc);
    arguments[2][0] = get(0x1b1b8);
    mutate(2, 1);
}
uint32_t entry_checked_call(void (*function)(void));
static int exercise(void)
{
    uint32_t implementation, i;
    oracle();
    for (implementation = 0; implementation < 3; ++implementation) {
        void (*function)(void) = implementation == 1 ? kernel_startup_applications :
            (void (*)(void))(0x810f38 + (implementation == 0 ? 0x100000 : 0));
        initialize();
        if (entry_checked_call(function)) return 10 + implementation;
        if (failure || events != 3) return 20 + implementation;
        for (i = 0; i < SIZE; ++i)
            if (((volatile uint8_t *)BASE)[i] != expected[i]) return 30 + implementation;
    }
    ++cases;
    return 0;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0' + value % 10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(digits + i), sizeof(digits) - i);
}
#define PRINT(text) syscall3(4, 1, (uint32_t)(text), sizeof(text) - 1)
static int run(void)
{
    uint32_t s, error;
    if (!map(BASE, SIZE) || !map(0x900000, 0x40000) || !map(0x800000, 0x40000)) return 1;
    if (!load("build/startup_apps_original.x", 0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 3;
#else
    if (!load("build/startup_apps_preview.x", 0x800000)) return 3;
#endif
    redirect(0x900000, original_calls, COUNT(original_calls));
    redirect(0x800000, candidate_calls, COUNT(candidate_calls));
    for (pattern = 0; pattern < 3; ++pattern)
    for (mutation = 0; mutation < 16; ++mutation)
    for (s = 0; s < COUNT(seeds); ++s) {
        seed = seeds[s];
        if ((error = exercise())) return error;
    }
    if (cases != 480) return 40;
    PRINT("Startup applications original/C/fixed-address/oracle cases: "); number(cases);
    PRINT("; 1440 executions; ordered launches, 20 ticks, live parameters, callback writes, full memory and ABI verified\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
