#include "../kernel/service_forward.h"

#define ARITY_1 1
#define ARITY_3 3
#define ARITY_4 4
#define ARITY_5 5
#define ARITY_6 6
#define ARITY_6S 6
#define ARITY_7 7
#define SIGNED_1 0
#define SIGNED_3 0
#define SIGNED_4 0
#define SIGNED_5 0
#define SIGNED_6 0
#define SIGNED_6S 1
#define SIGNED_7 0
#define VALUES_1 a0
#define VALUES_3 VALUES_1, a1, a2
#define VALUES_4 VALUES_3, a3
#define VALUES_5 VALUES_4, a4
#define VALUES_6 VALUES_5, a5
#define VALUES_6S VALUES_6
#define VALUES_7 VALUES_6, a6

static uint32_t trace[12], expected_trace[12];
static uint32_t return_word, mode, table_address;
static uint32_t stack_after[7] __attribute__((used)), expected_stack[7];
static uint8_t expected_low[0x1000], expected_tables[0x4000];
static const uint32_t values[] = {0, 1, 0x7f, 0x80, 0xff, 0x100,
    0x7fff, 0x8000, 0x7fffffff, 0x80000000, 0xdeadbeef, 0xffffffff};

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
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

/* Copy all seven outer argument words, then observe them after the wrapper
 * returns. Callbacks intentionally overwrite their own first parameter slot;
 * an accidental sibling jump would leak that write into this argument area. */
__attribute__((naked, noinline)) static uint32_t invoke(
    uint32_t entry __attribute__((unused)), const uint32_t *args __attribute__((unused)))
{
    __asm__("pushl %ebp\n\tmovl %esp,%ebp\n\tsubl $32,%esp\n\t"
            "movl 12(%ebp),%edx\n\t"
            "movl 0(%edx),%eax\n\tmovl %eax,0(%esp)\n\t"
            "movl 4(%edx),%eax\n\tmovl %eax,4(%esp)\n\t"
            "movl 8(%edx),%eax\n\tmovl %eax,8(%esp)\n\t"
            "movl 12(%edx),%eax\n\tmovl %eax,12(%esp)\n\t"
            "movl 16(%edx),%eax\n\tmovl %eax,16(%esp)\n\t"
            "movl 20(%edx),%eax\n\tmovl %eax,20(%esp)\n\t"
            "movl 24(%edx),%eax\n\tmovl %eax,24(%esp)\n\t"
            "call *8(%ebp)\n\tmovl %eax,-4(%ebp)\n\t"
            "movl $stack_after,%edx\n\t"
            "movl 0(%esp),%ecx\n\tmovl %ecx,0(%edx)\n\t"
            "movl 4(%esp),%ecx\n\tmovl %ecx,4(%edx)\n\t"
            "movl 8(%esp),%ecx\n\tmovl %ecx,8(%edx)\n\t"
            "movl 12(%esp),%ecx\n\tmovl %ecx,12(%edx)\n\t"
            "movl 16(%esp),%ecx\n\tmovl %ecx,16(%edx)\n\t"
            "movl 20(%esp),%ecx\n\tmovl %ecx,20(%edx)\n\t"
            "movl 24(%esp),%ecx\n\tmovl %ecx,24(%edx)\n\t"
            "movl -4(%ebp),%eax\n\tleave\n\tret");
    __builtin_unreachable();
}

static uint32_t observe(unsigned slot, unsigned count, const uint32_t *args)
{
    unsigned i;
    ++trace[0];
    trace[1] = slot;
    trace[2] = count;
    trace[3] = *(volatile uint32_t *)0x1b0e4;
    for (i = 0; i < count; ++i) trace[4 + i] = args[i];
    *(volatile uint32_t *)(0x1b020 + slot * 4) = return_word ^ args[0];
    if (mode & 1) *(volatile uint32_t *)0x1b0e4 = 0x603001;
    if (mode & 2) ((volatile uint32_t *)(uintptr_t)table_address)[slot] = 0xa55ac33c;
    return return_word;
}

#define SERVICE(suffix, slot, form, start, end) \
    __attribute__((noinline)) static uint32_t callback_##suffix(SERVICE_PARAMETERS_##form) \
    { \
        const uint32_t arguments[] = {VALUES_##form}; \
        *(volatile uint32_t *)&a0 ^= 0x91f06bcd; \
        return observe(slot, ARITY_##form, arguments); \
    }
#include "../kernel/service_forward.def"
#undef SERVICE

struct description { uint32_t address, function, callback, slot, count, signed_byte; };
static const struct description functions[] = {
#define SERVICE(suffix, slot, form, start, end) \
    {start, (uint32_t)kernel_service_##suffix, (uint32_t)callback_##suffix, \
     slot, ARITY_##form, SIGNED_##form},
#include "../kernel/service_forward.def"
#undef SERVICE
};

static void initialize(unsigned pattern)
{
    volatile uint8_t *low = (volatile uint8_t *)0x1b000;
    volatile uint8_t *tables = (volatile uint8_t *)0x600000;
    volatile uint32_t *table;
    unsigned i;
    for (i = 0; i < sizeof(expected_low); ++i) low[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < sizeof(expected_tables); ++i) tables[i] = (uint8_t)(i * 17 + pattern * 59);
    table_address = 0x600003 + pattern * 0x1001;
    table = (volatile uint32_t *)(uintptr_t)table_address;
    for (i = 0; i < sizeof(functions) / sizeof(functions[0]); ++i)
        table[functions[i].slot] = functions[i].callback;
    *(volatile uint32_t *)0x1b0e4 = table_address;
    for (i = 0; i < 12; ++i) trace[i] = 0;
}

static int compare(int save)
{
    volatile uint8_t *low = (volatile uint8_t *)0x1b000;
    volatile uint8_t *tables = (volatile uint8_t *)0x600000;
    unsigned i;
    for (i = 0; i < sizeof(expected_low); ++i) {
        if (save) expected_low[i] = low[i];
        else if (expected_low[i] != low[i]) return 0;
    }
    for (i = 0; i < sizeof(expected_tables); ++i) {
        if (save) expected_tables[i] = tables[i];
        else if (expected_tables[i] != tables[i]) return 0;
    }
    for (i = 0; i < 12; ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    for (i = 0; i < 7; ++i) {
        if (save) expected_stack[i] = stack_after[i];
        else if (expected_stack[i] != stack_after[i]) return 0;
    }
    return 1;
}

static int exercise(const struct description *function, unsigned pattern, uint32_t args[7])
{
    uint32_t original, rewritten;
    unsigned i;
    initialize(pattern);
    /* These original wrappers contain no relative calls or absolute local
     * references, so the reference copy can execute at its +1 MiB mapping. */
    original = invoke(function->address + 0x100000, args);
    if (original != return_word || trace[0] != 1 || trace[1] != function->slot ||
        trace[2] != function->count) return 0;
    for (i = 0; i < function->count; ++i) {
        uint32_t value = args[i];
        if (function->signed_byte && i == 5) value = (uint32_t)(int32_t)(int8_t)value;
        if (trace[4 + i] != value) return 0;
    }
    for (i = 0; i < 7; ++i) if (stack_after[i] != args[i]) return 0;
    compare(1);
    initialize(pattern);
#ifdef KERNEL_LINKED_TEST
    rewritten = invoke(function->address, args);
#else
    rewritten = invoke(function->function, args);
#endif
    return rewritten == original && compare(0);
}

static int run_test(void)
{
    unsigned function, pattern, vector, i;
    uint32_t args[7];
    if (!map_memory(0x1b000, 0x1000, 3) || !map_memory(0x600000, 0x4000, 3) ||
        !map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7)) return 1;
    if (!load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 2;
    for (function = 0; function < sizeof(functions) / sizeof(functions[0]); ++function)
        for (pattern = 0; pattern < 2; ++pattern)
            for (mode = 0; mode < 4; ++mode) {
                for (vector = 0; vector < sizeof(values) / sizeof(values[0]); ++vector) {
                    for (i = 0; i < 7; ++i) args[i] = values[(vector + i) % 12];
                    return_word = values[(vector + 7) % 12];
                    if (!exercise(&functions[function], pattern, args)) return 10 + function;
                }
                if (!functions[function].signed_byte) continue;
                for (vector = 0; vector < 512; ++vector) {
                    for (i = 0; i < 7; ++i) args[i] = 0x93a51000 + i * 0x123456;
                    args[5] = (vector & 255) | (vector < 256 ? 0 : 0x9abcde00);
                    return_word = values[vector % 12];
                    if (!exercise(&functions[function], pattern, args)) return 40 + function;
                }
            }
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
