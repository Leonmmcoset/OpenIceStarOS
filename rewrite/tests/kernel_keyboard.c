#include <stdint.h>
#include <stdarg.h>

uint32_t kernel_translate_scancode(uint32_t scancode);
uint32_t kernel_update_keyboard_state(uint32_t scancode);
void kernel_drain_keyboard(void);
uint32_t kernel_read_draw_character(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

static uint8_t expected_memory[0x5000];
static uint32_t trace[2048], expected_trace[2048], trace_length, expected_length;
static uint32_t expected_result, case_number;
static unsigned mutation;

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

static void observe(uint32_t tag, uint32_t address, uint32_t value)
{
    volatile uint32_t *state = (volatile uint32_t *)0x1b000;
    uint32_t hash = 0;
    unsigned i;
    for (i = 0; i < 16; ++i) hash = hash * 31 + state[i];
    trace[trace_length++] = tag;
    trace[trace_length++] = address;
    trace[trace_length++] = value;
    trace[trace_length++] = hash;
    if ((mutation & 1) && trace_length == 4) {
        state[0x18 / 4] ^= 0x80000001;
        state[0x0c / 4] ^= 1;
        state[0x14 / 4] ^= 1;
        state[0x24 / 4] = 0x1c203;
        state[0x28 / 4] = 0x1c303;
        state[0x38 / 4] = 0x1e203;
    }
    if ((mutation & 2) && tag == 1) {
        state[0x14 / 4] ^= 1;
        state[0x24 / 4] = 0x1c203;
        state[0x28 / 4] = 0x1c303;
        state[0x38 / 4] = 0x1e203;
    }
}

uint32_t kernel_physical_read_byte(uint32_t address)
{
    uint32_t value = *(volatile uint8_t *)(uintptr_t)address;
    observe(1, address, value);
    return value;
}

uint32_t kernel_physical_read_dword(uint32_t address)
{
    uint32_t value = *(volatile uint32_t *)(uintptr_t)address;
    observe(2, address, value);
    return value;
}

void kernel_physical_write_dword(uint32_t address, uint32_t value)
{
    observe(3, address, value);
    *(volatile uint32_t *)(uintptr_t)address = value;
}

void kernel_set_keyboard_leds(uint32_t value)
{
    observe(4, 0x1a812, value);
    *(volatile uint8_t *)0x1a812 = (uint8_t)value;
}

void kernel_format_text(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                        uint32_t e, const char *format, ...)
{
    va_list args;
    uint32_t character;
    va_start(args, format);
    character = va_arg(args, uint32_t);
    va_end(args);
    observe(5, (uint32_t)(uintptr_t)format, character);
    trace[trace_length++] = a;
    trace[trace_length++] = b;
    trace[trace_length++] = c;
    trace[trace_length++] = d;
    trace[trace_length++] = e;
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
    jump(address + 0x373a8, (uintptr_t)kernel_physical_read_byte);
    jump(address + 0x373d7, (uintptr_t)kernel_physical_read_dword);
    jump(address + 0x3738f, (uintptr_t)kernel_physical_write_dword);
    jump(address + 0x19c7c, (uintptr_t)kernel_set_keyboard_leds);
    jump(address + 0x1f88a, (uintptr_t)kernel_format_text);
    return 1;
}

static void initialize(unsigned pattern, unsigned flags, uint32_t led)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1a000;
    volatile uint32_t *state = (volatile uint32_t *)0x1b000;
    volatile uint32_t *queue = (volatile uint32_t *)(uintptr_t)(0x1d003 + pattern);
    unsigned i, table;
    for (i = 0; i < sizeof(expected_memory); ++i) memory[i] = (uint8_t)(i * 37 + pattern * 83);
    state[0x0c / 4] = (flags & 1) ? 0x80000001 : 0;
    state[0x10 / 4] = (flags & 2) ? 2 : 0;
    state[0x14 / 4] = (flags & 4) ? 0xffffffff : 0;
    state[0x18 / 4] = (flags & 8) ? 0x80000000 : 0;
    state[0x2c / 4] = (flags & 16) ? 0x80000000 : 0;
    state[0x1c / 4] = (flags & 32) ? 0xffffffff : 0;
    state[0x24 / 4] = 0x1c003 + pattern;
    state[0x28 / 4] = 0x1c103 + pattern;
    state[0x38 / 4] = (uint32_t)(uintptr_t)queue;
    for (table = 0; table < 4; ++table) {
        volatile uint8_t *bytes = (volatile uint8_t *)(uintptr_t)
            (0x1c003 + 256 * table + (table < 2 ? pattern : 0));
        for (i = 0; i < 66; ++i) {
            bytes[2 * i] = (uint8_t)(i * 37 + pattern * 83 + table * 11);
            bytes[2 * i + 1] = (uint8_t)(i * 73 + pattern * 91 + table * 33);
        }
        /* Test first-match semantics, NUL mappings, and both search limits. */
        bytes[40] = bytes[2];
        bytes[5] = 0;
        bytes[103] = 'z';
        bytes[131] = 'a';
    }
    *(volatile uint8_t *)0x1a812 = (uint8_t)led;
    queue[0] = pattern;
    queue[1] = 31;
    queue[2] = 1;
    queue[3] = 32;
    trace_length = 0;
}

static int compare(int save, uint32_t result)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1a000;
    unsigned i;
    if (trace_length > 2048) return 0;
    if (save) { expected_result = result; expected_length = trace_length; }
    else if (result != expected_result || trace_length != expected_length) return 0;
    for (i = 0; i < sizeof(expected_memory); ++i) {
        if (save) expected_memory[i] = memory[i];
        else if (expected_memory[i] != memory[i]) return 0;
    }
    for (i = 0; i < trace_length; ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static uint32_t call(unsigned kind, int original, uint32_t scan)
{
    if (original) {
        if (kind == 0) return ((uint32_t (*)(uint32_t))0x919e90)(scan);
        if (kind == 1) return ((uint32_t (*)(uint32_t))0x919fde)(scan);
        return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))0x91a1aa)
            (0, 0x7fffffff, 0x80000000, 0xffffffff, 0x12345678);
    }
#ifdef KERNEL_LINKED_TEST
    if (kind == 0) return ((uint32_t (*)(uint32_t))0x819e90)(scan);
    if (kind == 1) return ((uint32_t (*)(uint32_t))0x819fde)(scan);
    return ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))0x81a1aa)
        (0, 0x7fffffff, 0x80000000, 0xffffffff, 0x12345678);
#else
    if (kind == 0) return kernel_translate_scancode(scan);
    if (kind == 1) return kernel_update_keyboard_state(scan);
    return kernel_read_draw_character(0, 0x7fffffff, 0x80000000, 0xffffffff, 0x12345678);
#endif
}

static int test_case(unsigned kind, unsigned pattern, unsigned flags,
                     uint32_t scancode, uint32_t led)
{
    uint32_t result;
    unsigned pass;
    ++case_number;
    for (pass = 0; pass < 2; ++pass) {
        initialize(pattern, flags, led);
        if (kind == 2)
            *(volatile uint8_t *)(uintptr_t)(0x1d003 + 16 + 2 * pattern) = (uint8_t)scancode;
        result = call(kind, pass == 0, scancode);
        if (!compare(pass == 0, result)) return 0;
    }
    return 1;
}

static uint32_t scan_value(unsigned i)
{
    static const uint32_t extra[] = {0x100, 0x7fffffff, 0x80000000,
        0xfffffffe, 0xffffffff, 0x100e0, 0x1002a};
    return i < 256 ? i : extra[i - 256];
}

static int test_drain(void)
{
    static const unsigned capacities[] = {1, 2, 3, 8, 17, 32};
    static const uint32_t modes[] = {0, 6, 0xffffffff};
    unsigned pattern, c, read, count, mode, pass;
    mutation = 0;
    for (pattern = 0; pattern < 3; ++pattern)
        for (c = 0; c < 6; ++c)
            for (read = 0; read < capacities[c]; ++read)
                for (count = 0; count <= capacities[c]; ++count)
                    for (mode = 0; mode < 3; ++mode) {
                        ++case_number;
                        for (pass = 0; pass < 2; ++pass) {
                            volatile uint32_t *q = (volatile uint32_t *)(uintptr_t)(0x1d003 + pattern);
                            initialize(pattern, 0, 0);
                            q[0] = read;
                            q[2] = count;
                            q[3] = capacities[c];
                            *(volatile uint32_t *)0x1b0a0 = modes[mode];
                            if (!pass) ((void (*)(void))0x919f9c)();
#ifdef KERNEL_LINKED_TEST
                            else ((void (*)(void))0x819f9c)();
#else
                            else kernel_drain_keyboard();
#endif
                            if (!compare(pass == 0, 0)) return 0;
                        }
                    }
    return 1;
}

static int run_test(void)
{
    unsigned pattern, flags, scan, led, kind;
    static const uint32_t special[] = {0x2a, 0xaa, 0x36, 0xb6, 0x45, 0x3a,
        0x46, 0xe0, 0xe1, 0, 0x13, 0xffffffff};
    if (!map_memory(0x1a000, 0x5000, 3) ||
        !load_kernel(0x900000, "build/icestar.x") ||
        !load_kernel(0x800000, "build/openicestaros_kernel.x")) return 1;
    for (kind = 0; kind < 2; ++kind)
        for (pattern = 0; pattern < 3; ++pattern)
            for (flags = 0; flags < (kind ? 64u : 32u); ++flags)
                for (scan = 0; scan < 263; ++scan)
                    if (!test_case(kind, pattern, flags, scan_value(scan), scan + flags + pattern)) return 2 + kind;
    for (led = 0; led < 256; ++led)
        for (flags = 0; flags < 8; ++flags)
            for (scan = 4; scan < 7; ++scan)
                if (!test_case(1, led % 3, (flags & 1) * 8 + (flags & 2) * 2 + (flags & 4) * 8,
                               special[scan], led)) return 3;
    for (flags = 0; flags < 32; ++flags)
        for (scan = 0; scan < 256; ++scan)
            if (!test_case(2, scan % 3, flags, scan, scan ^ flags)) return 4;
    for (mutation = 1; mutation < 4; ++mutation)
        for (kind = 0; kind < 3; ++kind)
            for (pattern = 0; pattern < 3; ++pattern)
                for (flags = 0; flags < 32; ++flags)
                    for (scan = 0; scan < 12; ++scan)
                        if (!test_case(kind, pattern, flags, special[scan], scan * 17)) return 5;
    if (!test_drain()) return 6;
    return 0;
}

void _start(void)
{
    int result = run_test();
    char message[] = "keyboard case 00000000 result 0\n";
    unsigned i;
    if (result) {
        for (i = 0; i < 8; ++i) message[14 + i] = "0123456789abcdef"[(case_number >> (28 - 4 * i)) & 15];
        message[29] = (char)('0' + result);
        syscall3(4, 2, (unsigned)(uintptr_t)message, sizeof(message) - 1);
    }
    syscall3(1, (unsigned)result, 0, 0);
    __builtin_unreachable();
}
