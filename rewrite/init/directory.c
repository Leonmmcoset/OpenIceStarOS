#include "directory.h"
#include "entry.h"

static int entry_name(const uint8_t *entry, char *name, int root_directory)
{
    unsigned i, length = 0;

    if (entry[0] == 0) return 0;
    if (entry[0] == 0xe5) return -1;
    if (entry[11] == 0x0f) return -2;
    for (i = 0; i < 8; ++i) {
        if (entry[i] == ' ') {
            if (entry[8] != ' ') name[length++] = '.';
            else goto finished;
            break;
        }
        name[length++] = (char)entry[i];
    }
    if (i == 8 && entry[8] != ' ') name[length++] = '.';
    for (i = 0; i < 3 && entry[8 + i] != ' '; ++i)
        name[length++] = (char)entry[8 + i];
finished:
    name[length] = 0;
    if (root_directory && (entry[11] & 8)) return -3;
    return 1;
}

int init_search_directory(const uint8_t *directory, uint32_t entries,
                          const char *name, int root_directory)
{
    uint32_t i;
    for (i = 0; i < entries; ++i) {
        const uint8_t *entry = directory + i * 32;
        char decoded[13];
        unsigned j;
        int status = entry_name(entry, decoded, root_directory);
        if (!status) break;
        if (status != 1) continue;
        for (j = 0; decoded[j] && decoded[j] == name[j]; ++j) { }
        if (decoded[j] != name[j]) continue;
        if (root_directory ? (entry[11] & 0x10) : !(entry[11] & 0x18))
            return (int)i;
    }
    return -1;
}

#ifndef INIT_DIRECTORY_HOST_TEST
static uint16_t word(const uint8_t *p)
{
    return (uint16_t)(p[0] | (uint16_t)p[1] << 8);
}

int init_find_root_directory(const char *name)
{
    const uint8_t *bpb = (const uint8_t *)0x520000;
    const uint8_t *root = bpb + (word(bpb + 14) + bpb[16] * word(bpb + 22)) * word(bpb + 11);
    return init_search_directory(root, word(bpb + 17), name, 1);
}

int init_find_file(const uint8_t *directory, const char *name)
{
    const uint8_t *bpb = (const uint8_t *)0x520000;
    return init_search_directory(directory, word(bpb + 11) * bpb[13] / 32, name, 0);
}
#endif
