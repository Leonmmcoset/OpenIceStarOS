/* Exercise the actual original x86 routines and recovered C at CPL0. Paging
 * maps virtual 0..0x10ffff to RAM, including BIOS ROM and address zero. Only
 * the embedded test copies' I/O entries are redirected to recording hooks. */
#include <stdint.h>
#include "../kernel/firmware_scan.h"
#define LOW_SIZE 0x110000u
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define USED __attribute__((used))
static uint32_t directory[1024] __attribute__((aligned(4096)));
static uint32_t tables[4][1024] __attribute__((aligned(4096)));
static uint8_t baseline[LOW_SIZE], expected[LOW_SIZE];
struct event { uint32_t kind, port, value; };
static struct event wanted[24];
static uint32_t wanted_count, seen_count, phase, scenario, count;
static uint32_t mutate_candidate;
static USED uint32_t entry, registers[4];

static void put(const char *s)
{ while (*s) __asm__ volatile("outb %0,$0xe9" :: "a"(*s++)); }
static void number(uint32_t n)
{
    char s[11]; uint32_t i = 10; s[i] = 0;
    do { s[--i] = '0' + n % 10; n /= 10; } while (n);
    put(s + i);
}
__attribute__((noreturn)) static void done(uint32_t result)
{ __asm__ volatile("outl %0,$0xf4" :: "a"(result)); for (;;) __asm__ volatile("hlt"); }
static void require(int condition, const char *what)
{
    if (condition) return;
    put("FAIL firmware scenario/phase: "); number(scenario); put("/"); number(phase);
    put(" "); put(what); put("\n"); done(0x11);
}
static void paging(void)
{
    uint32_t i;
    for (i = 0; i < 4096; ++i) tables[i / 1024][i % 1024] = i * 4096u | 3u;
    for (i = 0; i < 4; ++i) directory[i] = (uint32_t)tables[i] | 3u;
    for (i = 0; i < LOW_SIZE / 4096; ++i) tables[0][i] = (0x200000u + i * 4096u) | 3u;
    __asm__ volatile("movl %0,%%cr3" :: "r"(directory) : "memory");
    __asm__ volatile("movl %%cr0,%%eax;orl $0x80000000,%%eax;movl %%eax,%%cr0"
                     ::: "eax", "memory");
}
static void store(uint8_t *image, uint32_t a, uint32_t value)
{
    uint32_t i; require(a <= LOW_SIZE - 4, "fixture store range");
    for (i = 0; i < 4; ++i) image[a + i] = (uint8_t)(value >> (i * 8));
}
static void signature(uint32_t a, const char *s)
{ while (*s) { require(a < LOW_SIZE, "fixture signature range"); baseline[a++] = *s++; } }
static void reset_case(void)
{
    uint32_t i;
    for (i = 0; i < LOW_SIZE / 4; ++i) ((uint32_t *)baseline)[i] = 0;
    store(baseline, 0x1b0dc, 0xdecafbad); store(baseline, 0x1b174, 0xc001c0de);
    mutate_candidate = 0; wanted_count = 0;
}
static void expect(uint32_t kind, uint32_t port, uint32_t value)
{
    require(wanted_count < 24, "fixture event capacity");
    wanted[wanted_count++] = (struct event){kind, port, value};
}
static void expect_acpi(uint32_t a, uint32_t b, uint32_t command, uint32_t control)
{
    expect(1, command, control); expect(2, b, 0); expect(2, a, 0);
    expect(3, a, 16); expect(4, 0, 0); expect(3, a + 2u, 16);
}
static void record(uint32_t kind, uint32_t port, uint32_t value)
{
    const struct event *e;
    require(seen_count < wanted_count, "unexpected I/O event");
    e = &wanted[seen_count++];
    require(kind == e->kind && port == e->port && value == e->value, "I/O order or arguments");
    if (seen_count == 1 && mutate_candidate) {
        /* The implementation must have captured every FACP field already. */
        WORD(mutate_candidate + 56u) = 0xdead0001;
        WORD(mutate_candidate + 64u) = 0xdead0002;
        WORD(mutate_candidate + 48u) = 0xdead0003;
        BYTE(mutate_candidate + 52u) = 1;
        WORD(0x1b0dc) = 0xbad00000;
    }
}
void kernel_write_port_byte(uint32_t port, uint32_t value) { record(1, port, value); }
uint32_t kernel_read_port_word(uint32_t port) { record(2, port, 0); return port ^ 0xa55a; }
void kernel_write_port_word(uint32_t port, uint32_t value) { record(3, port, value); }
void kernel_delay_three_nops(void) { record(4, 0, 0); }

static void patch(uint32_t address, uint32_t target)
{ BYTE(address) = 0xe9; WORD(address + 1u) = target - (address + 5u); }
static void hooks(uint32_t base)
{
    patch(base + 0x1a261, (uint32_t)kernel_write_port_byte);
    patch(base + 0x1a26d, (uint32_t)kernel_read_port_word);
    patch(base + 0x1a27b, (uint32_t)kernel_write_port_word);
    patch(base + 0x1a246, (uint32_t)kernel_delay_three_nops);
}
__attribute__((naked,noinline)) static void invoke(void)
{
    __asm__("pushl %ebx;pushl %esi;pushl %edi;pushl %ebp;"
            "movl $0x11223344,%ebx;movl $0x22334455,%esi;"
            "movl $0x33445566,%edi;movl $0x44556677,%ebp;"
            "call *entry;movl %ebx,registers;movl %esi,registers+4;"
            "movl %edi,registers+8;movl %ebp,registers+12;"
            "popl %ebp;popl %edi;popl %esi;popl %ebx;ret");
    __builtin_unreachable();
}
static void prepare_expected(void)
{
    uint32_t i;
    for (i = 0; i < LOW_SIZE / 4; ++i) ((uint32_t *)expected)[i] = ((uint32_t *)baseline)[i];
}
static void run(uint32_t pmid)
{
    uint32_t i;
    ++scenario;
    for (phase = 0; phase <
#ifdef TEST_INSTALLED
         3
#else
         2
#endif
         ; ++phase) {
        for (i = 0; i < LOW_SIZE / 4; ++i) WORD(i * 4u) = ((uint32_t *)baseline)[i];
        seen_count = 0;
        if (!phase) entry = pmid ? 0x9152cc : 0x915106;
        else if (phase == 1) entry = pmid ? (uint32_t)kernel_scan_pmid : (uint32_t)kernel_scan_acpi;
        else entry = pmid ? 0x8152cc : 0x815106;
        invoke();
        require(registers[0] == 0x11223344 && registers[1] == 0x22334455 &&
                registers[2] == 0x33445566 && registers[3] == 0x44556677, "callee-saved registers");
        require(seen_count == wanted_count, "missing I/O event");
        for (i = 0; i < LOW_SIZE / 4; ++i)
            require(WORD(i * 4u) == ((uint32_t *)expected)[i], "unexpected low-memory change");
        ++count;
    }
}
static void table(uint32_t sig, uint32_t root, uint32_t length, uint32_t candidate,
                  uint32_t a, uint32_t b, uint32_t command, uint8_t control)
{
    signature(sig, "RSD PTR!"); store(baseline, sig + 16u, root);
    store(baseline, root + 4u, length); store(baseline, root + 36u, candidate);
    signature(candidate, "FACP");
    store(baseline, candidate + 56u, a); store(baseline, candidate + 64u, b);
    store(baseline, candidate + 48u, command); baseline[candidate + 52u] = control;
    baseline[candidate + 46u] = 0x80; baseline[candidate + 47u] = 0xfe;
}
static void acpi(uint32_t sig, uint32_t root, uint32_t length, uint32_t candidate,
                 uint32_t a, uint32_t b, uint32_t command, uint8_t control, uint32_t matches)
{
    reset_case(); table(sig, root, length, candidate, a, b, command, control);
    prepare_expected();
    if (matches) {
        expect_acpi(a, b, command, control < 128 ? control : 0xffffff00u | control);
        store(expected, 0x1b0dc, b);
    }
    run(0);
}
void firmware_scan_qemu_main(void)
{
    static const uint32_t positions[] = {0, 1, 0x12345, 0x1b173, 0x1b174, 0xffffc, 0xffffd, 0xffffe};
    static const uint32_t lengths[] = {0, 1, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 43, 44,
                                    0x80000000, 0x80000023, 0x80000024, 0x80000025, 0x80000027,
                                    0x80000028, 0xffffffff};
    uint32_t i, j;
    paging(); hooks(0x900000);
#ifdef TEST_INSTALLED
    hooks(0x800000);
#endif
    reset_case(); prepare_expected(); run(1); run(0);
    for (i = 0; i < sizeof(positions) / sizeof(*positions); ++i) {
        reset_case(); signature(positions[i], "PMID"); prepare_expected();
        store(expected, 0x1b174, positions[i]); run(1);
    }
    for (i = 0; i < 4; ++i) {
        reset_case(); signature(0x12345, "PMID"); baseline[0x12345 + i] ^= 1;
        prepare_expected(); run(1);
    }
    for (i = 0xfffff; i <= 0x100000; ++i) {
        reset_case(); signature(i, "PMID"); prepare_expected(); run(1);
    }
    reset_case(); signature(7, "PMID"); signature(0x80001, "PMID");
    prepare_expected(); store(expected, 0x1b174, 7); run(1);
    for (i = 0; i < sizeof(lengths) / sizeof(*lengths); ++i) {
        uint32_t match = (lengths[i] >= 40 && lengths[i] <= 0x80000023);
        acpi(0xe0000, 0x20000, lengths[i], 0x40000, 0x404, 0x408, 0xb2, 0x80, match);
    }
    for (i = 0; i < 256; ++i)
        acpi(0xe0001, 0x20001, 40, 0x40001, 0xffffffff, 0x12345678, 0x87654321, i, 1);
    acpi(0xffffe, 0, 40, 0x40000, 0x404, 0x408, 0xb2, 0xff, 1);
    acpi(0xe0000, 0xfffffffc, 40, 0x40000, 0x404, 0x408, 0xb2, 0x7f, 1);
    acpi(0xe0000, 0x20000, 40, 0, 0x404, 0x408, 0xb2, 0, 1);
    acpi(0xfffff, 0x20000, 40, 0x40000, 0x404, 0x408, 0xb2, 0, 0);
    for (i = 0; i < 7; ++i) {
        reset_case(); table(0xe0000, 0x20000, 40, 0x40000, 0x404, 0x408, 0xb2, 1);
        baseline[0xe0000 + i] ^= 1; prepare_expected(); run(0);
    }
    /* The eighth RSD signature byte is ignored, including a NUL value. */
    reset_case(); table(0xe0000, 0x20000, 40, 0x40000, 0x404, 0x408, 0xb2, 1);
    baseline[0xe0007] = 0; prepare_expected(); expect_acpi(0x404, 0x408, 0xb2, 1);
    store(expected, 0x1b0dc, 0x408); run(0);
    for (i = 0; i < 4; ++i) {
        reset_case(); table(0xe0000, 0x20000, 40, 0x40000, 0x404, 0x408, 0xb2, 1);
        baseline[0x40000 + i] ^= 1; prepare_expected(); run(0);
    }
    /* The root's later entries are not read: candidate advances in memory. */
    for (i = 1; i <= 4; ++i) for (j = 0; j < 2; ++j) {
        reset_case(); table(0xe0000, 0x20000, 36 + 4 * i, 0x40000 + 4 * (i - j),
                           0x404, 0x408, 0xb2, 1);
        store(baseline, 0x20024, 0x40000);
        store(baseline, 0x20028, 0x60000); signature(0x60000, "FACP");
        prepare_expected();
        if (j) { expect_acpi(0x404, 0x408, 0xb2, 1); store(expected, 0x1b0dc, 0x408); }
        run(0);
    }
    /* Every outer signature is processed; only the first inner match is. */
    reset_case(); table(0xe0010, 0x20000, 44, 0x40000, 0x404, 0x408, 0xb2, 1);
    signature(0x40004, "FACP");
    table(0xe0100, 0x21000, 40, 0x41000, 0x504, 0x508, 0xc2, 0xfe);
    prepare_expected(); expect_acpi(0x404, 0x408, 0xb2, 1);
    expect_acpi(0x504, 0x508, 0xc2, 0xfffffffe); store(expected, 0x1b0dc, 0x508); run(0);
    reset_case(); table(0xe0000, 0x20000, 40, 0x40000, 0x404, 0x408, 0xb2, 0x80);
    mutate_candidate = 0x40000; prepare_expected(); expect_acpi(0x404, 0x408, 0xb2, 0xffffff80);
    store(expected, 0x40038, 0xdead0001); store(expected, 0x40040, 0xdead0002);
    store(expected, 0x40030, 0xdead0003); expected[0x40034] = 1;
    store(expected, 0x1b0dc, 0x408); run(0);
    put("PASS: "); number(scenario); put(" firmware scenarios, "); number(count);
    put(" original/C");
#ifdef TEST_INSTALLED
    put("/installed");
#endif
    put(" executions and independent memory/I/O oracle\n");
    put("Address zero, scan bounds, signed lengths/control, captured fields and linear walk verified\n");
    done(0x10);
}
