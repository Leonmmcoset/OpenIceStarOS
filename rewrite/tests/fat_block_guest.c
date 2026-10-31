/* Diagnostic X only: exercise all eight readers against a disposable disk. */
#include <stdint.h>
#define B(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define W(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define D(a) (*(volatile uint32_t *)(uintptr_t)(a))
typedef uint32_t (*one_fn)(uint32_t);
typedef uint32_t (*two_fn)(uint32_t, uint32_t);
typedef uint32_t (*three_fn)(uint32_t, uint32_t, uint32_t);
typedef void (*free_fn)(uint32_t);
typedef void (*no_fn)(void);
static uint8_t buffer[0x10000];
static uint8_t directory[64];
volatile uint32_t block_report[22];

static uint32_t hash(uint32_t address, uint32_t size)
{
    uint32_t value = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) value = (value ^ B(address+i)) * 16777619u;
    return value;
}

void fat_guest_entry(void)
{
    uint32_t target = (uint32_t)(uintptr_t)buffer;
    uint32_t root = 0x520000u + (W(0x52000eu) + B(0x520010u)*W(0x520016u))*W(0x52000bu);
    uint32_t start = D(0x7dc6u) + W(0x52000eu) + B(0x520010u)*W(0x520016u)
        + (W(0x520011u)*32u+W(0x52000bu)-1u)/W(0x52000bu);
    uint32_t lba = start + (W(root+5u*32u+26u)-2u)*B(0x52000du);
    block_report[0] = 0x424c4b31u;
    uint32_t allocated = ((two_fn)0x825476)(lba,3);
    block_report[1] = allocated != 0;
    block_report[2] = hash(allocated,1536);
    ((free_fn)0x815978)(allocated);
    block_report[3] = ((three_fn)0x8254ca)(lba,3,target) == target;
    block_report[4] = hash(target,1536);
    allocated = ((one_fn)0x8252b4)(lba);
    block_report[5] = allocated != 0;
    block_report[6] = hash(allocated,1536);
    ((free_fn)0x815978)(allocated);
    block_report[7] = ((two_fn)0x82539e)(lba,target) == target;
    block_report[8] = hash(target,1536);
    block_report[9] = ((two_fn)0x825506)(0,target) == target+4096;
    block_report[10] = hash(target,512);
    block_report[11] = hash(target+4096,32768);
    block_report[12] = ((two_fn)0x8255ae)(4,target);
    block_report[13] = hash(target,8192);
    allocated = ((one_fn)0x8256a8)(4);
    block_report[14] = allocated != 0;
    block_report[15] = hash(allocated,8192);
    ((free_fn)0x815978)(allocated);
    for (uint32_t i = 0; i < 32; ++i) directory[32+i] = B(root+4u*32u+i);
    allocated = ((two_fn)0x8257bc)(1,(uint32_t)(uintptr_t)directory);
    block_report[16] = allocated != 0;
    block_report[17] = hash(allocated,8192);
    ((free_fn)0x815978)(allocated);
    block_report[18] = ((two_fn)0x8255ae)(5,target);
    block_report[19] = ((one_fn)0x8256a8)(5);
    directory[43] = 0x20;
    block_report[20] = ((two_fn)0x8257bc)(1,(uint32_t)(uintptr_t)directory);
    block_report[21] = ((three_fn)0x8254ca)(lba,0xffffffffu,target) == target;
    for (uint32_t i = 0; i < 22; ++i) {
        uint32_t value = block_report[i];
        for (uint32_t j = 0; j < 4; ++j) {
            __asm__ volatile("outb %0, %1" : : "a"((uint8_t)value), "Nd"((uint16_t)0xe9));
            value >>= 8;
        }
    }
    for (;;) ((no_fn)(uintptr_t)D(D(0x1b0e4u)+108u*4u))();
}
