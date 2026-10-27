/* Startup display record (0x81acc4) and object-table counters (0x82aa0f).
 * Read the boot parameters at each original access: the captured record may
 * alias those parameters or the pointer global. Palette and software color-table setup are separate recovered routines. */
#include <stdint.h>
#include "startup_state.h"
#include "display_palette.h"

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

void kernel_init_display_state(void)
{
    uint32_t record = DWORD(0x1b034);
    DWORD(record) = DWORD(0x1a822);
    DWORD(record + 4) = 0;
    DWORD(record + 8) = 0;
    DWORD(record + 12) = DWORD(0x1a81a);
    DWORD(record + 16) = DWORD(0x1a81e);
    DWORD(record + 20) = DWORD(0x1a81a);
    DWORD(record + 24) = DWORD(0x1a81e);
    DWORD(record + 36) = DWORD(0x1a816);
    DWORD(record + 40) = UINT32_MAX;
    DWORD(record + 44) = 0;
    DWORD(record + 48) = 0;
    DWORD(record + 52) = 0;
    if (DWORD(record + 36) == 8)
        kernel_display_init_palette();
    /* Palette callbacks can change this field. Re-read the captured record. */
    kernel_display_select_depth(DWORD(record + 36));
}

void kernel_init_object_table_state(void)
{
    DWORD(0x1b0b8) = 0x4000;
    DWORD(0x1b0bc) = 0;
    DWORD(0x1b0c0) = 0;
}
