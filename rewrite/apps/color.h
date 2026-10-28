#ifndef OPENICESTAR_COLOR_H
#define OPENICESTAR_COLOR_H
#include <stdint.h>

struct color_state {
    uint32_t window_record, unused0, surface, unused1;
};
extern volatile struct color_state color_state;
extern const char color_title[5];
void color_entry(void);
uint32_t color_close(void);
void color_draw(void);
#endif
