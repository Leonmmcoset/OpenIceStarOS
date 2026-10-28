/* A full-memory and vararg/ABI oracle for both clock display wrappers. */
#include <stdint.h>
#include <stdarg.h>
#include "../kernel/clock_display.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
static uint8_t expected[4096];
static uint32_t arguments[5], wanted[8], function, calls, failure, cases;
static const uint32_t addresses[] = {0x82ac53, 0x82acdd};
static const uint32_t candidates[] = {(uint32_t)kernel_draw_clock_time, (uint32_t)kernel_draw_clock_date};
static const uint32_t values[] = {0, 1, 0x7fffffff, 0x80000000, 0xffffffff,
    0x235959, 0x20261028, 0xabcdef01, 0x600001};
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
static void model_write(uint32_t offset, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) expected[offset+i] = (uint8_t)(value >> (8*i));
}
void kernel_format_text(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                        uint32_t e, const char *format, ...)
{
    uint32_t i, actual[] = {a, b, c, d, e};
    va_list ap;
    if (++calls != 1 || (uint32_t)format != (function ? 0x83cf7e : 0x83cf6f)) failure = 1;
    for (i = 0; i < 5; ++i) if (actual[i] != arguments[i]) failure = 1;
    va_start(ap, format);
    for (i = 0; i < (function ? 8u : 6u); ++i)
        if (va_arg(ap, uint32_t) != wanted[i]) failure = 1;
    va_end(ap);
    DWORD(0x1b004) = 0xfeedcafe;
    DWORD(0x1b008) = 0xdeadbeef;
    DWORD(0x1b101) = a ^ b ^ c ^ d ^ e;
}
uint32_t clock_checked_call(uint32_t function, const uint32_t *arguments);
static int exercise(uint32_t value, uint32_t pattern)
{
    uint32_t i, implementation, divisor = 1;
    for (i = 0; i < 4096; ++i) expected[i] = (uint8_t)(i*37+pattern*83);
    model_write(4, value); model_write(8, value);
    for (i = 0; i < 5; ++i) arguments[i] = values[(i+pattern)%9] ^ (pattern*0x01010101u);
    for (i = function ? 8 : 6; i > 0; --i) {
        wanted[i-1] = value / divisor % 16;
        if (i > 1) divisor *= 16;
    }
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t target = implementation == 1 ? candidates[function] :
            addresses[function] + (implementation == 0 ? 0x100000 : 0);
        for (i = 0; i < 4096; ++i) ((volatile uint8_t *)0x1b000)[i] = expected[i];
        calls = failure = 0;
        if (clock_checked_call(target, arguments)) return 10+implementation;
        if (failure || calls != 1) return 20+implementation;
        /* Expected callback effects are set only while comparing output. */
        model_write(4, 0xfeedcafe); model_write(8, 0xdeadbeef);
        model_write(0x101, arguments[0]^arguments[1]^arguments[2]^arguments[3]^arguments[4]);
        for (i = 0; i < 4096; ++i)
            if (((volatile uint8_t *)0x1b000)[i] != expected[i]) return 30+implementation;
        model_write(4, value); model_write(8, value);
        for (i = 0; i < 4; ++i) expected[0x101+i] = (uint8_t)((0x101+i)*37+pattern*83);
    }
    ++cases;
    return 0;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0'+value%10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(digits+i), sizeof(digits)-i);
}
static int run(void)
{
    uint32_t i, digit, position, error, random = 0x12345678;
    if (!map(0x1b000, 4096) || !map(0x900000, 0x40000) || !map(0x800000, 0x40000) ||
        !load("build/clock_display_original.x", 0x900000)) return 1;
#ifdef TEST_INSTALLED
    if (!load("build/openicestaros_kernel.x", 0x800000)) return 2;
#else
    if (!load("build/clock_display_preview.x", 0x800000)) return 2;
#endif
    {
#include "../build/clock_display_calls.h"
    }
    for (function = 0; function < 2; ++function) {
        for (i = 0; i < 65536; ++i)
            if ((error = exercise((i << 16) | (65535-i), i%9))) return error;
        for (position = 0; position < 8; ++position)
        for (digit = 0; digit < 16; ++digit)
            if ((error = exercise(digit << (position*4), digit%9))) return error;
        for (i = 0; i < 4096; ++i) {
            random = random * 1664525u + 1013904223u;
            if ((error = exercise(random, i%9))) return error;
        }
    }
    PRINT("Clock display cases: "); number(cases); PRINT("; executions: "); number(cases*3);
    PRINT("; both clock wrappers verified against full-memory/varargs/ABI oracle\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
