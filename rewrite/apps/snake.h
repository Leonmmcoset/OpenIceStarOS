#ifndef ICESTAR_SNAKE_H
#define ICESTAR_SNAKE_H
#include <stdint.h>

/* Preserve the observed data layout, including untouched padding and the
 * original unchecked 100-element body arrays. Offsets are relative to BSS. */
struct snake_state {
    uint32_t surface, window;               /* 0x000 */
    uint8_t untouched_008[24];
    uint32_t x[100];                        /* 0x020 */
    uint8_t untouched_1b0[16];
    uint32_t y[100];                        /* 0x1c0 */
    uint32_t food_x, food_y;                /* 0x350 */
    uint32_t direction, game_over, score;   /* 0x358 */
    uint32_t last_tick, current_tick;       /* 0x364 */
    uint32_t background, body_color, error_color, food_color, text_color;
};

extern volatile struct snake_state snake_state;
extern volatile uint32_t snake_length;
void snake_cell(uint32_t x, uint32_t y, uint32_t color);
void snake_food(void);
void snake_reset(void);
void snake_move(void);
void snake_draw(void);
void snake_key(uint32_t scan, uint32_t character);
uint32_t snake_entry(void);
#endif
