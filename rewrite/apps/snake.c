/* Complete application logic recovered from SNAKE.X, SHA-256
 * be2bbb4d1369977369bdc5292a29d0f02350d92bf4207e9f6123466c6f570488.
 * Original routine offsets: 40, a8, 102, 182, 2ac, 4ca, 5dc.
 */
#include "snake.h"
#include "xapi.h"

volatile uint32_t snake_length __attribute__((section(".snake_length"))) = 1;
volatile struct snake_state snake_state __attribute__((section(".bss.snake"), aligned(32)));
_Static_assert(sizeof(struct snake_state) == 0x380, "original BSS layout");
_Static_assert(__builtin_offsetof(struct snake_state, y) == 0x1c0, "original Y array");
_Static_assert(__builtin_offsetof(struct snake_state, food_x) == 0x350, "original food position");

const struct {
    char game_over[31], title[11], failure[25];
} snake_strings __attribute__((section(".snake_strings"), aligned(1))) = {
    "Game Over! Press R to restart.", "Snake Game", "Failed to create window!"
};

/* Integer-address accesses deliberately retain wraparound and unchecked array
 * overflow into neighboring storage. Adding a capacity check would change the
 * recovered game. DWORD arithmetic below is unsigned; branches use the
 * original signed comparisons explicitly. */
#define X(i) (*(volatile uint32_t *)((uintptr_t)&snake_state.x[0] + (uint32_t)(i) * 4u))
#define Y(i) (*(volatile uint32_t *)((uintptr_t)&snake_state.y[0] + (uint32_t)(i) * 4u))

void snake_cell(uint32_t x, uint32_t y, uint32_t color)
{
    x_rectangle(snake_state.surface, x * 10u + 10u, y * 10u + 40u,
                x * 10u + 20u, y * 10u + 50u, color);
}

void snake_food(void)
{
    snake_state.food_x = x_random(0, 29);
    snake_state.food_y = x_random(0, 19);
    snake_cell(snake_state.food_x, snake_state.food_y, snake_state.food_color);
}

void snake_reset(void)
{
    snake_length = 1;
    X(0) = 15;
    Y(0) = 10;
    snake_state.direction = 0;
    snake_state.game_over = 0;
    snake_state.score = 0;
    snake_food();
    /* Original order: food is drawn before the background is cleared. */
    x_rectangle(snake_state.surface, 10, 40, 310, 240, snake_state.background);
}

void snake_move(void)
{
    uint32_t i;
    for (i = snake_length - 1u; (int32_t)i > 0; --i) {
        X(i) = X(i - 1u);
        Y(i) = Y(i - 1u);
    }
    if (snake_state.direction == 0) ++X(0);
    else if (snake_state.direction == 1) ++Y(0);
    else if (snake_state.direction == 2) --X(0);
    else if (snake_state.direction == 3) --Y(0);
    if (X(0) == snake_state.food_x && Y(0) == snake_state.food_y) {
        ++snake_length;
        snake_state.score += 10u;
        snake_food();
    }
    if ((int32_t)X(0) < 0 || (int32_t)X(0) > 29 ||
        (int32_t)Y(0) < 0 || (int32_t)Y(0) > 19) snake_state.game_over = 1;
    for (i = 1; (int32_t)i < (int32_t)snake_length; ++i)
        if (X(0) == X(i) && Y(0) == Y(i)) snake_state.game_over = 1;
}

void snake_draw(void)
{
    char text[32], digits[16];
    char *output = text;
    int count = 0;
    int32_t number;
    uint32_t i;
    if (snake_state.game_over) {
        x_text(snake_state.surface, 40, 140, snake_state.error_color,
               snake_state.background, snake_strings.game_over);
        return;
    }
    x_rectangle(snake_state.surface, 10, 40, 310, 240, snake_state.background);
    for (i = 0; (int32_t)i < (int32_t)snake_length; ++i)
        snake_cell(X(i), Y(i), snake_state.body_color);
    snake_cell(snake_state.food_x, snake_state.food_y, snake_state.food_color);
    *output++ = 'S'; *output++ = 'c'; *output++ = 'o'; *output++ = 'r';
    *output++ = 'e'; *output++ = ':'; *output++ = ' ';
    number = (int32_t)snake_state.score;
    if (number == 0) *output++ = '0';
    else {
        /* Negative scores originally produce just the prefix, not a minus. */
        while (number > 0) {
            digits[count++] = (char)('0' + number % 10);
            number /= 10;
        }
        while (count > 0) *output++ = digits[--count];
    }
    *output = 0;
    x_text(snake_state.surface, 10, 245, snake_state.background,
           snake_state.text_color, text);
}

void snake_key(uint32_t scan, uint32_t character)
{
    if (character == 0) {
        if (scan == 0x4d && snake_state.direction != 2) snake_state.direction = 0;
        else if (scan == 0x50 && snake_state.direction != 3) snake_state.direction = 1;
        else if (scan == 0x4b && snake_state.direction != 0) snake_state.direction = 2;
        else if (scan == 0x48 && snake_state.direction != 1) snake_state.direction = 3;
    } else if (character == 'r' && (int32_t)snake_state.game_over > 0) snake_reset();
    else if (character == 'd' && snake_state.direction != 2) snake_state.direction = 0;
    else if (character == 's' && snake_state.direction != 3) snake_state.direction = 1;
    else if (character == 'a' && snake_state.direction != 0) snake_state.direction = 2;
    else if (character == 'w' && snake_state.direction != 1) snake_state.direction = 3;
}

uint32_t snake_entry(void)
{
    volatile uint32_t events[20];
    uint32_t handle, i;
    snake_state.background = x_color(0);
    snake_state.body_color = x_color(7);
    snake_state.error_color = x_color(1);
    snake_state.food_color = x_color(2);
    snake_state.text_color = x_color(8);
    snake_state.window = x_window_create(100, 100, 320, 270, 1,
                                        snake_state.background, 2, snake_strings.title);
    if ((int32_t)snake_state.window <= 0)
        return x_text(snake_state.surface, 10, 40, snake_state.error_color,
                      snake_state.background, snake_strings.failure);
    for (i = 0; i < 20; ++i) events[i] = 0;
    handle = x_service_41_01(snake_state.window);
    snake_state.surface = x_surface(snake_state.window);
    x_events(snake_state.window, events);
    snake_reset();
    x_service_40_13(snake_state.window);
    snake_state.last_tick = x_ticks();
    for (;;) {
        if (events[2]) {
            events[2] = 0;
            x_service_40_17(handle);
        }
        if (events[15]) {
            events[15] = 0;
            snake_key(events[17], events[18]);
        }
        if (snake_state.game_over) continue;
        snake_state.current_tick = x_ticks();
        if ((int32_t)(snake_state.current_tick - snake_state.last_tick) > 50) {
            snake_state.last_tick = snake_state.current_tick;
            snake_move();
            snake_draw();
            x_service_40_16(snake_state.window);
        }
    }
}
