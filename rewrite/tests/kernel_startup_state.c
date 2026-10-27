/* Original x86 / ordinary C / fixed-address C against a bytewise oracle.
 * The original palette and depth-selection calls are redirected only in
 * fixture copies. Their actual subsystem bodies are outside this test. */
#include <stdint.h>
#include "../kernel/startup_state.h"

#define BASE 0x18000u
#define SIZE 0x5000u
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t expected[SIZE];
static uint32_t record, pattern, mutation, depth, events, failure;
static uint32_t expected_count, expected_arguments[2], display_cases, table_cases;
static const uint32_t records[] = {
    0x1b800, 0x1b801, 0x1b803, 0x1a800, 0x1a80e, 0x1a816, 0x1a81a,
    0x1a81e, 0x1a822, 0x1b020, 0x1b034, 0x1b0b8, 0x1afff
};
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
static uint32_t get(uint32_t address)
{
    uint32_t i, value = 0;
    for (i = 0; i < 4; ++i) value |= (uint32_t)expected[address - BASE + i] << (8 * i);
    return value;
}
static void put(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) expected[address - BASE + i] = (uint8_t)(value >> (8 * i));
}
static uint32_t callback_value(uint32_t kind)
{
    return 0x12345678u ^ (pattern * 0x10203u) ^ (mutation * 0x11111111u) ^ kind;
}
static void callback_oracle(uint32_t kind, uint32_t argument)
{
    uint32_t value = callback_value(kind);
    if (expected_count >= 2) { failure = 1; return; }
    expected_arguments[expected_count++] = kind ? argument : UINT32_MAX;
    if (mutation & 1) put(0x1b034, 0xfedcba98);
    if (mutation & 2) put(record + 36, value);
    if (mutation & 4) {
        put(0x1a81a, ~value);
        put(0x1a81e, value ^ argument);
    }
    if (mutation & 8) put(record + 28, value + argument);
}
static void callback(uint32_t kind, uint32_t argument)
{
    uint32_t value = callback_value(kind);
    if (events >= expected_count || expected_arguments[events] !=
            (kind ? argument : UINT32_MAX)) failure = 2;
    if (kind != (events == 0 && expected_count == 2 ? 0u : 1u)) failure = 3;
    ++events;
    if (mutation & 1) DWORD(0x1b034) = 0xfedcba98;
    if (mutation & 2) DWORD(record + 36) = value;
    if (mutation & 4) {
        DWORD(0x1a81a) = ~value;
        DWORD(0x1a81e) = value ^ argument;
    }
    if (mutation & 8) DWORD(record + 28) = value + argument;
}
void kernel_display_init_palette(void) { callback(0, 0); }
void kernel_display_select_depth(uint32_t argument) { callback(1, argument); }

struct call_patch { uint32_t offset, target; };
#include "../build/startup_state_calls.h"
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
    DWORD(0x1a816) = depth;
    DWORD(0x1a81a) = 0x87654321u ^ pattern;
    DWORD(0x1a81e) = 0x7fffffff + pattern;
    DWORD(0x1a822) = 0xffffffffu - depth;
    DWORD(0x1b0b8) = depth;
    DWORD(0x1b0bc) = ~depth;
    DWORD(0x1b0c0) = depth + 1;
    DWORD(0x1b034) = record;
}
static void oracle(uint32_t table)
{
    uint32_t i, captured;
    initialize();
    for (i = 0; i < SIZE; ++i) expected[i] = ((volatile uint8_t *)BASE)[i];
    expected_count = 0;
    if (table) {
        put(0x1b0b8, 0x4000); put(0x1b0bc, 0); put(0x1b0c0, 0);
        return;
    }
    captured = get(0x1b034);
    put(captured, get(0x1a822));
    put(captured + 4, 0); put(captured + 8, 0);
    put(captured + 12, get(0x1a81a)); put(captured + 16, get(0x1a81e));
    put(captured + 20, get(0x1a81a)); put(captured + 24, get(0x1a81e));
    put(captured + 36, get(0x1a816)); put(captured + 40, UINT32_MAX);
    put(captured + 44, 0); put(captured + 48, 0); put(captured + 52, 0);
    if (get(captured + 36) == 8) callback_oracle(0, 0);
    callback_oracle(1, get(captured + 36));
}
uint32_t entry_checked_call(void (*function)(void));
static int exercise(uint32_t table)
{
    uint32_t implementation, i;
    oracle(table);
    if (failure) return 4;
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t address = table ? 0x82aa0f : 0x81acc4;
        void (*function)(void) = implementation == 1 ?
            (table ? kernel_init_object_table_state : kernel_init_display_state) :
            (void (*)(void))(address + (implementation == 0 ? 0x100000 : 0));
        initialize();
        if (entry_checked_call(function)) return 10 + implementation;
        if (failure || events != expected_count) return 20 + implementation;
        for (i = 0; i < SIZE; ++i)
            if (((volatile uint8_t *)BASE)[i] != expected[i]) return 30 + implementation;
    }
    if (table) ++table_cases; else ++display_cases;
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
    if (!load("build/startup_state_original.x", 0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 3;
#else
    if (!load("build/startup_state_preview.x", 0x800000)) return 3;
#endif
    redirect(0x900000, original_calls, COUNT(original_calls));
    redirect(0x800000, candidate_calls, COUNT(candidate_calls));
    for (pattern = 0; pattern < 3; ++pattern)
    for (r = 0; r < COUNT(records); ++r)
    for (d = 0; d < COUNT(depths); ++d)
    for (mutation = 0; mutation < 16; ++mutation) {
        record = records[r]; depth = depths[d];
        if ((error = exercise(0))) return error;
    }
    for (pattern = 0; pattern < 3; ++pattern)
    for (d = 0; d < COUNT(depths); ++d) {
        record = records[0]; depth = depths[d]; mutation = 0;
        if ((error = exercise(1))) return error;
    }
    if (display_cases != 6864 || table_cases != 33) return 40;
    PRINT("Startup display original/C/fixed-address/oracle cases: "); number(display_cases);
    PRINT("; aliases, live depth, callbacks, untouched fields, full memory and ABI verified\n");
    PRINT("Startup object-table state original/C/fixed-address/oracle cases: "); number(table_cases);
    PRINT("; only three counter DWORDs changed\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
