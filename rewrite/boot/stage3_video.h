#ifndef ICESTAR_STAGE3_VIDEO_H
#define ICESTAR_STAGE3_VIDEO_H
#include <stdint.h>
void stage_video_select(void);
uint32_t stage_video_bios(uint32_t function,uint32_t argument);
uint32_t stage_video_read(uint32_t address,uint32_t bytes);
void stage_video_write(uint32_t address,uint32_t value,uint32_t bytes);
#endif
