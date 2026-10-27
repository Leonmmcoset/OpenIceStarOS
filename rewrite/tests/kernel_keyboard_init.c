#include <stdint.h>

void kernel_init_keyboard(void);
void kernel_set_keyboard_leds(uint32_t value);
void kernel_wait_keyboard_ready(void);
void kernel_discard_keyboard_byte(void);

static uint8_t expected_memory[0x5000];
static uint32_t trace[512], expected_trace[512];
static uint32_t trace_length, expected_length, reads, discards, writes, status;
static unsigned delay, mutation;

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
    volatile uint8_t *memory = (volatile uint8_t *)0x1a000;
    uint32_t hash = 0;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) hash = hash * 31 + memory[i];
    trace[trace_length++] = tag;
    trace[trace_length++] = address;
    trace[trace_length++] = value;
    trace[trace_length++] = hash;
    if ((mutation & 1) && tag <= 2)
        *(volatile uint32_t *)0x1b038 = 0x1e103;
    if ((mutation & 2) && tag >= 3)
        *(volatile uint32_t *)0x1b038 = 0x1e203;
}

uint32_t kernel_read_port_byte(uint32_t port)
{
    uint32_t value = status;
    if (port == 0x64) {
        value &= ~2u;
        if (reads++ % (delay + 1) != delay) value |= 2;
    } else {
        ++discards;
    }
    observe(1, port, value);
    return value;
}

void kernel_write_port_byte(uint32_t port, uint32_t value)
{
    ++writes;
    observe(2, port, value);
}

uint32_t kernel_physical_read_byte(uint32_t address)
{
    uint32_t value = *(volatile uint8_t *)(uintptr_t)address;
    observe(3, address, value);
    return value;
}

void kernel_physical_write_byte(uint32_t address, uint32_t value)
{
    observe(4, address, value);
    *(volatile uint8_t *)(uintptr_t)address = (uint8_t)value;
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
    jump(address + 0x373a8, (uintptr_t)kernel_physical_read_byte);
    jump(address + 0x3735b, (uintptr_t)kernel_physical_write_byte);
    return 1;
}

static void initialize(unsigned pattern)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1a000;
    unsigned i;
    for (i = 0; i < sizeof(expected_memory); ++i) memory[i] = (uint8_t)(i * 37 + pattern * 83);
    *(volatile uint32_t *)0x1b038 = 0x1c203 + pattern;
    reads = discards = writes = trace_length = 0;
    for (i = 0; i < 512; ++i) trace[i] = 0;
}

static int compare(int save)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1a000;
    unsigned i;
    if (save) expected_length = trace_length;
    else if (expected_length != trace_length) return 0;
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

static int run_test(void)
{
    unsigned byte, pattern;
    if (!map_memory(0x1a000, 0x5000, 3) ||
        !load_kernel(0x900000, "build/icestar.x")) return 1;
#ifdef KERNEL_LINKED_TEST
    if (!load_kernel(0x800000, "build/icestar_c.x")) return 1;
#endif
    for (pattern = 0; pattern < 3; ++pattern)
        for (delay = 0; delay < 4; ++delay)
            for (mutation = 0; mutation < 4; ++mutation) {
                status = 0xaabb00fc | pattern;
                initialize(pattern);
                ((void (*)(void))0x919cf8)();
                if (reads != 6 * (delay + 1) || discards != 3 || writes != 6) return 2;
                compare(1);
                initialize(pattern);
#ifdef KERNEL_LINKED_TEST
                ((void (*)(void))0x819cf8)();
#else
                kernel_init_keyboard();
#endif
                if (!compare(0) || reads != 6 * (delay + 1) || discards != 3 || writes != 6) return 3;
            }
    for (byte = 0; byte < 256; ++byte)
        for (delay = 0; delay < 4; ++delay)
            for (mutation = 0; mutation < 4; ++mutation) {
                uint32_t value = 0xaabb0000 | byte;
                status = 0xccdd0000 | byte;
                initialize(byte % 3);
                ((void (*)(uint32_t))0x919c7c)(value);
                if (reads != 3 * (delay + 1) || discards != 3 || writes != 3) return 4;
                compare(1);
                initialize(byte % 3);
#ifdef KERNEL_LINKED_TEST
                ((void (*)(uint32_t))0x819c7c)(value);
#else
                kernel_set_keyboard_leds(value);
#endif
                if (!compare(0) || reads != 3 * (delay + 1) || discards != 3 || writes != 3) return 5;
            }
    mutation = 0;
    for (byte = 0; byte < 256; ++byte)
        for (delay = 0; delay < 4; ++delay) {
            status = 0xaabb0000 | byte;
            initialize(byte % 3);
            ((void (*)(void))0x919c40)();
            compare(1);
            initialize(byte % 3);
#ifdef KERNEL_LINKED_TEST
            ((void (*)(void))0x819c40)();
#else
            kernel_wait_keyboard_ready();
#endif
            if (!compare(0) || reads != delay + 1 || writes || discards) return 6;
        }
    for (byte = 0; byte < 256; ++byte) {
        status = 0xaabb0000 | byte;
        initialize(byte % 3);
        ((void (*)(void))0x919c60)();
        compare(1);
        initialize(byte % 3);
#ifdef KERNEL_LINKED_TEST
        ((void (*)(void))0x819c60)();
#else
        kernel_discard_keyboard_byte();
#endif
        if (!compare(0) || reads || writes || discards != 1) return 7;
    }
    return 0;
}

void _start(void)
{
    syscall3(1, (unsigned)run_test(), 0, 0);
    __builtin_unreachable();
}
