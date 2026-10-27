/* Execute the original and rebuilt i386 functions. Ptrace supplies only
 * CPUID outputs and optional immutable EFLAGS.ID hardware behavior. */
#include "../kernel/cpuid.h"
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (uint32_t)(uintptr_t)(s), sizeof(s) - 1)
#define USED __attribute__((used))
#define BUFFER ((volatile uint8_t *)0x600000)
#define BUFFER_SIZE 8192
static USED uint32_t entry_point, call_flags, output_address;
static uint32_t cases, function_index, mode, available, vector, fault_expected;
static uint8_t expected_buffer[BUFFER_SIZE];
static const uint32_t words[] = {
    0, 1, 0x7f, 0x80, 0xff, 0x100, 0xffff, 0x10000,
    0x7fffffff, 0x80000000, 0xdeadbeef, 0xffffffff
};
static const uint32_t addresses[] = {
    0x600000, 0x600001, 0x600002, 0x600003,
    0x600ff0, 0x600ff1, 0x601fef, 0x601ff0
};
static const uint32_t entries[] = {0x82b678, 0x82b6a0, 0x82b6e8};

struct registers {
    uint32_t ebx, ecx, edx, esi, edi, ebp, eax;
    uint32_t ds, es, fs, gs, original_eax, eip, cs, flags, esp, ss;
};
static struct registers last_registers;
static int syscall3(int number, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}
static int trace_call(uint32_t request, uint32_t child, uint32_t address, uint32_t data)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (26), "b" (request), "c" (child), "d" (address), "S" (data) : "memory");
    return result;
}
static void finish(uint32_t code)
{
    syscall3(1, code, 0, 0);
    __builtin_unreachable();
}
static int map_memory(uint32_t address, uint32_t size, uint32_t protection, uint32_t flags)
{
    uint32_t args[] = {address, size, protection, flags, 0xffffffff, 0};
    return (uint32_t)syscall3(90, (uint32_t)(uintptr_t)args, 0, 0) == address;
}
static int load_kernel(const char *path, uint32_t address)
{
    int fd = syscall3(5, (uint32_t)(uintptr_t)path, 0, 0), size;
    if (fd < 0) return 0;
    size = syscall3(3, (uint32_t)fd, address, 0x40000);
    syscall3(6, (uint32_t)fd, 0, 0);
    return size == 250099;
}
static void print_number(uint32_t n)
{
    char digits[10]; uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + n % 10); n /= 10; } while (n);
    syscall3(4, 1, (uint32_t)(uintptr_t)(digits + 10 - size), size);
}

extern const char before_cpuid_call, after_cpuid_call;
__attribute__((naked, noinline)) static void capture_call(void)
{
    __asm__ volatile (
        "movl $0x50fff0,%esp\n\tpushl output_address\n\t"
        "pushl call_flags\n\tpopfl\n\t"
        "movl $0x10293847,%eax\n\tmovl $0x21384957,%ecx\n\t"
        "movl $0x32495867,%edx\n\tmovl $0x435a6977,%ebx\n\t"
        "movl $0x546b7a87,%ebp\n\tmovl $0x657c8b97,%esi\n\t"
        "movl $0x768d9ca7,%edi\n\t"
        ".global before_cpuid_call\n\tbefore_cpuid_call:\tint3\n\t"
        "call *entry_point\n\t"
        ".global after_cpuid_call\n\tafter_cpuid_call:\tint3\n\tcld\n\t"
        "movl $1,%eax\n\txorl %ebx,%ebx\n\tint $0x80");
    __builtin_unreachable();
}
static int wait_child(int child, int *status)
{
    return syscall3(7, (uint32_t)child, (uint32_t)(uintptr_t)status, 0) == child;
}
static void initialize_buffer(void)
{
    uint32_t i;
    for (i = 0; i < BUFFER_SIZE; ++i) BUFFER[i] = (uint8_t)(i * 37 + cases * 19 + 0x65);
}
static uint32_t sample(uint32_t index) { return words[(vector + 3 * index) % COUNT(words)]; }
static int execute_case(int original, struct registers *result)
{
    int child, status = 0;
    uint32_t steps = 0, cpuid_count = 0, pops = 0;
    entry_point = entries[function_index] + (original ? 0x100000 : 0);
#ifndef KERNEL_LINKED_TEST
    if (!original) {
        static const uint32_t functions[] = {
            (uint32_t)(uintptr_t)kernel_cpuid_available,
            (uint32_t)(uintptr_t)kernel_cpuid_vendor,
            (uint32_t)(uintptr_t)kernel_cpuid_features
        };
        entry_point = functions[function_index];
    }
#endif
    initialize_buffer();
    child = syscall3(2, 0, 0, 0);
    if (child < 0) return 0;
    if (!child) {
        if (trace_call(0, 0, 0, 0) < 0) finish(71);
        syscall3(37, (uint32_t)syscall3(20, 0, 0, 0), 19, 0);
        capture_call(); finish(72);
    }
    if (!wait_child(child, &status) || status != 0x137f || trace_call(7, child, 0, 0) < 0) goto failed;
    while (wait_child(child, &status)) {
        struct registers r;
        uint32_t expected;
        if ((status != 0x57f && status != 0xb7f) || ++steps > 96 ||
            trace_call(12, child, 0, (uint32_t)(uintptr_t)&r) < 0) goto failed;
        last_registers = r;
        if (status == 0xb7f) {
            if (!fault_expected || cpuid_count != 1 || pops) goto failed;
            *result = r;
            syscall3(37, child, 9, 0);
            if (!wait_child(child, &status) || status != 9) goto failed;
            return 1;
        }
        if (r.eip == (uint32_t)(uintptr_t)&after_cpuid_call + 1) {
            expected = !function_index ? available : output_address + (function_index == 2 ? 12 : 0);
            if (fault_expected || r.eax != expected || cpuid_count != (function_index ? 1u : 0u) ||
                pops != (function_index ? 0u : 2u) || r.esp != 0x50ffec ||
                r.ebx != 0x435a6977 || r.ebp != 0x546b7a87 ||
                r.esi != 0x657c8b97 || r.edi != 0x768d9ca7 ||
                ((r.flags ^ call_flags) & 0x200400u)) goto failed;
            *result = r;
            /* PUSHFL/POPFL inside the probe can retain ptrace's TF. Clear
             * only the debugger's trace flag before the child exits. */
            r.flags &= ~0x100u;
            if (trace_call(13, child, 0, (uint32_t)(uintptr_t)&r) < 0 ||
                trace_call(7, child, 0, 0) < 0 || !wait_child(child, &status) || status) goto failed;
            return 1;
        }
        if (*(const uint8_t *)(uintptr_t)r.eip == 0x9d && !function_index) {
            ++pops;
            if (!available && pops == 1) {
                uint32_t saved;
                /* Model a processor whose ID flag cannot change. Emulate
                 * this POPFL before its following PUSHFL samples the flag. */
                if (trace_call(2, child, r.esp, (uint32_t)(uintptr_t)&saved) < 0) goto failed;
                r.flags = (r.flags & ~0xcd5u) | (saved & 0xcd5u);
                r.flags = (r.flags & ~0x200000u) | (call_flags & 0x200000u);
                ++r.eip; r.esp += 4;
                if (trace_call(13, child, 0, (uint32_t)(uintptr_t)&r) < 0) goto failed;
            }
        }
        if (*(const uint16_t *)(uintptr_t)r.eip == 0xa20f) {
            if (!function_index || r.eax != function_index - 1 || cpuid_count++) goto failed;
            r.eax = sample(0); r.ebx = sample(1); r.ecx = sample(2); r.edx = sample(3);
            r.eip += 2;
            if (trace_call(13, child, 0, (uint32_t)(uintptr_t)&r) < 0) goto failed;
        }
        if (trace_call(9, child, 0, 0) < 0) goto failed;
    }
failed:
    syscall3(37, child, 9, 0); (void)wait_child(child, &status);
    PRINT("CPUID mismatch case/function/original/steps/cpuid/pops/status: ");
    print_number(cases); PRINT("/"); print_number(function_index); PRINT("/");
    print_number((uint32_t)original); PRINT("/"); print_number(steps); PRINT("/");
    print_number(cpuid_count); PRINT("/"); print_number(pops); PRINT("/");
    print_number((uint32_t)status); PRINT("\n");
    PRINT("last EIP/EAX/flags/expected flags/ESP: ");
    print_number(last_registers.eip); PRINT("/"); print_number(last_registers.eax); PRINT("/");
    print_number(last_registers.flags); PRINT("/"); print_number(call_flags); PRINT("/");
    print_number(last_registers.esp); PRINT("\n");
    return 0;
}
static int check_case(void)
{
    struct registers original, rebuilt;
    uint32_t i, j;
    if (!execute_case(1, &original)) return 0;
    for (i = 0; i < BUFFER_SIZE; ++i) expected_buffer[i] = BUFFER[i];
    if (!execute_case(0, &rebuilt)) return 0;
    for (i = 0; i < BUFFER_SIZE; ++i) {
        uint8_t oracle = (uint8_t)(i * 37 + cases * 19 + 0x65);
        uint32_t offset = 0x600000u + i - output_address;
        uint32_t writable = fault_expected ? ((0x602000u - output_address) / 4) * 4 : 16;
        if (function_index && offset < 16 && offset < writable) {
            j = offset / 4;
            if (function_index == 1 && j >= 2) j = 5 - j;
            oracle = (uint8_t)(sample(j) >> ((offset % 4) * 8));
        } else if (function_index == 1 && offset == 16 && !fault_expected) oracle = 0;
        if (BUFFER[i] != expected_buffer[i] || BUFFER[i] != oracle) {
            PRINT("CPUID output mismatch at buffer byte "); print_number(i); PRINT("\n");
            return 0;
        }
    }
    ++cases;
    return 1;
}
static int run_test(void)
{
    uint32_t address;
    if (!map_memory(0x500000, 0x10000, 3, 0x32) ||
        !map_memory(0x600000, BUFFER_SIZE, 3, 0x31) ||
        !map_memory(0x602000, 0x1000, 0, 0x32) ||
        !map_memory(0x800000, 0x40000, 7, 0x32) ||
        !map_memory(0x900000, 0x40000, 7, 0x32) ||
        !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/icestar_c.x", 0x800000)) return 1;
    function_index = 0;
    output_address = 0x600000;
    for (mode = 0; mode < 4; ++mode)
        for (available = 0; available < 2; ++available) {
            call_flags = (mode & 1 ? 0xe93 : 0x246) | (mode & 2 ? 0x200000 : 0);
            if (!check_case()) return 10;
        }
    PRINT("CPUID availability original/C cases: "); print_number(cases); PRINT("\n");
    cases = 0;
    for (function_index = 1; function_index < 3; ++function_index)
        for (mode = 0; mode < 4; ++mode)
            for (vector = 0; vector < COUNT(words); ++vector)
                for (address = 0; address < COUNT(addresses); ++address) {
                    if (function_index == 1 && addresses[address] == 0x601ff0) continue;
                    output_address = addresses[address];
                    call_flags = (mode & 1 ? 0xe93 : 0x246) | (mode & 2 ? 0x200000 : 0);
                    if (!check_case()) return 11;
                }
    PRINT("CPUID leaf original/C/oracle cases: "); print_number(cases); PRINT("\n");
    cases = 0;
    fault_expected = 1;
    for (function_index = 1; function_index < 3; ++function_index)
        for (mode = 0; mode < 4; ++mode)
            for (address = 0; address < 5; ++address) {
                if (function_index == 2 && !address) continue;
                output_address = 0x601ff0 + 4 * address;
                call_flags = (mode & 1 ? 0xe93 : 0x246) | (mode & 2 ? 0x200000 : 0);
                vector = address + mode;
                if (!check_case()) return 12;
            }
    PRINT("CPUID guard-page fault original/C/oracle cases: "); print_number(cases); PRINT("\n");
    return 0;
}
void _start(void) { finish((uint32_t)run_test()); }
