/* Original x86 versus host-linked and installed C heap services. Only flags
 * and CLI are common callbacks. Compare all mapped data and boundary traces. */
#include "../kernel/memory.h"
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define FIELD(a, o) WORD((a) + (o))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)
#define ROOT 0x2000000u

/* Nonmonotonic links create unsigned spans large enough to accept 2 GiB
   requests. Map their resulting headers too, without masking wraparound. */
static const uint32_t regions[][2] = {
    {0x1b000, 0x1000}, {0x1ffe000, 0x12000}, {0x81ffe000, 0x12000}
};
static uint8_t expected_memory[0x25000];
static uint32_t trace[256], expected_trace[256], trace_length, expected_length;
static uint32_t input[3], nodes[4], node_count, path, cases, seed, mode, pattern;
static uint32_t scenario, request, alignment, variant, flags_reads, flags_writes;
static uint32_t mismatch_address;
static const uint32_t values[] = {
    0, 1, 2, 3, 0x202, 0xffff, 0x10000, 0x7fffffff,
    0x80000000, 0xfffffffe, 0xffffffff, 0xa55ac33c
};
static const uint32_t requests[] = {
    0, 1, 15, 16, 0x3ef, 0x3f0, 0x3f1, 0x400, 0x7f0, 0x7f1,
    0x1000, 0x1ff0, 0x4000, 0x7fffffff, 0x80000000,
    0xfffff000, 0xfffff001, 0xfffffbf0, 0xfffffbf1, 0xfffffffe, 0xffffffff
};
static const uint32_t alignments[] = {1, 2, 3, 4, 7, 8, 15, 16, 17, 31, 64, 255, 256, 1024, 4096, 16384};
/* count, first span, second/third absolute offsets, four used-state fields. */
static const uint32_t layouts[][8] = {
    {1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 1, 0, 0, 0},
    {2, 0x400, 0, 0, 0, 0, 0, 0},
    {2, 0x800, 0, 0, 0, 0, 0, 0},
    {2, 0xc00, 0, 0, 0, 0, 0, 0},
    {2, 0x80f, 0, 0, 0, 0, 0, 0},
    {2, 0x810, 0, 0, 0, 0, 0, 0},
    {2, 0x2001, 0, 0, 1, 0, 0, 0},
    {2, 0x2003, 0, 0, 2, 1, 0, 0},
    {4, 0x1000, 0x3000, 0x6000, 0, 0, 0, 0},
    {4, 0x1001, 0x3002, 0x6003, 1, 0, 1, 0},
    {4, 0x400, 0x800, 0xc00, 1, 1, 1, 1},
    {4, 0x400, 0x800, 0xc00, 0x80000000, 2, 0xffffffff, 0},
    {4, 0x400, 0x3000, 0x6000, 0, 1, 0, 1},
    {4, 0x400, 0x3000, 0x6000, 1, 0, 1, 0},
    {3, 0x400, 0x800, 0, 0, 1, 0, 0},
    {3, 0x400, 0x800, 0, 1, 1, 0, 0},
    {3, 0x400, 0x800, 0, 0, 1, 1, 0},
    {4, 0x10, 0x20, 0x30, 1, 0, 1, 0},
    {4, 0x6000, 0x1000, 0x3000, 1, 1, 1, 0},
    {3, 0x101, 0x1001, 0, 2, 0, 0, 0},
    {4, 0x401, 0x801, 0xc01, 0, 0, 0, 2}
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

static void observe(uint32_t event, uint32_t argument)
{
    uint32_t i, j;
    if (trace_length + 23u > COUNT(trace)) finish(90);
    trace[trace_length++] = event;
    trace[trace_length++] = argument;
    trace[trace_length++] = WORD(0x1b050);
    trace[trace_length++] = WORD(0x1b0d8);
    trace[trace_length++] = flags_reads;
    trace[trace_length++] = flags_writes;
    for (i = 0; i < 4; ++i)
        for (j = 0; j < 16; j += 4) trace[trace_length++] = FIELD(nodes[i], j);
}

uint32_t kernel_read_flags(void)
{
    uint32_t flags = values[seed % COUNT(values)];
    observe(1, flags);
    ++flags_reads;
    if (mode & 1) {
        WORD(0x1b0d8) = values[(seed + 3) % COUNT(values)];
        FIELD(ROOT, 8) = (seed & 1) ? 0 : 2;
    }
    return flags;
}

void kernel_disable_interrupts(void)
{
    observe(2, 0);
    if (mode & 2) {
        WORD(0x1b0d8) = values[(seed + 7) % COUNT(values)];
        WORD(0x1b050) = ROOT + 0x6800;
        if (node_count > 1) FIELD(nodes[1], 8) = (seed / 7) % 3;
    }
}

void kernel_write_flags(uint32_t flags)
{
    observe(3, flags);
    ++flags_writes;
    if (mode & 4) {
        WORD(0x1b0d8) ^= 0xa55ac33c;
        FIELD(ROOT, 12) ^= 0x80000000;
    }
}

static void redirect(uint32_t address, uint32_t target)
{
    BYTE(address) = 0xe9;
    WORD(address + 1u) = target - address - 5u;
}

static void short_trampoline(uint32_t address, uint32_t destination, uintptr_t callback)
{
    int32_t distance = (int32_t)(destination - address - 2u);
    if (distance < -128 || distance > 127) finish(91);
    *(volatile uint16_t *)(uintptr_t)address = 0xeb | ((uint16_t)(uint8_t)distance << 8);
    redirect(destination, (uint32_t)callback);
}

static void initialize(void)
{
    uint32_t i, j, address, owner;
    trace_length = flags_reads = flags_writes = 0;
    for (i = 0; i < COUNT(regions); ++i)
        for (j = 0; j < regions[i][1]; ++j)
            BYTE(regions[i][0] + j) = (uint8_t)(j * 29u + seed * 71u + pattern * 17u);
    node_count = layouts[scenario][0];
    nodes[0] = ROOT;
    for (i = 1; i < 4; ++i) nodes[i] = ROOT + (i < node_count ? layouts[scenario][i] : 0x7000 + 16u * i);
    for (i = 0; i < node_count; ++i) {
        address = nodes[i];
        FIELD(address, 0) = i + 1u < node_count ? nodes[i + 1u] : 0;
        FIELD(address, 4) = i ? nodes[i - 1u] : 0;
        FIELD(address, 8) = layouts[scenario][i + 4u];
        owner = (seed / 3u + i) % COUNT(values);
        FIELD(address, 12) = values[owner];
    }
    /* The original aligned allocator unconditionally dereferences previous,
       including for the first header. Supply a mapped sentinel in these
       nonfaulting comparisons, and test the real null case separately. */
    if (path == 2) FIELD(ROOT, 4) = ROOT + 0x7100;
    WORD(0x1b050) = ROOT + 0x9000;
    WORD(0x1b0d8) = values[(seed + 1) % COUNT(values)];
    input[0] = request;
    input[1] = values[(seed / 5) % COUNT(values)];
    input[2] = alignment;
    if (path == 3 || path == 4) {
        input[0] = variant == 0 ? 0 : nodes[(variant - 1u) % node_count] + 16u;
        if (path == 3 && variant > node_count) {
            address = input[0] - 16u;
            /* Self-links and a previous pointer aliasing this header must
               retain the original write/reload order. No traversal follows. */
            if (variant == node_count + 1u) FIELD(address, 4) = address;
            if (variant == node_count + 2u) FIELD(address, 0) = address;
        }
        if (path == 4 && input[0]) FIELD(input[0] - 16u, 0) = request;
    }
    if (path == 5) input[0] = request;
}

static uint32_t invoke(int original)
{
    static const uint32_t offsets[] = {0x15464, 0x155ca, 0x15728, 0x15978, 0x15a0a, 0x15a42};
    uint32_t entry = (original ? 0x900000 : 0x800000) + offsets[path];
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        if (path == 0) return kernel_allocate(input[0]);
        if (path == 1) return kernel_allocate_for_task(input[0], input[1]);
        if (path == 2) return kernel_allocate_aligned(input[0], input[1], input[2]);
        if (path == 3) { kernel_free_allocation(input[0]); return 0; }
        if (path == 4) return kernel_allocation_size(input[0]);
        return kernel_task_allocation_total(input[0]);
    }
#endif
    if (path == 3) { ((void (*)(uint32_t))entry)(input[0]); return 0; }
    if (path == 0 || path >= 4) return ((uint32_t (*)(uint32_t))entry)(input[0]);
    if (path == 1) return ((uint32_t (*)(uint32_t, uint32_t))entry)(input[0], input[1]);
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t))entry)(input[0], input[1], input[2]);
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
    PRINT("heap mismatch path/case/scenario/request/alignment/mode/expected/actual/address: ");
    print_number(path); PRINT("/"); print_number(cases); PRINT("/");
    print_number(scenario); PRINT("/"); print_number(request); PRINT("/");
    print_number(alignment); PRINT("/"); print_number(mode); PRINT("/");
    print_number(expected); PRINT("/"); print_number(actual); PRINT("/");
    print_number(mismatch_address); PRINT("\n");
    return 0;
}

static int fault_status(int original, uint32_t fault)
{
    int child = syscall3(2, 0, 0, 0), status = 0;
    if (child < 0) return -1;
    if (!child) {
        path = 2; scenario = 0; mode = 0; pattern = 0; seed = 0;
        request = 8; alignment = fault == 0 ? 0 : fault == 1 ? 16 : 0xffffffff;
        initialize();
        if (fault == 1) FIELD(ROOT, 4) = 0;
        if (fault == 2) {
            /* Signed address division INT_MIN / -1 after an ordinary first
               occupied header. Header crosses the two mapped high pages. */
            FIELD(ROOT, 8) = 1;
            FIELD(ROOT, 0) = 0x7ffffff2;
            FIELD(0x7ffffff2, 0) = ROOT + 0x1000;
        }
        (void)invoke(original);
        finish(0);
    }
    if (syscall3(7, (unsigned)child, (unsigned)(uintptr_t)&status, 0) != child) return -1;
    return status & 127;
}

static int run_test(void)
{
    uint32_t i, j, k, base, fault, limits[2] = {0, 0};
    syscall3(75, 4, (unsigned)(uintptr_t)limits, 0); /* No core files from expected faults. */
    for (i = 0; i < COUNT(regions); ++i)
        if (!map_memory(regions[i][0], regions[i][1], 3)) return 1;
    if (!map_memory(0x7ffff000, 0x2000, 3) ||
        !map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7) ||
        !load_kernel("build/icestar.x", 0x900000) || !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 1;
    for (base = 0x800000; base <= 0x900000; base += 0x100000) {
        /* CLI and flags-read entries are shorter than a near JMP. */
        short_trampoline(base + 0x1a24a, base + 0x1a210, (uintptr_t)kernel_read_flags);
        short_trampoline(base + 0x1a242, base + 0x1a200, (uintptr_t)kernel_disable_interrupts);
        redirect(base + 0x1a24d, (uint32_t)(uintptr_t)kernel_write_flags);
    }
    for (path = 0; path < 6; ++path) {
        cases = 0;
        for (scenario = 0; scenario < COUNT(layouts); ++scenario)
            for (pattern = 0; pattern < 2; ++pattern) {
                if (path <= 2) {
                    for (i = 0; i < COUNT(requests); ++i)
                        for (j = 0; j < (path == 2 ? COUNT(alignments) : 1u); ++j)
                            for (mode = 0; mode < (path == 2 ? 4u : 8u); ++mode) {
                                request = requests[i]; alignment = alignments[j];
                                if (!run_case()) return 10;
                            }
                } else if (path == 3) {
                    for (variant = 0; variant < 7; ++variant)
                        for (mode = 0; mode < 8; ++mode)
                            if (!run_case()) return 10;
                } else if (path == 4) {
                    mode = 0;
                    for (variant = 0; variant < 5; ++variant)
                        for (k = 0; k < COUNT(values); ++k) {
                            request = values[k];
                            if (!run_case()) return 10;
                        }
                } else {
                    mode = 0;
                    for (k = 0; k < COUNT(values); ++k) {
                        request = values[k];
                        if (!run_case()) return 10;
                    }
                }
            }
        PRINT("heap path/cases: "); print_number(path); PRINT("/"); print_number(cases); PRINT("\n");
    }
    for (fault = 0; fault < 3; ++fault) {
        int expected = fault_status(1, fault), actual = fault_status(0, fault);
        if (expected != (fault == 1 ? 11 : 8) || actual != expected) {
            PRINT("heap fault mismatch path/expected/actual: ");
            print_number(fault); PRINT("/"); print_number((uint32_t)expected); PRINT("/");
            print_number((uint32_t)actual); PRINT("\n");
            return 20;
        }
    }
    PRINT("heap divide-by-zero, null-previous, and signed-divide-overflow faults match\n");
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
