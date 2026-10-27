/* Execute original x86 and rebuilt transfers with identical data, registers,
 * flags, XMM state and a fixed test stack. No transfer helper is mocked. */
#include "../kernel/memory_accel.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)
#define USED __attribute__((used))

static USED uint32_t entry_point, call_arguments[3], call_flags, saved_host_stack;
static USED uint32_t machine_state[12], xmm_state[32], xmm_seed[32];
static uint32_t expected_machine[12], expected_xmm[32];
static uint8_t expected_memory[0x4000];
static uint32_t path, cases, bound, length, pattern, alignment, variant, value, mode;
static uint32_t mismatch_address;
static const uint32_t counts[] = {
    0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 63, 64, 65, 127, 255, 256, 257, 511
};
static const uint32_t negative_counts[] = {
    0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffffb, 0x80000000, 0x80000001
};
static const uint32_t sse_counts[] = {1, 2, 3, 4, 7, 15, 16, 17, 31, 63};
static const uint32_t values[] = {0, 1, 0x80, 0xff, 0x100, 0xffff, 0x10000, 0xa55ac33c, 0xffffffff};
static const int32_t overlaps[] = {
    -65, -64, -63, -17, -16, -15, -4, -3, -2, -1, 0,
    1, 2, 3, 4, 15, 16, 17, 63, 64, 65
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

static int map_memory(uint32_t address, uint32_t size, uint32_t protection, uint32_t flags)
{
    uint32_t args[] = {address, size, protection, flags, 0xffffffff, 0};
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

__attribute__((naked, noinline)) static void capture_call(void)
{
    __asm__ volatile (
        "pushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl %esp,saved_host_stack\n\tmovl $0x50fff0,%esp\n\t"
        "pushl call_arguments+8\n\tpushl call_arguments+4\n\tpushl call_arguments\n\t"
        "movups xmm_seed,%xmm0\n\tmovups xmm_seed+16,%xmm1\n\t"
        "movups xmm_seed+32,%xmm2\n\tmovups xmm_seed+48,%xmm3\n\t"
        "movups xmm_seed+64,%xmm4\n\tmovups xmm_seed+80,%xmm5\n\t"
        "movups xmm_seed+96,%xmm6\n\tmovups xmm_seed+112,%xmm7\n\t"
        "pushl call_flags\n\tpopfl\n\t"
        "movl $0x10293847,%eax\n\tmovl $0x21384957,%ecx\n\t"
        "movl $0x32495867,%edx\n\tmovl $0x435a6977,%ebx\n\t"
        "movl $0x546b7a87,%ebp\n\tmovl $0x657c8b97,%esi\n\t"
        "movl $0x768d9ca7,%edi\n\tcall *entry_point\n\t"
        "pushfl\n\tpushal\n\tcld\n\tmovl %esp,%esi\n\t"
        "movl $machine_state,%edi\n\tmovl $12,%ecx\n\trep movsl\n\t"
        "movups %xmm0,xmm_state\n\tmovups %xmm1,xmm_state+16\n\t"
        "movups %xmm2,xmm_state+32\n\tmovups %xmm3,xmm_state+48\n\t"
        "movups %xmm4,xmm_state+64\n\tmovups %xmm5,xmm_state+80\n\t"
        "movups %xmm6,xmm_state+96\n\tmovups %xmm7,xmm_state+112\n\t"
        "movl saved_host_stack,%esp\n\tpopl %edi\n\tpopl %esi\n\t"
        "popl %ebx\n\tpopl %ebp\n\tret");
    __builtin_unreachable();
}

static void initialize(void)
{
    uint32_t i, source = 0x601800 + alignment, destination = 0x600100 + alignment;
    for (i = 0; i < sizeof(expected_memory); ++i) {
        uint8_t byte = (uint8_t)(i * 29u + cases * 71u + (i >> 8) * 17u);
        if (pattern == 1) byte = 0;
        if (pattern == 2) byte = 0xff;
        BYTE(0x600000 + i) = byte;
    }
    for (i = 0; i < COUNT(xmm_seed); ++i)
        xmm_seed[i] = 0x87654321u + i * 0x1234567u + cases;
    call_flags = mode ? 0xe93 : 0x246;
    if (path == 2 || path == 4 || path == 6) {
        if (variant == 0) destination = 0x600000 + alignment;
        if (variant == 1) destination = 0x601800 + alignment;
        if (variant == 2) destination = 0x604000 - length;
        if (variant == 3) destination = 0xffffffff;
    } else {
        if (variant == 1) { source = 0x600100 + alignment; destination = 0x602000 + alignment; }
        if (variant >= 2 && variant <= 22) destination = source + (uint32_t)overlaps[variant - 2];
        if (variant == 23) source = 0x604000 - length;
        if (variant == 24) destination = 0x604000 - length;
        if (variant == 25) { source = 0x600000; destination = 0x602000; }
        if (variant == 26) { source = 0xffffffff; destination = 0; }
        if (variant == 27) { source = 0; destination = 0xffffffff; }
    }
    call_arguments[0] = destination;
    call_arguments[1] = path == 2 || path == 4 || path == 6 ? value : source;
    call_arguments[2] = bound;
}

static void invoke(int original)
{
    static const uint32_t offsets[] = {
        0x173da, 0x17484, 0x174a2, 0x174c8, 0x174e8, 0x17510, 0x1752e, 0x28e8f, 0x28eb1
    };
    entry_point = (original ? 0x900000 : 0x800000) + offsets[path];
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        static void (*const functions[])(uint32_t, uint32_t, uint32_t) = {
            (void (*)(uint32_t, uint32_t, uint32_t))kernel_copy_dwords_sse,
            kernel_rep_copy_bytes, kernel_rep_fill_bytes, kernel_rep_copy_words,
            kernel_rep_fill_words, kernel_rep_copy_dwords, kernel_rep_fill_dwords,
            kernel_copy_sse16_blocks, kernel_copy_sse64_blocks
        };
        entry_point = (uint32_t)(uintptr_t)functions[path];
    }
#endif
    capture_call();
}

static void print_number(uint32_t number)
{
    char digits[10];
    uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + number % 10); number /= 10; } while (number);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - size), size);
}

static int memory_matches(int save)
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

static int run_case(void)
{
    uint32_t i;
    initialize();
    invoke(1);
    memory_matches(1);
    for (i = 0; i < COUNT(machine_state); ++i) expected_machine[i] = machine_state[i];
    for (i = 0; i < COUNT(xmm_state); ++i) expected_xmm[i] = xmm_state[i];
    initialize();
    invoke(0);
    if (!memory_matches(0)) goto mismatch;
    for (i = 0; i < COUNT(machine_state); ++i) {
        /* C dispatcher's caller-volatile registers/flags and argument slots
         * are unspecified. Leaf routines retain their exact machine ABI. */
        if (!path && (i == 5 || i == 6 || i == 7 || i >= 9)) continue;
        if (i == 8) {
            uint32_t mask = path ? 0xcd5u : 0x400u;
            if ((machine_state[i] & mask) == (expected_machine[i] & mask)) continue;
        } else if (machine_state[i] == expected_machine[i]) continue;
        mismatch_address = 0xdead0000u + i;
        goto mismatch;
    }
    for (i = 0; i < COUNT(xmm_state); ++i)
        if (xmm_state[i] != expected_xmm[i]) { mismatch_address = 0xbeef0000u + i; goto mismatch; }
    ++cases;
    return 1;
mismatch:
    PRINT("accelerated memory mismatch path/case/variant/count/alignment/mode/address: ");
    print_number(path); PRINT("/"); print_number(cases); PRINT("/");
    print_number(variant); PRINT("/"); print_number(bound); PRINT("/");
    print_number(alignment); PRINT("/"); print_number(mode); PRINT("/");
    print_number(mismatch_address); PRINT("\n");
    return 0;
}

static int fault_status(int original)
{
    int child, status = 0;
    initialize();
    /* Shared data retains writes performed by the child before a fault. */
    call_arguments[0] = 0x600000;
    call_arguments[1] = path == 2 || path == 4 || path == 6 ? 0xa55ac33c : 0x602080;
    call_arguments[2] = bound;
    if (variant == 1) call_arguments[0] = 0x604000;
    if (variant == 2) call_arguments[1] = 0x604000;
    child = syscall3(2, 0, 0, 0);
    if (child < 0) return -1;
    if (!child) { invoke(original); finish(0); }
    if (syscall3(7, (unsigned)child, (unsigned)(uintptr_t)&status, 0) != child) return -1;
    return status & 127;
}

static int run_fault_cases(void)
{
    uint32_t i, original_changed, fault_cases = 0;
    for (path = 1; path < 9; ++path)
        for (mode = 0; mode < 2; ++mode)
            for (variant = 0; variant < 3; ++variant)
                for (i = 0; i < 3; ++i) {
                    int expected, actual;
                    if (variant == 2 && (path == 2 || path == 4 || path == 6)) continue;
                    bound = i == 0 ? 0 : i == 1 ? 0xffffffff : 0x80000000;
                    if (!bound && path < 7) continue;
                    length = 0; pattern = 0; alignment = 0;
                    expected = fault_status(1);
                    memory_matches(1);
                    original_changed = BYTE(0x600000) != (uint8_t)(cases * 71u);
                    actual = fault_status(0);
                    if (expected != 11 || actual != expected || !memory_matches(0) ||
                        (path >= 7 && !bound && !variant && !original_changed)) {
                        PRINT("accelerated memory fault mismatch path/variant/count/expected/actual: ");
                        print_number(path); PRINT("/"); print_number(variant); PRINT("/");
                        print_number(bound); PRINT("/"); print_number((uint32_t)expected); PRINT("/");
                        print_number((uint32_t)actual); PRINT("\n");
                        return 0;
                    }
                    ++fault_cases;
                }
    PRINT("accelerated memory matching fault cases: "); print_number(fault_cases); PRINT("\n");
    return 1;
}

static int run_test(void)
{
    uint32_t i, j, count, limits[2] = {0, 0};
    syscall3(75, 4, (unsigned)(uintptr_t)limits, 0);
    if (!map_memory(0x500000, 0x10000, 3, 0x32) ||
        !map_memory(0x5ff000, 0x1000, 0, 0x32) || !map_memory(0x600000, 0x4000, 3, 0x31) ||
        !map_memory(0x604000, 0x1000, 0, 0x32) || !map_memory(0x800000, 0x40000, 7, 0x32) ||
        !map_memory(0x900000, 0x40000, 7, 0x32) || !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/icestar_c.x", 0x800000)) return 1;
    for (path = 0; path < 9; ++path) {
        int fill = path == 2 || path == 4 || path == 6;
        cases = 0;
        count = path >= 7 ? COUNT(sse_counts) : COUNT(counts);
        if (!path) count += COUNT(negative_counts);
        for (i = 0; i < count; ++i) {
            bound = path >= 7 ? sse_counts[i] : i < COUNT(counts) ? counts[i] : negative_counts[i - COUNT(counts)];
            length = !path ? (int32_t)bound > 0 ? bound * 4u : 0 :
                     path <= 2 ? bound : path <= 4 ? bound * 2u :
                     path <= 6 ? bound * 4u : path == 7 ? bound * 16u : bound * 64u;
            for (pattern = 0; pattern < 3; ++pattern)
                for (alignment = 0; alignment < (path >= 7 ? 16u : 4u); ++alignment)
                    for (mode = 0; mode < 2; ++mode)
                        for (variant = 0; variant < (fill ? 4u : 28u); ++variant) {
                            if (length && variant >= (fill ? 3u : 26u)) continue;
                            for (j = 0; j < (fill ? COUNT(values) : 1u); ++j) {
                                value = values[j];
                                if (!run_case()) return 10;
                            }
                        }
        }
        PRINT("accelerated memory path/cases: ");
        print_number(path); PRINT("/"); print_number(cases); PRINT("\n");
    }
    return run_fault_cases() ? 0 : 11;
}

void _start(void) { finish((unsigned)run_test()); }
