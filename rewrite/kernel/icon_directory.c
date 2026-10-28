/* ICO directory dispatch at 0x82af08..0x82b0a0. Preserve the original
 * unsigned centering, signed frame check, unchecked indices and 32-bit
 * pointer wrapping. PNG and DIB decoding remain separate original routines. */
#include "icon_directory.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

void kernel_icon_png_8241d0(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t data, uint32_t size);
void kernel_icon_dib_82b20c(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t data);

__attribute__((noinline))
void kernel_icon_draw_frame(uint32_t surface, uint32_t x, uint32_t y,
                            uint32_t package, uint32_t frame)
{
    if ((int32_t)WORD(package + 4u) > (int32_t)frame) {
        uint32_t entry = package + 6u + frame * 16u;
        if (BYTE(entry) == 0 && BYTE(entry + 1u) == 0) {
            uint32_t size = DWORD(entry + 8u);
            uint32_t data = package + DWORD(entry + 12u);
            kernel_icon_png_8241d0(surface, x, y, data, size);
        } else {
            uint32_t data = package + DWORD(entry + 12u);
            kernel_icon_dib_82b20c(surface, x, y, data);
        }
    }
}

void kernel_icon_draw_id(uint32_t surface_id, uint32_t x, uint32_t y,
                         uint32_t package, uint32_t frame)
{
    kernel_icon_draw_frame(DWORD(0x500000u + surface_id * 4u),
                            x, y, package, frame);
}

void kernel_icon_draw_centered(uint32_t surface, uint32_t package)
{
    uint32_t entry = package + 6u, largest = 0, selected = 0, index;
    for (index = 0; index < WORD(package + 4u); ++index) {
        if (BYTE(entry) == 0) {
            largest = 256;
            selected = index;
            break;
        }
        if (BYTE(entry) > largest) {
            largest = BYTE(entry);
            selected = index;
        }
        entry += 16u;
    }
    /* The selected width is subtracted from both dimensions. Heights in
     * directory entries are ignored, and SHR is deliberately not SAR. */
    uint32_t y = (DWORD(surface + 16u) - largest) >> 1;
    uint32_t x = (DWORD(surface + 12u) - largest) >> 1;
    kernel_icon_draw_frame(surface, x, y, package, selected);
}
