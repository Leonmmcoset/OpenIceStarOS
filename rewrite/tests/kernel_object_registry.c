/* Three implementations are checked against a bytewise model. Only the name
 * comparison CALL is redirected, in private fixture copies of the binaries. */
#include <stdint.h>
#include "../kernel/object_registry.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)

static const struct region { uint32_t address, size, offset; } regions[] = {
    {0x1b000, 0x4000, 0}, {0x600000, 0x4000, 0x4000},
    {0x6a0000, 0x2000, 0x8000}
};
static uint8_t expected[0xa000];
static const uint32_t addresses[] = {0x8104a0, 0x810520, 0x81054e, 0x8105dc};
static const uint32_t candidates[] = {(uint32_t)kernel_object_find_name,
    (uint32_t)kernel_object_set_state, (uint32_t)kernel_object_registry_add,
    (uint32_t)kernel_object_registry_remove};
static const uint32_t values[] = {0, 1, 2, 0x7f, 0xff, 0x100,
    0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};
static const uint32_t counts[] = {0, 1, 2, 3, 5, 7,
    0x80000000, 0x80000001, 0xfffffffe, 0xffffffff};
static const uint32_t capacities[] = {0, 1, 2, 4, 8, 16, 0x80000000, 0xffffffff};
static const uint32_t tables[] = {0x600400, 0x600401, 0x600402, 0x600403,
    0x600ffc, 0x600fff, 0x1b400, 0x1b0ec, 0x1b0f0, 0x1b0f1,
    0x1b0f4, 0x1b0f5, 0x1b0f8, 0x1b0fc};
static const uint32_t indices[] = {0, 1, 2, 3, 255, 256, 511, 1023};
static const uint32_t records[] = {0x600000, 0x600001, 0x600002, 0x600003,
    0x600ed8, 0x601000, 0x1b000, 0x1b0f4-0x127,
    0x1b0f0-0x127, 0x1b0f8-0x127, 0x6a0000-0x127, 0x6a0fff-0x127};
static uint32_t pattern, layout, mutation, mode, events, wanted_events;
static uint32_t failure, invalid, steps, cases[4], excluded, seen_mutable_count;
static uint32_t call_arguments[24][2];

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
    uint32_t i;
    for (i = 0; i < COUNT(regions); ++i)
        if (address - regions[i].address < regions[i].size)
            return regions[i].offset + address - regions[i].address;
    invalid = 1;
    return 0;
}
static uint32_t read_model(uint32_t address)
{
    uint32_t i, result = 0;
    for (i = 0; i < 4; ++i) result |= (uint32_t)expected[offset(address+i)] << (i*8);
    return result;
}
static void write_model(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) expected[offset(address+i)] = (uint8_t)(value >> (i*8));
}
static uint32_t signed_less(uint32_t a, uint32_t b)
{
    /* Unsigned sign-bit ordering, independent of the implementation casts. */
    return (a ^ 0x80000000u) < (b ^ 0x80000000u);
}
static void write(uint32_t address, uint32_t value, uint32_t model)
{
    if (model) write_model(address, value);
    else DWORD(address) = value;
}
static void compare_mutation(uint32_t event, uint32_t model)
{
    if (mutation & 1) write(0x1b098, event & 1 ? 0x80000000u : 0, model);
    if (mutation & 2) {
        write(0x6a0000 + ((event*3+4)%15+1)*4,
              event & 1 ? 0xffffffffu : 0x600123u+event, model);
        write(0x6a0000 + ((event*5+7)%15+1)*4, event & 1 ? 0x600345 : 0, model);
    }
    if (mutation & 4) write(0x600ffdu + event*7u, 0x13579bdfu ^ event, model);
}
static uint32_t compare_result(uint32_t event)
{
    if (mode == 5) return event == 2 ? 1 : event+2;
    return values[mode == 4 ? 9 : mode == 3 ? 7 : mode];
}
uint32_t kernel_strings_equal(uint32_t first, uint32_t second)
{
    uint32_t event = events++;
    if (event >= wanted_events || first != call_arguments[event][0] ||
        second != call_arguments[event][1]) failure = 1;
    compare_mutation(event, 0);
    return compare_result(event);
}
static void initialize(uint32_t f, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t i, r;
    for (r = 0; r < COUNT(regions); ++r)
        for (i = 0; i < regions[r].size; ++i)
            ((volatile uint8_t *)regions[r].address)[i] = (uint8_t)(i*37+pattern*83+r*59);
    events = failure = 0;
    if (f == 0) {
        static const uint32_t entries[] = {0, 0x80000000, 0xffffffff,
            0x600200, 0x7fffffff, 1, 0x600300, 0x600003};
        for (i = 1; i <= 16; ++i) DWORD(0x6a0000+i*4) = entries[(i+layout)%COUNT(entries)];
        DWORD(0x1b098) = c;
    } else if (f == 1) {
        DWORD(0x6a0000+a*4u) = c;
    } else {
        for (i = 0; i < 24; ++i) DWORD(tables[layout] + i*4) = 0x12000000u+i*0x10203u+pattern;
        DWORD(0x1b0f8) = tables[layout];
        DWORD(0x1b0f0) = d;
        DWORD(0x1b0f4) = c;
    }
    (void)b;
}
static void capture(void)
{
    uint32_t r, i;
    for (r = 0; r < COUNT(regions); ++r)
        for (i = 0; i < regions[r].size; ++i)
            expected[regions[r].offset+i] = ((volatile uint8_t *)regions[r].address)[i];
    invalid = steps = wanted_events = 0;
}
static uint32_t oracle(uint32_t f, uint32_t a, uint32_t b)
{
    uint32_t index, table, capacity, count, found = 0xffffffff;
    if (!f) {
        capacity = read_model(0x1b098);
        for (index = 1; signed_less(index, capacity); ++index) {
            if (++steps > 32) { invalid = 1; return 0; }
            table = read_model(0x6a0000+index*4u);
            if (table && !(table & 0x80000000u)) {
                uint32_t event = wanted_events++;
                if (event >= COUNT(call_arguments)) { invalid = 1; return 0; }
                call_arguments[event][0] = read_model(0x6a0000+index*4u) + 16u;
                call_arguments[event][1] = a;
                compare_mutation(event, 1);
                if (compare_result(event) == 1) return index;
            }
        }
        return 0;
    }
    if (f == 1) {
        write_model(read_model(0x6a0000+a*4u) + 0x127u, b);
        return b;
    }
    table = read_model(0x1b0f8); capacity = read_model(0x1b0f0);
    for (index = 0; signed_less(index, read_model(0x1b0f4)); ++index) {
        if (++steps > 64) { invalid = 1; return 0; }
        if (read_model(table+index*4u) == a) { found = index; break; }
    }
    if (invalid) return 0;
    if (f == 2) {
        if (found != 0xffffffffu) return found;
        count = read_model(0x1b0f4);
        if (!signed_less(count, capacity)) return 0xffffffffu;
        write_model(table+read_model(0x1b0f4)*4u, a);
        if (read_model(0x1b0f4) != count) seen_mutable_count = 1;
        write_model(0x1b0f4, read_model(0x1b0f4)+1u);
        return read_model(0x1b0f4)-1u;
    }
    if (found == 0xffffffffu) return found;
    count = read_model(0x1b0f4);
    for (index = found; signed_less(index, read_model(0x1b0f4)); ++index) {
        if (++steps > 64) { invalid = 1; return 0; }
        write_model(table+index*4u, read_model(table+(index+1u)*4u));
        if (read_model(0x1b0f4) != count) seen_mutable_count = 1;
        if (invalid) return 0;
    }
    write_model(0x1b0f4, read_model(0x1b0f4)-1u);
    return found;
}
uint32_t object_checked_call(uint32_t function, uint32_t a, uint32_t b, uint32_t *result);
static int exercise(uint32_t f, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t result, wanted, implementation, r, i;
    initialize(f, a, b, c, d); capture(); wanted = oracle(f, a, b);
    /* Exclude unmapped/nonterminating alias inputs before any execution. */
    if (invalid) { ++excluded; return 0; }
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t target = implementation == 1 ? candidates[f] :
            addresses[f] + (implementation == 0 ? 0x100000 : 0);
        initialize(f, a, b, c, d);
        if (object_checked_call(target, a, b, &result)) return 10+implementation;
        if (result != wanted) return 20+implementation;
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
static int run(void)
{
    uint32_t r, c, cap, v, f, idx, wrap, rec, request, error, total = 0;
    for (r = 0; r < COUNT(regions); ++r) if (!map(regions[r].address, regions[r].size)) return 1;
    if (!map(0x900000, 0x40000) || !map(0x800000, 0x40000) ||
        !load("build/object_registry_original.x", 0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 3;
#else
    if (!load("build/object_registry_preview.x", 0x800000)) return 3;
#endif
    {
#include "../build/object_registry_calls.h"
        DWORD(0x900000+ORIGINAL_COMPARE_CALL+1) = (uint32_t)kernel_strings_equal-(0x900000+ORIGINAL_COMPARE_CALL+5);
        DWORD(0x800000+CANDIDATE_COMPARE_CALL+1) = (uint32_t)kernel_strings_equal-(0x800000+CANDIDATE_COMPARE_CALL+5);
    }
    for (pattern = 0; pattern < 3; ++pattern)
    for (layout = 0; layout < 8; ++layout)
    for (c = 0; c < COUNT(capacities); ++c)
    for (mode = 0; mode < 6; ++mode)
    for (mutation = 0; mutation < 8; ++mutation)
        if ((error = exercise(0, values[layout], 0, capacities[c], 0))) return error;
    mode = 1;
    for (layout = 0; layout < 8; ++layout)
    for (mutation = 0; mutation < 8; ++mutation)
        if ((error = exercise(0, 0xffffffff, 0, 0x7fffffff, 0))) return error;
    for (pattern = 0; pattern < 3; ++pattern)
    for (idx = 0; idx < COUNT(indices); ++idx)
    for (wrap = 0; wrap < 4; ++wrap)
    for (rec = 0; rec < COUNT(records); ++rec)
    for (v = 0; v < COUNT(values); ++v)
        if ((error = exercise(1, indices[idx]+wrap*0x40000000u, values[v], records[rec], 0))) return error;
    for (f = 2; f < 4; ++f)
    for (pattern = 0; pattern < 3; ++pattern)
    for (layout = 0; layout < COUNT(tables); ++layout)
    for (c = 0; c < COUNT(counts); ++c)
    for (cap = 0; cap < COUNT(values); ++cap)
    for (v = 0; v < 5; ++v) {
        initialize(f, 0, 0, counts[c], values[cap]);
        request = v == 0 ? 0xdeadbeefu : v == 1 ? DWORD(tables[layout]) :
            v == 2 ? DWORD(tables[layout]+(counts[c] && counts[c]<8 ? counts[c]-1u : 0)*4u) :
            v == 3 ? DWORD(tables[layout]+(counts[c]<8 ? counts[c]/2 : 0)*4u) : values[cap];
        if ((error = exercise(f, request, 0, counts[c], values[cap]))) {
            PRINT("registry failure f/layout/count/cap/value: "); number(f); PRINT("/"); number(layout);
            PRINT("/"); number(c); PRINT("/"); number(cap); PRINT("/"); number(v); PRINT("\n");
            return error;
        }
    }
    if (!seen_mutable_count) return 50;
    for (f = 0; f < 4; ++f) {
        total += cases[f];
        PRINT("Object helper "); number(f); PRINT(" cases: "); number(cases[f]); PRINT("\n");
    }
    PRINT("Total cases: "); number(total); PRINT("; executions: "); number(total*3);
    PRINT("; excluded unmapped/unbounded aliases: "); number(excluded);
    PRINT("; all four object helpers verified against independent full-memory/call/ABI oracle\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
