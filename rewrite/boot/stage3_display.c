/* Protected-mode display handoff at original stage offsets 0x30b..0x38c.
 * Retain the original raw descriptor packing: the limit is the wrapped
 * pixel count, not a corrected framebuffer byte count or page count. */
#include <stdint.h>

#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(a))

void stage_prepare_display(void)
{
    uint32_t pixels = DWORD(0x1a81a) * DWORD(0x1a81e);

    if (DWORD(0x1a80a) != 0) {
        uint32_t base = DWORD(0x1a822);
        volatile uint32_t *gdt = (volatile uint32_t *)(uintptr_t)DWORD(0x1a828);
        gdt[4] = (base << 16) | (pixels & 0xffffu);
        gdt[5] = (base & 0xff000000u) | ((base >> 16) & 0xffu) |
                 (pixels & 0xffff0000u) | 0x00c09200u;
        __asm__ volatile ("lgdt 0x1a826" : : : "memory");
    }
    __asm__ volatile ("movw $0x10, %%ax; movw %%ax, %%gs" : : : "eax", "memory");

    if (BYTE(0x1a816) == 8) {
        do {
            __asm__ volatile ("movb $0, %%gs:(%0)" : : "r"(pixels - 1) : "memory");
        } while (--pixels);
    } else if (BYTE(0x1a816) == 32) {
        uint32_t offset = 0;
        do {
            __asm__ volatile ("movl $0, %%gs:(%0)" : : "r"(offset) : "memory");
            offset += 4;
        } while (--pixels);
    } else {
        for (;;) { }
    }
}
