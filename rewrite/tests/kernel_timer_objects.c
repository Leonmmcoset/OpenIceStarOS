/* Independent bytewise model of timer x86 memory effects and callback order.
 * Only privileged flag helpers and allocator calls are redirected in copies
 * of the binaries. Find-free calls and all timer memory accesses stay real. */
#include <stdint.h>
#include "../kernel/timer_objects.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
#define SLOT(i) (0x6c0000u + (i)*4u)

static const struct region { uint32_t address, size, offset; } regions[] = {
    {0x1b000, 0x4000, 0}, {0x600000, 0x4000, 0x4000},
    {0x6c0000, 0x10000, 0x8000}
};
static uint8_t expected[0x18000];
static const uint32_t addresses[] = {0x82aa33, 0x82aaad, 0x82abaf, 0x82ac21};
static const uint32_t candidates[] = {(uint32_t)kernel_timer_find_free,
    (uint32_t)kernel_timer_create, (uint32_t)kernel_timer_delete, (uint32_t)kernel_timer_rearm};
static const uint32_t values[] = {0, 1, 2, 15, 0x100, 0x600100,
    0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};
static const uint32_t bounds[] = {0, 1, 2, 4, 8, 0x4000, 0x7fffffff, 0x80000000, 0xffffffff};
static const uint32_t allocations[] = {0, 0x600100, 0x600101, 0x1b080,
    0x1b0b8, 0x1b0bc, 0x1b0c0, 0x6c0000, 0x6c0004, 0x6c0008};
static const uint32_t records[] = {0x600000, 0x600001, 0x600fff, 0x1b080,
    0x1b0b4, 0x1b0b8, 0x1b0bc, 0x6c0000, 0x6c0004, 0x6c0008};
static const uint32_t indices[] = {0, 1, 2, 0x3fff};
static const struct state { uint32_t count, capacity, holes; } states[] = {
    {0, 8, 0}, {2, 8, 0}, {4, 4, 1}, {4, 4, 0}, {4, 8, 0x80000000},
    {0xffffffff, 8, 1}, {0, 0x80000000, 0}, {0x4000, 0x4000, 1}
};
static uint32_t pattern, layout, mutation, selected, allocation, saved_flags;
static uint32_t events, wanted_events, failure, invalid, cases[4], excluded;
static uint32_t call_arguments[8][2], seen_no_restore, seen_restore, seen_alias;
enum { READ_FLAGS, DISABLE, ALLOCATE, FREE, WRITE_FLAGS };

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static int map(uint32_t address, uint32_t size)
{
    uint32_t args[] = {address, size, 7, 0x32, 0xffffffff, 0};
    return (uint32_t)syscall3(90, (uint32_t)args, 0, 0) == address;
}
static int load(const char *path, uint32_t address)
{
    int fd = syscall3(5, (uint32_t)path, 0, 0), size;
    if (fd < 0) return 0;
    size = syscall3(3, (uint32_t)fd, address, 0x40000);
    syscall3(6, (uint32_t)fd, 0, 0);
    return size == 250099;
}
static uint32_t offset(uint32_t address)
{
    uint32_t r;
    for (r = 0; r < COUNT(regions); ++r)
        if (address-regions[r].address < regions[r].size)
            return regions[r].offset+address-regions[r].address;
    invalid = 1;
    return 0;
}
static uint32_t read_model(uint32_t address)
{
    uint32_t i, result = 0;
    for (i = 0; i < 4; ++i) result |= (uint32_t)expected[offset(address+i)] << (8*i);
    return result;
}
static void write_model(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) expected[offset(address+i)] = (uint8_t)(value >> (8*i));
}
static uint32_t less(uint32_t a, uint32_t b)
{
    return (a ^ 0x80000000u) < (b ^ 0x80000000u);
}
static void write(uint32_t address, uint32_t value, uint32_t model)
{
    if (model) write_model(address, value); else DWORD(address) = value;
}
static void mutate(uint32_t kind, uint32_t model)
{
    if (kind == READ_FLAGS && (mutation & 1))
        write(0x1b084, 0xfffffffeu+layout, model);
    if (kind == DISABLE && (mutation & 2)) {
        write(0x1b0bc, layout % 3, model);
        write(0x1b0b8, 8, model);
        write(0x1b0c0, 1, model);
        write(SLOT(layout % 3), 0, model);
    }
    if ((kind == ALLOCATE || kind == FREE) && (mutation & 4)) {
        write(0x1b084, layout & 1 ? 0x600100 : 0xffffffff, model);
        write(0x1b0bc, layout & 1 ? 0x80000000 : 7, model);
        write(0x1b0c0, layout & 1 ? 0x80000000 : 0xffffffff, model);
        write(SLOT(selected), 0x600200, model);
    }
    if (kind == WRITE_FLAGS && (mutation & 8)) {
        write(0x1b084, 0xdeadbeef, model);
        write(0x1b0bc, 0x76543210, model);
        write(SLOT(selected), 0xabcdef00, model);
    }
}
static void event(uint32_t kind, uint32_t argument)
{
    if (events >= wanted_events || call_arguments[events][0] != kind ||
        call_arguments[events][1] != argument) failure = 1;
    ++events;
    mutate(kind, 0);
}
uint32_t kernel_read_flags(void) { event(READ_FLAGS, 0); return saved_flags; }
void kernel_disable_interrupts(void) { event(DISABLE, 0); }
void kernel_write_flags(uint32_t flags) { event(WRITE_FLAGS, flags); }
uint32_t kernel_allocate(uint32_t size) { event(ALLOCATE, size); return allocation; }
void kernel_free_allocation(uint32_t address) { event(FREE, address); }
static void model_event(uint32_t kind, uint32_t argument)
{
    if (wanted_events >= COUNT(call_arguments)) { invalid = 1; return; }
    call_arguments[wanted_events][0] = kind;
    call_arguments[wanted_events++][1] = argument;
    mutate(kind, 1);
}
static void initialize(uint32_t f, uint32_t a, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t r, i;
    for (r = 0; r < COUNT(regions); ++r)
        for (i = 0; i < regions[r].size; ++i)
            ((volatile uint8_t *)regions[r].address)[i] = (uint8_t)(i*37+pattern*83+r*59);
    for (i = 0; i < 0x4000; ++i) DWORD(SLOT(i)) = 0x600100;
    if (layout == 0) DWORD(SLOT(0)) = 0;
    if (layout == 1) DWORD(SLOT(1)) = 0;
    if (layout == 2) DWORD(SLOT(3)) = 0;
    if (layout == 3) DWORD(SLOT(0x3fff)) = 0;
    if (layout == 4) {
        DWORD(SLOT(0)) = 0xffffffff;
        DWORD(SLOT(1)) = 0x80000000;
        DWORD(SLOT(2)) = 1;
    }
    DWORD(0x1b0bc) = c; DWORD(0x1b0b8) = d; DWORD(0x1b0c0) = e;
    DWORD(0x1b084) = values[pattern];
    if (f >= 2) DWORD(SLOT(a)) = allocation;
    selected = f >= 2 ? a : layout % 3;
    saved_flags = values[(pattern+layout)%COUNT(values)];
    events = failure = 0;
}
static void capture(void)
{
    uint32_t r, i;
    for (r = 0; r < COUNT(regions); ++r)
        for (i = 0; i < regions[r].size; ++i)
            expected[regions[r].offset+i] = ((volatile uint8_t *)regions[r].address)[i];
    invalid = wanted_events = 0;
}
static uint32_t find_model(void)
{
    uint32_t i;
    if (less(0, read_model(0x1b0c0)))
        for (i = 0; less(i, read_model(0x1b0bc)); ++i) {
            if (i >= 0x4000) { invalid = 1; return 0xffffffff; }
            if (read_model(SLOT(i)) == 0) return i;
        }
    if (less(read_model(0x1b0bc), read_model(0x1b0b8))) return read_model(0x1b0bc);
    return 0xffffffff;
}
static uint32_t oracle(uint32_t f, uint32_t a, uint32_t b)
{
    uint32_t index, record, deadline;
    if (!f) return find_model();
    if (f == 3) {
        record = read_model(SLOT(a));
        deadline = read_model(0x1b084) + b;
        write_model(record+4, deadline);
        return deadline;
    }
    model_event(READ_FLAGS, 0); model_event(DISABLE, 0);
    if (f == 2) {
        if (less(0, read_model(SLOT(a)))) {
            model_event(FREE, read_model(SLOT(a)));
            write_model(SLOT(a), 0);
            write_model(0x1b0c0, read_model(0x1b0c0)+1u);
        }
        model_event(WRITE_FLAGS, saved_flags);
        return 0; /* The recovered delete API is void: EAX is incidental. */
    }
    index = find_model();
    if (index & 0x80000000u) return 0xffffffff;
    model_event(ALLOCATE, 12);
    write_model(SLOT(index), allocation);
    if (!read_model(SLOT(index))) return 0xffffffff;
    record = read_model(SLOT(index));
    deadline = read_model(0x1b084) + a;
    write_model(record+4, deadline);
    record = read_model(SLOT(index));
    write_model(record, b);
    record = read_model(SLOT(index));
    write_model(record+8, 0);
    if (less(index, read_model(0x1b0bc)))
        write_model(0x1b0c0, read_model(0x1b0c0)-1u);
    else
        write_model(0x1b0bc, read_model(0x1b0bc)+1u);
    model_event(WRITE_FLAGS, saved_flags);
    return index;
}
uint32_t timer_checked_call(uint32_t function, uint32_t a, uint32_t b, uint32_t *result);
static int exercise(uint32_t f, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
    uint32_t result, wanted, implementation, r, i;
    initialize(f, a, c, d, e); capture(); wanted = oracle(f, a, b);
    if (invalid) { ++excluded; return 0; }
    if (f == 1) {
        if (call_arguments[wanted_events-1][0] == WRITE_FLAGS) ++seen_restore;
        else ++seen_no_restore;
        if (allocation >= 0x6c0000 && allocation < 0x6d0000 && wanted != 0xffffffff) ++seen_alias;
    }
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t target = implementation == 1 ? candidates[f] :
            addresses[f] + (implementation == 0 ? 0x100000 : 0);
        initialize(f, a, c, d, e);
        if (timer_checked_call(target, a, b, &result)) return 10+implementation;
        if (f != 2 && result != wanted) return 20+implementation;
        if (failure || events != wanted_events) return 30+implementation;
        for (r = 0; r < COUNT(regions); ++r)
            for (i = 0; i < regions[r].size; ++i)
                if (((volatile uint8_t *)regions[r].address)[i] != expected[regions[r].offset+i])
                    return 40+implementation;
    }
    ++cases[f];
    return 0;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0'+value%10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(digits+i), sizeof(digits)-i);
}
static int report(int error, uint32_t f)
{
    if (!error) return 0;
    PRINT("Timer failure code/function/pattern/layout/mutation/allocation: ");
    number(error); PRINT("/"); number(f); PRINT("/"); number(pattern);
    PRINT("/"); number(layout); PRINT("/"); number(mutation); PRINT("/"); number(allocation); PRINT("\n");
    return error;
}
static int run(void)
{
    uint32_t r, c, cap, h, state, v, p, idx, wrap, rec, error, total = 0;
    for (r = 0; r < COUNT(regions); ++r) if (!map(regions[r].address, regions[r].size)) return 1;
    if (!map(0x900000, 0x40000) || !map(0x800000, 0x40000) ||
        !load("build/timer_objects_original.x", 0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 3;
#else
    if (!load("build/timer_objects_preview.x", 0x800000)) return 3;
#endif
    {
#include "../build/timer_objects_calls.h"
    }
    for (pattern = 0; pattern < 2; ++pattern)
    for (layout = 0; layout < 6; ++layout)
    for (c = 0; c < COUNT(bounds); ++c)
    for (cap = 0; cap < COUNT(bounds); ++cap)
    for (h = 0; h < COUNT(values); ++h)
        if ((error = report(exercise(0, 0, 0, bounds[c], bounds[cap], values[h]), 0))) return error;
    for (pattern = 0; pattern < 2; ++pattern)
    for (layout = 0; layout < 5; ++layout)
    for (state = 0; state < COUNT(states); ++state)
    for (rec = 0; rec < COUNT(allocations); ++rec)
    for (v = 0; v < COUNT(values); ++v)
    for (mutation = 0; mutation < 16; ++mutation) {
        allocation = allocations[rec];
        if ((error = report(exercise(1, values[v], values[(v+5)%COUNT(values)],
            states[state].count, states[state].capacity, states[state].holes), 1))) return error;
    }
    for (pattern = 0; pattern < 2; ++pattern)
    for (layout = 0; layout < 4; ++layout)
    for (idx = 0; idx < COUNT(indices); ++idx)
    for (wrap = 0; wrap < 4; ++wrap)
    for (v = 0; v < COUNT(values); ++v)
    for (mutation = 0; mutation < 16; ++mutation) {
        allocation = values[v];
        if ((error = report(exercise(2, indices[idx]+wrap*0x40000000u, 0, 4, 8, values[v]), 2))) return error;
    }
    mutation = 0;
    for (p = 0; p < COUNT(values); ++p)
    for (layout = 0; layout < 2; ++layout)
    for (idx = 0; idx < COUNT(indices); ++idx)
    for (wrap = 0; wrap < 4; ++wrap)
    for (rec = 0; rec < COUNT(records); ++rec)
    for (v = 0; v < COUNT(values); ++v) {
        pattern = p; allocation = records[rec];
        if ((error = report(exercise(3, indices[idx]+wrap*0x40000000u, values[v], 4, 8, 1), 3))) return error;
    }
    if (!seen_no_restore || !seen_restore || !seen_alias) return 50;
    for (r = 0; r < 4; ++r) {
        total += cases[r];
        PRINT("Timer helper "); number(r); PRINT(" cases: "); number(cases[r]); PRINT("\n");
    }
    PRINT("Total cases: "); number(total); PRINT("; executions: "); number(total*3);
    PRINT("; excluded unmapped/unbounded aliases: "); number(excluded);
    PRINT("; create without restore: "); number(seen_no_restore);
    PRINT("; create with restore: "); number(seen_restore);
    PRINT("; successful table aliases: "); number(seen_alias);
    PRINT("; all four timer helpers verified against independent full-memory/call/ABI oracle\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
