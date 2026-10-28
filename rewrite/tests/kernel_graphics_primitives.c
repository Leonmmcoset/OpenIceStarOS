/* Original/C color execution and instruction-level VGA polling with ptrace.
 * Only the privileged IN instruction is supplied a deterministic port value. */
#include "../kernel/graphics_primitives.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (unsigned)(uintptr_t)(s), sizeof(s) - 1)
#define USED __attribute__((used))

static USED uint32_t entry_point, call_flags;
static uint32_t colors[2], alpha, depth, cases;
static uint32_t wait_count, final_status, mode;
/* Include 32 again with full-width alpha inputs, and both signed extremes.
 * The exhaustive channel sweep below only populates alpha's lower 22 bits. */
static const uint32_t depths[] = {
    0, 1, 8, 15, 16, 24, 31, 32, 33, 64, 0x7fffffff, 0x80000000, 0xffffffff
};
static const uint32_t wait_counts[] = {0, 1, 7};

struct registers {
    uint32_t ebx, ecx, edx, esi, edi, ebp, eax;
    uint32_t ds, es, fs, gs, original_eax, eip, cs, flags, esp, ss;
};

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
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

static void print_number(uint32_t number)
{
    char digits[10];
    uint32_t size = 0;
    do { digits[9 - size++] = (char)('0' + number % 10); number /= 10; } while (number);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - size), size);
}

static uint32_t blend(int original)
{
#ifndef KERNEL_LINKED_TEST
    if (!original) return kernel_blend_color(colors[0], colors[1], alpha, depth);
#endif
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))
            ((original ? 0x900000 : 0x800000) + 0x28f1b))(colors[0], colors[1], alpha, depth);
}

static int color_case(void)
{
    uint32_t a = alpha & 255, foreground = colors[0], background = colors[1];
    uint32_t expected = blend(1), actual = blend(0), oracle;
    if (depth != 32) oracle = a ? foreground : background;
    else {
        uint32_t blue = ((foreground & 255) * a + (background & 255) * (256 - a)) / 256;
        uint32_t green = (((foreground >> 8) & 255) * a + ((background >> 8) & 255) * (256 - a)) / 256;
        uint32_t red = (((foreground >> 16) & 255) * a + ((background >> 16) & 255) * (256 - a)) / 256;
        oracle = blue + 256 * green + 65536 * red;
    }
    if (actual != expected || expected != oracle) {
        PRINT("color blend mismatch case/depth/alpha/expected/actual/oracle: ");
        print_number(cases); PRINT("/"); print_number(depth); PRINT("/"); print_number(alpha); PRINT("/");
        print_number(expected); PRINT("/"); print_number(actual); PRINT("/"); print_number(oracle); PRINT("\n");
        return 0;
    }
    ++cases;
    return 1;
}

extern const char after_vblank;
__attribute__((naked, noinline)) static void capture_vblank(void)
{
    __asm__ volatile (
        "movl $0x50fff0,%esp\n\tpushl call_flags\n\tpopfl\n\t"
        "movl $0x10293847,%eax\n\tmovl $0x21384957,%ecx\n\t"
        "movl $0x32495867,%edx\n\tmovl $0x435a6977,%ebx\n\t"
        "movl $0x546b7a87,%ebp\n\tmovl $0x657c8b97,%esi\n\t"
        "movl $0x768d9ca7,%edi\n\tcall *entry_point\n\t"
        ".global after_vblank\n\tafter_vblank:\tint3\n\t"
        "movl $1,%eax\n\txorl %ebx,%ebx\n\tint $0x80");
    __builtin_unreachable();
}

static int wait_child(int child, int *status)
{
    return syscall3(7, (unsigned)child, (unsigned)(uintptr_t)status, 0) == child;
}

static int poll_case(int original, struct registers *result)
{
    int child, status;
    uint32_t reads = 0;
    entry_point = original ? 0x928f0f : 0x828f0f;
#ifndef KERNEL_LINKED_TEST
    if (!original) entry_point = (uint32_t)(uintptr_t)kernel_wait_vertical_retrace;
#endif
    call_flags = mode ? 0xe93 : 0x246;
    child = syscall3(2, 0, 0, 0);
    if (child < 0) return 0;
    if (!child) {
        if (trace_call(0, 0, 0, 0) < 0) finish(71);
        syscall3(37, (unsigned)syscall3(20, 0, 0, 0), 19, 0);
        capture_vblank();
        finish(72);
    }
    if (!wait_child(child, &status) || status != 0x137f) goto failed;
    if (trace_call(7, (unsigned)child, 0, 0) < 0) goto failed;
    while (wait_child(child, &status)) {
        struct registers regs;
        uint32_t stop = (unsigned)status >> 8;
        if ((status & 255) != 127 ||
            trace_call(12, (unsigned)child, 0, (uint32_t)(uintptr_t)&regs) < 0) goto failed;
        if (stop == 11) {
            uint32_t sample;
            if (regs.eip != entry_point + 5 || BYTE(regs.eip) != 0xec ||
                (regs.edx & 65535) != 0x3da || reads > wait_count) goto failed;
            sample = reads < wait_count ? (reads * 37u + cases * 29u) & 0xf7u : final_status;
            regs.eax = (regs.eax & 0xffffff00u) | sample;
            ++regs.eip;
            ++reads;
            if (trace_call(13, (unsigned)child, 0, (uint32_t)(uintptr_t)&regs) < 0 ||
                trace_call(7, (unsigned)child, 0, 0) < 0) goto failed;
        } else if (stop == 5) {
            if (reads != wait_count + 1 || regs.eip != (uint32_t)(uintptr_t)&after_vblank + 1 ||
                regs.eax != (0x10293800u | final_status) || regs.edx != 0x32495867 ||
                regs.ecx != 0x21384957 || regs.ebx != 0x435a6977 || regs.ebp != 0x546b7a87 ||
                regs.esi != 0x657c8b97 || regs.edi != 0x768d9ca7 || regs.esp != 0x50fff0 ||
                (regs.flags & 0x400) != (call_flags & 0x400)) goto failed;
            *result = regs;
            if (trace_call(7, (unsigned)child, 0, 0) < 0 ||
                !wait_child(child, &status) || status != 0) goto failed;
            return 1;
        } else goto failed;
    }
failed:
    syscall3(37, (unsigned)child, 9, 0);
    (void)wait_child(child, &status);
    PRINT("VGA polling mismatch case/original/waits/final/reads/status: ");
    print_number(cases); PRINT("/"); print_number((uint32_t)original); PRINT("/");
    print_number(wait_count); PRINT("/"); print_number(final_status); PRINT("/");
    print_number(reads); PRINT("/"); print_number((uint32_t)status); PRINT("\n");
    return 0;
}

static int run_test(void)
{
    uint32_t i, first, second, a, seed = 0x12345678, limits[2] = {0, 0};
    syscall3(75, 4, (unsigned)(uintptr_t)limits, 0);
    if (!map_memory(0x500000, 0x10000, 3) || !map_memory(0x800000, 0x40000, 7) ||
        !map_memory(0x900000, 0x40000, 7) || !load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/openicestaros_kernel.x", 0x800000)) return 1;
    depth = 32;
    for (a = 0; a < 256; ++a)
        for (first = 0; first < 256; ++first)
            for (second = 0; second < 256; ++second) {
                /* Each channel independently covers every 256^3 input triple. */
                colors[0] = 0xab000000u | first | ((first ^ 0x55u) << 8) | (((first * 37u + a) & 255u) << 16);
                colors[1] = 0xcd000000u | second | ((second ^ 0xaau) << 8) | (((second * 29u + a) & 255u) << 16);
                alpha = a | ((first * 17u + second * 31u) << 8);
                if (!color_case()) return 10;
            }
    for (i = 0; i < COUNT(depths); ++i)
        for (a = 0; a < 1024; ++a) {
            seed = seed * 1664525u + 1013904223u; colors[0] = seed;
            seed = seed * 1664525u + 1013904223u; colors[1] = seed;
            depth = depths[i]; alpha = a | (seed & 0xfffffc00u);
            if (!color_case()) return 11;
        }
    PRINT("color blending original/C/oracle cases: "); print_number(cases); PRINT("\n");
    cases = 0;
    for (i = 0; i < COUNT(wait_counts); ++i)
        for (mode = 0; mode < 2; ++mode)
            for (final_status = 0; final_status < 256; ++final_status) {
                struct registers expected, actual;
                if (!(final_status & 8)) continue;
                wait_count = wait_counts[i];
                if (!poll_case(1, &expected) || !poll_case(0, &actual) ||
                    ((expected.flags ^ actual.flags) & 0xcc5u)) return 12;
                ++cases;
            }
    PRINT("VGA polling original/C cases: "); print_number(cases); PRINT("\n");
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
