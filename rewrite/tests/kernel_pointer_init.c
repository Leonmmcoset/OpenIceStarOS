#include <stdint.h>

void kernel_init_pointer(void);
void kernel_wait_ps2_ready(void);
static uint8_t expected_memory[0x5000];
static uint32_t trace[512], expected_trace[512], trace_length, reads, writes;
static unsigned delay, mutation;
static uint32_t status;

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

static void observe(uint32_t tag, uint32_t port, uint32_t value)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    uint32_t hash = 0;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) hash = hash * 31 + memory[i];
    trace[trace_length++] = tag;
    trace[trace_length++] = port;
    trace[trace_length++] = value;
    trace[trace_length++] = hash;
    if (mutation) {
        *(volatile uint32_t *)0x1b034 = 0x1e100;
        *(volatile uint32_t *)0x1b044 = 0x1e200;
        *(volatile uint32_t *)0x1b048 = 0x1e300;
        *(volatile uint32_t *)0x1b04c = 0x1e400;
    }
}

uint32_t kernel_read_port_byte(uint32_t port)
{
    uint32_t value = status & ~2u;
    if (reads++ % (delay + 1) != delay) value |= 2;
    observe(1, port, value);
    return value;
}

void kernel_write_port_byte(uint32_t port, uint32_t byte)
{
    ++writes;
    observe(2, port, byte);
}

static void jump(uint32_t address, uintptr_t target)
{
    *(volatile uint8_t *)(uintptr_t)address = 0xe9;
    *(volatile uint32_t *)(uintptr_t)(address + 1) = (uint32_t)target - address - 5;
}

static int load_kernel(uint32_t address, const char *path)
{
    int fd, count;
    if (!map_memory(address, 0x40000, 7)) return 0;
    fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0);
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, address, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    if (count != 250099) return 0;
    jump(address + 0x1a254, (uintptr_t)kernel_read_port_byte);
    jump(address + 0x1a261, (uintptr_t)kernel_write_port_byte);
    return 1;
}

static void initialize(unsigned pattern, uint32_t width, uint32_t height)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) memory[i] = (uint8_t)(i * 37 + pattern * 83);
    *(volatile uint32_t *)0x1b034 = 0x1c103 + pattern;
    *(volatile uint32_t *)0x1b044 = 0x1c203 + pattern;
    *(volatile uint32_t *)0x1b048 = 0x1d103 + pattern;
    *(volatile uint32_t *)0x1b04c = 0x1d203 + pattern;
    *(volatile uint32_t *)(uintptr_t)(0x1c10f + pattern) = width;
    *(volatile uint32_t *)(uintptr_t)(0x1c113 + pattern) = height;
    reads = writes = trace_length = 0;
    for (i = 0; i < 512; ++i) trace[i] = 0;
}

static int compare(int save)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) {
        if (save) expected_memory[i] = memory[i];
        else if (expected_memory[i] != memory[i]) return 0;
    }
    for (i = 0; i < 512; ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static int run_test(void)
{
    static const uint32_t dimensions[] = {0, 1, 2, 23, 24, 25, 639, 640,
        0x7ffffffe, 0x7fffffff, 0x80000000, 0x80000001, 0xfffffffd, 0xffffffff};
    unsigned pattern, w, h, byte;
    if (!map_memory(0x1b000, 0x5000, 3) ||
        !load_kernel(0x900000, "build/icestar.x")) return 1;
#ifdef KERNEL_LINKED_TEST
    if (!load_kernel(0x800000, "build/icestar_c.x")) return 1;
#endif
    for (pattern = 0; pattern < 3; ++pattern)
        for (w = 0; w < 14; ++w)
            for (h = 0; h < 14; ++h)
                for (delay = 0; delay < 4; ++delay)
                    for (mutation = 0; mutation < 2; ++mutation) {
                        status = 0xaabb00fc | pattern;
                        initialize(pattern, dimensions[w], dimensions[h]);
                        ((void (*)(void))0x912c70)();
                        if (writes != 5 || reads != 5 * (delay + 1)) return 2;
                        compare(1);
                        initialize(pattern, dimensions[w], dimensions[h]);
#ifdef KERNEL_LINKED_TEST
                        ((void (*)(void))0x812c70)();
#else
                        kernel_init_pointer();
#endif
                        if (!compare(0) || writes != 5 || reads != 5 * (delay + 1)) return 3;
                    }
    for (byte = 0; byte < 256; ++byte)
        for (delay = 0; delay < 4; ++delay) {
            status = 0xaabb0000 | byte;
            mutation = 0;
            initialize(0, 640, 480);
            ((void (*)(void))0x912e5e)();
            compare(1);
            initialize(0, 640, 480);
#ifdef KERNEL_LINKED_TEST
            ((void (*)(void))0x812e5e)();
#else
            kernel_wait_ps2_ready();
#endif
            if (!compare(0) || reads != delay + 1 || writes != 0) return 4;
        }
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
