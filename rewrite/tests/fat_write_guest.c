/* Test-only X: exercise the real root writer and path splitter through CMD.
 * Production images never contain this diagnostic. Report words use port E9. */
#include <stdint.h>
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*three_fn)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*no_fn)(void);
static uint8_t payload[0x8000];
static char parent[32], filename[32];
volatile uint32_t write_report[12];

static void fill(uint32_t seed)
{
    for (uint32_t i = 0; i < sizeof(payload); ++i)
        payload[i] = (uint8_t)(i * 29u + (i >> 7) * 3u + seed);
}

static uint32_t write_file(const char *name, uint32_t bytes)
{
    return ((three_fn)0x82867e)((uint32_t)(uintptr_t)name,
                              (uint32_t)(uintptr_t)payload, bytes);
}

void fat_guest_entry(void)
{
    write_report[0] = 0x57525431u;
    write_report[1] = ((three_fn)0x828018)((uint32_t)(uintptr_t)"C:\\WRNEW.TST",
        (uint32_t)(uintptr_t)parent, (uint32_t)(uintptr_t)filename);
    write_report[2] = parent[0] == 'C' && parent[1] == ':' && !parent[2];
    write_report[3] = filename[0] == 'W' && filename[1] == 'R' &&
        filename[2] == 'N' && filename[3] == 'E' && filename[4] == 'W' &&
        filename[5] == '.' && filename[6] == 'T' && filename[7] == 'S' &&
        filename[8] == 'T' && !filename[9];
    fill(17);
    write_report[4] = write_file("WRNEW.TST", 8193);
    fill(83);
    write_report[5] = write_file("WROLD.TST", 1);
    write_report[6] = write_file("WRZERO.TST", 0);
    fill(149);
    write_report[7] = write_file("WRREUSE.TST", 16385);
    static const char *const names[] = {"WRNEW.TST", "WROLD.TST", "WRZERO.TST", "WRREUSE.TST"};
    for (uint32_t i = 0; i < 4; ++i)
        write_report[8u+i] = ((one_fn)0x825c8e)((uint32_t)(uintptr_t)names[i]);
    for (uint32_t i = 0; i < 12; ++i) {
        uint32_t value = write_report[i];
        for (uint32_t j = 0; j < 4; ++j) {
            __asm__ volatile("outb %0, %1" : : "a"((uint8_t)value), "Nd"((uint16_t)0xe9));
            value >>= 8;
        }
    }
    for (;;) ((no_fn)(uintptr_t)D(D(0x1b0e4u) + 108u * 4u))();
}
