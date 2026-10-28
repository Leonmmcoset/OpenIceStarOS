#ifndef OPENICESTAR_MOV_H
#define OPENICESTAR_MOV_H
#include <stdint.h>

struct mov_strings {
    char title[9];
    char paths[4][30];
};
extern const struct mov_strings mov_strings;
extern volatile uint32_t mov_frames[4];
void mov_entry(void);
uint32_t mov_close(uint32_t window);
#endif
