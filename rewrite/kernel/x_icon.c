/* X-header icon offset at 0x812c0e. This API does not validate the file,
 * allocation, header magic or resulting pointer, including a null input. */
#include "x_icon.h"
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

uint32_t kernel_x_icon_pointer(uint32_t file)
{
    if (DWORD(file + 40u) != 0)
        return file + DWORD(file + 40u);
    return 0;
}
