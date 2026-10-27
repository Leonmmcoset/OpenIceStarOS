#include <stdint.h>
#include "../apps/snake.h"
#include "snake_bindings.h"

enum { MAX_CALLS = 256, STATE_SIZE = 0x480 };
struct call { uint32_t slot, count, args[8], events[20]; char text[64]; };
static struct call calls[MAX_CALLS], expected_calls[MAX_CALLS];
static unsigned call_count, expected_count, random_count, tick_count, case_count;
static unsigned mode, entry_mode, iteration;
static uint32_t original_base, rewritten_base, active_base;
static uint8_t expected_state[STATE_SIZE];
static uint32_t expected_length, random_x, random_y, window_result;
static volatile uint32_t *events;
static volatile struct snake_state *state;
static unsigned failure_byte, failure_call;
static volatile uint32_t saved_sp __attribute__((used));
static volatile uint32_t saved_bp __attribute__((used));
static volatile uint32_t resume_pc __attribute__((used));
static const uint32_t values[] = {0, 1, 9, 10, 19, 20, 29, 30, 0x7fffffff,
                                  0x80000000, 0xfffffffe, 0xffffffff};
static const uint32_t lengths[] = {0, 1, 2, 3, 16, 99, 100, 101, 104, 108,
                                   0xffffffff, 0x80000001};
static const uint32_t directions[] = {0, 1, 2, 3, 4, 0xffffffff, 0x80000000};
static const uint32_t over_values[] = {0, 1, 0xffffffff, 0x7fffffff};

static int syscall3(int number, unsigned a, unsigned b, unsigned c)
{
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result)
                      : "a"(number), "b"(a), "c"(b), "d"(c) : "memory");
    return result;
}

static void print(const char *text)
{
    unsigned size = 0;
    while (text[size]) ++size;
    syscall3(4, 1, (unsigned)text, size);
}

static void number(uint32_t value)
{
    char buffer[12];
    unsigned i = sizeof(buffer);
    buffer[--i] = 0;
    do { buffer[--i] = (char)('0' + value % 10); value /= 10; } while (value);
    print(buffer + i);
}

static int map_memory(uint32_t address, unsigned size)
{
    uint32_t args[] = {address, size, 7, 0x32, 0xffffffff, 0};
    return syscall3(90, (unsigned)args, 0, 0) == (int)address;
}

static int load(const char *path, uint32_t base)
{
    unsigned i;
    uint32_t *header = (uint32_t *)base;
    uint8_t *memory = (uint8_t *)base;
    int fd = syscall3(5, (unsigned)path, 0, 0), count;
    if (fd < 0) return 0;
    count = syscall3(3, (unsigned)fd, base, 0xb24);
    syscall3(6, (unsigned)fd, 0, 0);
    if (count != 0xb24 || header[1] != 0xb24) return 0;
    for (i = 0; i < header[8]; ++i) {
        uint32_t offset = *(uint32_t *)(base + header[9] + i * 4);
        *(uint32_t *)(base + offset) += base;
    }
    header[7] = base;
    for (i = header[9]; i < header[9] + header[8] * 4 + header[11] + header[3] * 4096; ++i)
        memory[i] = 0;
    return 1;
}

/* Permit terminating the intentionally infinite application entry loop from
 * a test syscall. Restore the harness frame and all callee-saved registers. */
__attribute__((naked, noinline)) static uint32_t enter(uint32_t function __attribute__((unused)))
{
    __asm__ volatile ("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
                      "movl %esp,saved_sp\n\tmovl %ebp,saved_bp\n\tmovl $1f,resume_pc\n\t"
                      "call *8(%ebp)\n\t1: popl %edi\n\tpopl %esi\n\tpopl %ebx\n\tleave\n\tret");
    __builtin_unreachable();
}

__attribute__((naked, noreturn)) static void finish(void)
{
    __asm__ volatile ("movl saved_sp,%esp\n\tmovl saved_bp,%ebp\n\t"
                      "movl resume_pc,%edx\n\txorl %eax,%eax\n\tjmp *%edx");
    __builtin_unreachable();
}

static struct call *record(unsigned slot, unsigned count, const uint32_t *args)
{
    unsigned i;
    struct call *call;
    if (call_count == MAX_CALLS) { print("trace overflow\n"); syscall3(1, 200, 0, 0); }
    call = &calls[call_count++];
    for (i = 0; i < sizeof(*call); ++i) ((uint8_t *)call)[i] = 0;
    call->slot = slot;
    call->count = count;
    for (i = 0; i < count; ++i) call->args[i] = args[i];
    if (events) for (i = 0; i < 20; ++i) call->events[i] = events[i];
    return call;
}

static void record_text(struct call *call, const char *text)
{
    unsigned i;
    for (i = 0; i + 1 < sizeof(call->text) && text[i]; ++i) call->text[i] = text[i];
}

static uint32_t rectangle(uint32_t s, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t c)
{
    const uint32_t args[] = {s, x1, y1, x2, y2, c};
    record(0x4002, 6, args);
    if (mode) {
        state->surface ^= 0x31415927;
        state->background ^= 0xa55a32dc;
        state->body_color += 3;
        state->food_color += 5;
    }
    return 0xface1234;
}

static uint32_t text(uint32_t s, uint32_t x, uint32_t y, uint32_t f, uint32_t b, const char *string)
{
    const uint32_t args[] = {s, x, y, f, b, 0};
    record_text(record(0x4009, 6, args), string);
    return 0x89abcdef;
}

static uint32_t create(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       uint32_t flags, uint32_t c, uint32_t style, const char *title)
{
    const uint32_t args[] = {x, y, w, h, flags, c, style, 0};
    record_text(record(0x4100, 8, args), title);
    return window_result;
}

static uint32_t show(uint32_t window)
{
    record(0x400d, 1, &window);
    return 0xdeadbeef;
}

static uint32_t update(uint32_t window)
{
    record(0x4010, 1, &window);
    if (entry_mode && tick_count == 5) {
        state->game_over = 1;
        events[15] = 1;
        events[18] = 'r';
    }
    return 0x2468ace0;
}

static uint32_t close_handle(uint32_t handle)
{
    record(0x4011, 1, &handle);
    return 0x13579bdf;
}

static uint32_t register_events(uint32_t window, volatile uint32_t *pointer)
{
    const uint32_t args[] = {window, 0};
    events = pointer;
    record(0x4012, 2, args);
    return 1;
}

static uint32_t get_handle(uint32_t window)
{
    record(0x4101, 1, &window);
    return 0x87654321;
}

static uint32_t get_surface(uint32_t window)
{
    record(0x4102, 1, &window);
    return 0x1234a011;
}

static uint32_t color(uint32_t index)
{
    record(0x4103, 1, &index);
    return 0xff000000 + index * 0x123456;
}

static uint32_t random_word(uint32_t low, uint32_t high)
{
    const uint32_t args[] = {low, high};
    record(0x4104, 2, args);
    if (mode && random_count & 1) state->food_x = 0xff00aabb;
    return random_count++ & 1 ? random_y : random_x;
}

static uint32_t ticks(void)
{
    static const uint32_t sequence[] = {0xffffffe0, 0x12, 0x13, 0x13, 0x7fffffff,
                                        0x80000013, 0x80000046, 0x80000079, 0x800000ac};
    static const char keys[] = "dsdwadsdw";
    record(0x4105, 0, 0);
    if (tick_count == sizeof(sequence) / sizeof(sequence[0])) finish();
    if (events) {
        events[2] = 1;
        events[15] = 1;
        events[17] = 0x4d;
        events[18] = keys[tick_count];
    }
    return sequence[tick_count++];
}

static void patch_apis(uint32_t base, const unsigned *offsets)
{
    const uint32_t callbacks[] = {(uint32_t)rectangle, (uint32_t)text, (uint32_t)create,
        (uint32_t)show, (uint32_t)update, (uint32_t)close_handle, (uint32_t)register_events,
        (uint32_t)get_handle, (uint32_t)get_surface, (uint32_t)color, (uint32_t)random_word, (uint32_t)ticks};
    unsigned i;
    for (i = 0; i < sizeof(callbacks) / sizeof(callbacks[0]); ++i) {
        uint8_t *code = (uint8_t *)(base + offsets[i]);
        code[0] = 0xb8;
        *(uint32_t *)(code + 1) = callbacks[i];
        code[5] = 0xff;
        code[6] = 0xe0;
    }
}

static void initialize(uint32_t base, unsigned length, uint32_t x, uint32_t y,
                        uint32_t direction, uint32_t over, uint32_t score, unsigned eat, unsigned collide)
{
    unsigned i;
    uint8_t *memory = (uint8_t *)(base + 0x980);
    state = (volatile struct snake_state *)memory;
    active_base = base;
    for (i = 0; i < STATE_SIZE; ++i) memory[i] = (uint8_t)(i * 37 + 83 + mode * 59);
    for (i = 0; i < 100; ++i) {
        state->x[i] = (i * 7) % 30;
        state->y[i] = (i * 3) % 20;
    }
    *(volatile uint32_t *)(base + 0x92c) = length;
    state->x[0] = x; state->y[0] = y;
    state->direction = direction;
    state->game_over = over;
    state->score = score;
    state->food_x = 0x44; state->food_y = 0x33;
    state->surface = 0x1234a011; state->window = 0x11112222;
    state->background = 0x12345678; state->body_color = 0xfedcba98;
    state->error_color = 0x99aabbcc; state->food_color = 0xdeadbeef; state->text_color = 0xabcdef01;
    state->last_tick = 0x80000000; state->current_tick = 0x7fffffff;
    if (direction == 0) ++x;
    else if (direction == 1) ++y;
    else if (direction == 2) --x;
    else if (direction == 3) --y;
    if (eat) { state->food_x = x; state->food_y = y; }
    /* After the shift, old element 1 becomes element 2. */
    if (collide) { state->x[1] = x; state->y[1] = y; }
    events = 0;
    call_count = random_count = tick_count = 0;
}

static void save(void)
{
    unsigned i;
    expected_length = *(uint32_t *)(active_base + 0x92c);
    for (i = 0; i < STATE_SIZE; ++i) expected_state[i] = ((volatile uint8_t *)state)[i];
    expected_count = call_count;
    for (i = 0; i < call_count; ++i) expected_calls[i] = calls[i];
}

static int compare(void)
{
    unsigned i;
    if (expected_length != *(uint32_t *)(active_base + 0x92c)) { failure_byte = 0x92c; return 0; }
    for (i = 0; i < STATE_SIZE; ++i)
        if (expected_state[i] != ((volatile uint8_t *)state)[i]) { failure_byte = 0x980 + i; return 0; }
    if (expected_count != call_count) { failure_call = 1000 + call_count; return 0; }
    for (i = 0; i < call_count * sizeof(struct call); ++i)
        if (((uint8_t *)expected_calls)[i] != ((uint8_t *)calls)[i]) { failure_call = i + 1; return 0; }
    return 1;
}

static int exercise(unsigned function, unsigned length, uint32_t x, uint32_t y, uint32_t direction,
                    uint32_t over, uint32_t score, unsigned eat, unsigned collide)
{
    unsigned copy;
    uint32_t result = 0, expected_result = 0;
    for (copy = 0; copy < 2; ++copy) {
        uint32_t base = copy ? rewritten_base : original_base;
        const unsigned *offsets = copy ? rewritten_functions : original_functions;
        typedef uint32_t (*routine)(uint32_t, uint32_t, uint32_t);
        initialize(base, length, x, y, direction, over, score, eat, collide);
        if (function == 6) result = enter(base + offsets[function]);
        else result = ((routine)(base + offsets[function]))(x, y, score);
        if (!copy) { save(); expected_result = result; }
        else if (!compare() || (function == 6 && expected_result != result)) return 0;
    }
    ++case_count;
    return 1;
}

static int run_test(void)
{
    unsigned pass, a, b, c, d, e;
    if (!map_memory(0x1000000, 0x20000) || !map_memory(0x1200000, 0x20000)) return 1;
    for (pass = 0; pass < 2; ++pass) {
        original_base = 0x1000000 + (pass ? 0x113 : 0);
        rewritten_base = 0x1200000 + (pass ? 0x513 : 0);
        if (!load("build/snake.original.x", original_base) || !load("build/snake.x", rewritten_base)) return 2;
        patch_apis(original_base, original_apis);
        patch_apis(rewritten_base, rewritten_apis);
        for (mode = 0; mode < 2; ++mode) {
            entry_mode = 0;
            random_x = 23; random_y = 17;
            for (a = 0; a < 12; ++a)
                for (b = 0; b < 12; ++b) {
                    if (!exercise(0, 1, values[a], values[b], 0, 0, values[(a + b) % 12], 0, 0)) return 10;
                    random_x = values[a]; random_y = values[b];
                    if (!exercise(1, 1, 1, 2, 3, 4, 5, 0, 0)) return 11;
                }
            if (!exercise(2, 108, 0xffffffff, 0x80000000, 3, 1, 0xffffffff, 0, 0)) return 12;
            random_x = 23; random_y = 17;
            for (a = 0; a < 12; ++a)
                for (b = 0; b < 12; ++b)
                    for (c = 0; c < 7; ++c)
                        for (d = 0; d < 12; ++d)
                            for (e = 0; e < 4; ++e)
                                if (!exercise(3, lengths[d], values[a], values[b], directions[c],
                                              over_values[e], values[(b + d) % 12], e & 1, e & 2)) return 13;
            for (a = 0; a < 12; ++a)
                for (b = 0; b < 12; ++b)
                    for (c = 0; c < 4; ++c)
                        if (!exercise(4, lengths[a], 15, 10, 0, over_values[c], values[b], 0, 0)) return 14;
            for (a = 0; a < 256; ++a)
                for (b = 0; b < 7; ++b)
                    for (c = 0; c < 4; ++c) {
                        if (!exercise(5, 3, a, 0, directions[b], over_values[c], 0, 0, 0)) return 15;
                        if (!exercise(5, 3, 0x4d, a, directions[b], over_values[c], 0, 0, 0)) return 16;
                    }
            for (a = 0; a < 12; ++a)
                for (b = 0; b < 12; ++b)
                    for (c = 0; c < 7; ++c)
                        if (!exercise(5, 3, values[a], values[b], directions[c], 1, 0, 0, 0)) return 17;
            for (a = 0; a < 4; ++a) {
                static const uint32_t failures[] = {0, 0xffffffff, 0x80000000, 0x80000001};
                window_result = failures[a];
                if (!exercise(6, 1, 15, 10, 0, 0, 0, 0, 0)) return 18;
            }
            entry_mode = 1;
            window_result = 0x12345678;
            if (!exercise(6, 1, 15, 10, 0, 0, 0, 0, 0)) return 19;
        }
        iteration = pass;
    }
    return 0;
}

void _start(void)
{
    unsigned result = (unsigned)run_test();
    if (result) {
        print("Snake differential failure code="); number(result);
        print(" case="); number(case_count);
        print(" pass="); number(iteration);
        print(" mode="); number(mode);
        print(" memory_offset="); number(failure_byte);
        print(" trace_byte="); number(failure_call); print("\n");
    } else {
        print("Snake original/C differential cases: "); number(case_count); print("\n");
    }
    syscall3(1, result, 0, 0);
    __builtin_unreachable();
}
