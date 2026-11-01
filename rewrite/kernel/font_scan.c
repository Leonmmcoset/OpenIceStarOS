/* Original fixed-point font scan conversion and 20-byte edge sorting.
 * Preserve wrapping coverage, live memory reads, NaN comparisons and the
 * original caller-area writes in fill and quicksort. Raster driver external. */
#include "font_scan.h"
#include "font_heap.h"
#include "math_extra.h"
#define B(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
#define F(a) (*(volatile float *)(uintptr_t)(uint32_t)(a))

uint32_t kernel_font_active_830c5a(uint32_t heap, volatile uint32_t edge, volatile int32_t offset_x, volatile float start_y, uint32_t userdata)
{
    uint32_t active = kernel_font_heap_alloc_830b4c(heap, 20, userdata);
    volatile float slope = ((double)F(edge + 8u) - F(edge)) / ((double)F(edge + 12u) - F(edge + 4u));
    if (!active) return 0;
    D(active + 8u) = __builtin_isless(slope, 0.0f)
        ? 0u - (uint32_t)(int32_t)kernel_floor_81a9ee((double)slope * F(0x83d05b))
        : (uint32_t)(int32_t)kernel_floor_81a9ee((double)slope * F(0x83d05f));
    D(active + 4u) = (uint32_t)(int32_t)kernel_floor_81a9ee((double)F(edge) * F(0x83d05f)
        + (int32_t)D(active + 8u) * ((double)start_y - F(edge + 4u)));
    D(active + 4u) -= (uint32_t)offset_x << 10;
    D(active + 12u) = D(edge + 12u);
    D(active) = 0;
    D(active + 16u) = D(edge + 16u) ? 1u : 0xffffffffu;
    return active;
}

void kernel_font_fill_scanline_830dc4(volatile uint32_t scanline, volatile int32_t width, uint32_t active, volatile int32_t weight)
{
    (void)active;
    volatile uint32_t *cursor = (volatile uint32_t *)__builtin_frame_address(0) + 4;
    int32_t first = 0;
    uint32_t winding = 0;
    while (*cursor) {
        if (!winding) {
            first = (int32_t)D(*cursor + 4u);
            winding += D(*cursor + 16u);
        } else {
            int32_t last = (int32_t)D(*cursor + 4u);
            winding += D(*cursor + 16u);
            if (!winding) {
                int32_t left = first >> 10, right = last >> 10;
                if (left < width && right >= 0) {
                    if (left == right) {
                        uint32_t address = scanline + (uint32_t)left;
                        B(address) += (int32_t)(((uint32_t)last - (uint32_t)first) * (uint32_t)weight) >> 10;
                    } else {
                        if (left >= 0) {
                            uint32_t address = scanline + (uint32_t)left;
                            B(address) += (int32_t)((1024u - ((uint32_t)first & 1023u)) * (uint32_t)weight) >> 10;
                        } else left = -1;
                        if (right < width) {
                            uint32_t address = scanline + (uint32_t)right;
                            B(address) += (int32_t)(((uint32_t)last & 1023u) * (uint32_t)weight) >> 10;
                        } else right = width;
                        ++left;
                        while (left < right) {
                            uint32_t address = scanline + (uint32_t)left;
                            B(address) += weight;
                            ++left;
                        }
                    }
                }
            }
        }
        *cursor = D(*cursor);
    }
}

static __attribute__((always_inline)) inline void edge_copy(uint32_t destination, uint32_t source)
{
    for (uint32_t i = 0; i < 20; i += 4) D(destination + i) = D(source + i);
}

static __attribute__((always_inline)) inline void edge_swap(uint32_t first, uint32_t second)
{
    uint32_t saved[5];
    for (uint32_t i = 0; i < 5; ++i) saved[i] = D(first + i * 4u);
    edge_copy(first, second);
    for (uint32_t i = 0; i < 5; ++i) D(second + i * 4u) = saved[i];
}

void kernel_font_edge_insertion_831340(volatile uint32_t edges, volatile int32_t count)
{
    for (int32_t i = 1; i < count; ++i) {
        uint32_t saved[5];
        for (uint32_t k = 0; k < 5; ++k) saved[k] = D(edges + (uint32_t)i * 20u + k * 4u);
        int32_t j = i;
        while (j > 0) {
            uint32_t previous = edges + (uint32_t)j * 20u - 20u;
            union { uint32_t bits; float value; } key = {saved[1]};
            if (!__builtin_isless(key.value, F(previous + 4u))) break;
            edge_copy(edges + (uint32_t)j * 20u, previous);
            --j;
        }
        if (i != j) for (uint32_t k = 0; k < 5; ++k) D(edges + (uint32_t)j * 20u + k * 4u) = saved[k];
    }
}

void kernel_font_edge_quicksort_831480(uint32_t edges, int32_t count)
{
    (void)edges; (void)count;
    volatile uint32_t *live = (volatile uint32_t *)__builtin_frame_address(0) + 2;
    while ((int32_t)live[1] > 12) {
        uint32_t middle = live[1] >> 1;
        int first_middle = __builtin_isless(F(live[0] + 4u), F(live[0] + middle * 20u + 4u));
        int middle_last = __builtin_isless(F(live[0] + middle * 20u + 4u), F(live[0] + live[1] * 20u - 16u));
        if (first_middle != middle_last) {
            int first_last = __builtin_isless(F(live[0] + 4u), F(live[0] + live[1] * 20u - 16u));
            uint32_t selected = first_last != middle_last ? live[1] - 1u : 0;
            edge_swap(live[0] + selected * 20u, live[0] + middle * 20u);
        }
        edge_swap(live[0], live[0] + middle * 20u);
        int32_t i = 1, j = (int32_t)(live[1] - 1u);
        for (;;) {
            while (__builtin_isless(F(live[0] + (uint32_t)i * 20u + 4u), F(live[0] + 4u))) ++i;
            while (__builtin_isless(F(live[0] + 4u), F(live[0] + (uint32_t)j * 20u + 4u))) --j;
            if (i >= j) break;
            edge_swap(live[0] + (uint32_t)i * 20u, live[0] + (uint32_t)j * 20u);
            ++i; --j;
        }
        if ((int32_t)(live[1] - (uint32_t)i) > j) {
            kernel_font_edge_quicksort_831480(live[0], j);
            live[0] += (uint32_t)i * 20u;
            live[1] -= (uint32_t)i;
        } else {
            kernel_font_edge_quicksort_831480(live[0] + (uint32_t)i * 20u, (int32_t)(live[1] - (uint32_t)i));
            live[1] = (uint32_t)j;
        }
    }
}

void kernel_font_edge_sort_831858(uint32_t edges, int32_t count)
{
    kernel_font_edge_quicksort_831480(edges, count);
    kernel_font_edge_insertion_831340(edges, count);
}
