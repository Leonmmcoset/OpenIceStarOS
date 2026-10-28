/* Startup caller 0x810f38..0x810f78. The image-launching callee at
 * 0x802282 is recovered in x_loader.c. */
#include <stdint.h>
#include "runtime.h"
#include "startup_apps.h"
#include "x_loader.h"

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

void kernel_startup_applications(void)
{
    uint32_t first, second;
    /* Preserve the original read order and do not capture the second pair
     * before either callback: both launch and wait may change the globals. */
    second = DWORD(0x1b15c);
    first = DWORD(0x1b158);
    kernel_startup_launch_image(first, second);
    (void)kernel_wait_ticks(20);
    second = DWORD(0x1b1bc);
    first = DWORD(0x1b1b8);
    kernel_startup_launch_image(first, second);
}
