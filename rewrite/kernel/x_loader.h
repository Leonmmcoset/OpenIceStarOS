#ifndef OPENICESTAR_KERNEL_X_LOADER_H
#define OPENICESTAR_KERNEL_X_LOADER_H
#include <stdint.h>

uint32_t kernel_x_load_relative_8020a6(uint32_t name);
uint32_t kernel_x_load_path_80214c(uint32_t name);
uint32_t kernel_startup_launch_image(uint32_t image, uint32_t name);
#endif
