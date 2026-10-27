/* Compare the original scheduler with C and its fixed-address machine code.
 * Flag control, FNINIT, and the hardware TSS jump use identical callbacks.
 * Both versions execute their real service-table forwarding wrappers. */
#include "../kernel/runtime.h"
#include "../kernel/service_forward.h"

#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define TASK(index, offset) WORD(captured_table + (index) * 108u + (offset))

static uint8_t expected_low[0x1000], expected_tasks[0x4000], expected_services[0x1000];
static uint32_t trace[96], expected_trace[96], trace_length, expected_length;
static uint32_t initial_index, active_index, captured_table, saved_flags, mode;
static uint32_t cases, seed, entry_path;
static const uint32_t indices[] = {0, 1, 17, 0x7fffffff, 0x80000000, 0xffffffff};
static const uint32_t deltas[] = {
    0, 1, 2, 17, 0x40000000, 0x80000000, 0xc0000000, 0xffffffff, 0xfffffffd
};
static const uint32_t states[][2] = {
    {0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2},
    {2, 0}, {2, 1}, {2, 2}, {0xffffffff, 0xffffffff},
    {0x80000000, 1}, {2, 0x80000000}
};
static const uint32_t values[] = {
    0, 1, 2, 0x202, 0x246, 0x7fff, 0x8000, 0x7fffffff,
    0x80000000, 0xfffffffe, 0xffffffff, 0xa55ac33c
};
static const uint32_t records[] = {
    0x1b700, 0x1b701, 0x1b114, 0x1b084, 0x1b0d0, 0x1b040,
    0x601000, 0x60100c
};

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a" (result)
                      : "a" (number), "b" (a), "c" (b), "d" (c) : "memory");
    return result;
}

static void finish(unsigned result)
{
    syscall3(1, result, 0, 0);
    __builtin_unreachable();
}

static int map_memory(uint32_t address, uint32_t length, int protection)
{
    uint32_t args[] = {address, length, (uint32_t)protection, 0x32, 0xffffffff, 0};
    return syscall3(90, (unsigned)(uintptr_t)args, 0, 0) == (int)address;
}

static int load_kernel(const char *path, uint32_t address)
{
    int fd = syscall3(5, (unsigned)(uintptr_t)path, 0, 0), count;
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, address, 0x40000);
    syscall3(6, (unsigned)fd, 0, 0);
    return count == 250099;
}

static void observe(uint32_t event, uint32_t arg0, uint32_t arg1)
{
    if (trace_length + 8 > COUNT(trace)) finish(90);
    trace[trace_length++] = event;
    trace[trace_length++] = arg0;
    trace[trace_length++] = arg1;
    trace[trace_length++] = WORD(0x1b0c8);
    trace[trace_length++] = WORD(0x1b0d8);
    trace[trace_length++] = WORD(0x1b0e4);
    trace[trace_length++] = TASK(active_index, 8);
    trace[trace_length++] = WORD(0x1b084);
    WORD(0x1b084) = WORD(0x1b084) + event;
}

uint32_t kernel_read_flags(void)
{
    observe(1, 0, 0);
    if (mode & 1) WORD(0x1b0c8) = captured_table + 0x2000;
    return saved_flags;
}

void kernel_disable_interrupts(void)
{
    observe(2, 0, 0);
    if (mode & 2) WORD(0x1b0d8) = active_index;
}

void kernel_write_flags(uint32_t flags)
{
    observe(3, flags, 0);
    WORD(0x1b020) = flags;
}

static uint32_t save_fpu(uint32_t context, uint32_t event)
{
    observe(event, context, 0);
    if (mode & 4) {
        TASK(active_index, 8) = active_index + 3u;
        TASK(active_index + 3u, 0) = (seed & 1) ? 1 : 2;
    }
    if (mode & 8) WORD(0x1b0e4) = 0x610803;
    if (mode & 32) {
        WORD(0x1b0d8) = initial_index + 5u;
        WORD(0x1b0c8) = captured_table + 0x2000;
    }
    return context ^ 0x91c73a5b;
}

static uint32_t restore_fpu(uint32_t context, uint32_t event)
{
    observe(event, context, 0);
    if (mode & 16) TASK(active_index, 8) = active_index + 4u;
    if (mode & 32) {
        WORD(0x1b0d8) = initial_index + 6u;
        WORD(0x1b0c8) = captured_table + 0x2000;
        TASK(active_index + 4u, 12) = 0xffffffff;
    }
    return context ^ 0x6ce19b42;
}

static uint32_t save_primary(uint32_t context) { return save_fpu(context, 15); }
static uint32_t save_secondary(uint32_t context) { return save_fpu(context, 115); }
static uint32_t restore_primary(uint32_t context) { return restore_fpu(context, 16); }
static uint32_t restore_secondary(uint32_t context) { return restore_fpu(context, 116); }

void kernel_fpu_reset(void)
{
    observe(4, 0, 0);
    if (mode & 16) TASK(active_index, 8) = active_index + 4u;
    if (mode & 32) {
        WORD(0x1b0d8) = initial_index + 7u;
        WORD(0x1b0c8) = captured_table + 0x2000;
        TASK(active_index + 4u, 12) = 0xffffffff;
    }
}

void kernel_far_task_jump(uint32_t offset, uint32_t selector)
{
    observe(5, offset, selector);
    /* Simulate other tasks running before this task resumes. No scheduler
       code may undo these changes after the far-jump boundary returns. */
    if (mode & 32) {
        WORD(0x1b0d8) = initial_index + 8u;
        WORD(0x1b0c8) = captured_table + 0x2000;
        TASK(active_index, 8) ^= 0xa55ac33c;
        WORD(0x1b0e4) = 0x610803;
    }
}

static void jump(uint32_t address, uintptr_t target)
{
    *(volatile uint8_t *)(uintptr_t)address = 0xe9;
    WORD(address + 1) = (uint32_t)target - address - 5;
}

static void short_trampoline(uint32_t address, uint32_t destination, uintptr_t callback)
{
    int32_t distance = (int32_t)(destination - address - 2u);
    if (distance < -128 || distance > 127) finish(91);
    *(volatile uint16_t *)(uintptr_t)address = 0xeb | ((uint16_t)(uint8_t)distance << 8);
    jump(destination, callback);
}

static void stub_boundaries(uint32_t base)
{
    short_trampoline(base + 0x1a242, base + 0x1a200, (uintptr_t)kernel_disable_interrupts);
    short_trampoline(base + 0x1a24a, base + 0x1a210, (uintptr_t)kernel_read_flags);
    jump(base + 0x1a24d, (uintptr_t)kernel_write_flags);
    short_trampoline(base + 0x37347, base + 0x37300, (uintptr_t)kernel_fpu_reset);
    jump(base + 0x28d2b, (uintptr_t)kernel_far_task_jump);
}

static void initialize(unsigned alignment, unsigned state, uint32_t delta)
{
    volatile uint8_t *low = (volatile uint8_t *)0x1b000;
    volatile uint8_t *tasks = (volatile uint8_t *)0x600000;
    volatile uint8_t *services = (volatile uint8_t *)0x610000;
    unsigned i;
    int relative;
    uint32_t next;
    for (i = 0; i < sizeof(expected_low); ++i) low[i] = (uint8_t)(i * 37 + seed * 83);
    for (i = 0; i < sizeof(expected_tasks); ++i) tasks[i] = (uint8_t)(i * 29 + seed * 71);
    for (i = 0; i < sizeof(expected_services); ++i) services[i] = (uint8_t)(i * 17 + seed * 59);
    captured_table = 0x601000u + alignment - initial_index * 108u;
    active_index = initial_index + ((entry_path == 0 && (mode & 2)) ? 2u : 0u);
    for (relative = -8; relative <= 20; ++relative) {
        uint32_t index = initial_index + (uint32_t)relative;
        TASK(index, 0) = (uint32_t)relative % 3u;
        TASK(index, 8) = active_index + 1u;
        TASK(index, 12) = values[((uint32_t)relative + 8u + seed) % COUNT(values)];
        TASK(index, 32) = values[((uint32_t)relative + 9u + seed) % COUNT(values)];
    }
    next = active_index + delta;
    TASK(active_index, 0) = states[state][0];
    TASK(next, 0) = states[state][1];
    TASK(active_index, 8) = next;
    WORD(0x1b0c8) = captured_table;
    WORD(0x1b0d8) = initial_index;
    WORD(0x1b0e4) = 0x610001;
    WORD(0x610001 + 15 * 4) = (uint32_t)(uintptr_t)save_primary;
    WORD(0x610001 + 16 * 4) = (uint32_t)(uintptr_t)restore_primary;
    WORD(0x610803 + 15 * 4) = (uint32_t)(uintptr_t)save_secondary;
    WORD(0x610803 + 16 * 4) = (uint32_t)(uintptr_t)restore_secondary;
    WORD(0x1b114) = values[(seed / 3) % COUNT(values)];
    WORD(0x1b084) = values[(seed / 5) % COUNT(values)];
    WORD(0x1b0d0) = values[(seed / 7) % COUNT(values)];
    /* When aliasing a task field, follow that table's alignment. Otherwise
       a partial flag write can corrupt its next-task link into unmapped RAM. */
    WORD(0x1b040) = records[(seed / 11) % COUNT(records)] +
                   (records[(seed / 11) % COUNT(records)] >= 0x600000 ? alignment : 0);
    saved_flags = values[seed % COUNT(values)];
    for (i = 0; i < COUNT(trace); ++i) trace[i] = 0;
    trace_length = 0;
}

static int region(uint32_t address, uint8_t *expected, unsigned length, int save)
{
    volatile uint8_t *data = (volatile uint8_t *)(uintptr_t)address;
    unsigned i;
    for (i = 0; i < length; ++i) {
        if (save) expected[i] = data[i];
        else if (expected[i] != data[i]) return 0;
    }
    return 1;
}

static int compare(int save)
{
    unsigned i;
    if (!region(0x1b000, expected_low, sizeof(expected_low), save) ||
        !region(0x600000, expected_tasks, sizeof(expected_tasks), save) ||
        !region(0x610000, expected_services, sizeof(expected_services), save)) return 0;
    if (save) expected_length = trace_length;
    else if (expected_length != trace_length) return 0;
    for (i = 0; i < COUNT(trace); ++i) {
        if (save) expected_trace[i] = trace[i];
        else if (expected_trace[i] != trace[i]) return 0;
    }
    return 1;
}

static void print_count(uint32_t value)
{
    char digits[10];
    unsigned n = 0;
    do { digits[9 - n++] = (char)('0' + value % 10); value /= 10; } while (value);
    syscall3(4, 1, (unsigned)(uintptr_t)(digits + 10 - n), n);
}

static void invoke(int original)
{
    if (original) {
        uint32_t address = entry_path == 0 ? 0x91434a :
                           entry_path == 1 ? 0x914176 : 0x913a5e;
        ((void (*)(void))(uintptr_t)address)();
    } else {
#ifdef KERNEL_LINKED_TEST
        uint32_t address = entry_path == 0 ? 0x81434a :
                           entry_path == 1 ? 0x814176 : 0x813a5e;
        ((void (*)(void))(uintptr_t)address)();
#else
        if (entry_path == 0) kernel_yield_task();
        else if (entry_path == 1) kernel_schedule_task();
        else kernel_timer_tick();
#endif
    }
}

static int run_test(void)
{
    unsigned index, delta, state, alignment;
    if (!map_memory(0x1b000, 0x1000, 3) || !map_memory(0x600000, 0x4000, 3) ||
        !map_memory(0x610000, 0x1000, 3) || !map_memory(0x800000, 0x40000, 7) ||
        !map_memory(0x900000, 0x40000, 7)) return 1;
    if (!load_kernel("build/icestar.x", 0x900000) ||
        !load_kernel("build/icestar_c.x", 0x800000)) return 2;
    stub_boundaries(0x900000);
    stub_boundaries(0x800000);
    for (entry_path = 0; entry_path < 3; ++entry_path) {
        cases = 0;
        for (index = 0; index < COUNT(indices); ++index)
            for (delta = 0; delta < COUNT(deltas); ++delta)
                for (state = 0; state < COUNT(states); ++state)
                    for (alignment = 0; alignment < 4; ++alignment)
                        for (mode = 0; mode < 64; ++mode) {
                            initial_index = indices[index];
                            seed = cases;
                            initialize(alignment, state, deltas[delta]);
                            invoke(1);
                            if (entry_path == 0 &&
                                (trace_length < 24 || trace[0] != 1 || trace[8] != 2 ||
                                 trace[trace_length - 8] != 3 ||
                                 trace[trace_length - 7] != saved_flags)) return 10;
                            compare(1);
                            initialize(alignment, state, deltas[delta]);
                            invoke(0);
                            if (!compare(0)) {
                                syscall3(4, 1, (unsigned)(uintptr_t)"failed scheduler path/case: ", 28);
                                print_count(entry_path);
                                syscall3(4, 1, (unsigned)(uintptr_t)"/", 1);
                                print_count(cases);
                                syscall3(4, 1, (unsigned)(uintptr_t)"\n", 1);
                                return 11;
                            }
                            ++cases;
                        }
        if (entry_path == 0)
            syscall3(4, 1, (unsigned)(uintptr_t)"task-yield differential cases: ", 31);
        else if (entry_path == 1)
            syscall3(4, 1, (unsigned)(uintptr_t)"timer-switch differential cases: ", 33);
        else
            syscall3(4, 1, (unsigned)(uintptr_t)"timer-tick differential cases: ", 31);
        print_count(cases);
        syscall3(4, 1, (unsigned)(uintptr_t)"\n", 1);
    }
    return 0;
}

void _start(void) { finish((unsigned)run_test()); }
