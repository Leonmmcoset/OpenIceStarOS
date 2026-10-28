/* Original x86 / ordinary C / fixed-address C, independently modeled.
 * DAC calls are intercepted only in the copies loaded into this fixture. */
#include <stdint.h>
#include "../kernel/display_palette.h"
#define BASE 0x18000u
#define SIZE 0x8000u
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static uint8_t expected[SIZE];
static uint32_t pattern, mutation, record, depth, events, failure;
static uint32_t palette_cases, table_cases;
uint32_t palette_test_function, palette_test_depth;
void palette_test_invoke(void);
static const uint32_t records[] = {0x1c000, 0x1c001, 0x1c003, 0x1b0e0,
    0x1b0dc, 0x1b0a0, 0x1b000, 0x1b0df, 0x1b0e1, 0x1b0e3, 0x1bfff, 0x18000};
static const uint32_t depths[] = {0, 1, 4, 7, 8, 9, 16, 24, 32, 0x80000000, 0xffffffff};
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
static void put(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) expected[address - BASE + i] = (uint8_t)(value >> (8 * i));
}
static uint8_t cube_byte(uint32_t i)
{
    uint32_t color = i / 3, channel = i % 3;
    return (uint8_t)(51 * (channel == 0 ? color % 6 :
                          channel == 1 ? (color / 6) % 6 : color / 36));
}
static void mutate(uint32_t event, uint32_t model)
{
    uint32_t address = 0x1b601 + event * 7;
    uint32_t value = 0x12345678u ^ (event * 0x1020304u) ^ pattern;
    if (mutation & 1) {
        if (model) put(0x1b030, 0xfedcba98);
        else DWORD(0x1b030) = 0xfedcba98;
    }
    if (mutation & 2) {
        if (model) put(address, value); else DWORD(address) = value;
    }
    if (mutation & 4) {
        if (model) put(record + 17, ~value); else DWORD(record + 17) = ~value;
    }
}
void kernel_display_write_palette(uint32_t first, uint32_t last, uint8_t *rgb)
{
    uint32_t i;
    if (events >= 2) { failure = 1; return; }
    if (first != (events ? 16u : 0u) || last != (events ? 231u : 15u)) failure = 2;
    if (!events) {
        if ((uint32_t)rgb != record) failure = 3;
    } else {
        for (i = 0; i < 648; ++i) if (rgb[i] != cube_byte(i)) failure = 4;
        /* Caller must tolerate writes within the temporary palette itself. */
        if (mutation & 4) for (i = 0; i < 648; ++i) rgb[i] ^= (uint8_t)i;
    }
    mutate(events++, 0);
}
struct call_patch { uint32_t offset, target; };
#include "../build/display_palette_calls.h"
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
    failure = events = 0;
    for (i = 0; i < SIZE; ++i)
        ((volatile uint8_t *)BASE)[i] = (uint8_t)(i * 37 + pattern * 83);
    DWORD(0x1b030) = record;
    DWORD(0x1b0e0) = record;
}
static void oracle(uint32_t table)
{
    uint32_t i;
    initialize();
    for (i = 0; i < SIZE; ++i) expected[i] = ((volatile uint8_t *)BASE)[i];
    if (!table) { mutate(0, 1); mutate(1, 1); return; }
    if (depth == 8) for (i = 0; i < 256; ++i) put(record + i * 4, i);
    if (depth == 32) {
        /* Explicit first colors and index-based cube, independent of the
         * recovered nested loops and table arrays. */
        static const uint32_t first[] = {0, 16711680, 65280, 16776960,
            255, 16711935, 65535, 16777215, 13027014, 8650752,
            33792, 8684544, 132, 8650884, 33924, 8684676};
        static const uint32_t scale[] = {0, 48, 101, 153, 206, 255};
        for (i = 0; i < 16; ++i) put(record + i * 4, first[i]);
        for (i = 0; i < 216; ++i) put(record + 64 + i * 4,
            scale[i % 6] * 65536 + scale[(i / 6) % 6] * 256 + scale[i / 36]);
        put(record + 928, 133169152);
    }
}
uint32_t entry_checked_call(void (*function)(void));
static int exercise(uint32_t table)
{
    uint32_t implementation, i;
    oracle(table);
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t address = table ? 0x81afbc : 0x81ae14;
        palette_test_function = implementation == 1 ? (table ?
            (uint32_t)kernel_display_select_depth : (uint32_t)kernel_display_init_palette) :
            address + (implementation == 0 ? 0x100000 : 0);
        palette_test_depth = depth;
        initialize();
        if (entry_checked_call(palette_test_invoke)) return 10 + implementation;
        if (failure || events != (table ? 0u : 2u)) return 20 + implementation;
        for (i = 0; i < SIZE; ++i)
            if (((volatile uint8_t *)BASE)[i] != expected[i]) return 30 + implementation;
    }
    if (table) ++table_cases; else ++palette_cases;
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
    uint32_t r, d, error;
    if (!map(BASE, SIZE) || !map(0x900000, 0x40000) || !map(0x800000, 0x40000)) return 1;
    if (!load("build/display_palette_original.x", 0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/openicestaros_kernel.x", 0x800000)) return 3;
#else
    if (!load("build/display_palette_preview.x", 0x800000)) return 3;
#endif
    redirect(0x900000, original_calls, COUNT(original_calls));
    redirect(0x800000, candidate_calls, COUNT(candidate_calls));
    for (pattern = 0; pattern < 3; ++pattern)
    for (r = 0; r < COUNT(records); ++r) {
        record = records[r]; depth = 0;
        for (mutation = 0; mutation < 8; ++mutation)
            if ((error = exercise(0))) return error;
        mutation = 0;
        for (d = 0; d < COUNT(depths); ++d) {
            depth = depths[d];
            if ((error = exercise(1))) return error;
        }
    }
    if (palette_cases != 288 || table_cases != 396) return 40;
    PRINT("Display VGA palette original/C/fixed-address/oracle cases: "); number(palette_cases);
    PRINT("; ordered ranges, captured source, all 648 RGB bytes and callback writes verified\n");
    PRINT("Display software color table original/C/fixed-address/oracle cases: "); number(table_cases);
    PRINT("; full-width depths, aliases, distinct RGB levels, extra entry, untouched tail, full memory and ABI verified\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
