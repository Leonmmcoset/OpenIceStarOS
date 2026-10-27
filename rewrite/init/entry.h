#ifndef ICESTAR_INIT_ENTRY_H
#define ICESTAR_INIT_ENTRY_H

#include <stdint.h>

int init_find_root_directory(const char *name);
int init_read_root_directory(int index, uint8_t *destination);
int init_find_file(const uint8_t *directory, const char *name);
int init_read_file(const uint8_t *entry, uint8_t *destination);

int init_load_file(const char *name, uint8_t *destination);
void init_entry(void);

#endif
