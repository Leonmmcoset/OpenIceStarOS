/* Startup drawing/resource orchestration at 0x81067a..0x810d4a. Address-named
 * dependencies resolve to C where installed, including FAT file readers. */
#include <stdint.h>
#include "startup_ui.h"
#include "runtime.h"

#define DWORD(address) (*(volatile uint32_t *)(uintptr_t)(address))

uint32_t kernel_startup_logo_821624(uint32_t layer, uint32_t x, uint32_t y, uint32_t name);
uint32_t kernel_startup_panel_81d6c2(uint32_t layer, uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height, uint32_t style, uint32_t color);
uint32_t kernel_startup_fill_837545(uint32_t layer, uint32_t x0, uint32_t y0,
                                 uint32_t x1, uint32_t y1, uint32_t color);
uint32_t kernel_startup_outline_837589(uint32_t layer, uint32_t x0, uint32_t y0,
                                    uint32_t x1, uint32_t y1, uint32_t color);
uint32_t kernel_startup_text_8376d3(uint32_t layer, uint32_t x, uint32_t y,
                                 uint32_t foreground, uint32_t background, uint32_t text);
uint32_t kernel_startup_lookup_8263c0(uint32_t name, uint32_t *metadata);
uint32_t kernel_startup_read_alloc_827454(uint32_t *metadata);
uint32_t kernel_startup_read_into_82760c(uint32_t *metadata, uint32_t target);
uint32_t kernel_startup_load_image_80214c(uint32_t name);

static uint32_t half(uint32_t address)
{
    return (uint32_t)((int32_t)DWORD(address) / 2);
}

static void relative_rectangle(uint32_t display, uint32_t colors, uint32_t outline,
                               uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                               uint32_t color_offset)
{
    /* Original right-to-left parameter reads. Never cache dimensions between
     * drawing callbacks or combine the repeated volatile source reads. */
    uint32_t color = DWORD(colors + color_offset);
    y1 += half(display + 16u);
    x1 += half(display + 12u);
    y0 += half(display + 16u);
    x0 += half(display + 12u);
    if (outline)
        (void)kernel_startup_outline_837589(0,x0,y0,x1,y1,color);
    else
        (void)kernel_startup_fill_837545(0,x0,y0,x1,y1,color);
}

void kernel_startup_ui(void)
{
    uint32_t display = DWORD(0x1b034), colors = DWORD(0x1b0e0);
    uint32_t metadata[8], i, x, y, color, background, name, destination;
    if ((int32_t)DWORD(0x1a80a) > 0) {
        y = half(display + 16u) - 150u;
        x = half(display + 12u) - 130u;
        (void)kernel_startup_logo_821624(0,x,y,0x83ccc7);
        color = DWORD(colors + 28u);
        y = half(display + 16u) + 160u;
        x = half(display + 12u) - 151u;
        (void)kernel_startup_panel_81d6c2(0,x,y,300,22,3,color);
    } else {
        relative_rectangle(display,colors,0,-60u,-40u,-5u,-5u,4);
        relative_rectangle(display,colors,0,5,-40u,60,-5u,40);
        relative_rectangle(display,colors,0,-60u,5,-5u,40,16);
        relative_rectangle(display,colors,0,5,5,60,40,60);
        background = DWORD(colors);
        color = DWORD(colors + 12u);
        y = half(display + 16u) + 50u;
        x = half(display + 12u) - 72u;
        (void)kernel_startup_text_8376d3(0,x,y,color,background,0x83cce3);
        relative_rectangle(display,colors,1,-150u,75,150,95,28);
        relative_rectangle(display,colors,1,-151u,76,149,94,28);
    }
    for (i = 0; i <= 68; ++i) {
        relative_rectangle(display,colors,0,i*4u-148u,163,i*4u-129u,177,16);
        if (i == 0 || i == 10 || i == 25) {
            destination = i == 0 ? 0x1b100 : i == 10 ? 0x1b1a8 : 0x1b1ac;
            name = i == 0 ? DWORD(0x1b154) : i == 10 ? 0x83ccf6 : 0x83cd33;
            if ((int32_t)kernel_startup_lookup_8263c0(name,metadata) > 0)
                DWORD(destination) = kernel_startup_read_alloc_827454(metadata);
        } else if (i == 5 || i == 30) {
            destination = i == 5 ? 0x1b158 : 0x1b1b8;
            name = i == 5 ? 0x1b15c : 0x1b1bc;
            if ((int32_t)kernel_startup_lookup_8263c0(DWORD(name),metadata) > 0)
                /* The original deliberately re-reads the name after lookup. */
                DWORD(destination) = kernel_startup_load_image_80214c(DWORD(name));
        } else if (i == 15 || i == 20) {
            name = i == 15 ? 0x83cd08 : 0x83cd1d;
            if ((int32_t)kernel_startup_lookup_8263c0(name,metadata) > 0)
                (void)kernel_startup_read_into_82760c(metadata,i == 15 ? 0x20200 : 0x62200);
        } else {
            (void)kernel_wait_ticks(1);
        }
    }
}
