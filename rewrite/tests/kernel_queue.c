#include <stdint.h>

uint32_t kernel_queue_write(volatile uint32_t *queue, uint32_t byte);
uint32_t kernel_queue_read(volatile uint32_t *queue);
uint32_t kernel_queue_available(const volatile uint32_t *queue);
uint32_t kernel_decode_pointer(volatile uint32_t *pointer, uint32_t byte);
static uint8_t expected[512], actual[512];

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}

static int load_kernel(uint32_t address, const char *path)
{
    uint32_t args[] = {address, 0x40000, 7, 0x32, 0xffffffff, 0};
    int fd, count;
    if (syscall3(90, (unsigned)(uintptr_t)args, 0, 0) != (int)address) return 0;
    fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0);
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, address, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return count == 250099;
}

static void initialize(unsigned pattern)
{
    unsigned i;
    for (i = 0; i < sizeof(expected); ++i)
        expected[i] = actual[i] = (uint8_t)(i * 37 + pattern * 83);
}

static int equal(uint32_t a, uint32_t b)
{
    unsigned i;
    if (a != b) return 0;
    for (i = 0; i < sizeof(expected); ++i) if (expected[i] != actual[i]) return 0;
    return 1;
}

static int test_queue(void)
{
    static const uint32_t capacities[] = {1, 2, 3, 8, 17, 32};
    static const uint32_t values[] = {0, 0xff, 0x12345678};
    unsigned pattern, c, read, write, count, operation;
    volatile uint32_t *q1, *q2;
    uint32_t a, b, capacity;
    for (pattern = 0; pattern < 3; ++pattern)
        for (c = 0; c < sizeof(capacities) / sizeof(capacities[0]); ++c) {
            capacity = capacities[c];
            for (read = 0; read < capacity; ++read)
                for (write = 0; write < capacity; ++write)
                    for (count = 0; count <= capacity + 1; ++count)
                        for (operation = 0; operation < 3; ++operation) {
                            initialize(pattern);
                            q1 = (volatile uint32_t *)(expected + 32 + pattern);
                            q2 = (volatile uint32_t *)(actual + 32 + pattern);
                            q1[0] = q2[0] = read;
                            q1[1] = q2[1] = write;
                            q1[2] = q2[2] = count;
                            q1[3] = q2[3] = capacity;
                            if (operation == 0) {
                                a = ((uint32_t (*)(volatile uint32_t *, uint32_t))0x912e7e)(q1, values[pattern]);
#ifdef KERNEL_LINKED_TEST
                                b = ((uint32_t (*)(volatile uint32_t *, uint32_t))0x812e7e)(q2, values[pattern]);
#else
                                b = kernel_queue_write(q2, values[pattern]);
#endif
                            } else if (operation == 1) {
                                a = ((uint32_t (*)(volatile uint32_t *))0x912f2c)(q1);
#ifdef KERNEL_LINKED_TEST
                                b = ((uint32_t (*)(volatile uint32_t *))0x812f2c)(q2);
#else
                                b = kernel_queue_read(q2);
#endif
                            } else {
                                a = ((uint32_t (*)(const volatile uint32_t *))0x912f72)(q1);
#ifdef KERNEL_LINKED_TEST
                                b = ((uint32_t (*)(const volatile uint32_t *))0x812f72)(q2);
#else
                                b = kernel_queue_available(q2);
#endif
                            }
                            if (!equal(a, b)) return 0;
                        }
        }
    return 1;
}

static int test_decoder(void)
{
    unsigned pattern, phase, header, value;
    volatile uint32_t *p1, *p2;
    uint32_t a, b;
    for (pattern = 0; pattern < 3; ++pattern)
        for (phase = 0; phase < 6; ++phase)
            for (header = 0; header < 256; ++header)
                for (value = 0; value < 256; ++value) {
                    initialize(pattern);
                    p1 = (volatile uint32_t *)(expected + 32 + pattern);
                    p2 = (volatile uint32_t *)(actual + 32 + pattern);
                    ((volatile uint8_t *)p1)[0] = ((volatile uint8_t *)p2)[0] = (uint8_t)header;
                    ((volatile uint8_t *)p1)[1] = ((volatile uint8_t *)p2)[1] = (uint8_t)(value * 83 + header);
                    ((volatile uint8_t *)p1)[3] = ((volatile uint8_t *)p2)[3] = (uint8_t)phase;
                    a = ((uint32_t (*)(volatile uint32_t *, uint32_t))0x912f96)(p1, 0xaabb0000 | value);
#ifdef KERNEL_LINKED_TEST
                    b = ((uint32_t (*)(volatile uint32_t *, uint32_t))0x812f96)(p2, 0xaabb0000 | value);
#else
                    b = kernel_decode_pointer(p2, 0xaabb0000 | value);
#endif
                    if (!equal(a, b)) return 0;
                }
    return 1;
}

static int run_test(void)
{
    /* These original leaf routines have no absolute code/data references.
     * Relocating the oracle allows the replacement to run at its real address
     * without reloading a 250 KB kernel for each of the million test cases. */
    if (!load_kernel(0x900000, "build/icestar.x")) return 1;
#ifdef KERNEL_LINKED_TEST
    if (!load_kernel(0x800000, "build/icestar_c.x")) return 1;
#endif
    if (!test_queue()) return 2;
    if (!test_decoder()) return 3;
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
