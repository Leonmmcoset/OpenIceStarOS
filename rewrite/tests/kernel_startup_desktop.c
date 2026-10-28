/* Compare original x86, ordinary C and fixed-address C with a bytewise
 * independent oracle. Boundary CALLs alone are redirected in fixture copies. */
#include <stdint.h>
#include "../kernel/startup_desktop.h"
#define LOW 0x1a000u
#define HIGH 0x4ff000u
#define SPAN 0x6000u
#define TOTAL (2u * SPAN)
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint8_t expected[TOTAL];
struct event { uint32_t kind, count, args[8]; };
static struct event trace[10];
static uint32_t trace_count, cursor, failure, cases;
static uint32_t pattern, layout, statuses, result_index, extension_index, mutation;
static uint32_t display, colors;
static const uint32_t results[] = {0, 1, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff};
static const uint8_t extensions[][4] = {
    {'J','P','G',0}, {'P','N','G',0}, {'j','p','g',0}, {'J','P','x',0},
    {'J','x','G',0}, {'P','N','x',0}, {'P','x','G',0}, {'J','P','G','X'}
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
static uint32_t address(uint32_t index)
{
    return index < SPAN ? LOW + index : HIGH + index - SPAN;
}
static uint32_t offset(uint32_t a)
{
    if (a >= LOW && a < LOW + SPAN) return a - LOW;
    if (a >= HIGH && a < HIGH + SPAN) return SPAN + a - HIGH;
    failure = 90; return 0;
}
static uint32_t get(uint32_t a)
{
    uint32_t i, v = 0;
    for (i = 0; i < 4; ++i) v |= (uint32_t)expected[offset(a + i)] << (i * 8);
    return v;
}
static void put(uint32_t a, uint32_t value, uint32_t model)
{
    uint32_t i;
    if (model) for (i = 0; i < 4; ++i) expected[offset(a + i)] = (uint8_t)(value >> (i * 8));
    else DWORD(a) = value;
}
static uint32_t bg_target(uint32_t e)
{
    if (layout == 5) return 0x4ffffc;
    if (layout == 6) return 0x500000;
    if (layout == 9) return display + 4;
    return 0x501001 + (e & 1u) * 0x80u;
}
static uint32_t desktop_target(uint32_t e)
{
    if (layout == 7) return 0x500000;
    if (layout == 8) return colors + 0x3a0u - 12u;
    if (layout == 9) return display - 4;
    return 0x501401 + (e & 1u) * 0x80u;
}
static uint32_t value(uint32_t e)
{
    return results[result_index] ^ (0x13579bdu * (e + 1u)) ^ (pattern * 0x1010101u);
}
static void mutate(uint32_t e, uint32_t model)
{
    uint32_t v = value(e);
    if (mutation & 1u) {
        put(0x1b034, 0x1cf00 + e * 4u, model);
        put(0x1b0e0, 0x1df00 + e * 4u, model);
        put(colors + 0x224u, v, model);
        put(colors + 0x3a0u, ~v, model);
        put(0x1b154, v + e, model);
        put(0x1b100, (e + result_index) & 1u ? v | 1u : 0, model);
    }
    if (mutation & 2u) {
        put(0x500008, bg_target(e), model);
        put(0x50000c, desktop_target(e), model);
        put(display + 12u, 0x501200u + e * 4u, model);
        put(display + 16u, 0x501300u + e * 4u, model);
    }
    put(0x1ba01 + e * 7u, v, model);
}
static uint32_t creation_result(uint32_t desktop)
{
    uint32_t v = results[result_index];
    return statuses & (1u << desktop) ? v | 0x80000000u : v & 0x7fffffffu;
}
static void record(uint32_t kind, uint32_t count, const uint32_t *args)
{
    uint32_t i, e = trace_count++;
    if (e >= COUNT(trace)) { failure = 91; return; }
    trace[e].kind = kind; trace[e].count = count;
    for (i = 0; i < count; ++i) trace[e].args[i] = args[i];
    mutate(e, 1);
}
static uint32_t callback(uint32_t kind, uint32_t count, const uint32_t *args)
{
    uint32_t i, e = cursor++;
    if (e >= trace_count) { failure = 1; return 0; }
    if (trace[e].kind != kind || trace[e].count != count) failure = 2;
    for (i = 0; i < count; ++i) if (args[i] != trace[e].args[i]) failure = 3;
    mutate(e, 0);
    return value(e);
}
uint32_t kernel_syscall_target_807a0a(uint32_t a, uint32_t b, uint32_t c, uint32_t d,
                                    uint32_t e, uint32_t f, uint32_t g, uint32_t h)
{
    uint32_t args[] = {a,b,c,d,e,f,g,h}, desktop = cursor;
    (void)callback(0, 8, args);
    return creation_result(desktop);
}
uint32_t kernel_startup_lookup_8263c0(uint32_t name, uint32_t *metadata)
{
    uint32_t i;
    (void)callback(1, 1, &name);
    for (i = 0; i < 8; ++i) metadata[i] = value(i);
    for (i = 0; i < 4; ++i) ((uint8_t *)metadata)[8 + i] = extensions[extension_index][i];
    metadata[7] = value(7);
    return results[result_index];
}
uint32_t kernel_startup_jpeg_823120(uint32_t a, uint32_t b, uint32_t c)
{ uint32_t args[] = {a,b,c}; return callback(2, 3, args); }
uint32_t kernel_startup_png_82443e(uint32_t a, uint32_t b, uint32_t c)
{ uint32_t args[] = {a,b,c}; return callback(3, 3, args); }
uint32_t kernel_startup_activate_810520(uint32_t a, uint32_t b)
{ uint32_t args[] = {a,b}; return callback(4, 2, args); }
uint32_t kernel_syscall_target_8082c6(uint32_t a) { return callback(5, 1, &a); }
uint32_t kernel_surface_order_front(uint32_t a) { return callback(6, 1, &a); }

struct call_patch { uint32_t offset, target; };
#include "../build/startup_desktop_calls.h"
static void redirect(uint32_t base, const struct call_patch *patches, uint32_t count)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        uint32_t a = base + patches[i].offset;
        DWORD(a + 1) = patches[i].target - a - 5;
    }
}
static void initialize(void)
{
    uint32_t i;
    static const uint32_t displays[] = {0x1c000,0x1c001,0x1cffa,0x502001,0x1b034};
    static const uint32_t tables[] = {0x1d000,0x1d001,0x1dfff,0x502501,0x1b000};
    display = displays[layout % COUNT(displays)];
    colors = tables[layout % COUNT(tables)];
    cursor = failure = 0;
    for (i = 0; i < TOTAL; ++i)
        *(volatile uint8_t *)(uintptr_t)address(i) = (uint8_t)(i * 37 + pattern * 83);
    DWORD(0x1b034) = display; DWORD(0x1b0e0) = colors;
    DWORD(display + 12u) = layout >= 5 ? 0x501200u : results[result_index];
    DWORD(display + 16u) = layout >= 5 ? 0x501300u : ~results[result_index];
    DWORD(colors + 0x224u) = value(20); DWORD(colors + 0x3a0u) = value(21);
    DWORD(0x500008) = bg_target(0); DWORD(0x50000c) = desktop_target(0);
    DWORD(0x1b154) = value(22);
    DWORD(0x1b100) = pattern ? 0x501c01u : 0;
}
static void dimensions(uint32_t slot, uint32_t captured_display)
{
    uint32_t a = get(slot), v = get(captured_display + 12u);
    put(a + 12u, v, 1);
    a = get(slot); v = get(captured_display + 16u);
    put(a + 16u, v, 1);
}
static void oracle(void)
{
    uint32_t i, args[8], captured_display, captured_colors, background, desktop, a, v;
    initialize(); trace_count = 0;
    for (i = 0; i < TOTAL; ++i) expected[i] = *(volatile uint8_t *)(uintptr_t)address(i);
    captured_display = get(0x1b034); captured_colors = get(0x1b0e0);
    args[0] = args[1] = args[4] = args[6] = 0; args[2] = 2560; args[3] = 1600;
    args[5] = get(captured_colors + 0x224u); args[7] = 0x83cd4c;
    record(0,8,args); background = creation_result(0);
    args[5] = get(captured_colors + 0x3a0u); args[7] = 0x83cd55;
    record(0,8,args); desktop = creation_result(1);
    if (!(background & 0x80000000u)) {
        args[0] = get(0x1b154); record(1,1,args);
        dimensions(0x500008, captured_display);
        v = results[result_index];
        if (v && !(v & 0x80000000u) && get(0x1b100)) {
            uint32_t kind = extension_index == 0 || extension_index == 7 ? 2u :
                            extension_index == 1 ? 3u : 0u;
            if (kind) {
                args[0] = 2; args[1] = get(0x1b100); args[2] = value(7);
                record(kind,3,args);
            }
        }
        args[0] = background; args[1] = 0; record(4,2,args);
        args[0] = background; record(5,1,args);
    }
    if (!(desktop & 0x80000000u)) {
        dimensions(0x50000c, captured_display);
        a = get(0x50000c); v = get(captured_colors + 0x3a0u); put(a + 40u,v,1);
        args[0] = desktop; args[1] = 0; record(4,2,args);
        args[0] = desktop; record(5,1,args);
    }
    args[0] = 1; record(6,1,args);
}
uint32_t entry_checked_call(void (*function)(void));
static int exercise(void)
{
    uint32_t implementation, i;
    oracle(); if (failure) return 4;
    for (implementation = 0; implementation < 3; ++implementation) {
        void (*function)(void) = implementation == 1 ? kernel_startup_desktop :
            (void (*)(void))(0x810d4a + (implementation == 0 ? 0x100000 : 0));
        initialize();
        if (entry_checked_call(function)) return 10 + implementation;
        if (failure || cursor != trace_count) return 20 + implementation;
        for (i = 0; i < TOTAL; ++i)
            if (*(volatile uint8_t *)(uintptr_t)address(i) != expected[i]) return 30 + implementation;
    }
    ++cases; return 0;
}
static void number(uint32_t v)
{
    char digits[10]; uint32_t i = sizeof(digits);
    do { digits[--i] = '0' + v % 10; v /= 10; } while (v);
    syscall3(4,1,(uint32_t)(digits+i),sizeof(digits)-i);
}
#define PRINT(s) syscall3(4,1,(uint32_t)(s),sizeof(s)-1)
static int run(void)
{
    uint32_t error;
    if (!map(LOW,SPAN) || !map(HIGH,SPAN) || !map(0x900000,0x40000) || !map(0x800000,0x40000)) return 1;
    if (!load("build/startup_desktop_original.x",0x900000)) return 2;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x",0x800000)) return 3;
#else
    if (!load("build/startup_desktop_preview.x",0x800000)) return 3;
#endif
    redirect(0x900000,original_calls,COUNT(original_calls));
    redirect(0x800000,candidate_calls,COUNT(candidate_calls));
    for (pattern=0; pattern<2; ++pattern)
    for (layout=0; layout<10; ++layout)
    for (statuses=0; statuses<4; ++statuses)
    for (result_index=0; result_index<COUNT(results); ++result_index)
    for (extension_index=0; extension_index<COUNT(extensions); ++extension_index)
    for (mutation=0; mutation<4; ++mutation) {
        if ((error=exercise())) {
            PRINT("FAIL "); number(error); PRINT(" case "); number(cases);
            PRINT(" pattern/layout/status/result/ext/mutation ");
            number(pattern); PRINT("/"); number(layout); PRINT("/"); number(statuses);
            PRINT("/"); number(result_index); PRINT("/"); number(extension_index);
            PRINT("/"); number(mutation); PRINT("\n"); return error;
        }
    }
    if (cases != 15360) return 40;
    PRINT("Startup desktop original/C/fixed-address/oracle cases: "); number(cases);
    PRINT("; 46080 executions; creation failures, JPG/PNG dispatch, live slots, aliases, callback mutations, 48 KiB memory and ABI verified\n");
    return 0;
}
void _start(void) { syscall3(1,run(),0,0); __builtin_unreachable(); }
