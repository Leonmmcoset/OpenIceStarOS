#ifndef OPENICESTAR_STARSCR_H
#define OPENICESTAR_STARSCR_H
#include <stdint.h>

struct starscr_state {
    uint32_t record, window, surface, unused;
};
extern volatile struct starscr_state starscr_state;
extern const char starscr_title[12];
uint32_t starscr_entry(void);
void starscr_key(uint32_t window, uint32_t key, uint32_t pressed);
uint32_t starscr_close(void);
void starscr_draw(uint32_t phase);
#endif
