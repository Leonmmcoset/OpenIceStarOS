#include <assert.h>
#include <stdint.h>

void kernel_clear_startup_record(volatile uint32_t *record);

int main(void)
{
    uint32_t words[10];
    unsigned i;
    for (i = 0; i < 10; ++i) words[i] = 0x12345678;
    kernel_clear_startup_record(words + 1);
    assert(words[0] == 0x12345678 && words[9] == 0x12345678);
    for (i = 1; i < 9; ++i) assert(words[i] == 0);
    return 0;
}
