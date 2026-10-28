#include <stdint.h>

void kernel_poll_events(void);
void kernel_handle_pointer_input(void);
static uint8_t expected_memory[0x5000], expected_table[0x1000];
static uint32_t trace[64], expected_trace[64], trace_length;
static unsigned mode;
static uint32_t initial_record;

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

static int load_kernel(int rewritten)
{
    const char *path = rewritten ? "build/openicestaros_kernel.x" : "build/icestar.x";
    int fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0);
    int count;
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, 0x800000, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    if (count != 250099) return 0;
    /* Isolate the unrecovered pointer handler, which performs privileged I/O.
     * Both the original and replacement poll call this same test callback. */
    *(volatile uint8_t *)0x801454 = 0xe9;
    *(volatile uint32_t *)0x801455 = (uint32_t)(uintptr_t)kernel_handle_pointer_input - 0x801459;
    return 1;
}

static void observe(uint32_t tag, uint32_t argument)
{
    volatile uint32_t *record = (volatile uint32_t *)(uintptr_t)initial_record;
    trace[trace_length++] = tag;
    trace[trace_length++] = argument;
    trace[trace_length++] = record[0] | record[1] << 4 | record[4] << 8;
    trace[trace_length++] = record[6];
    if (mode & 1) {
        record[1] = 1;
        record[4] = 1;
        *(volatile uint32_t *)0x1c108 = 1;
        *(volatile uint32_t *)0x1c208 = 1;
    }
    if (mode & 2) {
        *(volatile uint32_t *)0x1b040 = 0x1e800;
        *(volatile uint32_t *)0x1b0a8 = 0x6b0800;
    }
}

static void slot1(void) { observe(1, 0); }
static void slot2(void) { observe(2, 0); }
static void slot3(void) { observe(3, 0); }
static void request(uint32_t argument) { observe(7, argument); }
void kernel_handle_pointer_input(void) { observe(4, 0); }

uint32_t kernel_input_available(const volatile uint32_t *queue)
{
    return ((uint32_t (*)(const volatile uint32_t *))0x819e44)(queue);
}

uint32_t kernel_queue_available(const volatile uint32_t *queue)
{
    return ((uint32_t (*)(const volatile uint32_t *))0x812f72)(queue);
}

static void initialize(unsigned pattern, unsigned flags)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    volatile uint8_t *table_bytes = (volatile uint8_t *)0x6b0000;
    volatile uint32_t *record, *alternate = (volatile uint32_t *)0x1e800;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) memory[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < sizeof(expected_table); ++i) table_bytes[i] = (uint8_t)(i * 17 + pattern * 59);
    for (i = 0; i < 2; ++i) {
        volatile uint32_t *table = (volatile uint32_t *)(uintptr_t)(0x6b0000 + i * 0x800);
        table[1] = (uint32_t)(uintptr_t)slot1;
        table[2] = (uint32_t)(uintptr_t)slot2;
        table[3] = (uint32_t)(uintptr_t)slot3;
        table[7] = (uint32_t)(uintptr_t)request;
    }
    initial_record = 0x1e103 + pattern;
    record = (volatile uint32_t *)(uintptr_t)initial_record;
    *(volatile uint32_t *)0x1b040 = initial_record;
    *(volatile uint32_t *)0x1b0a8 = 0x6b0000;
    *(volatile uint32_t *)0x1b038 = 0x1c100;
    *(volatile uint32_t *)0x1b044 = 0x1c200;
    record[0] = flags & 1;
    record[1] = (flags >> 1) & 1;
    record[2] = 0xaabbccdd;
    record[3] = 0x11223344;
    record[4] = (flags >> 4) & 1;
    record[5] = 7;
    record[6] = 0;
    record[7] = 0xfedcba98;
    for (i = 0; i < 8; ++i) alternate[i] = record[i];
    alternate[7] = 0x12345678;
    *(volatile uint32_t *)0x1c108 = (flags >> 2) & 1;
    *(volatile uint32_t *)0x1c208 = (flags >> 3) & 1;
    trace_length = 0;
    for (i = 0; i < 64; ++i) trace[i] = 0;
}

static int compare(int save)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    volatile uint8_t *table = (volatile uint8_t *)0x6b0000;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) {
        if (save) expected_memory[i] = memory[i];
        else if (expected_memory[i] != memory[i]) return 0;
    }
    for (i = 0; i < sizeof(expected_table); ++i) {
        if (save) expected_table[i] = table[i];
        else if (expected_table[i] != table[i]) return 0;
    }
    for (i = 0; i < 64; ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static int run_test(void)
{
    unsigned pattern, flags;
    if (!map_memory(0x1b000, 0x5000, 3) || !map_memory(0x6b0000, 0x1000, 3) ||
        !map_memory(0x800000, 0x40000, 7)) return 1;
    for (pattern = 0; pattern < 3; ++pattern)
        for (flags = 0; flags < 32; ++flags)
            for (mode = 0; mode < 4; ++mode) {
                if (!load_kernel(0)) return 2;
                initialize(pattern, flags);
                ((void (*)(void))0x8012dc)();
                compare(1);
                initialize(pattern, flags);
#ifdef KERNEL_LINKED_TEST
                if (!load_kernel(1)) return 2;
                ((void (*)(void))0x8012dc)();
#else
                kernel_poll_events();
#endif
                if (!compare(0)) return 3;
            }
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
