/* File-backed icon wrappers at 0x82b0a0..0x82b20c. Keep the unchecked path
 * lookahead, uppercase-only suffix tests, draw-before-free order, and the
 * auto wrapper's returned address into an already freed file allocation. */
#include "icon_files.h"
#include "icon_directory.h"
#include "x_icon.h"
#include "helpers.h"
#include "memory.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))

uint32_t kernel_icon_read_relative_826f38(uint32_t name);
uint32_t kernel_icon_read_absolute_827798(uint32_t name);

static __attribute__((always_inline)) inline uint32_t read_icon_file(uint32_t name)
{
    if (BYTE(name + 1u) == ':' && BYTE(name + 2u) == '\\')
        return kernel_icon_read_absolute_827798(name);
    return kernel_icon_read_relative_826f38(name);
}

void kernel_icon_draw_file(uint32_t surface_id, uint32_t x, uint32_t y,
                           uint32_t name, uint32_t frame)
{
    uint32_t file = read_icon_file(name);
    kernel_icon_draw_id(surface_id, x, y, file, frame);
    if (file) kernel_free_allocation(file);
}

uint32_t kernel_icon_draw_file_auto(uint32_t surface_id, uint32_t x, uint32_t y,
                                    uint32_t name, uint32_t frame)
{
    uint32_t icon = 0;
    uint32_t file = read_icon_file(name);
    uint32_t length = kernel_string_length(name);
    if (length > 4u && BYTE(name + length - 1u) == 'O' &&
        BYTE(name + length - 2u) == 'C' && BYTE(name + length - 3u) == 'I' &&
        BYTE(name + length - 4u) == '.')
        icon = file;
    if (length > 2u && BYTE(name + length - 1u) == 'X' &&
        BYTE(name + length - 2u) == '.')
        icon = kernel_x_icon_pointer(file);
    kernel_icon_draw_id(surface_id, x, y, icon, frame);
    if (file) kernel_free_allocation(file);
    return icon;
}
