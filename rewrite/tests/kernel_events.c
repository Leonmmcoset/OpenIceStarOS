#include <stdint.h>

void kernel_queue_api_request(uint32_t index, uint32_t argument);
void kernel_dispatch_event(void);
void kernel_dispatch_api_request(void);
void kernel_dispatch_slot1(void);
void kernel_dispatch_slot2(void);
void kernel_dispatch_slot3(void);

static uint8_t expected_memory[0x5000], expected_table[0x1000];
static uint32_t trace[8], expected_trace[8];
static uint32_t callback_mode, record_address;
static const uint32_t indices[] = {0, 1, 2, 3, 7, 13};
static const uint32_t arguments[] = {0, 1, 0x80000000, 0xffffffff};

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
    const char *path = rewritten ? "build/icestar_c.x" : "build/icestar.x";
    int fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0);
    int count;
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, 0x800000, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return count == 250099;
}

static void observe(uint32_t tag, uint32_t argument)
{
    volatile uint32_t *record = (volatile uint32_t *)(uintptr_t)record_address;
    ++trace[0];
    trace[1] = tag;
    trace[2] = argument;
    trace[3] = record[4];
    trace[4] = record[5];
    trace[5] = record[6];
    trace[6] = record[7];
    if (callback_mode & 1) {
        record[4] = 0x11223344;
        record[5] = 0x55667788;
        record[6] = 0x99aabbcc;
        record[7] = 0xddeeff00;
    }
    if (callback_mode & 2) {
        *(volatile uint32_t *)0x1b040 = 0x1e800;
        *(volatile uint32_t *)0x1b0a8 = 0x6b0800;
    }
}

static void callback0(void) { observe(0, 0); }
static void callback1(void) { observe(1, 0); }
static void callback2(void) { observe(2, 0); }
static void callback3(void) { observe(3, 0); }
static void argument0(uint32_t argument) { observe(0, argument); }
static void argument1(uint32_t argument) { observe(1, argument); }
static void argument2(uint32_t argument) { observe(2, argument); }
static void argument3(uint32_t argument) { observe(3, argument); }

static void initialize(unsigned pattern, uint32_t index, uint32_t argument, int with_argument)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    volatile uint8_t *table_bytes = (volatile uint8_t *)0x6b0000;
    volatile uint32_t *table = (volatile uint32_t *)0x6b0000;
    volatile uint32_t *record;
    const uintptr_t callbacks[] = {(uintptr_t)callback0, (uintptr_t)callback1,
                                  (uintptr_t)callback2, (uintptr_t)callback3};
    const uintptr_t argument_callbacks[] = {(uintptr_t)argument0, (uintptr_t)argument1,
                                           (uintptr_t)argument2, (uintptr_t)argument3};
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i)
        memory[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < sizeof(expected_table); ++i) table_bytes[i] = (uint8_t)(i * 17 + pattern * 59);
    for (i = 0; i < 16; ++i)
        table[i] = (uint32_t)(with_argument ? argument_callbacks[i % 4] : callbacks[i % 4]);
    record_address = 0x1e103 + pattern;
    record = (volatile uint32_t *)(uintptr_t)record_address;
    *(volatile uint32_t *)0x1b040 = record_address;
    *(volatile uint32_t *)0x1b0a8 = 0x6b0000;
    record[4] = 0x12345678;
    record[5] = index;
    record[6] = index;
    record[7] = argument;
    for (i = 0; i < 8; ++i) trace[i] = 0;
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
    for (i = 0; i < 8; ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static int exercise(uint32_t address, void (*function)(void), unsigned pattern,
                    uint32_t index, uint32_t argument, int with_argument)
{
    if (!load_kernel(0)) return 0;
    initialize(pattern, index, argument, with_argument);
    ((void (*)(void))(uintptr_t)address)();
    compare(1);
    initialize(pattern, index, argument, with_argument);
#ifdef KERNEL_LINKED_TEST
    (void)function;
    if (!load_kernel(1)) return 0;
    ((void (*)(void))(uintptr_t)address)();
#else
    function();
#endif
    return compare(0);
}

static int run_test(void)
{
    unsigned pattern, index, arg, mode, slot;
    const uint32_t addresses[] = {0x8013f4, 0x801414, 0x801434};
    void (*const functions[])(void) = {kernel_dispatch_slot1, kernel_dispatch_slot2,
                                      kernel_dispatch_slot3};
    if (!map_memory(0x1b000, 0x5000, 3) || !map_memory(0x6b0000, 0x1000, 3) ||
        !map_memory(0x800000, 0x40000, 7)) return 1;
    for (pattern = 0; pattern < 3; ++pattern) {
        for (index = 0; index < sizeof(indices) / sizeof(indices[0]); ++index)
            for (arg = 0; arg < sizeof(arguments) / sizeof(arguments[0]); ++arg) {
                if (!load_kernel(0)) return 2;
                initialize(pattern, indices[index], arguments[arg], 0);
                ((void (*)(uint32_t, uint32_t))0x80129e)(indices[index], arguments[arg]);
                compare(1);
                initialize(pattern, indices[index], arguments[arg], 0);
#ifdef KERNEL_LINKED_TEST
                if (!load_kernel(1)) return 2;
                ((void (*)(uint32_t, uint32_t))0x80129e)(indices[index], arguments[arg]);
#else
                kernel_queue_api_request(indices[index], arguments[arg]);
#endif
                if (!compare(0)) return 3;
                for (mode = 0; mode < 4; ++mode) {
                    callback_mode = mode;
                    if (!exercise(0x801372, kernel_dispatch_event, pattern,
                                  indices[index], arguments[arg], 0)) return 4;
                    if (!exercise(0x8013aa, kernel_dispatch_api_request, pattern,
                                  indices[index], arguments[arg], 1)) return 5;
                    for (slot = 0; slot < 3; ++slot)
                        if (!exercise(addresses[slot], functions[slot], pattern,
                                      indices[index], arguments[arg], 0)) return 6;
                }
            }
    }
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
