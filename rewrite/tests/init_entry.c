#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "entry.h"

static int fail_at, calls;
static uint8_t buffer[128];

void kernel_init_startup_record(void) { }

int init_find_root_directory(const char *name)
{
    assert(!strcmp(name, "ICESTAR"));
    ++calls;
    return fail_at == 1 ? -1 : 3;
}

int init_read_root_directory(int index, uint8_t *destination)
{
    assert(index == 3 && destination == buffer);
    ++calls;
    return fail_at == 2 ? 0 : 1;
}

int init_find_file(const uint8_t *directory, const char *name)
{
    assert(directory == buffer && !strcmp(name, "DAT.BIN"));
    ++calls;
    return fail_at == 3 ? -1 : 2;
}

int init_read_file(const uint8_t *entry, uint8_t *destination)
{
    assert(entry == buffer + 64 && destination == buffer);
    ++calls;
    return fail_at == 4 ? 0 : 1;
}

int main(void)
{
    for (fail_at = 1; fail_at <= 4; ++fail_at) {
        calls = 0;
        assert(init_load_file("DAT.BIN", buffer) == 0);
        assert(calls == fail_at);
    }
    fail_at = calls = 0;
    assert(init_load_file("DAT.BIN", buffer) == 1);
    assert(calls == 4);
    return 0;
}
