#ifndef ICESTAR_INIT_DIRECTORY_H
#define ICESTAR_INIT_DIRECTORY_H

#include <stdint.h>

int init_search_directory(const uint8_t *directory, uint32_t entries,
                          const char *name, int root_directory);

#endif
