/* DIB icon renderer at 0x82b20c. This deliberately follows the original's
 * layout rather than a general BMP/ICO specification: packed pixel rows,
 * height-based mask stride, and a different mask walk for 32 -> 8 pixels.
 * Input validation, clipping and the three drawing/conversion helpers are
 * outside this function. All address and coordinate arithmetic wraps at 32 bits.
 */
#include "icon_dib.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define WORD(a) (*(volatile uint16_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))

void kernel_icon_put32_81b3c0(uint32_t surface, uint32_t x, uint32_t y,
                            uint32_t color);
void kernel_icon_put8_81b37c(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t color);
uint32_t kernel_icon_quantize_81fd90(uint32_t red, uint32_t green,
                                    uint32_t blue, uint32_t x, uint32_t y);

void kernel_icon_dib_82b20c(uint32_t surface, uint32_t x, uint32_t y,
                           uint32_t dib)
{
    uint32_t width = DWORD(dib + 4u);
    uint32_t height = DWORD(dib + 8u) >> 1;
    uint32_t stride = height <= 47u ? 4u : 8u;
    uint32_t pixels, mask, palette = dib + 40u, source32, target32;

    if (WORD(dib + 14u) == 32u) {
        source32 = 1;
        pixels = dib + 40u;
        mask = pixels + width * height * 4u + (height - 1u) * stride;
    } else if (WORD(dib + 14u) == 8u) {
        source32 = 0;
        pixels = dib + 1064u;
        mask = pixels + width * height + (height - 1u) * stride;
    } else {
        return;
    }

    if (DWORD(surface + 36u) == 32u) target32 = 1;
    else if (DWORD(surface + 36u) == 8u) target32 = 0;
    else return;

    for (uint32_t row = 0; row < height; ++row) {
        uint32_t bit = 0, mask_byte = 0;
        for (uint32_t col = 0; col < width; ++col) {
            if (!(BYTE(mask + mask_byte) & (0x80u >> bit))) {
                uint32_t color;
                uint32_t pixel = pixels + ((height - row - 1u) * width + col)
                                             * (source32 ? 4u : 1u);
                if (!source32) pixel = palette + (uint32_t)BYTE(pixel) * 4u;
                if (target32) {
                    color = DWORD(pixel);
                    if (source32) color &= 0x00ffffffu;
                    kernel_icon_put32_81b3c0(surface, x + col, y + row, color);
                } else {
                    /* Preserve B,G,R load order and use only the result's AL.
                     * Quantizer coordinates are local, drawing coordinates
                     * include x/y and wrap independently. Alpha is ignored. */
                    uint32_t blue = BYTE(pixel);
                    uint32_t green = BYTE(pixel + 1u);
                    uint32_t red = BYTE(pixel + 2u);
                    color = kernel_icon_quantize_81fd90(red, green, blue, col, row);
                    kernel_icon_put8_81b37c(surface, x + col, y + row, color & 255u);
                }
            }
            ++bit;
            /* Original 32 -> 8 path advances one byte for every pixel,
             * plus another byte after every group of eight. */
            if (source32 && !target32) ++mask_byte;
            if (bit == 8u) { bit = 0; ++mask_byte; }
        }
        mask -= stride;
    }
}
