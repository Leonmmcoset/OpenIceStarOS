/* Differential execution of original x86, host-linked C, and installed C.
 * All copy/compare logic executes. Only the allocation wrapper boundaries
 * share callbacks; their full implementations have separate heap tests. */
#include "../kernel/memory_ops.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)

static uint8_t expected_memory[0x4000];
static uint32_t trace[8], expected_trace[8], trace_length, expected_length;
static uint32_t path, cases, input[3], length, bound, pattern, alignment, variant;
static uint32_t value, result_value, mode, mismatch_address;
static const uint32_t lengths[] = {
    0, 1, 2, 3, 4, 7, 15, 16, 17, 31, 63, 127, 255, 511,
    1199, 1200, 1201, 2399, 2400, 3604
};
static const uint32_t dword_counts[] = {
    0, 1, 2, 3, 4, 7, 16, 299, 300, 301, 599, 600, 601, 900, 901,
    0xffffffff, 0xfffffffe, 0xfffffed5, 0xfffffed4, 0xfffffed3,
    0x80000000, 0x80000001
};
static const uint32_t values[] = {
    0, 1, 0x7f, 0x80, 0xff, 0x100, 0xffff, 0x10000,
    0x7fffffff, 0x80000000, 0xffffffff, 0xa55ac33c
};

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

uint32_t kernel_allocate(uint32_t size)
{
    trace[trace_length++] = 1;
    trace[trace_length++] = size;
    if (mode & 1) BYTE(0x600000) ^= (uint8_t)size;
    if (mode & 2) BYTE(0x603fff) += (uint8_t)(size >> 24);
    if (mode & 4) result_value ^= 0x80000000;
    return result_value;
}

void kernel_free_allocation(uint32_t address)
{
    trace[trace_length++] = 2;
    trace[trace_length++] = address;
    if (mode & 1) BYTE(0x600000) ^= (uint8_t)address;
    if (mode & 2) BYTE(0x603fff) += (uint8_t)(address >> 24);
    if (mode & 4) BYTE(0x601003) ^= 0xff;
}

static void stub(uint32_t address, uint32_t callback)
{
    BYTE(address) = 0xe9;
    *(uint32_t *)(uintptr_t)(address + 1u) = callback - address - 5u;
}

static void initialize(void)
{
    uint32_t i, source = 0x601800 + alignment, destination = 0x600100 + alignment;
    trace_length = 0;
    result_value = values[(cases / COUNT(values)) % COUNT(values)];
    for (i = 0; i < sizeof(expected_memory); ++i) {
        uint8_t byte = (uint8_t)(i * 29u + cases * 71u + (i >> 8) * 17u);
        if (pattern == 1) byte = 0;
        if (pattern == 2) byte = 0xff;
        if (pattern == 3) byte ^= (uint8_t)(i >> 3);
        BYTE(0x600000 + i) = byte;
    }
    if (path >= 5) { input[0] = value; return; }
    if (path == 4) {
        if (variant == 4) destination = source;
        if (variant == 5) source = 0x604000 - length;
        if (variant == 6) destination = 0x604000 - length;
        if (variant == 7) { source = 0x600000; destination = 0x602000; }
        for (i = 0; i < length; ++i) BYTE(destination + i) = BYTE(source + i);
        if (length) {
            if (variant == 1) BYTE(destination) ^= 0x80;
            if (variant == 2) BYTE(destination + length / 2u) ^= 0xff;
            if (variant == 3) BYTE(destination + length - 1u) ^= 1;
        }
        if (variant == 8) { destination = 0; source = 0xffffffff; }
        if (variant == 9) { destination = 0xffffffff; source = 0; }
        input[0] = destination;
        input[1] = source;
        input[2] = bound;
        return;
    }
    if (variant == 1) { source = 0x600100 + alignment; destination = 0x602000 + alignment; }
    if (variant == 2) destination = source - length;
    if (variant == 3) destination = source - 17u;
    if (variant == 4) destination = source - 4u;
    if (variant == 5) destination = source - 3u;
    if (variant == 6) destination = source - 2u;
    if (variant == 7) destination = source - 1u;
    if (variant == 8) destination = source;
    if (variant == 9) destination = source + 1u;
    if (variant == 10) destination = source + 2u;
    if (variant == 11) destination = source + 3u;
    if (variant == 12) destination = source + 4u;
    if (variant == 13) destination = source + 17u;
    if (variant == 14) destination = source + length;
    if (variant == 15) source = 0x604000 - length;
    if (variant == 16) { source = 0x600100 + alignment; destination = 0x604000 - length; }
    if (variant == 17) { source = 0x600000; destination = 0x602000; }
    if (variant == 18) { destination = 0; source = 0xffffffff; }
    if (variant == 19) { destination = 0xffffffff; source = 0; }
    input[0] = destination;
    input[1] = path == 1 ? value : source;
    input[2] = path == 3 ? bound : length;
}

static uint32_t invoke(int original)
{
    static const uint32_t offsets[] = {0x15a90, 0x15b02, 0x15b2c, 0x17322, 0x17548, 0x17590, 0x175a4};
    uint32_t entry = (original ? 0x900000 : 0x800000) + offsets[path];
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        if (path == 0) return kernel_move_bytes(input[0], input[1], input[2]);
        if (path == 1) kernel_fill_bytes(input[0], input[1], input[2]);
        if (path == 2) kernel_copy_1200_bytes(input[0], input[1]);
        if (path == 3) kernel_copy_dwords(input[0], input[1], (int32_t)input[2]);
        if (path == 4) return kernel_memory_differs(input[0], input[1], (int32_t)input[2]);
        if (path == 5) return kernel_malloc(input[0]);
        if (path == 6) kernel_free(input[0]);
        return 0;
    }
#endif
    if (path == 0 || path == 4)
        return ((uint32_t (*)(uint32_t, uint32_t, uint32_t))entry)(input[0], input[1], input[2]);
    if (path == 5) return ((uint32_t (*)(uint32_t))entry)(input[0]);
    if (path == 6) ((void (*)(uint32_t))entry)(input[0]);
    else if (path == 2) ((void (*)(uint32_t, uint32_t))entry)(input[0], input[1]);
    else ((void (*)(uint32_t, uint32_t, uint32_t))entry)(input[0], input[1], input[2]);
    return 0;
}

static int compare_memory(int save)
{
    uint32_t i;
    for (i = 0; i < sizeof(expected_memory); ++i) {
        if (save) expected_memory[i] = BYTE(0x600000 + i);
        else if (expected_memory[i] != BYTE(0x600000 + i)) {
            mismatch_address = 0x600000 + i;
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
    initialize();
    expected = invoke(1);
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
    PRINT("memory operation mismatch path/case/variant/length/bound/expected/actual/address: ");
    print_number(path); PRINT("/"); print_number(cases); PRINT("/");
    print_number(variant); PRINT("/"); print_number(length); PRINT("/");
    print_number(bound); PRINT("/"); print_number(expected); PRINT("/");
    print_number(actual); PRINT("/"); print_number(mismatch_address); PRINT("\n");
    return 0;
}

static int run_test(void)
{
    uint32_t i, j;
    if (!map_memory(0x5ff000, 0x1000, 0) || !map_memory(0x600000, 0x4000, 3) ||
        !map_memory(0x604000, 0x1000, 0) || !map_memory(0x800000, 0x40000, 7) ||
        !map_memory(0x900000, 0x40000, 7) || !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/icestar_c.x", 0x800000)) return 1;
    for (i = 0x800000; i <= 0x900000; i += 0x100000) {
        stub(i + 0x15464, (uint32_t)(uintptr_t)kernel_allocate);
        stub(i + 0x15978, (uint32_t)(uintptr_t)kernel_free_allocation);
    }
    for (path = 0; path < 7; ++path) {
        cases = 0;
        if (path >= 5) {
            for (i = 0; i < COUNT(values); ++i)
                for (pattern = 0; pattern < 4; ++pattern)
                    for (mode = 0; mode < 8; ++mode) {
                        value = values[i];
                        if (!run_case()) return 10;
                    }
        } else if (path == 4) {
            for (i = 0; i < COUNT(lengths); ++i)
                for (pattern = 0; pattern < 4; ++pattern)
                    for (alignment = 0; alignment < 4; ++alignment)
                        for (variant = 0; variant < 8; ++variant) {
                            length = lengths[i]; bound = length;
                            if (!run_case()) return 11;
                        }
            for (i = 15; i < COUNT(dword_counts); ++i)
                for (variant = 8; variant < 10; ++variant) {
                    length = 0; bound = dword_counts[i];
                    if (!run_case()) return 12;
                }
            for (i = 0; i < 2; ++i) {
                length = 1; bound = i ? 0x7fffffff : 0;
                variant = i ? 1 : 8;
                if (!run_case()) return 13;
            }
        } else {
            uint32_t amount = path == 2 ? 1 : path == 3 ? COUNT(dword_counts) : COUNT(lengths);
            for (i = 0; i < amount; ++i)
                for (pattern = 0; pattern < 4; ++pattern)
                    for (alignment = 0; alignment < 4; ++alignment)
                        for (variant = 0; variant < 20; ++variant) {
                            length = path == 2 ? 1200 : lengths[i % COUNT(lengths)];
                            if (path == 3) {
                                bound = dword_counts[i];
                                length = (int32_t)bound > 0 ? bound * 4u : 0;
                            }
                            if (variant >= 18 && length != 0) continue;
                            for (j = 0; j < (path == 1 ? COUNT(values) : 1); ++j) {
                                value = values[j];
                                if (!run_case()) return 14;
                            }
                        }
        }
        PRINT("memory operation path/cases: ");
        print_number(path); PRINT("/"); print_number(cases); PRINT("\n");
    }
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
