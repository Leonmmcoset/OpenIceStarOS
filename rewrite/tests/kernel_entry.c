/* Execute original x86, ordinary C and fixed-address C. Hardware/subsystem
 * calls are hooked only inside fixture copies, with independently specified
 * call-order and memory oracles. Production images are never modified here. */
#include <stdint.h>
#include "../kernel/entry.h"
#define BASE 0x1b000u
#define SIZE 0x3000u
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t expected[SIZE];
static uint32_t calls, failure, pattern, mutation, seed, cases;
static const uint32_t seeds[] = {0, 1, 0x7f, 0x80, 0xff, 0x100,
                                 0x7fffffff, 0x80000000, 0xffffffff};
static const uint32_t order[] = {
    0x81a242, 0x800104, 0x81acc4, 0x80049a, 0x8153d4, 0x837448,
    0x82aa0f, 0x8004fa, 0x8005aa, 0x813320, 0x837143, 0x81a244,
    0x83717c, 0x837189, 0x819cf8, 0x812c70, 0x8175c0, 0x81a261,
    0x81a246, 0x81a261, 0x815106, 0x8152cc,
    0x814536, 0x81067a, 0x810d4a, 0x8130cc, 0x810f38, 0x8012ca
};
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
static uint32_t boundary(uint32_t address, uint32_t a, uint32_t b)
{
    uint32_t offset, value = seed ^ (address * 17u + calls * 0x12345u);
    if (calls >= COUNT(order) || order[calls] != address) failure = 1;
    if ((calls == 17 && (a != 0x21 || b != 0xd8)) ||
        (calls == 19 && (a != 0xa1 || b != 0xec))) failure = 2;
    if (address != 0x81a261 && (a || b)) failure = 3;
    offset = (calls * 137u + pattern * 31u) % (SIZE - 4u);
    if (mutation & 1) *(volatile uint8_t *)(BASE + offset) = (uint8_t)value;
    if (mutation & 2) *(volatile uint16_t *)(BASE + offset) = (uint16_t)(value >> 8);
    if (mutation & 4) WORD(BASE + offset) = value;
    if (mutation & 8) WORD(0x1b170 + (calls % 2u) * 4u) = value ^ a ^ b;
    ++calls;
    return value;
}
#define ENTRY_BOUNDARY(type, name, address) \
    type name(void) { return (type)boundary(address, 0, 0); }
#include "../kernel/entry_boundaries.def"
#undef ENTRY_BOUNDARY
void kernel_write_port_byte(uint32_t port, uint32_t value)
{
    (void)boundary(0x81a261, port, value);
}
/* Patch the CALL operands, not tiny CLI/STI/NOP entry points which overlap
 * if a five-byte detour is written over them. Offsets come from disassembly. */
struct call_patch { uint32_t offset, target; };
#include "../build/entry_calls.h"
static void redirect(uint32_t base, const struct call_patch *patches, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        uint32_t address = base + patches[i].offset;
        WORD(address + 1) = patches[i].target - address - 5;
    }
}
static void initialize(void)
{
    uint32_t i;
    calls = failure = 0;
    for (i = 0; i < SIZE; ++i)
        ((volatile uint8_t *)BASE)[i] = (uint8_t)(i * 37 + pattern * 83);
}
static void store_expected(uint32_t offset, uint32_t value, uint32_t bytes)
{
    uint32_t i;
    for (i = 0; i < bytes; ++i) expected[offset + i] = (uint8_t)(value >> (8u * i));
}
static void oracle(uint32_t count)
{
    uint32_t i;
    for (i = 0; i < SIZE; ++i) expected[i] = (uint8_t)(i * 37 + pattern * 83);
    for (i = 0; i < count; ++i) {
        uint32_t offset = (i * 137u + pattern * 31u) % (SIZE - 4u);
        uint32_t value = seed ^ (order[i] * 17u + i * 0x12345u);
        if (mutation & 1) store_expected(offset, value, 1);
        if (mutation & 2) store_expected(offset, value >> 8, 2);
        if (mutation & 4) store_expected(offset, value, 4);
        if (mutation & 8) store_expected(0x170 + (i % 2u) * 4u,
            value ^ (i == 17 ? 0xf9u : i == 19 ? 0x4du : 0), 4);
    }
}
uint32_t entry_checked_call(void (*function)(void));
static int exercise(uint32_t full)
{
    uint32_t implementation, i, count = full ? 28 : 22;
    oracle(count);
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t address = (full ? 0x800040 : 0x800070);
        void (*function)(void) = implementation == 1 ?
            (full ? kernel_entry : kernel_bootstrap) :
            (void (*)(void))(address + (implementation == 0 ? 0x100000 : 0));
        initialize();
        if (entry_checked_call(function)) return 10 + implementation;
        if (failure || calls != count) return 20 + implementation;
        for (i = 0; i < SIZE; ++i)
            if (((volatile uint8_t *)BASE)[i] != expected[i]) return 30 + implementation;
    }
    ++cases;
    return 0;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0' + value % 10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(digits + i), sizeof(digits) - i);
}
#define PRINT(text) syscall3(4, 1, (uint32_t)(text), sizeof(text) - 1)
static int run(void)
{
    uint32_t s, full, error;
    if (!map(BASE, SIZE) || !map(0x900000, 0x40000) || !map(0x800000, 0x40000)) return 1;
    if (!load("build/entry_original.x", 0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 3;
#else
    if (!load("build/entry_preview.x", 0x800000)) return 3;
#endif
    redirect(0x900000, original_calls, COUNT(original_calls));
    redirect(0x800000, candidate_calls, COUNT(candidate_calls));
    for (pattern = 0; pattern < 3; ++pattern)
    for (mutation = 0; mutation < 16; ++mutation)
    for (s = 0; s < COUNT(seeds); ++s)
    for (full = 0; full < 2; ++full) {
        seed = seeds[s]; error = exercise(full);
        if (error) return error;
    }
    if (cases != 864) return 40;
    PRINT("Kernel entry/bootstrap original/C/fixed-address/oracle cases: "); number(cases);
    PRINT("; 2592 executions; 22/28 ordered calls, PIC masks, full memory, stack and callee-saved registers verified\n");
    return 0;
}
void _start(void) { syscall3(1, run(), 0, 0); __builtin_unreachable(); }
