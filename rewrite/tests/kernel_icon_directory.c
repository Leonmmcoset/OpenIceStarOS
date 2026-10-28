/* Original x86, ordinary C and fixed-address C against an independent
 * bytewise ICO-directory/callback/memory/ABI oracle. No decoder executes. */
#include <stdint.h>
#include "../kernel/icon_directory.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
#define REGION 8192u
#define TOTAL (REGION*2u)
static uint8_t initial[TOTAL], expected[TOTAL];
static uint32_t arguments[5], wanted[5], wanted_kind, calls, failure, cases[3];
static uint32_t function, package, surface, mutation, excluded, branches[3];
static const uint32_t addresses[] = {0x82af08, 0x82af4c, 0x82b004};
static const uint32_t candidates[] = {(uint32_t)kernel_icon_draw_id,
    (uint32_t)kernel_icon_draw_centered, (uint32_t)kernel_icon_draw_frame};
static const uint32_t dimensions[] = {0, 1, 15, 16, 31, 255, 256, 257,
    0x7fffffff, 0x80000000, 0xffffffff, 0x80000101};
static const uint32_t ids[] = {0, 1, 7, 1023, 0x3fffffff, 0x40000000,
    0x40000001, 0x80000000, 0xc0000001, 0xffffffff};
#include "../build/icon_directory_resources.h"

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
                     : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static void number(uint32_t value)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0'+value%10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(digits+i), sizeof(digits)-i);
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
    if (address >= 0x4ff000 && address < 0x501000) return address-0x4ff000;
    if (address >= 0x600000 && address < 0x602000) return REGION+address-0x600000;
    return TOTAL;
}
static void set8(uint32_t address, uint32_t value) { initial[offset(address)] = (uint8_t)value; }
static void set16(uint32_t address, uint32_t value)
{
    set8(address, value); set8(address+1, value >> 8);
}
static void set32(uint32_t address, uint32_t value)
{
    uint32_t i; for (i = 0; i < 4; ++i) set8(address+i, value >> (i*8));
}
static uint32_t get16(uint32_t address)
{
    uint32_t o = offset(address); return initial[o]+256u*initial[o+1];
}
static uint32_t get32(uint32_t address)
{
    uint32_t i, value = 0;
    for (i = 0; i < 4; ++i) value += (uint32_t)initial[offset(address+i)] << (i*8);
    return value;
}
static void effect(uint32_t address, uint32_t value, int model)
{
    uint32_t i;
    if (!model) { DWORD(address) = value; return; }
    for (i = 0; i < 4; ++i) expected[offset(address+i)] = (uint8_t)(value >> (i*8));
}
static void mutate(int model)
{
    if (!mutation) return;
    effect(package+4u, 0xdeadbeef, model);
    effect(surface+12u, 0xfedcba98, model);
    effect(surface+16u, 0x76543210, model);
    effect(0x500000, 0xbaadf00d, model);
    effect(package+0x180u, wanted[0]^wanted[1]^wanted[2]^wanted[3]^wanted[4], model);
}
static void observe(uint32_t kind, uint32_t a, uint32_t b, uint32_t c,
                    uint32_t d, uint32_t e)
{
    uint32_t i, actual[] = {a, b, c, d, e};
    if (++calls != 1 || kind != wanted_kind) failure = 1;
    for (i = 0; i < (kind == 1 ? 5u : 4u); ++i)
        if (actual[i] != wanted[i]) failure = 1;
    mutate(0);
}
void kernel_icon_png_8241d0(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{ observe(1, a, b, c, d, e); }
void kernel_icon_dib_82b20c(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{ observe(2, a, b, c, d, 0); }

static void prepare(uint32_t seed, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < TOTAL; ++i) initial[i] = (uint8_t)(i*37+seed*83);
    surface = 0x600101+(seed%4);
    package = 0x600801+((seed/4)%4);
    arguments[0] = function == 0 ? ids[seed%10] : surface;
    arguments[1] = dimensions[seed%12];
    arguments[2] = dimensions[(seed/12)%12];
    arguments[3] = package;
    arguments[4] = 0;
    if (function == 1) arguments[1] = package;
    set32(surface+12, dimensions[seed%12]);
    set32(surface+16, dimensions[(seed/12)%12]);
    if (function == 0) set32(0x500000+arguments[0]*4, surface);
    set16(package+4, count);
    mutation = seed%3;
}
static void entry(uint32_t index, uint32_t width, uint32_t height, uint32_t seed)
{
    uint32_t a = package+6+index*16;
    set8(a, width); set8(a+1, height);
    set32(a+8, seed*0x1234567u);
    set32(a+12, dimensions[seed%12] ^ (seed*0x01010101u));
}
static int plan(void)
{
    uint32_t frame = arguments[4], x = arguments[1], y = arguments[2], a, i;
    uint32_t count = get16(package+4);
    if (function == 1) {
        uint32_t best = 0;
        frame = 0;
        for (i = 0; i < count; ++i) {
            uint32_t width = initial[offset(package+6+i*16)];
            if (width == 0) { best = 256; frame = i; break; }
            if (width > best) { best = width; frame = i; }
        }
        /* Divide wrapped unsigned differences instead of reproducing SHR. */
        x = (get32(surface+12)-best)/2;
        y = (get32(surface+16)-best)/2;
    }
    wanted_kind = 0;
    if (!(frame >= 0x80000000u || frame < count)) return 1;
    a = package+6+frame*16;
    if (offset(a) >= TOTAL-16 || offset(a+15) != offset(a)+15) {
        ++excluded; return 0;
    }
    wanted_kind = initial[offset(a)] == 0 && initial[offset(a+1)] == 0 ? 1 : 2;
    wanted[0] = function == 0 ? get32(0x500000+arguments[0]*4) : surface;
    wanted[1] = x; wanted[2] = y;
    wanted[3] = package+get32(a+12);
    wanted[4] = wanted_kind == 1 ? get32(a+8) : 0;
    return 1;
}
uint32_t icon_checked_call(uint32_t function, const uint32_t *arguments);
static int exercise(void)
{
    uint32_t i, implementation;
    if (!plan()) return 0;
    for (i = 0; i < TOTAL; ++i) expected[i] = initial[i];
    if (wanted_kind) mutate(1);
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t target = implementation == 1 ? candidates[function] :
            addresses[function]+(implementation == 0 ? 0x100000 : 0);
        for (i = 0; i < REGION; ++i) {
            ((volatile uint8_t *)0x4ff000)[i] = initial[i];
            ((volatile uint8_t *)0x600000)[i] = initial[REGION+i];
        }
        calls = failure = 0;
        if (icon_checked_call(target, arguments)) return 10+implementation;
        if (failure || calls != (wanted_kind ? 1u : 0u)) return 20+implementation;
        for (i = 0; i < REGION; ++i)
            if (((volatile uint8_t *)0x4ff000)[i] != expected[i] ||
                ((volatile uint8_t *)0x600000)[i] != expected[REGION+i]) return 30+implementation;
    }
    ++cases[function]; ++branches[wanted_kind]; return 0;
}
static int run(void)
{
    uint32_t a, b, i, j, k, f, error;
    const uint32_t frames[] = {0, 1, 2, 3, 4, 7, 127, 128, 0x7fff, 0x8000,
        0xfffe, 0xffff, 0x10000, 0x10000000, 0x40000000, 0x7fffffff,
        0x80000000, 0x80000001, 0x8000007f, 0x8fffffff, 0x90000000,
        0xf0000000, 0xffffff80, 0xfffffff8, 0xfffffffe, 0xffffffff};
    const uint32_t counts[] = {0, 1, 2, 4, 0x7fff, 0x8000, 0xffff};
    if (!map(0x4ff000, REGION) || !map(0x600000, REGION) ||
        !map(0x900000, 0x40000) || !map(0x800000, 0x40000) ||
        !load("build/icon_directory_original.x", 0x900000)) return 1;
#ifdef TEST_INSTALLED
    if (!load("build/openicestaros_kernel.x", 0x800000)) return 2;
#else
    if (!load("build/icon_directory_preview.x", 0x800000)) return 2;
#endif
    {
#include "../build/icon_directory_calls.h"
    }
    /* Every possible width/height pair, through both direct and ID APIs. */
    for (function = 0; function < 3; function += 2)
    for (a = 0; a < 256; ++a) for (b = 0; b < 256; ++b) {
        prepare(a*256+b, 1); entry(0, a, b, a*256+b);
        if ((error = exercise())) return error;
    }
    /* Every ordered width pair in four entries: ties, early zero, heights
     * ignored, first maximum retained and nonmonotonic directories. */
    function = 1;
    for (a = 0; a < 256; ++a) for (b = 0; b < 256; ++b) {
        prepare(a*256+b, 4);
        entry(0, a, b, 0); entry(1, b, a, 1);
        entry(2, b, 255, 2); entry(3, a^b, 0, 3);
        if ((error = exercise())) return error;
    }
    /* Every representable width with all chosen signed/wrap dimensions. */
    for (a = 0; a < 256; ++a)
    for (b = 0; b < 12; ++b) for (i = 0; i < 12; ++i) {
        prepare(a+b+i, 1); entry(0, a, 17, a);
        set32(surface+12, dimensions[b]); set32(surface+16, dimensions[i]);
        if ((error = exercise())) return error;
    }
    /* High WORD counts terminate at the first zero-width entry. */
    for (i = 0; i < 7; ++i) for (j = 0; j < 10; ++j) {
        prepare(i*10+j, counts[i]); entry(0, 0, j, j);
        if ((error = exercise())) return error;
    }
    for (function = 0; function < 3; function += 2)
    for (i = 0; i < 7; ++i) for (j = 0; j < 26; ++j)
    for (k = 0; k < 4; ++k) {
        prepare(i*104+j*4+k, counts[i]); arguments[4] = frames[j];
        a = package+6+frames[j]*16;
        if (offset(a) < TOTAL-16 && offset(a+15) == offset(a)+15)
            entry(frames[j], k&1, k>>1, i*104+j*4+k);
        /* A negative frame may overlap the header and change count. The
         * oracle deliberately reads the final bytes rather than counts[i]. */
        if ((error = exercise())) return error;
    }
    /* Original disk directories: every declared frame, ID forwarding and
     * centered selection. Pixel payloads are never interpreted by stubs. */
    for (f = 0; f < 4; ++f) for (function = 0; function < 3; ++function) {
        uint32_t count = resource_fixtures[f].bytes[4];
        for (j = 0; j < (function == 1 ? 1u : count+1); ++j) {
            prepare(f*31+j, count);
            for (i = 0; i < resource_fixtures[f].size; ++i)
                set8(package+i, resource_fixtures[f].bytes[i]);
            arguments[4] = j;
            if ((error = exercise())) return error;
        }
    }
    for (i = a = 0; i < 3; ++i) {
        PRINT("ICO wrapper "); number(i); PRINT(" cases: "); number(cases[i]); PRINT("\n");
        a += cases[i];
    }
    PRINT("Total cases: "); number(a); PRINT("; executions: "); number(a*3);
    PRINT("; no draw/PNG/DIB: ");
    for (i = 0; i < 3; ++i) { number(branches[i]); PRINT(" "); }
    PRINT("; excluded unmapped entries: "); number(excluded);
    PRINT("; all three ICO wrappers verified against directory/callback/16 KiB memory/ABI oracle\n");
    return 0;
}
void _start(void)
{
    uint32_t error = run();
    if (error) {
        PRINT("FAIL: "); number(error); PRINT(" function "); number(function);
        PRINT(" frame "); number(arguments[4]); PRINT("\n");
    }
    syscall3(1, error, 0, 0); __builtin_unreachable();
}
