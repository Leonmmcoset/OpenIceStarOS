/* Test-only X application: exercise eight reader and seventeen directory services
 * against INIT.X and root/nested directories using real disk I/O.
 * Never installed in production images. */
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
    uint32_t entries[8];
    struct fat_case cases[16];
};
volatile struct fat_report fat_report;
struct directory_case {
    uint32_t entry, result;
    uint8_t output[64]; /* 16-byte guards around the 32-byte result. */
};
struct directory_report {
    uint32_t magic, status, completed;
    uint32_t entries[17];
    struct directory_case cases[21];
};
volatile struct directory_report fat_directory_report;
static uint8_t copied_directory[160];
_Static_assert(sizeof(struct fat_case) == 32812, "FAT guest case layout");
_Static_assert(sizeof(struct fat_report) == 525040, "FAT guest report layout");
_Static_assert(sizeof(struct directory_report) == 1592, "directory guest report layout");

static uint32_t check_directory_services(uint32_t index, uint32_t directory)
{
    typedef uint32_t (*one_fn)(uint32_t);
    typedef uint32_t (*two_fn)(uint32_t, uint32_t);
    typedef uint32_t (*three_fn)(uint32_t, uint32_t, uint32_t);
    static const char name[] = "INIT.X";
    static const char directory_name[] = "ICESTAR";
    static const char *const paths[] = {"C:\\ICESTAR\\GAMES", "C:\\ICESTAR",
        "C:\\ICESTAR\\MISSING", "C:\\", "C:\\\\ICESTAR"};
    static const uint32_t slots[] = {114, 119, 207, 208, 122, 123, 201, 202, 115, 120,
        203, 204, 205, 206, 124, 125, 117};
    static const uint32_t entries[] = {0x8258a6, 0x825a7e, 0x825bcc, 0x825c46,
        0x825c8e, 0x825d34, 0x826940, 0x82698c, 0x8269a6, 0x8269f2,
        0x826a0e, 0x826a5a, 0x826a76, 0x826ac2, 0x825df4, 0x825e86, 0x825f48};
    fat_directory_report.magic = 0x44495231u;
    for (uint32_t i = 0; i < 17; ++i) {
        fat_directory_report.entries[i] = SERVICE(slots[i]);
        if (fat_directory_report.entries[i] != entries[i]) return 0;
    }
    for (uint32_t i = 0; i < 21; ++i) {
        volatile struct directory_case *c = &fat_directory_report.cases[i];
        c->entry = entries[i < 17 ? i : 16];
        for (uint32_t j = 0; j < 64; ++j) c->output[j] = 0xa5u;
        uint32_t target = (uint32_t)(uintptr_t)(c->output + 16u);
        if (i == 0 || i == 2) c->result = ((two_fn)(uintptr_t)c->entry)(index, target);
        else if (i == 1 || i == 3) c->result = ((three_fn)(uintptr_t)c->entry)(directory, index, target);
        else if (i == 4) c->result = ((one_fn)(uintptr_t)c->entry)((uint32_t)(uintptr_t)name);
        else if (i == 5) c->result = ((two_fn)(uintptr_t)c->entry)(directory, (uint32_t)(uintptr_t)name);
        else if (i == 6 || i == 8 || i == 10 || i == 12) c->result = ((one_fn)(uintptr_t)c->entry)(index);
        else if (i < 14) c->result = ((two_fn)(uintptr_t)c->entry)(directory, index);
        else if (i == 14) c->result = ((one_fn)(uintptr_t)c->entry)((uint32_t)(uintptr_t)directory_name);
        else if (i == 15) c->result = ((two_fn)(uintptr_t)c->entry)(directory, (uint32_t)(uintptr_t)directory_name);
        else c->result = ((two_fn)(uintptr_t)c->entry)((uint32_t)(uintptr_t)paths[i - 16u], target);
        fat_directory_report.completed = i + 1u;
    }
    fat_directory_report.status = 1;
    fat_directory_report.magic = 0x44495232u;
    return 1;
}

void fat_guest_entry(void)
{
    typedef uint32_t (*read_fn)(uint32_t, uint32_t);
    typedef uint32_t (*cluster_fn)(uint32_t, uint32_t, int32_t);
    typedef uint32_t (*one_fn)(uint32_t);
    typedef uint32_t (*poll_fn)(void);
    static const char name[] = "INIT.X";
    static const uint32_t expected[] = {0x826ade, 0x826c3a, 0x826c6e,
                                        0x826dd0, 0x826f38, 0x827032, 0x82711e, 0x8272f4};
    uint32_t index, i, j, directory, first_cluster;
    fat_report.magic = 0x46415431u;
    for (i = 0; i < 8; ++i) {
        fat_report.entries[i] = SERVICE(89u + i);
        if (fat_report.entries[i] != expected[i]) goto failed;
    }
    index = ((one_fn)(uintptr_t)SERVICE(122))((uint32_t)(uintptr_t)name);
    fat_report.index = index;
    /* Bound every write for this exact image: four 8192-byte clusters. */
    if (index != 1 || *(volatile uint16_t *)0x52000b != 512 ||
        *(volatile uint8_t *)0x52000d != 16) goto failed;
    directory = 0x520000u +
        (*(volatile uint16_t *)0x52000e + *(volatile uint8_t *)0x520010 *
         *(volatile uint16_t *)0x520016) * 512u;
    first_cluster = *(volatile uint16_t *)(uintptr_t)(directory + index * 32u + 26u);
    for (j = 0; j < 128u; ++j)
        copied_directory[j] = *(volatile uint8_t *)(uintptr_t)(directory + j);
    for (i = 0; i < 16; ++i) {
        volatile struct fat_case *c = &fat_report.cases[i];
        uint32_t target = (uint32_t)(uintptr_t)(c->data + 16u);
        for (j = 0; j < BYTES + 32u; ++j) c->data[j] = 0xa5u;
        c->entry = fat_report.entries[i < 6 ? i : i < 10 ? i % 2u + 2u : i < 13 ? 6 : 7];
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
        } else if (i < 10) {
            int32_t skip = i < 4 ? 2 : i < 8 ? 4 : -1;
            c->result = ((cluster_fn)(uintptr_t)c->entry)(
                i % 2u == 0 ? index : (uint32_t)(uintptr_t)name, target, skip);
        } else {
            uint32_t count = i == 13 ? 1u : i == 14 ? 2u : 6u;
            uint32_t allocation = ((read_fn)(uintptr_t)c->entry)(
                i < 13 ? (i == 11 ? (uint32_t)(uintptr_t)copied_directory : directory) : first_cluster,
                i < 13 ? (i == 12 ? 0xffffffffu : index) : count);
            c->result = allocation;
            if (i == 12) {
                if (allocation) goto failed;
            } else {
                uint32_t copied = i < 13 || count > 4u ? BYTES : count * 8192u;
                if (!allocation) goto failed;
                for (j = 0; j < copied; ++j)
                    c->data[16u + j] = *(volatile uint8_t *)(uintptr_t)(allocation + j);
                ((one_fn)(uintptr_t)SERVICE(121))(allocation);
                c->allocation_freed = 1;
            }
        }
        fat_report.completed = i + 1u;
    }
    if (!check_directory_services(index, (uint32_t)(uintptr_t)copied_directory)) goto failed;
    fat_report.status = 1;
    goto finished;
failed:
    fat_report.status = 2;
finished:
    fat_report.magic = 0x46415432u;
    for (;;) ((poll_fn)(uintptr_t)SERVICE(108))();
}
