/* Test-only X: release two disposable fixture chains, delete their records,
 * and flush through the real filesystem/ATA paths. Reports via QEMU port E9.
 * This program is never installed in a production image. */
#include <stdint.h>
#define B(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*no_fn)(void);
volatile uint32_t mutation_report[12];

static void emit(uint32_t value)
{
    for (uint32_t i = 0; i < 4; ++i) {
        __asm__ volatile("outb %0, %1" : : "a"((uint8_t)value), "Nd"((uint16_t)0xe9));
        value >>= 8;
    }
}

void fat_guest_entry(void)
{
    uint32_t index_a, index_b, root;
    static const char name_a[] = "MUTROOT.TST", name_b[] = "MUTREC.TST";
    mutation_report[0] = 0x4d555431u;
    index_a = ((one_fn)0x825c8e)((uint32_t)(uintptr_t)name_a);
    index_b = ((one_fn)0x825c8e)((uint32_t)(uintptr_t)name_b);
    mutation_report[1] = index_a;
    mutation_report[2] = index_b;
    if (index_a != 4 || index_b != 5 || W(0x52000b) != 512 ||
            B(0x520010) != 2 || W(0x520016) != 16) goto finished;
    root = 0x520000u + (W(0x52000e) + B(0x520010) * W(0x520016)) * W(0x52000b);
    mutation_report[3] = ((one_fn)0x827d4a)(index_a);
    mutation_report[4] = ((one_fn)0x827f58)(index_a);
    mutation_report[5] = ((one_fn)0x827e6c)(root + index_b * 32u);
    mutation_report[6] = ((one_fn)0x827fe2)((uint32_t)(uintptr_t)name_b);
    mutation_report[7] = ((one_fn)0x827f58)(index_a);
    mutation_report[8] = ((one_fn)0x827fe2)((uint32_t)(uintptr_t)name_b);
    mutation_report[9] = ((no_fn)0x827cc4)();
    mutation_report[10] = (B(root + index_a * 32u) << 8) | B(root + index_b * 32u);
    mutation_report[11] = 1;
finished:
    for (uint32_t i = 0; i < 12; ++i) emit(mutation_report[i]);
    for (;;) ((no_fn)(uintptr_t)D(D(0x1b0e4u) + 108u * 4u))();
}
