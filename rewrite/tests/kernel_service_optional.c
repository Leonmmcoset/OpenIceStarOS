/* Differential execution of all 25 optional service-table wrappers. */
#include "../kernel/service_optional.h"
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)
#define ARITY_0 0
#define ARITY_1 1
#define ARITY_2 2
#define ARITY_3W02 3
#define ARITY_6W12B5 6
#define ARITY_2W1 2
#define ARITY_3W2 3
#define WORDS_0 0
#define WORDS_1 0
#define WORDS_2 0
#define WORDS_3W02 5
#define WORDS_6W12B5 6
#define WORDS_2W1 2
#define WORDS_3W2 4
#define BYTES_0 0
#define BYTES_1 0
#define BYTES_2 0
#define BYTES_3W02 0
#define BYTES_6W12B5 32
#define BYTES_2W1 0
#define BYTES_3W2 0
#define EMPTY_Z 0
#define EMPTY_A 1
#define VALUES_0 0
#define VALUES_1 a0
#define VALUES_2 a0, a1
#define VALUES_3W02 a0, a1, a2
#define VALUES_6W12B5 a0, a1, a2, a3, a4, a5
#define VALUES_2W1 VALUES_2
#define VALUES_3W2 VALUES_3W02
#define CLOBBER_0
#define CLOBBER_1 *(volatile uint32_t *)&a0 ^= 0x91f06bcd;
#define CLOBBER_2 CLOBBER_1
#define CLOBBER_3W02 CLOBBER_1
#define CLOBBER_6W12B5 CLOBBER_1
#define CLOBBER_2W1 CLOBBER_1
#define CLOBBER_3W2 CLOBBER_1

static uint32_t trace[12], expected_trace[12];
static uint32_t return_word, mode, table_address, empty_slot, cases;
static uint32_t stack_after[7] __attribute__((used)), expected_stack[7];
static uint8_t expected_low[0x1000], expected_tables[0x4000];
static const uint32_t values[] = {
    0, 1, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000,
    0xffff, 0x10000, 0x100ff, 0x7fffffff, 0x80000000,
    0xdeadbeef, 0xffffff00, 0xffffffff
};

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
    *(volatile uint32_t *)(0x1b020 + slot * 4) = return_word ^ (count ? args[0] : 0x21436587);
    if (mode & 1) *(volatile uint32_t *)0x1b0e4 = 0x603001;
    if (mode & 2) ((volatile uint32_t *)(uintptr_t)table_address)[slot] = 0xa55ac33c;
    return return_word;
}

#define OPTIONAL(suffix, slot, form, empty, start, end) \
    __attribute__((noinline)) static uint32_t callback_##suffix(OPTIONAL_PARAMETERS_##form) \
    { \
        const uint32_t arguments[] = {VALUES_##form}; \
        CLOBBER_##form \
        return observe(slot, ARITY_##form, arguments); \
    }
#include "../kernel/service_optional.def"
#undef OPTIONAL

struct description {
    uint32_t address, function, callback, slot, count, word_mask, byte_mask, empty_address;
};
static const struct description functions[] = {
#define OPTIONAL(suffix, slot, form, empty, start, end) \
    {start, (uint32_t)kernel_optional_##suffix, (uint32_t)callback_##suffix, \
     slot, ARITY_##form, WORDS_##form, BYTES_##form, EMPTY_##empty},
#include "../kernel/service_optional.def"
#undef OPTIONAL
};

static void initialize(unsigned pattern, unsigned slot)
{
    volatile uint8_t *low = (volatile uint8_t *)0x1b000;
    volatile uint8_t *tables = (volatile uint8_t *)0x600000;
    volatile uint32_t *table;
    unsigned i;
    for (i = 0; i < sizeof(expected_low); ++i) low[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < sizeof(expected_tables); ++i) tables[i] = (uint8_t)(i * 17 + pattern * 59);
    table_address = 0x600000 + pattern * 0x1001;
    table = (volatile uint32_t *)(uintptr_t)table_address;
    for (i = 0; i < COUNT(functions); ++i)
        table[functions[i].slot] = functions[i].callback;
    if (empty_slot) table[slot] = 0;
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

static void print_number(uint32_t number)
{
    char digits[10];
    uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + number % 10); number /= 10; } while (number);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - size), size);
}

static int exercise(const struct description *function, unsigned pattern, uint32_t args[7])
{
    uint32_t original, rewritten, expected;
    unsigned i;
    initialize(pattern, function->slot);
    expected = empty_slot ? (function->empty_address ? table_address + function->slot * 4 : 0) : return_word;
    /* No relative calls or absolute local references occur in these wrappers. */
    original = invoke(function->address + 0x100000, args);
    if (original != expected || trace[0] != !empty_slot) goto failed;
    if (!empty_slot) {
        if (trace[1] != function->slot || trace[2] != function->count || trace[3] != table_address) goto failed;
        for (i = 0; i < function->count; ++i) {
            uint32_t value = args[i];
            if (function->word_mask & (1u << i)) value &= 65535;
            if (function->byte_mask & (1u << i)) value &= 255;
            if (trace[4 + i] != value) goto failed;
        }
    }
    for (i = 0; i < 7; ++i) if (stack_after[i] != args[i]) goto failed;
    compare(1);
    initialize(pattern, function->slot);
#ifdef KERNEL_LINKED_TEST
    rewritten = invoke(function->address, args);
#else
    rewritten = invoke(function->function, args);
#endif
    if (rewritten != original || !compare(0)) goto failed;
    ++cases;
    return 1;
failed:
    PRINT("optional service mismatch slot/case/empty/pattern/mode: ");
    print_number(function->slot); PRINT("/"); print_number(cases); PRINT("/");
    print_number(empty_slot); PRINT("/"); print_number(pattern); PRINT("/");
    print_number(mode); PRINT("\n");
    return 0;
}

/* Linux i386 PTRACE_PEEKUSER/POKEUSER expose u_debugreg at byte offset 252.
 * Two hardware read/write watchpoints observe the global pointer and slot.
 * Change child memory after the first read, before its next instruction. */
struct registers {
    uint32_t ebx, ecx, edx, esi, edi, ebp, eax;
    uint32_t ds, es, fs, gs, original_eax, eip, cs, flags, esp, ss;
};

static int trace_call(uint32_t request, uint32_t child, uint32_t address, uint32_t data)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (26), "b" (request), "c" (child), "d" (address), "S" (data) : "memory");
    return result;
}

__attribute__((naked, noinline)) static uint32_t watched_first(void)
{
    __asm__("movl $0x91a2b3c4,%eax\n\tret");
    __builtin_unreachable();
}

__attribute__((naked, noinline)) static uint32_t watched_second(void)
{
    __asm__("movl $0xe5f60718,%eax\n\tret");
    __builtin_unreachable();
}

extern const char after_watch_call;
__attribute__((noinline)) static void watched_child(uint32_t entry, uint32_t args[7])
{
    uint32_t value = invoke(entry, args);
    __asm__ volatile (".global after_watch_call\n\tafter_watch_call:\tint3" : : "a" (value) : "memory");
    syscall3(1, 0, 0, 0);
    __builtin_unreachable();
}

static int wait_child(int child, int *status)
{
    return syscall3(7, (unsigned)child, (unsigned)(uintptr_t)status, 0) == child;
}

static int reread_case(const struct description *function, unsigned scenario, int original)
{
    uint32_t args[7] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00, 0x13579bdf, 0x2468ace0, 0xffffffff};
    uint32_t global_reads = 0, slot_reads = 0, traps = 0;
    uint32_t slot_address, entry = function->address + 0x100000;
    int child, status = 0;
    empty_slot = scenario == 2;
    initialize(0, function->slot);
    slot_address = table_address + 4 * function->slot;
    *(volatile uint32_t *)(uintptr_t)slot_address = empty_slot ? 0 : (uint32_t)watched_first;
    *(volatile uint32_t *)(uintptr_t)(0x603000 + 4 * function->slot) = (uint32_t)watched_second;
    if (!original) {
#ifdef KERNEL_LINKED_TEST
        entry = function->address;
#else
        entry = function->function;
#endif
    }
    child = syscall3(2, 0, 0, 0);
    if (child < 0) return 0;
    if (!child) {
        if (trace_call(0, 0, 0, 0) < 0) syscall3(1, 81, 0, 0);
        syscall3(37, (unsigned)syscall3(20, 0, 0, 0), 19, 0);
        watched_child(entry, args);
    }
    if (!wait_child(child, &status) || status != 0x137f ||
        trace_call(6, (unsigned)child, 252, 0x1b0e4) < 0 ||
        trace_call(6, (unsigned)child, 256, slot_address) < 0 ||
        trace_call(6, (unsigned)child, 280, 0xff0005) < 0 ||
        trace_call(7, (unsigned)child, 0, 0) < 0) goto failed;
    while (wait_child(child, &status)) {
        struct registers regs;
        uint32_t debug_status = 0, stop = (unsigned)status >> 8;
        if (++traps > 8 || (status & 255) != 127 ||
            trace_call(12, (unsigned)child, 0, (uint32_t)(uintptr_t)&regs) < 0) goto failed;
        if (stop == 11 && scenario == 1 && regs.eip == 0 && global_reads == 1 && slot_reads == 2) {
            syscall3(37, (unsigned)child, 9, 0);
            return wait_child(child, &status);
        }
        if (stop != 5) goto failed;
        if (regs.eip == (uint32_t)(uintptr_t)&after_watch_call + 1) {
            uint32_t expected = scenario == 0 ? 0xe5f60718 :
                scenario == 2 ? (function->empty_address ? slot_address : 0) : 0x91a2b3c4;
            if (scenario == 1 || regs.eax != expected || global_reads != 1 ||
                slot_reads != (scenario == 2 ? 1u : 2u) ||
                trace_call(7, (unsigned)child, 0, 0) < 0 ||
                !wait_child(child, &status) || status != 0) goto failed;
            return 1;
        }
        if (trace_call(3, (unsigned)child, 276, (uint32_t)(uintptr_t)&debug_status) < 0) goto failed;
        if (debug_status & 1) {
            if (++global_reads != 1 || trace_call(5, (unsigned)child, 0x1b0e4, 0x603000) < 0) goto failed;
        }
        if (debug_status & 2) {
            if (++slot_reads > 2) goto failed;
            if (slot_reads == 1 && scenario < 3 &&
                trace_call(5, (unsigned)child, slot_address,
                           scenario == 1 ? 0 : (uint32_t)watched_second) < 0) goto failed;
        }
        if (!(debug_status & 3) || trace_call(6, (unsigned)child, 276, 0) < 0 ||
            trace_call(7, (unsigned)child, 0, 0) < 0) goto failed;
    }
failed:
    syscall3(37, (unsigned)child, 9, 0);
    (void)wait_child(child, &status);
    PRINT("optional reread mismatch slot/scenario/original/global/slot/status: ");
    print_number(function->slot); PRINT("/"); print_number(scenario); PRINT("/");
    print_number((uint32_t)original); PRINT("/"); print_number(global_reads); PRINT("/");
    print_number(slot_reads); PRINT("/"); print_number((uint32_t)status); PRINT("\n");
    return 0;
}

static int run_test(void)
{
    unsigned function, pattern, vector, i;
    uint32_t args[7];
    if (!map_memory(0x1b000, 0x1000, 3) || !map_memory(0x600000, 0x4000, 3) ||
        !map_memory(0x800000, 0x40000, 7) || !map_memory(0x900000, 0x40000, 7)) return 1;
    if (!load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 2;
    for (function = 0; function < COUNT(functions); ++function) {
        for (pattern = 0; pattern < 4; ++pattern)
            for (mode = 0; mode < 4; ++mode)
                for (empty_slot = 0; empty_slot < 2; ++empty_slot)
                    for (vector = 0; vector < COUNT(values); ++vector) {
                        for (i = 0; i < 7; ++i) args[i] = values[(vector + i) % COUNT(values)];
                        return_word = values[(vector + 7) % COUNT(values)];
                        if (!exercise(&functions[function], pattern, args)) return 10 + function;
                    }
        empty_slot = 0;
        if (functions[function].word_mask)
            for (vector = 0; vector < 65536; ++vector) {
                for (i = 0; i < 7; ++i)
                    args[i] = (0x93a50000u ^ (i << 20)) |
                        ((vector * (2 * i + 1) + 257 * i) & 65535);
                return_word = values[vector % COUNT(values)];
                mode = (vector >> 2) & 3;
                if (!exercise(&functions[function], vector & 3, args)) return 40 + function;
            }
        if (functions[function].byte_mask)
            for (vector = 0; vector < 512; ++vector) {
                for (i = 0; i < 7; ++i) args[i] = 0x93a51000u + i * 0x123456u;
                args[5] = (vector & 255) | (vector < 256 ? 0 : 0x9abcde00u);
                return_word = values[vector % COUNT(values)];
                mode = (vector >> 2) & 3;
                if (!exercise(&functions[function], vector & 3, args)) return 70 + function;
            }
    }
    PRINT("optional service original/C comparisons: "); print_number(cases); PRINT("\n");
    cases = 0;
    for (function = 0; function < COUNT(functions); ++function)
        for (vector = 0; vector < 4; ++vector) {
            if (!reread_case(&functions[function], vector, 1) ||
                !reread_case(&functions[function], vector, 0)) return 100 + function;
            ++cases;
        }
    PRINT("optional service watched read-order comparisons: "); print_number(cases); PRINT("\n");
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
