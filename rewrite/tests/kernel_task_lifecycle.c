/* Differential execution of application creation, deletion, thread creation,
 * and active-state queries. Actual free-slot search and both task constructors
 * execute. Allocation, flags, descriptors, and string copy are boundaries. */
#include "../kernel/tasks.h"

#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define TASK(i, o) WORD(captured_table + (i) * 108u + (o))

static const uint32_t regions[][2] = {
    {0x1b000, 0x1000}, {0x600000, 0x41000},
    {0x6d0000, 0x4000}, {0x700000, 0x8000}
};
static uint8_t expected_memory[0x4e000];
static uint32_t trace[512], expected_trace[512], trace_length, expected_length;
static uint32_t seed, mode, captured_table, flags_reads, flags_writes, cases;
static uint32_t input[7], path;
static uint32_t allocation_size, allocation_result, delete_table;
static const uint32_t values[] = {
    0, 1, 2, 0x38, 0x40, 0x202, 0xffff, 0x10000,
    0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 0xa55ac33c
};
static const uint32_t scenarios[][3] = {
    {0, 0, 0xffffffff}, {0, 1, 0xffffffff}, {0, 0x71c, 0xffffffff},
    {1, 1, 0xffffffff}, {1, 1, 0}, {1, 0, 0},
    {3, 3, 0xffffffff}, {3, 4, 0xffffffff}, {3, 2, 1},
    {7, 10, 0}, {7, 10, 3}, {7, 10, 6},
    {31, 32, 17}, {32, 32, 31}, {32, 32, 0xffffffff}
};
static const uint32_t sizes[] = {
    0, 1, 0xfff, 0x1000, 0x1001, 0x1fff, 0x2000,
    0x7fffffff, 0x80000000, 0xfffff000, 0xfffff001,
    0xfffff800, 0xfffffffe, 0xffffffff
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

static void observe(uint32_t event, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t i, hash = 0;
    if (trace_length + 13 > COUNT(trace)) finish(90);
    /* Include the whole shared record plus both candidate task blocks at
       every boundary; final comparison covers every mapped data byte. */
    for (i = 0; i < 0x200; ++i) hash = hash * 31u + BYTE(0x1b000 + i);
    for (i = 0; i < 0x1000; ++i) {
        hash = hash * 31u + BYTE(0x600000 + i);
        hash = hash * 31u + BYTE(0x630000 + i);
        hash = hash * 31u + BYTE(0x6d0000 + i);
    }
    trace[trace_length++] = event;
    trace[trace_length++] = a;
    trace[trace_length++] = b;
    trace[trace_length++] = c;
    trace[trace_length++] = d;
    trace[trace_length++] = hash;
    trace[trace_length++] = WORD(0x1b0c8);
    trace[trace_length++] = WORD(0x1b0cc);
    trace[trace_length++] = WORD(0x1b0d0);
    trace[trace_length++] = WORD(0x1b0d4);
    trace[trace_length++] = WORD(0x1b0d8);
    trace[trace_length++] = WORD(0x1b118);
    trace[trace_length++] = WORD(0x1b160);
}

uint32_t kernel_read_flags(void)
{
    uint32_t flags = values[(seed + flags_reads) % COUNT(values)];
    observe(1, flags, flags_reads++, 0, 0);
    if (mode & 1) WORD(0x1b0c8) = 0x630000 + (seed & 3);
    return flags;
}

void kernel_disable_interrupts(void)
{
    observe(2, 0, 0, 0, 0);
    if (mode & 2) {
        WORD(0x1b0c8) = 0x630000 + (seed & 3);
        WORD(0x1b0d0) = seed % 5;
        WORD(0x1b0cc) = seed % 7;
        WORD(0x1b118) = (seed / 7) % 8;
        WORD(0x1b160) = 0x704001;
    }
    delete_table = WORD(0x1b0c8);
}

void kernel_write_flags(uint32_t flags)
{
    observe(3, flags, flags_writes, 0, 0);
    WORD(0x640800 + flags_writes++ * 4) = flags;
    if (mode & 32) {
        WORD(0x1b0c8) = 0x630000 + (seed & 3);
        WORD(0x1b0d0) ^= 0xa55ac33c;
        WORD(0x1b118) ^= 0x80000000;
    }
}

uint32_t kernel_copy_string(uint32_t destination, uint32_t source)
{
    uint32_t table = (destination < 0x630000 ? captured_table : 0x630000 + (seed & 3));
    uint32_t index = (destination - table - 44u) / 108u, at = destination;
    observe(4, destination, source, 0, 0);
    if (destination && source) {
        do {
            BYTE(at++) = BYTE(source);
        } while (BYTE(source++));
    } else at = 0;
    if (mode & 4) {
        /* Callback-time globals and record changes must be reloaded where
           the original reloads them, without replacing its captured table. */
        static const uint32_t aliases[] = {0x706003, 0x1b040, 0x1b080, 0};
        uint32_t target = aliases[(seed / 3) % COUNT(aliases)];
        if (!target) target = destination - 44u;
        WORD(destination - 16u) = target;
        WORD(destination - 20u) = 0xfffffff1u;
        WORD(0x1b118) = 2;
        WORD(0x1b0c8) = 0x630000 + (seed & 3);
    }
    if (mode & 8) {
        WORD(0x1b0d0) = (seed & 1) ? index : index + 1u;
        WORD(0x1b0d4) = values[(seed / 11) % COUNT(values)];
    }
    return at;
}

void kernel_set_segment_descriptor(uint32_t address, uint32_t limit,
                                   uint32_t base, uint32_t access)
{
    observe(5, address, limit, base, access);
    WORD(0x640900) = address;
    WORD(0x640904) = limit;
    WORD(0x640908) = base;
    WORD(0x64090c) = access;
    if (mode & 16) {
        WORD(0x1b0d0) = values[seed % COUNT(values)];
        WORD(0x1b0d4) = values[(seed + 1) % COUNT(values)];
        WORD(0x1b0c8) = 0x630000 + (seed & 3);
    }
}

void kernel_load_gdt(uint32_t limit, uint32_t base)
{
    observe(6, limit, base, 0, 0);
    captured_table = WORD(0x1b0c8);
}

void kernel_load_task_register(uint32_t selector)
{
    observe(7, selector, 0, 0, 0);
}

static void allocation_mutation(void)
{
    if (mode & 64) {
        WORD(0x1b0c8) = 0x630000 + (seed & 3);
        WORD(0x1b0d0) = seed % 5;
        WORD(0x1b0cc) = (seed / 5) % 7;
        WORD(0x1b118) = (seed / 7) % 8;
        WORD(0x1b160) = 0x704001;
    }
}

uint32_t kernel_allocate(uint32_t size)
{
    observe(8, size, allocation_result, 0, 0);
    allocation_mutation();
    return allocation_result;
}

uint32_t kernel_allocation_size(uint32_t address)
{
    observe(9, address, allocation_size, 0, 0);
    allocation_mutation();
    return allocation_size;
}

void kernel_free_allocation(uint32_t address)
{
    uint32_t record = delete_table + input[0] * 108u;
    observe(10, address, 0, 0, 0);
    WORD(0x640a00) = address;
    if (mode & 128) {
        WORD(0x1b0c8) = 0x600000 + (seed & 3);
        WORD(0x1b118) = seed & 1 ? input[0] : input[0] + 1u;
        WORD(record + 4) = values[(seed / 7) % COUNT(values)];
        WORD(0x1b0d4) = values[(seed / 11) % COUNT(values)];
    }
}

static void jump(uint32_t address, uintptr_t callback)
{
    BYTE(address) = 0xe9;
    WORD(address + 1) = (uint32_t)callback - address - 5;
}

static void stub_boundaries(uint32_t base)
{
    /* The two tiny flag primitives need nearby short-jump trampolines. */
    *(volatile uint16_t *)(uintptr_t)(base + 0x1a242) = 0xbceb;
    jump(base + 0x1a200, (uintptr_t)kernel_disable_interrupts);
    *(volatile uint16_t *)(uintptr_t)(base + 0x1a24a) = 0xc4eb;
    jump(base + 0x1a210, (uintptr_t)kernel_read_flags);
    jump(base + 0x1a24d, (uintptr_t)kernel_write_flags);
    jump(base + 0x135c6, (uintptr_t)kernel_set_segment_descriptor);
    jump(base + 0x2a32b, (uintptr_t)kernel_copy_string);
    jump(base + 0x372e8, (uintptr_t)kernel_load_gdt);
    jump(base + 0x372f8, (uintptr_t)kernel_load_task_register);
    jump(base + 0x15464, (uintptr_t)kernel_allocate);
    jump(base + 0x15978, (uintptr_t)kernel_free_allocation);
    jump(base + 0x15a0a, (uintptr_t)kernel_allocation_size);
}

static void initialize(uint32_t scenario, uint32_t alignment)
{
    uint32_t r, i, table, name_size, record;
    const uint32_t *spec = scenarios[scenario % COUNT(scenarios)];
    for (r = 0; r < COUNT(regions); ++r)
        for (i = 0; i < regions[r][1]; ++i)
            BYTE(regions[r][0] + i) = (uint8_t)(i * 29u + seed * 71u + r * 17u);
    captured_table = 0x600000 + alignment;
    delete_table = captured_table;
    WORD(0x1b0c8) = captured_table;
    WORD(0x1b0cc) = spec[1];
    WORD(0x1b0d0) = spec[0];
    WORD(0x1b0d4) = values[(seed / 3) % COUNT(values)];
    WORD(0x1b0d8) = values[(seed / 5) % COUNT(values)];
    WORD(0x1b118) = (seed / 7) % 32;
    WORD(0x1b114) = values[(seed / 11) % COUNT(values)];
    WORD(0x1b160) = 0x700000 + alignment;
    WORD(0x1b164) = values[(seed / 13) % COUNT(values)];
    for (r = 0; r < 2; ++r) {
        table = r ? 0x630000 + (seed & 3) : captured_table;
        for (i = 0; i < 36; ++i) {
            WORD(table + i * 108u) = i % 3u + 1u;
            WORD(table + i * 108u + 4) = (i + 35) % 36;
            WORD(table + i * 108u + 8) = (i + 1) % 36;
            name_size = (seed / 19) % 4;
            name_size = name_size == 0 ? 0 : name_size == 1 ? 1 : name_size == 2 ? 30 : 63;
            for (record = 0; record < name_size; ++record)
                BYTE(table + i * 108u + 44u + record) = (uint8_t)(33u + record);
            BYTE(table + i * 108u + 44u + name_size) = 0;
        }
        if (spec[2] != 0xffffffffu) WORD(table + spec[2] * 108u) = 0;
    }
    for (i = 0; i < COUNT(input); ++i) input[i] = values[(seed + i * 3) % COUNT(values)];
    switch ((seed / 17) % 8) {
    case 0: input[5] = 0; break;
    case 1: input[5] = 0x80000000; break;
    case 2: input[5] = 0xffffffff; break;
    case 3: input[5] = 0x640400 + alignment; break;
    case 4: input[5] = 0x1b0cc + 4; break;
    case 5: input[5] = 0x1b0d0 + 4; break;
    case 6: input[5] = 0x1b118 + 4; break;
    default: input[5] = captured_table + 4; break;
    }
    allocation_result = input[5];
    allocation_size = sizes[(seed / 7) % COUNT(sizes)];
    input[6] = seed % 5 ? 0x640100 + alignment : 0;
    name_size = (seed / 19) % 4;
    name_size = name_size == 0 ? 0 : name_size == 1 ? 1 : name_size == 2 ? 63 : 80;
    for (i = 0; i < name_size; ++i) BYTE(0x640100 + alignment + i) = (uint8_t)(33u + i);
    BYTE(0x640100 + alignment + name_size) = 0;
    if (path == 1) {
        /* Full-width indices can wrap to the same physical task record.
           All nine previous/next alias combinations are exercised. */
        input[0] = (scenario % 4u) * 0x40000000u + (seed / 13u) % 32u;
        for (r = 0; r < 2; ++r) {
            table = r ? 0x630000 + (seed & 3) : captured_table;
            record = table + input[0] * 108u;
            WORD(record) = values[(seed / 5u) % COUNT(values)];
            WORD(record + 4u) = input[0] + (scenario / 4u) % 3u;
            WORD(record + 8u) = input[0] + (scenario / 12u) % 3u;
            WORD(record + 36u) = values[(seed / 11u) % COUNT(values)];
        }
        WORD(0x1b118) = seed & 1 ? input[0] : input[0] + 1u;
    } else if (path == 2) {
        input[1] = sizes[(seed / 7) % COUNT(sizes)];
        input[2] = (seed / 31u) % 32u;
    } else if (path == 3) {
        static const uint32_t indices[] = {
            0, 1, 2, 31, 32, 0x40000000, 0x40000001,
            0x80000000, 0xc0000001, 0xfffffffe, 0xffffffff
        };
        input[0] = indices[scenario % COUNT(indices)];
        WORD(0x1b0d0) = values[scenario / COUNT(indices)];
        if ((input[0] & 0x3fffffffu) <= 32u)
            TASK(input[0] & 0x3fffffffu, 0) = values[(seed / 4u) % COUNT(values)];
        /* A bad table pointer proves rejected indices do not touch records. */
        if ((seed & 1) && (input[0] >= WORD(0x1b0d0) || (int32_t)input[0] < 0))
            WORD(0x1b0c8) = 0;
    }
    trace_length = flags_reads = flags_writes = 0;
    for (i = 0; i < COUNT(trace); ++i) trace[i] = 0;
}

static int compare(int save)
{
    uint32_t r, i, at = 0;
    for (r = 0; r < COUNT(regions); ++r)
        for (i = 0; i < regions[r][1]; ++i, ++at) {
            if (save) expected_memory[at] = BYTE(regions[r][0] + i);
            else if (expected_memory[at] != BYTE(regions[r][0] + i)) return 0;
        }
    if (save) expected_length = trace_length;
    else if (expected_length != trace_length) return 0;
    for (i = 0; i < COUNT(trace); ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static uint32_t invoke(int original)
{
    uint32_t base = original ? 0x900000 : 0x800000;
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        if (path == 0) return (uint32_t)kernel_create_application_task(
            input[0], input[1], input[2], input[3], input[4], input[5], input[6]);
        if (path == 1) { kernel_delete_task(input[0]); return 0; }
        if (path == 2) return (uint32_t)kernel_create_thread(input[0], input[1], input[2]);
        return kernel_task_is_active(input[0]);
    }
#endif
    if (path == 0) return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t,
                          uint32_t, uint32_t, uint32_t))(base + 0x14a56))
                         (input[0], input[1], input[2], input[3], input[4], input[5], input[6]);
    if (path == 1) { ((void (*)(uint32_t))(base + 0x14e54))(input[0]); return 0; }
    if (path == 2) return ((uint32_t (*)(uint32_t, uint32_t, uint32_t))(base + 0x15002))
                         (input[0], input[1], input[2]);
    return ((uint32_t (*)(uint32_t))(base + 0x150aa))(input[0]);
}

static void print_number(uint32_t number)
{
    char digits[10];
    uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + number % 10); number /= 10; } while (number);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - size), size);
}

static int run_case(uint32_t scenario, uint32_t alignment)
{
    uint32_t expected, actual;
    initialize(scenario, alignment);
    expected = invoke(1);
    compare(1);
    initialize(scenario, alignment);
    actual = invoke(0);
    if (actual != expected || !compare(0)) {
        static const char message[] = "task lifecycle mismatch, path/case: ";
        syscall3(4, 1, (unsigned)(uintptr_t)message, sizeof(message) - 1);
        print_number(path);
        syscall3(4, 1, (unsigned)(uintptr_t)"/", 1);
        print_number(cases);
        syscall3(4, 1, (unsigned)(uintptr_t)"\n", 1);
        return 0;
    }
    ++cases;
    return 1;
}

static int run_test(void)
{
    uint32_t r, scenario, alignment;
    for (r = 0; r < COUNT(regions); ++r)
        if (!map_memory(regions[r][0], regions[r][1], 3)) return 1;
    if (!map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7) ||
        !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 2;
    stub_boundaries(0x900000);
    stub_boundaries(0x800000);
    for (path = 0; path < 4; ++path) {
        uint32_t scenario_count = path == 1 ? 36 : path == 3 ? 11 * COUNT(values) : COUNT(scenarios);
        uint32_t mode_count = path == 3 ? COUNT(values) : path == 1 ? 16 : 128;
        cases = 0;
        for (scenario = 0; scenario < scenario_count; ++scenario)
            for (alignment = 0; alignment < 4; ++alignment)
                for (r = 0; r < mode_count; ++r) {
                    mode = path == 1 ? (r & 3u) | ((r & 4u) << 3) | ((r & 8u) << 4) :
                           path == 3 ? 0 : r;
                    seed = cases;
                    if (!run_case(scenario, alignment)) return 10;
                }
        syscall3(4, 1, (unsigned)(uintptr_t)"task lifecycle path/cases: ", 27);
        print_number(path);
        syscall3(4, 1, (unsigned)(uintptr_t)"/", 1);
        print_number(cases);
        syscall3(4, 1, (unsigned)(uintptr_t)"\n", 1);
    }
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
