#include <stdint.h>

void kernel_init_global_state(void);
void kernel_init_api_table(void);

static uint8_t expected[0x5000];
static uint8_t expected_table[0x1000];

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c)
                      : "memory");
    return result;
}

static int map_memory(uint32_t address, uint32_t length, int protection)
{
    uint32_t args[] = {address, length, (uint32_t)protection, 0x32, 0xffffffff, 0};
    return syscall3(90, (unsigned)(uintptr_t)args, 0, 0);
}

static int run_test(const char *path)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    volatile uint8_t *table = (volatile uint8_t *)0x6b0000;
    unsigned pattern, i;
    int fd, count;

    if (map_memory(0x1b000, 0x5000, 3) != 0x1b000 ||
        map_memory(0x800000, 0x40000, 7) != 0x800000 ||
        map_memory(0x6b0000, 0x1000, 3) != 0x6b0000) return 1;
    fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0);
    if (fd < 0) return 2;
    count = syscall3(3, (unsigned)fd, 0x800000, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    if (count < 0x17547) return 3;

    for (pattern = 0; pattern < 3; ++pattern) {
        for (i = 0; i < sizeof(expected); ++i)
            memory[i] = (uint8_t)(i * 37 + pattern * 83);
        *(volatile uint32_t *)(memory + 0x134) = 0x1e8e3 + pattern;
        ((void (*)(void))0x800104)();
        for (i = 0; i < sizeof(expected); ++i) expected[i] = memory[i];

        for (i = 0; i < sizeof(expected); ++i)
            memory[i] = (uint8_t)(i * 37 + pattern * 83);
        *(volatile uint32_t *)(memory + 0x134) = 0x1e8e3 + pattern;
        kernel_init_global_state();
        for (i = 0; i < sizeof(expected); ++i)
            if (memory[i] != expected[i]) return 4;

        for (i = 0; i < sizeof(expected_table); ++i)
            table[i] = (uint8_t)(i * 17 + pattern * 59);
        ((void (*)(void))0x8004fa)();
        for (i = 0; i < sizeof(expected); ++i) expected[i] = memory[i];
        for (i = 0; i < sizeof(expected_table); ++i) expected_table[i] = table[i];
        for (i = 0; i < sizeof(expected_table); ++i)
            table[i] = (uint8_t)(i * 17 + pattern * 59);
        *(volatile uint32_t *)(memory + 0xac) = 0x12345678;
        kernel_init_api_table();
        for (i = 0; i < sizeof(expected); ++i)
            if (memory[i] != expected[i]) return 5;
        for (i = 0; i < sizeof(expected_table); ++i)
            if (table[i] != expected_table[i]) return 6;
    }
    return 0;
}

void _start(void)
{
    int result = run_test("build/icestar.x");
    syscall3(1, (unsigned)result, 0, 0);
    __builtin_unreachable();
}
