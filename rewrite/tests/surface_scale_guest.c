/* Test-only type-0x0a application. Run six bounded scaler calls through the
 * original shared service ABI. It is never installed in production images. */
#include <stdint.h>

#define WORD(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define TABLE ((volatile uint32_t *)(uintptr_t)0x500000u)
#define SERVICE(n) WORD(WORD(0x1b0e4u) + 4u * (n))

struct scale_case {
    uint32_t depth, sw, sh, tw, th, result;
    uint32_t source[64], destination[96];
};
struct scale_report {
    uint32_t magic, status, entry, slots[2], old_slots[2];
    uint32_t high_before, high_after, capacity, flags_before, flags_after, completed;
    uint32_t destination[14], source[14];
    struct scale_case cases[6];
};
volatile struct scale_report scale_report;
_Static_assert(sizeof(struct scale_case) == 664, "guest case layout");
_Static_assert(sizeof(struct scale_report) == 4148, "guest report layout");

void scale_guest_entry(void)
{
    typedef uint32_t (*scale_fn)(uint32_t, uint32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t);
    typedef uint32_t (*poll_fn)(void);
    uint32_t flags, restored, i, j, first, second;
    scale_report.magic = 0x53434c31u;
    scale_report.entry = SERVICE(163);
    for (i = 0; i < 6; ++i) {
        volatile struct scale_case *c = &scale_report.cases[i];
        c->depth = i < 3 ? 8 : 32;
        c->sw = i % 3 == 0 ? 6 : i % 3 == 1 ? 7 : 3;
        c->sh = i % 3 == 0 ? 4 : i % 3 == 1 ? 5 : 2;
        c->tw = i % 3 == 1 ? 4 : 6;
        c->th = i % 3 == 0 ? 4 : i % 3 == 1 ? 3 : 5;
        for (j = 0; j < 64; ++j)
            c->source[j] = 0x9b000000u + i * 0x01010101u + j * 0x00030507u;
        for (j = 0; j < 96; ++j) c->destination[j] = 0xa5a5a5a5u;
    }
    /* These applications execute with the original flat privileged selectors.
     * Keep the borrowed zero slots invisible to interrupts and other tasks. */
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory", "cc");
    scale_report.flags_before = flags;
    scale_report.high_before = WORD(0x1b068u);
    scale_report.capacity = WORD(0x1b064u);
    first = scale_report.high_before;
    second = first + 1u;
    scale_report.slots[0] = first;
    scale_report.slots[1] = second;
    if (first >= scale_report.capacity || second >= scale_report.capacity ||
        first < 2 || TABLE[first] || TABLE[second] || scale_report.entry != 0x8194e4u) {
        scale_report.status = 2;
    } else {
        scale_report.old_slots[0] = TABLE[first];
        scale_report.old_slots[1] = TABLE[second];
        TABLE[first] = (uint32_t)(uintptr_t)scale_report.destination;
        TABLE[second] = (uint32_t)(uintptr_t)scale_report.source;
        scale_report.destination[3] = 12;
        scale_report.destination[4] = 8;
        for (i = 0; i < 6; ++i) {
            volatile struct scale_case *c = &scale_report.cases[i];
            scale_report.destination[0] = (uint32_t)(uintptr_t)c->destination;
            scale_report.destination[9] = c->depth;
            scale_report.source[0] = (uint32_t)(uintptr_t)c->source;
            scale_report.source[3] = c->sw;
            scale_report.source[4] = c->sh;
            scale_report.source[9] = c->depth;
            c->result = ((scale_fn)(uintptr_t)scale_report.entry)(first, second, 2, 1, c->tw, c->th);
            scale_report.completed = i + 1u;
        }
        TABLE[first] = scale_report.old_slots[0];
        TABLE[second] = scale_report.old_slots[1];
        scale_report.status = 1;
    }
    scale_report.high_after = WORD(0x1b068u);
    __asm__ volatile("pushl %1; popfl; pushfl; popl %0"
                     : "=r"(restored) : "r"(flags) : "memory", "cc");
    scale_report.flags_after = restored;
    /* Publish completion only after restoring the shared table and flags. */
    scale_report.magic = 0x53434c32u;
    for (;;) ((poll_fn)(uintptr_t)SERVICE(108))();
}
