/* Exhaustive BCD arithmetic with deterministic two-byte IN replacements.
 * OUT instructions become NOPs only in fixture copies. Python separately
 * requires every unmodified compiled body to equal original machine code. */
#include <stdint.h>
#include "../kernel/cmos_fields.h"
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
static const uint32_t starts[] = {
    0x82adab, 0x82adc5, 0x82addf, 0x82adf9, 0x82ae13, 0x82ae2d, 0x82ae47
};
static const uint32_t ordinary[] = {
    (uint32_t)kernel_cmos_seconds, (uint32_t)kernel_cmos_minutes,
    (uint32_t)kernel_cmos_hours, (uint32_t)kernel_cmos_weekday,
    (uint32_t)kernel_cmos_day, (uint32_t)kernel_cmos_month, (uint32_t)kernel_cmos_year
};
static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static int map(uint32_t address)
{
    uint32_t args[] = {address, 0x40000, 7, 0x32, 0xffffffff, 0};
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
static void io_replace(uint32_t address, uint32_t year)
{
    volatile uint8_t *p = (volatile uint8_t *)address;
    uint32_t out = year ? 7 : 6;
    p[out] = p[out+1] = 0x90;
    p[out+2] = 0xb0;
    if (year) { p[37] = p[38] = 0x90; p[39] = 0xb0; }
}
uint32_t cmos_checked_call(uint32_t function, uint32_t *output);
static uint32_t decimal(uint32_t raw) { return (raw/16)*10 + raw%16; }
static int run(void)
{
    uint32_t f, implementation, value, result, target, wanted, scenarios = 0;
    if (!map(0x800000) || !map(0x900000) ||
        !load("build/cmos_fields_original.x", 0x900000)) return 1;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 2;
#else
    if (!load("build/cmos_fields_preview.x", 0x800000)) return 2;
#endif
    for (f = 0; f < 7; ++f) {
        if (syscall3(125, ordinary[f] & ~4095u, 8192, 7)) return 3;
        for (implementation = 0; implementation < 3; ++implementation) {
            target = implementation == 1 ? ordinary[f] :
                starts[f]+(implementation == 0 ? 0x100000 : 0);
            io_replace(target, f == 6);
            for (value = 0; value < (f == 6 ? 65536u : 256u); ++value) {
                volatile uint8_t *p = (volatile uint8_t *)target;
                p[f == 6 ? 10 : 9] = (uint8_t)value;
                if (f == 6) p[40] = (uint8_t)(value >> 8);
                wanted = f == 6 ? decimal(value & 255)*100+decimal(value >> 8) : decimal(value);
                if (cmos_checked_call(target, &result)) return 10+implementation;
                if (result != wanted) return 20+implementation;
                if (!implementation) ++scenarios;
            }
        }
    }
    if (scenarios != 67072) return 30;
    PRINT("PASS: 67072 CMOS field scenarios, 201216 executions; exhaustive raw BCD inputs and full general-register ABI\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
