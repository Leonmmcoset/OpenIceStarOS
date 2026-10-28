/* Execute the original and rebuilt memory-probe wrapper and heap initializer.
 * Hardware boundaries share callbacks; all memory and ordered traces match. */
#include "../kernel/memory_init.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)

static const uint32_t regions[][2] = {{0x1b000, 0x1000}, {0x2000000, 0x1000}};
static const uint32_t flag_values[] = {
    0, 0x40000, 0xffffffff, 0xfffbffff, 0x202, 0x40202, 0x80000000, 0x80040000
};
static const uint32_t values[] = {
    0, 1, 4095, 4096, 4097, 0x400000, 0x2000000, 0x8000000,
    0x7ffff000, 0x7fffffff, 0x80000000, 0x80000001,
    0xffffefff, 0xfffff000, 0xfffffffe, 0xffffffff
};
static uint8_t expected_memory[0x2000];
static uint32_t trace[256], expected_trace[256], trace_length, expected_length;
static uint32_t flags_input[2], probe_result, control, seed, path, mode, pattern, cases;
static uint32_t flags_reads, flags_writes, control_reads, control_writes, page_calls;
static uint32_t mismatch_address;

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}

static void finish(unsigned code)
{
    syscall3(1, code, 0, 0);
    __builtin_unreachable();
}

static int map_memory(uint32_t address, uint32_t size, uint32_t protection)
{
    uint32_t args[] = {address, size, protection, 0x32, 0xffffffff, 0};
    return (uint32_t)syscall3(90, (unsigned)(uintptr_t)args, 0, 0) == address;
}

static int load_kernel(const char *name, uint32_t address)
{
    int fd = syscall3(5, (unsigned)(uintptr_t)name, 0, 0), size;
    if (fd < 0) return 0;
    size = syscall3(3, (unsigned)fd, address, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return size == 250099;
}

static void observe(uint32_t event, uint32_t a, uint32_t b)
{
    uint32_t i;
    if (trace_length + 17u > COUNT(trace)) finish(90);
    trace[trace_length++] = event;
    trace[trace_length++] = a;
    trace[trace_length++] = b;
    trace[trace_length++] = flags_reads;
    trace[trace_length++] = flags_writes;
    trace[trace_length++] = control_reads;
    trace[trace_length++] = control_writes;
    trace[trace_length++] = page_calls;
    trace[trace_length++] = control;
    for (i = 0; i < 16; i += 4) trace[trace_length++] = WORD(0x1b050 + i);
    for (i = 0; i < 16; i += 4) trace[trace_length++] = WORD(0x2000000 + i);
}

static void change_memory(uint32_t salt)
{
    WORD(0x1b050) = values[(seed + salt) % COUNT(values)];
    WORD(0x1b054) ^= 0xa55ac33c + salt;
    WORD(0x1b058) += salt;
    WORD(0x1b05c) ^= 0x80000000u | salt;
    WORD(0x2000000) ^= salt * 17u;
    WORD(0x2000004) += salt * 31u;
    WORD(0x2000008) ^= 0xdead0000u | salt;
    WORD(0x200000c) += salt * 53u;
}

uint32_t kernel_read_flags(void)
{
    uint32_t result;
    if (flags_reads >= 2) finish(91);
    result = flags_input[flags_reads];
    observe(1, result, 0);
    ++flags_reads;
    if (mode & 1) { change_memory(flags_reads); control ^= 0x60000000; }
    return result;
}

void kernel_write_flags(uint32_t flags)
{
    observe(2, flags, 0);
    ++flags_writes;
    if (mode & 2) { change_memory(flags_writes + 3u); control += flags; }
}

uint32_t kernel_read_cr0(void)
{
    uint32_t result = control;
    observe(3, result, 0);
    ++control_reads;
    if (mode & 4) { change_memory(control_reads + 7u); control ^= 0xffffffff; }
    return result;
}

void kernel_write_cr0(uint32_t value)
{
    observe(4, value, 0);
    ++control_writes;
    control = value;
    if (mode & 8) change_memory(control_writes + 11u);
}

uint32_t kernel_probe_pages(uint32_t first, uint32_t last)
{
    observe(5, first, last);
    if (first != 0x400000 || last != 0xffffffff || page_calls != 0) finish(92);
    ++page_calls;
    if (mode & 16) {
        change_memory(17);
        control = values[(seed + 9u) % COUNT(values)];
    }
    return probe_result;
}

static void redirect(uint32_t address, uintptr_t target)
{
    BYTE(address) = 0xe9;
    WORD(address + 1u) = (uint32_t)target - address - 5u;
}

static void short_trampoline(uint32_t address, uint32_t destination, uintptr_t callback)
{
    int32_t distance = (int32_t)(destination - address - 2u);
    if (distance < -128 || distance > 127) finish(93);
    *(volatile uint16_t *)(uintptr_t)address = 0xeb | ((uint16_t)(uint8_t)distance << 8);
    redirect(destination, callback);
}

static void stub_boundaries(uint32_t base)
{
    short_trampoline(base + 0x1a24a, base + 0x1a210, (uintptr_t)kernel_read_flags);
    redirect(base + 0x1a24d, (uintptr_t)kernel_write_flags);
    /* The original wrapper uses duplicate CR0 entries. Recovered C calls the
       equivalent, already recovered leaves. Both have identical callbacks. */
    short_trampoline(base + 0x372fe, base + 0x372b0, (uintptr_t)kernel_read_cr0);
    redirect(base + 0x37302, (uintptr_t)kernel_write_cr0);
    short_trampoline(base + 0x3743c, base + 0x37420, (uintptr_t)kernel_read_cr0);
    redirect(base + 0x37440, (uintptr_t)kernel_write_cr0);
    redirect(base + 0x373ee, (uintptr_t)kernel_probe_pages);
}

static void initialize(void)
{
    uint32_t i, j;
    trace_length = flags_reads = flags_writes = control_reads = control_writes = page_calls = 0;
    control = values[(seed + 5u) % COUNT(values)];
    for (i = 0; i < COUNT(regions); ++i)
        for (j = 0; j < regions[i][1]; ++j)
            BYTE(regions[i][0] + j) = (uint8_t)(j * 29u + seed * 71u + pattern * 17u);
}

static uint32_t invoke(int original)
{
    uint32_t base = original ? 0x900000 : 0x800000;
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        if (path == 0) return kernel_probe_memory();
        kernel_init_heap();
        return 0;
    }
#endif
    if (path == 0) return ((uint32_t (*)(void))(base + 0x15320))();
    ((void (*)(void))(base + 0x153d4))();
    return 0;
}

static int compare_memory(int save)
{
    uint32_t i, j, at = 0;
    for (i = 0; i < COUNT(regions); ++i)
        for (j = 0; j < regions[i][1]; ++j, ++at) {
            if (save) expected_memory[at] = BYTE(regions[i][0] + j);
            else if (expected_memory[at] != BYTE(regions[i][0] + j)) {
                mismatch_address = regions[i][0] + j;
                return 0;
            }
        }
    return 1;
}

static void print_number(uint32_t number)
{
    char digits[10];
    uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + number % 10); number /= 10; } while (number);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - size), size);
}

static int run_case(void)
{
    uint32_t expected, actual, i;
    seed = cases + path * 977u;
    initialize();
    expected = invoke(1);
    if (flags_reads != 2 || flags_writes != 2 || page_calls != 1 ||
        control_reads != ((flags_input[1] & 0x40000) ? 2u : 0u) ||
        control_writes != control_reads) return 0;
    compare_memory(1);
    expected_length = trace_length;
    for (i = 0; i < trace_length; ++i) expected_trace[i] = trace[i];
    initialize();
    actual = invoke(0);
    mismatch_address = 0;
    if (!compare_memory(0) || actual != expected || trace_length != expected_length) goto mismatch;
    for (i = 0; i < trace_length; ++i)
        if (trace[i] != expected_trace[i]) { mismatch_address = 0xdead0000 + i; goto mismatch; }
    ++cases;
    return 1;
mismatch:
    PRINT("memory initialization mismatch path/case/mode/result/expected/actual/address: ");
    print_number(path); PRINT("/"); print_number(cases); PRINT("/");
    print_number(mode); PRINT("/"); print_number(probe_result); PRINT("/");
    print_number(expected); PRINT("/"); print_number(actual); PRINT("/");
    print_number(mismatch_address); PRINT("\n");
    return 0;
}

static int run_test(void)
{
    uint32_t i, j, k;
    for (i = 0; i < COUNT(regions); ++i)
        if (!map_memory(regions[i][0], regions[i][1], 3)) return 1;
    if (!map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7) ||
        !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 2;
    stub_boundaries(0x800000);
    stub_boundaries(0x900000);
    for (path = 0; path < 2; ++path) {
        cases = 0;
        for (i = 0; i < COUNT(flag_values); ++i)
            for (j = 0; j < COUNT(flag_values); ++j)
                for (k = 0; k < COUNT(values); ++k)
                    for (mode = 0; mode < 32; ++mode)
                        for (pattern = 0; pattern < 2; ++pattern) {
                            flags_input[0] = flag_values[i];
                            flags_input[1] = flag_values[j];
                            probe_result = values[k];
                            if (!run_case()) return 10;
                        }
        PRINT("memory initialization path/cases: ");
        print_number(path); PRINT("/"); print_number(cases); PRINT("\n");
    }
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
