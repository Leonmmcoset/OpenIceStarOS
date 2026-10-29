/* Test-only X application: exercise the six production FAT service entries
 * against INIT.X using real disk I/O. Never installed in production images. */
#include <stdint.h>
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define SERVICE(n) D(D(0x1b0e4u) + (n) * 4u)
#define BYTES 32768u

struct fat_case {
    uint32_t entry, result, allocation_freed;
    uint8_t data[BYTES + 32u];
};
struct fat_report {
    uint32_t magic, status, index, completed;
    uint32_t entries[6];
    struct fat_case cases[10];
};
volatile struct fat_report fat_report;
_Static_assert(sizeof(struct fat_case) == 32812, "FAT guest case layout");
_Static_assert(sizeof(struct fat_report) == 328160, "FAT guest report layout");

void fat_guest_entry(void)
{
    typedef uint32_t (*read_fn)(uint32_t, uint32_t);
    typedef uint32_t (*cluster_fn)(uint32_t, uint32_t, int32_t);
    typedef uint32_t (*one_fn)(uint32_t);
    typedef uint32_t (*poll_fn)(void);
    static const char name[] = "INIT.X";
    static const uint32_t expected[] = {0x826ade, 0x826c3a, 0x826c6e,
                                        0x826dd0, 0x826f38, 0x827032};
    uint32_t index, i, j;
    fat_report.magic = 0x46415431u;
    for (i = 0; i < 6; ++i) {
        fat_report.entries[i] = SERVICE(89u + i);
        if (fat_report.entries[i] != expected[i]) goto failed;
    }
    index = ((one_fn)(uintptr_t)SERVICE(122))((uint32_t)(uintptr_t)name);
    fat_report.index = index;
    /* Bound every write for this exact image: four 8192-byte clusters. */
    if (index != 1 || *(volatile uint16_t *)0x52000b != 512 ||
        *(volatile uint8_t *)0x52000d != 16) goto failed;
    for (i = 0; i < 10; ++i) {
        volatile struct fat_case *c = &fat_report.cases[i];
        uint32_t target = (uint32_t)(uintptr_t)(c->data + 16u);
        for (j = 0; j < BYTES + 32u; ++j) c->data[j] = 0xa5u;
        c->entry = fat_report.entries[i < 6 ? i : i % 2u + 2u];
        if (i < 2) {
            c->result = ((read_fn)(uintptr_t)c->entry)(
                i == 0 ? index : (uint32_t)(uintptr_t)name, target);
        } else if (i == 4 || i == 5) {
            uint32_t allocation = ((one_fn)(uintptr_t)c->entry)(
                i == 4 ? (uint32_t)(uintptr_t)name : index);
            c->result = allocation;
            if (!allocation) goto failed;
            for (j = 0; j < BYTES; ++j)
                c->data[16u + j] = *(volatile uint8_t *)(uintptr_t)(allocation + j);
            ((one_fn)(uintptr_t)SERVICE(121))(allocation);
            c->allocation_freed = 1;
        } else {
            int32_t skip = i < 4 ? 2 : i < 8 ? 4 : -1;
            c->result = ((cluster_fn)(uintptr_t)c->entry)(
                i % 2u == 0 ? index : (uint32_t)(uintptr_t)name, target, skip);
        }
        fat_report.completed = i + 1u;
    }
    fat_report.status = 1;
    goto finished;
failed:
    fat_report.status = 2;
finished:
    fat_report.magic = 0x46415432u;
    for (;;) ((poll_fn)(uintptr_t)SERVICE(108))();
}
