#include <stdint.h>

void kernel_init_global_state(void);
void kernel_init_startup_record(void);
void kernel_init_api_table(void);
void kernel_init_service_table(void);

static uint8_t expected[0x5000];
static uint8_t expected_table[0x1000];
static uint8_t expected_services[0x1000];
static const uint32_t display_depths[] = {0, 8, 16, 24, 32, 0xffffffff};
static const uint32_t cpu_features[] = {0, 1u << 25, ~(1u << 25), 0xffffffff};

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

static int load_kernel(const char *path)
{
    int fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0);
    int count;
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, 0x800000, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return count == 250099;
}

static int original(uint32_t address)
{
    if (!load_kernel("build/icestar.x")) return 0;
    ((void (*)(void))(uintptr_t)address)();
    return 1;
}

static int candidate(uint32_t address, void (*function)(void))
{
#ifdef KERNEL_LINKED_TEST
    (void)function;
    if (!load_kernel("build/icestar_c.x")) return 0;
    ((void (*)(void))(uintptr_t)address)();
#else
    (void)address;
    function();
#endif
    return 1;
}

static int run_test(void)
{
    volatile uint8_t *memory = (volatile uint8_t *)0x1b000;
    volatile uint8_t *table = (volatile uint8_t *)0x6b0000;
    volatile uint8_t *services = (volatile uint8_t *)0x7f0000;
    unsigned pattern, i, depth, feature;

    if (map_memory(0x1b000, 0x5000, 3) != 0x1b000 ||
        map_memory(0x800000, 0x40000, 7) != 0x800000 ||
        map_memory(0x6b0000, 0x1000, 3) != 0x6b0000 ||
        map_memory(0x7f0000, 0x1000, 3) != 0x7f0000 ||
        map_memory(0x1a000, 0x1000, 3) != 0x1a000) return 1;

    for (pattern = 0; pattern < 3; ++pattern) {
        for (i = 0; i < sizeof(expected); ++i)
            memory[i] = (uint8_t)(i * 37 + pattern * 83);
        *(volatile uint32_t *)(memory + 0x134) = 0x1e8e3 + pattern;
        if (!original(0x800104)) return 3;
        for (i = 0; i < sizeof(expected); ++i) expected[i] = memory[i];

        for (i = 0; i < sizeof(expected); ++i)
            memory[i] = (uint8_t)(i * 37 + pattern * 83);
        *(volatile uint32_t *)(memory + 0x134) = 0x1e8e3 + pattern;
        if (!candidate(0x800104, kernel_init_global_state)) return 3;
        for (i = 0; i < sizeof(expected); ++i)
            if (memory[i] != expected[i]) return 4;

        for (i = 0; i < sizeof(expected_table); ++i)
            table[i] = (uint8_t)(i * 17 + pattern * 59);
        if (!original(0x8004fa)) return 3;
        for (i = 0; i < sizeof(expected); ++i) expected[i] = memory[i];
        for (i = 0; i < sizeof(expected_table); ++i) expected_table[i] = table[i];
        for (i = 0; i < sizeof(expected_table); ++i)
            table[i] = (uint8_t)(i * 17 + pattern * 59);
        *(volatile uint32_t *)(memory + 0xac) = 0x12345678;
        if (!candidate(0x8004fa, kernel_init_api_table)) return 3;
        for (i = 0; i < sizeof(expected); ++i)
            if (memory[i] != expected[i]) return 5;
        for (i = 0; i < sizeof(expected_table); ++i)
            if (table[i] != expected_table[i]) return 6;

        for (i = 0; i < sizeof(expected); ++i)
            memory[i] = (uint8_t)(i * 23 + pattern * 67);
        *(volatile uint32_t *)(memory + 0x40) = 0x1e101 + pattern;
        if (!original(0x80049a)) return 3;
        for (i = 0; i < sizeof(expected); ++i) expected[i] = memory[i];
        for (i = 0; i < sizeof(expected); ++i)
            memory[i] = (uint8_t)(i * 23 + pattern * 67);
        *(volatile uint32_t *)(memory + 0x40) = 0x1e101 + pattern;
        if (!candidate(0x80049a, kernel_init_startup_record)) return 3;
        for (i = 0; i < sizeof(expected); ++i)
            if (memory[i] != expected[i]) return 9;

        for (depth = 0; depth < sizeof(display_depths) / sizeof(display_depths[0]); ++depth)
            for (feature = 0; feature < sizeof(cpu_features) / sizeof(cpu_features[0]); ++feature) {
                *(volatile uint32_t *)(memory + 0x34) = 0x1d0f8;
                *(volatile uint32_t *)(memory + 0xe4) = 0x7f0000;
                *(volatile uint32_t *)0x1d11c = display_depths[depth];
                *(volatile uint32_t *)0x1a868 = cpu_features[feature];
                for (i = 0; i < sizeof(expected_services); ++i)
                    services[i] = (uint8_t)(i * 13 + pattern * 71);
                if (!original(0x8005aa)) return 3;
                for (i = 0; i < sizeof(expected_services); ++i)
                    expected_services[i] = services[i];
                for (i = 0; i < sizeof(expected); ++i) expected[i] = memory[i];
                for (i = 0; i < sizeof(expected_services); ++i)
                    services[i] = (uint8_t)(i * 13 + pattern * 71);
                if (!candidate(0x8005aa, kernel_init_service_table)) return 3;
                for (i = 0; i < sizeof(expected_services); ++i)
                    if (services[i] != expected_services[i]) return 7;
                for (i = 0; i < sizeof(expected); ++i)
                    if (memory[i] != expected[i]) return 8;
            }
    }
    return 0;
}

void _start(void)
{
    int result = run_test();
    syscall3(1, (unsigned)result, 0, 0);
    __builtin_unreachable();
}
