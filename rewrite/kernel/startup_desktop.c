/* Desktop/background orchestration at 0x810d4a..0x810f38. Object creation,
 * image lookup/rendering and activation remain explicit original boundaries. */
#include <stdint.h>
#include "startup_desktop.h"
#include "syscalls.h"
#include "surface_order.h"

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

uint32_t kernel_startup_lookup_8263c0(uint32_t name, uint32_t *metadata);
uint32_t kernel_startup_jpeg_823120(uint32_t layer, uint32_t buffer, uint32_t image);
uint32_t kernel_startup_png_82443e(uint32_t layer, uint32_t buffer, uint32_t image);
uint32_t kernel_startup_activate_810520(uint32_t object, uint32_t state);

/* Read each slot again, and evaluate its pointer before reading the source.
 * A preceding store may alias either a slot or the captured display record. */
static inline void copy_dimensions(uint32_t slot, uint32_t display)
{
    uint32_t target = DWORD(slot);
    uint32_t value = DWORD(display + 12u);
    DWORD(target + 12u) = value;
    target = DWORD(slot);
    value = DWORD(display + 16u);
    DWORD(target + 16u) = value;
}

void kernel_startup_desktop(void)
{
    uint32_t display = DWORD(0x1b034), colors = DWORD(0x1b0e0);
    uint32_t metadata[8], background, desktop, target, color;
    uint8_t *extension = (uint8_t *)metadata + 8;
    background = kernel_syscall_target_807a0a(0, 0, 2560, 1600, 0,
                                            DWORD(colors + 0x224u), 0, 0x83cd4c);
    desktop = kernel_syscall_target_807a0a(0, 0, 2560, 1600, 0,
                                         DWORD(colors + 0x3a0u), 0, 0x83cd55);
    if ((int32_t)background >= 0) {
        uint32_t found = kernel_startup_lookup_8263c0(DWORD(0x1b154), metadata);
        copy_dimensions(0x500008, display);
        if ((int32_t)found > 0 && DWORD(0x1b100) != 0) {
            if (extension[0] == 'J' && extension[1] == 'P' && extension[2] == 'G')
                (void)kernel_startup_jpeg_823120(2, DWORD(0x1b100), metadata[7]);
            else if (extension[0] == 'P' && extension[1] == 'N' && extension[2] == 'G')
                (void)kernel_startup_png_82443e(2, DWORD(0x1b100), metadata[7]);
        }
        (void)kernel_startup_activate_810520(background, 0);
        (void)kernel_syscall_target_8082c6(background);
    }
    if ((int32_t)desktop >= 0) {
        copy_dimensions(0x50000c, display);
        target = DWORD(0x50000c);
        color = DWORD(colors + 0x3a0u);
        DWORD(target + 40u) = color;
        (void)kernel_startup_activate_810520(desktop, 0);
        (void)kernel_syscall_target_8082c6(desktop);
    }
    (void)kernel_surface_order_front(1);
}
