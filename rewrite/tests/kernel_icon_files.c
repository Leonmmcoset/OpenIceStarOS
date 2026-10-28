/* Isolated wrappers: filesystem/length/X-header/draw/free callbacks are
 * boundaries. The real X-header getter is checked separately, without stubs.
 * All variants face the same independent bytewise memory and event oracle. */
#include <stdint.h>
#include "../kernel/icon_files.h"
#include "../kernel/x_icon.h"
#define BASE 0x600000u
#define SIZE 8192u
#define PRINT(s) syscall3(4, 1, (uint32_t)(s), sizeof(s)-1)
static uint8_t initial[SIZE], expected[SIZE];
static uint32_t arguments[5], function, name, header, file_value, extracted;
static uint32_t mutation, forced, forced_length, calls, failed, total[3];
static uint32_t event_count, result_expected, events[6][6], routes[2], draws[3];
static const uint32_t addresses[] = {0x812c0e, 0x82b0a0, 0x82b112};
static const uint32_t candidates[] = {(uint32_t)kernel_x_icon_pointer,
    (uint32_t)kernel_icon_draw_file, (uint32_t)kernel_icon_draw_file_auto};
static const uint32_t values[] = {0, 1, 0x600a01, 0x7fffffff,
    0x80000000, 0xffffffff, 0x12345678, 0xffff0000};
#include "../build/icon_files_resources.h"

static int syscall3(int n, uint32_t a, uint32_t b, uint32_t c)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result)
        : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}
static void number(uint32_t value)
{
    char s[10]; uint32_t i = sizeof(s);
    do { s[--i] = '0'+value%10; value /= 10; } while (value);
    syscall3(4, 1, (uint32_t)(s+i), sizeof(s)-i);
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
static uint8_t get(uint32_t address, int model)
{
    if (address-BASE >= SIZE) { failed = 1; return 0; }
    return model ? expected[address-BASE] : *(volatile uint8_t *)address;
}
static void put(uint32_t address, uint32_t byte, int model)
{
    if (address-BASE >= SIZE) { failed = 1; return; }
    if (model) expected[address-BASE] = (uint8_t)byte;
    else *(volatile uint8_t *)address = (uint8_t)byte;
}
static uint32_t length(uint32_t address, int model)
{
    uint32_t n = 0;
    while (n < SIZE && get(address+n, model)) ++n;
    return n;
}
static void text_at(uint32_t address, const char *text, int model)
{
    uint32_t i = 0;
    do { put(address+i, text[i], model); } while (text[i++]);
}
static void set32(uint32_t address, uint32_t value)
{
    uint32_t i;
    for (i = 0; i < 4; ++i) initial[address-BASE+i] = value >> (i*8);
}
static void effect(uint32_t kind, int model)
{
    uint32_t i, n;
    put(BASE+0x100+kind, kind*31+mutation, model);
    if (kind <= 2 && (mutation&1)) text_at(name, "Z:\\CHANGED.X", model);
    if (kind == 3 && (mutation&2)) {
        n = length(name, model);
        if (n >= 4) for (i = 0; i < 4; ++i) put(name+n-4+i, ".ICO"[i], model);
    }
    if ((kind == 4 && (mutation&4)) || (kind == 5 && (mutation&8)) ||
        (kind == 6 && (mutation&16))) {
        for (i = 0; i < 4; ++i) put(header+40+i, kind*17+i, model);
        put(name+3, 0, model);
    }
}
static void event(uint32_t kind, uint32_t a, uint32_t b, uint32_t c,
                   uint32_t d, uint32_t e, int model)
{
    uint32_t actual[] = {kind, a, b, c, d, e}, i;
    if (model) {
        if (event_count >= 6) { failed = 1; return; }
        for (i = 0; i < 6; ++i) events[event_count][i] = actual[i];
        ++event_count;
    } else {
        if (calls >= event_count) { failed = 1; return; }
        for (i = 0; i < 6; ++i)
            if (actual[i] != events[calls][i]) failed = 1;
        ++calls;
    }
    effect(kind, model);
}
uint32_t kernel_icon_read_relative_826f38(uint32_t p)
{ event(1, p, 0, 0, 0, 0, 0); return file_value; }
uint32_t kernel_icon_read_absolute_827798(uint32_t p)
{ event(2, p, 0, 0, 0, 0, 0); return file_value; }
uint32_t kernel_string_length(uint32_t p)
{
    uint32_t n = forced ? forced_length : length(p, 0);
    event(3, p, 0, 0, 0, 0, 0); return n;
}
uint32_t icon_mock_x_pointer(uint32_t p)
{ event(4, p, 0, 0, 0, 0, 0); return extracted; }
void kernel_icon_draw_id(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{ event(5, a, b, c, d, e, 0); }
void kernel_free_allocation(uint32_t p)
{ event(6, p, 0, 0, 0, 0, 0); }

static int suffix(uint32_t n, const char *text, uint32_t count)
{
    uint32_t i;
    if (n <= count) return 0;
    for (i = 0; i < count; ++i)
        if (get(name+n-count+i, 1) != (uint8_t)text[i]) return 0;
    return 1;
}
static void plan(void)
{
    uint32_t i, n, route, icon;
    event_count = 0; failed = 0;
    for (i = 0; i < SIZE; ++i) expected[i] = initial[i];
    if (function == 0) {
        uint32_t value = 0;
        for (i = 0; i < 4; ++i)
            value |= (uint32_t)get(arguments[0]+40+i, 1) << (i*8);
        result_expected = value ? (uint32_t)((uint64_t)arguments[0]+value) : 0;
        return;
    }
    route = get(name+1, 1) == ':' && get(name+2, 1) == '\\' ? 2 : 1;
    ++routes[route-1];
    event(route, name, 0, 0, 0, 0, 1);
    icon = file_value;
    if (function == 2) {
        n = forced ? forced_length : length(name, 1);
        event(3, name, 0, 0, 0, 0, 1);
        icon = 0;
        if (suffix(n, ".ICO", 4)) icon = file_value;
        if (suffix(n, ".X", 2)) {
            event(4, file_value, 0, 0, 0, 0, 1); icon = extracted;
        }
    }
    ++draws[icon == 0 ? 0 : icon == file_value ? 1 : 2];
    event(5, arguments[0], arguments[1], arguments[2], icon, arguments[4], 1);
    if (file_value) event(6, file_value, 0, 0, 0, 0, 1);
    result_expected = icon;
}
static void prepare(uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < SIZE; ++i) initial[i] = (uint8_t)(i*37+seed*83);
    name = BASE+0x501+seed%4; header = BASE+0xa01+(seed/4)%4;
    arguments[0] = function ? values[seed%8] : header;
    arguments[1] = values[(seed/8)%8]; arguments[2] = values[(seed/64)%8];
    arguments[3] = name; arguments[4] = values[(seed/512)%8];
    file_value = values[(seed/8)%8]; extracted = values[(seed/64+3)%8];
    forced = mutation = forced_length = 0;
    for (i = 0; i < 11; ++i) initial[name-BASE+i] = "A:\\ICO.ICO"[i];
    /* Ten visible bytes plus a terminator, including the original lookahead. */
    initial[name-BASE+11] = 0;
}
extern uint32_t icon_files_return;
uint32_t icon_files_checked_call(uint32_t target, const uint32_t *arguments);
static int exercise(void)
{
    uint32_t i, implementation;
    plan(); if (failed) return 3;
    for (implementation = 0; implementation < 3; ++implementation) {
        uint32_t target = implementation == 1 ? candidates[function] :
            addresses[function]+(implementation == 0 ? 0x100000 : 0);
        for (i = 0; i < SIZE; ++i) ((volatile uint8_t *)BASE)[i] = initial[i];
        calls = failed = 0;
        if (icon_files_checked_call(target, arguments)) return 10+implementation;
        if (function != 1 && icon_files_return != result_expected) return 20+implementation;
        if (failed || calls != event_count) return 30+implementation;
        for (i = 0; i < SIZE; ++i)
            if (((volatile uint8_t *)BASE)[i] != expected[i]) return 40+implementation;
    }
    ++total[function]; return 0;
}
static int run(void)
{
    uint32_t a, b, i, j, k, error;
    const char *names[] = {"", "A", ".X", "A.X", ".ICO", "A.ICO", "a.ico",
        "a.x", "A:/B.X", "0:\\B.ICO", "Q\\B.X", "A.ICO.X", "A.X.ICO"};
    const uint32_t lengths[] = {0, 1, 2, 3, 4, 5, 10, 11, 0xfffffffd,
        0xfffffffe, 0xffffffff};
    if (!map(BASE, SIZE) || !map(0x800000, 0x40000) || !map(0x900000, 0x40000) ||
        !load("build/icon_files_original.x", 0x900000)) return 1;
#ifdef TEST_INSTALLED
    if (!load("build/icestar_c.x", 0x800000)) return 2;
#else
    if (!load("build/icon_files_preview.x", 0x800000)) return 2;
#endif
    {
#include "../build/icon_files_calls.h"
    }
    /* All low/high WORD offsets, four header alignments and wrapped sums. */
    function = 0;
    for (i = 0; i < 65536; ++i) for (j = 0; j < 2; ++j) {
        prepare(i*2+j); set32(header+40, i+(j ? 0xffff0000u : 0));
        if ((error = exercise())) return error;
    }
    for (i = 0; i < 8; ++i) {
        prepare(i); set32(header+40, values[i]);
        if ((error = exercise())) return error;
        set32(header+40, 0-header); /* Nonzero offset can produce null. */
        if ((error = exercise())) return error;
    }
    for (i = 0; i < sizeof(resource_headers)/sizeof(*resource_headers); ++i) {
        prepare(i);
        for (j = 0; j < 64; ++j) initial[header-BASE+j] = resource_headers[i][j];
        if ((error = exercise())) return error;
    }
    /* Exhaust the two lookahead bytes through both file APIs, including
     * embedded NULs and prefixes that are not ASCII drive letters. */
    for (function = 1; function < 3; ++function)
    for (a = 0; a < 256; ++a) for (b = 0; b < 256; ++b) {
        prepare(a*256+b); initial[name-BASE+1] = a; initial[name-BASE+2] = b;
        mutation = (a+b)%32;
        if ((error = exercise())) return error;
    }
    for (function = 1; function < 3; ++function) for (a = 0; a < 256; ++a) {
        prepare(a); initial[name-BASE] = a;
        if ((error = exercise())) return error;
    }
    function = 2;
    /* Every possible final two bytes; then each ICO suffix byte individually.
     * Embedded NULs deliberately change the actual length. */
    for (a = 0; a < 256; ++a) for (b = 0; b < 256; ++b) {
        prepare(a*256+b); initial[name-BASE+8] = a; initial[name-BASE+9] = b;
        if ((error = exercise())) return error;
    }
    for (i = 0; i < 4; ++i) for (a = 0; a < 256; ++a) {
        prepare(i*256+a); initial[name-BASE+6+i] = a;
        if ((error = exercise())) return error;
    }
    /* Callback mutations, null/opaque loaded buffers and arbitrary extractor
     * returns; fake lengths exercise unsigned underflow and pointer wrap. */
    for (function = 1; function < 3; ++function)
    for (i = 0; i < 13; ++i) for (j = 0; j < 32; ++j)
    for (k = 0; k < 8; ++k) {
        uint32_t n;
        prepare(i*256+j*8+k); mutation = j;
        file_value = values[k]; extracted = values[(k+j)%8];
        for (n = 0; names[i][n]; ++n) initial[name-BASE+n] = names[i][n];
        initial[name-BASE+n] = 0;
        if ((error = exercise())) return error;
    }
    function = 2;
    for (i = 0; i < 11; ++i) for (j = 0; j < 32; ++j) {
        prepare(i*32+j); forced = 1; forced_length = lengths[i]; mutation = j;
        if ((error = exercise())) return error;
    }
    for (i = 0; i < 3; ++i) {
        PRINT("Icon file API "); number(i); PRINT(" cases: "); number(total[i]); PRINT("\n");
    }
    PRINT("Total cases: "); number(total[0]+total[1]+total[2]);
    PRINT("; executions: "); number((total[0]+total[1]+total[2])*3);
    PRINT("; relative/absolute routes: "); number(routes[0]); PRINT(" "); number(routes[1]);
    PRINT("; null/file/extracted draws: ");
    for (i = 0; i < 3; ++i) { number(draws[i]); PRINT(" "); }
    PRINT("; all three file icon APIs verified against return/event/8 KiB memory/ABI oracle\n");
    return 0;
}
void _start(void)
{
    uint32_t error = run();
    if (error) {
        PRINT("FAIL: "); number(error); PRINT(" function "); number(function);
        PRINT(" cases "); number(total[function]); PRINT("\n");
    }
    syscall3(1, error, 0, 0); __builtin_unreachable();
}
