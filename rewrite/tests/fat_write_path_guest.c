/* Test-only X: create/overwrite nested files and forward a root path through
 * the real absolute writer. No production application registration changes. */
#include <stdint.h>
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*three_fn)(uint32_t, uint32_t, uint32_t);
typedef uint32_t (*two_fn)(uint32_t, uint32_t);
typedef uint32_t (*no_fn)(void);
static uint8_t payload[0x8000];
volatile uint32_t path_report[16];

static void fill(uint32_t seed)
{
    for (uint32_t i = 0; i < sizeof(payload); ++i)
        payload[i] = (uint8_t)(i * 29u + (i >> 7) * 3u + seed);
}

static uint32_t write_file(const char *name, uint32_t bytes)
{
    return ((three_fn)0x8280e4)((uint32_t)(uintptr_t)name,
                              (uint32_t)(uintptr_t)payload,bytes);
}

void fat_guest_entry(void)
{
    uint8_t record[32];
    static const char *const names[] = {"C:\\WRDIR\\SUB\\WRNEW.TST",
        "C:\\WRDIR\\SUB\\WROLD.TST", "C:\\WRDIR\\SUB\\WRZERO.TST"};
    path_report[0] = 0x50415431u;
    fill(17);
    path_report[1] = write_file("c:\\wrdir\\sub\\wrnew.tst",8193);
    fill(83);
    path_report[2] = write_file(names[1],1);
    path_report[3] = write_file(names[2],0);
    fill(211);
    path_report[4] = write_file("c:\\rootfor.tst",513);
    path_report[5] = write_file("C:\\MISSING\\FAIL.TST",3);
    path_report[6] = write_file("NOSPLIT.TST",3);
    path_report[7] = write_file("D:\\WRDIR\\SUB\\FAIL.TST",3);
    for (uint32_t i = 0; i < 3; ++i) {
        path_report[8u+i*2u] = ((two_fn)0x8263c0)((uint32_t)(uintptr_t)names[i],
                                                 (uint32_t)(uintptr_t)record);
        path_report[9u+i*2u] = D((uint32_t)(uintptr_t)record+28u);
    }
    path_report[14] = ((two_fn)0x8263c0)((uint32_t)(uintptr_t)"C:\\ROOTFOR.TST",
                                        (uint32_t)(uintptr_t)record);
    path_report[15] = D((uint32_t)(uintptr_t)record+28u);
    for (uint32_t i = 0; i < 16; ++i) {
        uint32_t value = path_report[i];
        for (uint32_t j = 0; j < 4; ++j) {
            __asm__ volatile("outb %0, %1" : : "a"((uint8_t)value), "Nd"((uint16_t)0xe9));
            value >>= 8;
        }
    }
    for (;;) ((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
