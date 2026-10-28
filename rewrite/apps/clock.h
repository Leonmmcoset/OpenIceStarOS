#ifndef ICESTAR_CLOCK_H
#define ICESTAR_CLOCK_H
#include <stdint.h>
struct clock_state { uint32_t record, window, surface, unused; };
struct clock_data {
    char title[5], twelve[3], number[3], padding[5];
    double label_angle, radius, center_x, center_y, hour_angle, minute_angle;
};
extern volatile struct clock_state clock_state;
extern const struct clock_data clock_data;
uint32_t clock_entry(void);
uint32_t clock_close(void);
uint32_t clock_palette(uint32_t);
void clock_face(void);
void clock_hand(double, int32_t, uint32_t);
void clock_hands(uint32_t);
#endif
