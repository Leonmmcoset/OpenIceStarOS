/* Recovered CLOCK.X +0x40..0x570. Original SHA-256:
 * 2b93983ddb1c812ffca6bfb27c6a2f8223054bbfff49aef60c583c4cbd6993d7.
 * Keep the original x87 arithmetic, BCD decoding and integer division. */
#include "clock.h"
#include "legacy_api.h"
#include "legacy_image_api.h"
#include "legacy_clock_api.h"
#define WORD(address) (*(volatile uint32_t *)(uintptr_t)(address))
volatile struct clock_state clock_state __attribute__((section(".bss.clock"),aligned(4)));
const struct clock_data clock_data __attribute__((section(".rodata.clock"),aligned(1))) = {
    "\xca\xb1\xd6\xd3", "12", "%d", {0},
    0x1.0c152382d7365p-1, 120.0, 200.0, 205.0,
    0x1.0c152382d7365p-1, 0x1.acee9f37bebd5p-4
};
_Static_assert(sizeof(clock_state) == 16, "CLOCK BSS layout");
_Static_assert(sizeof(clock_data) == 64, "CLOCK initialized data layout");

__attribute__((noinline)) uint32_t clock_palette(uint32_t index)
{
    return WORD(WORD(0x1b0e0) + index*4u);
}
void clock_face(void)
{
    uint32_t color = clock_palette(1);
    legacy_thick_circle(clock_state.surface,200,205,150,color,3);
    for (volatile int32_t i = 0; i < 12; ++i) {
        volatile double angle = i * clock_data.label_angle;
        uint32_t x = 200u + (uint32_t)(int32_t)(legacy_sine(angle) * clock_data.radius);
        angle = i * clock_data.label_angle;
        uint32_t y = 205u - (uint32_t)(int32_t)(legacy_cosine(angle) * clock_data.radius);
        uint32_t back = clock_palette(8), fore = clock_palette(1);
        legacy_format(clock_state.surface,x,y,fore,back,
                      i == 0 ? clock_data.twelve : clock_data.number,i);
    }
}
void clock_hand(double angle, volatile int32_t length, volatile uint32_t width)
{
    volatile double saved_angle = angle;
    volatile double saved_length = length;
    double sine = legacy_sine(saved_angle);
    volatile double x = sine * saved_length + clock_data.center_x;
    saved_length = length;
    double cosine = legacy_cosine(saved_angle);
    volatile double y = clock_data.center_y - cosine * saved_length;
    uint32_t color = clock_palette(1);
    int32_t iy = (int32_t)y, ix = (int32_t)x;
    legacy_thick_line(clock_state.surface,200,205,(uint32_t)ix,(uint32_t)iy,color,width);
}
void clock_hands(uint32_t time)
{
    int32_t h = ((time>>20)&15)*10 + ((time>>16)&15);
    int32_t m = ((time>>12)&15)*10 + ((time>>8)&15);
    int32_t s = ((time>>4)&15)*10 + (time&15);
    volatile double hour = (h%12 + m/60 + s/3600) * clock_data.hour_angle;
    volatile double minute = (m + s/60) * clock_data.minute_angle;
    volatile double second = s * clock_data.minute_angle;
    clock_hand(hour,75,8);
    clock_hand(minute,105,5);
    clock_hand(second,135,2);
}
uint32_t clock_close(void)
{
    return legacy_service_6b(WORD(clock_state.record+4u));
}
uint32_t clock_entry(void)
{
    uint32_t window = legacy_window_create(200,100,400,380,1,0,2,clock_data.title);
    uint32_t previous = WORD(0x1b004);
    if ((int32_t)window > 0) {
        clock_state.record = WORD(0x6a0000u+window*4u);
        WORD(clock_state.record+0x5b7u) = (uint32_t)(uintptr_t)clock_close;
        clock_state.window = WORD(clock_state.record);
        clock_state.surface = legacy_surface(clock_state.window);
        clock_face();
        clock_hands(previous);
        legacy_window_show(window);
        for (;;) {
            uint32_t current = WORD(0x1b004);
            if (current != previous) {
                uint32_t background = clock_palette(8);
                legacy_rectangle(clock_state.surface,12,32,376,368,background);
                clock_face();
                clock_hands(current);
                previous = current;
                legacy_window_refresh(window);
            }
            legacy_poll();
        }
    }
    return legacy_service_6b(WORD(clock_state.record+4u));
}
