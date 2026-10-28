#include <stdint.h>

void kernel_handle_pointer_input(void);
static uint8_t expected_memory[0x5000], expected_table[0x1000];
static uint32_t trace[32], expected_trace[32], trace_length;
static uint32_t initial_record, initial_pointer, saved_flags;
static unsigned mode;

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

uint32_t kernel_read_flags(void)
{
    trace[trace_length++] = 11;
    return saved_flags;
}

void kernel_disable_interrupts(void) { trace[trace_length++] = 12; }
void kernel_write_flags(uint32_t flags)
{
    trace[trace_length++] = 13;
    trace[trace_length++] = flags;
}

static void jump(uint32_t address, uintptr_t target)
{
    *(volatile uint8_t *)(uintptr_t)address = 0xe9;
    *(volatile uint32_t *)(uintptr_t)(address + 1) = (uint32_t)target - address - 5;
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
    /* The flag helpers are adjacent and too short for full JMP instructions.
     * Two-byte jumps reach test-only trampolines in otherwise unused code.
     * This never modifies the disk image or bypasses pointer/queue logic. */
    *(volatile uint16_t *)0x81a242 = 0xbceb; /* short jump to 0x81a200 */
    *(volatile uint16_t *)0x81a24a = 0xc4eb; /* short jump to 0x81a210 */
    jump(0x81a200, (uintptr_t)kernel_disable_interrupts);
    jump(0x81a210, (uintptr_t)kernel_read_flags);
    jump(0x81a24d, (uintptr_t)kernel_write_flags);
    return 1;
}

uint32_t kernel_queue_available(const volatile uint32_t *queue)
{
    return ((uint32_t (*)(const volatile uint32_t *))0x812f72)(queue);
}

uint32_t kernel_queue_read(volatile uint32_t *queue)
{
    return ((uint32_t (*)(volatile uint32_t *))0x812f2c)(queue);
}

uint32_t kernel_decode_pointer(volatile uint32_t *pointer, uint32_t byte)
{
    return ((uint32_t (*)(volatile uint32_t *, uint32_t))0x812f96)(pointer, byte);
}

void kernel_dispatch_event(void) { ((void (*)(void))0x801372)(); }

static void event(void)
{
    volatile uint32_t *record = (volatile uint32_t *)(uintptr_t)initial_record;
    volatile uint32_t *pointer = (volatile uint32_t *)(uintptr_t)initial_pointer;
    trace[trace_length++] = record[6];
    trace[trace_length++] = pointer[3];
    trace[trace_length++] = pointer[4];
    trace[trace_length++] = pointer[5];
    trace[trace_length++] = *(volatile uint32_t *)0x1c108;
    if (mode & 1) {
        record[6] = 15;
        *(volatile uint32_t *)0x1b078 = 0x98765432;
    }
    if (mode & 2) {
        *(volatile uint32_t *)0x1b040 = 0x1e800;
        *(volatile uint32_t *)0x1b048 = 0x1d800;
        *(volatile uint32_t *)0x1b0a8 = 0x6b0800;
    }
}

static void initialize(unsigned pattern, unsigned stream, unsigned state, unsigned cursor)
{
    static const uint8_t streams[24][16] = {
        {0}, {1, 0xfa}, {2, 0, 0xfa}, {2, 0xfa, 8},
        {3, 0xfa, 8, 0}, {4, 0xfa, 8, 0, 0},
        {4, 0xfa, 9, 0, 0}, {4, 0xfa, 8, 1, 1},
        {4, 0xfa, 0x38, 0xff, 0xff}, {4, 0xfa, 0x3f, 0, 0},
        {4, 0xfa, 0x18, 0, 0x80}, {4, 0xfa, 0x28, 0x80, 0},
        {7, 0xfa, 9, 0, 0, 8, 0, 0},
        {7, 0xfa, 9, 1, 1, 8, 0, 0},
        {10, 0xfa, 9, 0, 0, 8, 0, 0, 9, 0, 0},
        {10, 0xfa, 8, 0xff, 0xff, 0x38, 0xff, 0xff, 8, 0, 0},
        {8, 0xfa, 0xff, 0xc8, 0, 8, 0, 0, 8},
        {6, 0, 0xff, 0xfa, 8, 0x80, 0x80},
        {1, 0}, {1, 0xff}, {2, 8, 0}, {3, 8, 0, 0},
        {6, 0x3f, 0, 0, 8, 0, 0},
        {9, 9, 0, 0, 8, 0, 0, 0x38, 0x80, 0x80}
    };
    static const uint32_t coordinates[10] = {
        0, 1, 639, 640, 641, 0xffffffff, 0x7fffffff, 0x80000000,
        0x8000007f, 0xffffff80
    };
    static const uint32_t ticks[10] = {0, 1, 99, 100, 101, 0xffffffff,
        0x7fffffff, 0x80000000, 0x80000001, 0xffffff80};
    static const uint32_t previous_buttons[10] = {0, 1, 7, 0xffffffff,
        0x80000000, 0x7fffffff, 0, 1, 0, 2};
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    volatile uint8_t *table_bytes = (volatile uint8_t *)0x6b0000;
    volatile uint32_t *record, *pointer, *queue = (volatile uint32_t *)0x1c100;
    unsigned i, read_index = (stream & 1) ? 29 : 0, length = streams[stream][0];
    for (i = 0; i < sizeof(expected_memory); ++i) memory[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < sizeof(expected_table); ++i) table_bytes[i] = (uint8_t)(i * 17 + pattern * 59);
    for (i = 0; i < 16; ++i) {
        *(volatile uint32_t *)(uintptr_t)(0x6b0000 + i * 4) = (uint32_t)(uintptr_t)event;
        *(volatile uint32_t *)(uintptr_t)(0x6b0800 + i * 4) = (uint32_t)(uintptr_t)event;
    }
    initial_record = 0x1e103 + pattern;
    initial_pointer = 0x1d103 + pattern;
    record = (volatile uint32_t *)(uintptr_t)initial_record;
    pointer = (volatile uint32_t *)(uintptr_t)initial_pointer;
    *(volatile uint32_t *)0x1b034 = 0x1c200;
    *(volatile uint32_t *)0x1b040 = initial_record;
    *(volatile uint32_t *)0x1b044 = 0x1c100;
    *(volatile uint32_t *)0x1b048 = initial_pointer;
    *(volatile uint32_t *)0x1b04c = 0x1c300;
    *(volatile uint32_t *)0x1b0a8 = 0x6b0000;
    *(volatile uint32_t *)0x1b078 = ticks[(state + 3) % 10];
    *(volatile uint32_t *)0x1b07c = state < 8 ? 100 : ticks[state];
    *(volatile uint32_t *)0x1b080 = previous_buttons[state];
    *(volatile uint32_t *)0x1b084 = ticks[state];
    *(volatile uint32_t *)0x1c20c = state < 8 ? 640 : 0xffffffff;
    *(volatile uint32_t *)0x1c210 = state < 9 ? 480 : 0x80000000;
    *(volatile uint32_t *)0x1c32c = cursor ? 0x80000000 : 0;
    queue[0] = read_index;
    queue[1] = (read_index + length) % 32;
    queue[2] = length;
    queue[3] = 32;
    for (i = 0; i < length; ++i)
        ((volatile uint8_t *)queue)[16 + (read_index + i) % 32] = streams[stream][i + 1];
    pointer[0] = 0x38 | 0xfe00;
    ((volatile uint8_t *)pointer)[3] = (uint8_t)((state + stream / 6) % 6);
    pointer[1] = 0x12345678;
    pointer[2] = 0x87654321;
    pointer[3] = coordinates[state];
    pointer[4] = coordinates[(state + 3) % 10];
    pointer[5] = previous_buttons[(state + 2) % 10];
    record[6] = (state + 4) % 11;
    saved_flags = 0x202 ^ state << 12 ^ pattern << 20;
    trace_length = 0;
    for (i = 0; i < 32; ++i) trace[i] = 0;
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
    for (i = 0; i < 32; ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static int run_test(void)
{
    unsigned pattern, stream, state, cursor;
    if (!map_memory(0x1b000, 0x5000, 3) || !map_memory(0x6b0000, 0x1000, 3) ||
        !map_memory(0x800000, 0x40000, 7)) return 1;
    for (pattern = 0; pattern < 3; ++pattern)
        for (stream = 0; stream < 24; ++stream)
            for (state = 0; state < 10; ++state)
                for (mode = 0; mode < 4; ++mode)
                    for (cursor = 0; cursor < 2; ++cursor) {
                        if (!load_kernel(0)) return 2;
                        initialize(pattern, stream, state, cursor);
                        ((void (*)(void))0x801454)();
                        compare(1);
                        initialize(pattern, stream, state, cursor);
#ifdef KERNEL_LINKED_TEST
                        if (!load_kernel(1)) return 2;
                        ((void (*)(void))0x801454)();
#else
                        kernel_handle_pointer_input();
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
