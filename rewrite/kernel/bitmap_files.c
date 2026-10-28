/* BMP file wrappers at 0x81fec2 and 0x82040c. Only C:..F: followed by
 * backslash use the absolute resolver. Preserve signed lookup results,
 * live caller arguments, draw-before-free and raw EAX failure/free results. */
#include "bitmap_files.h"
#include "bitmap.h"
#define BYTE(a) (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define DWORD(a) (*(volatile uint32_t *)(uintptr_t)(uint32_t)(a))
uint32_t kernel_startup_lookup_8263c0(uint32_t,uint32_t *);
uint32_t kernel_startup_read_alloc_827454(uint32_t *);
uint32_t kernel_bitmap_lookup_relative_825c8e(uint32_t);
uint32_t kernel_bitmap_read_relative_827032(uint32_t);
uint32_t kernel_bitmap_fit_8204ac(uint32_t,uint32_t);
/* Address alias: the public allocator API is void, but these wrappers
 * leave the free routine's EAX in the caller-visible return register. */
uint32_t kernel_bitmap_free_815978(uint32_t);

#define LOAD_BITMAP(name) \
    uint32_t record[8],file; \
    if((int8_t)BYTE(name)>'B' && (int8_t)BYTE(name)<='F' && \
       BYTE((name)+1u)==':' && BYTE((name)+2u)=='\\'){ \
        result=kernel_startup_lookup_8263c0(name,record); \
        if((int32_t)result<=0)return result; \
        file=kernel_startup_read_alloc_827454(record); \
    }else{ \
        result=kernel_bitmap_lookup_relative_825c8e(name); \
        if((int32_t)result<0)return result; \
        file=kernel_bitmap_read_relative_827032(result); \
    } \
    if(!file)return 0

uint32_t kernel_bitmap_draw_file_81fec2(volatile uint32_t id,volatile uint32_t x,
    volatile uint32_t y,volatile uint32_t name)
{
    uint32_t result;
    __asm__ volatile("":"=a"(result)::"memory");
    if((int32_t)x<0)return result;
    result=DWORD(DWORD(0x500000u+id*4u)+12u);
    if((int32_t)result<=(int32_t)x || (int32_t)y<0)return result;
    result=DWORD(DWORD(0x500000u+id*4u)+16u);
    if((int32_t)result<=(int32_t)y)return result;
    LOAD_BITMAP(name);
    kernel_bitmap_draw_81ffc4(id,x,y,file);
    return kernel_bitmap_free_815978(file);
}

uint32_t kernel_bitmap_fit_file_82040c(volatile uint32_t id,volatile uint32_t name)
{
    uint32_t result;
    LOAD_BITMAP(name);
    kernel_bitmap_fit_8204ac(id,file);
    return kernel_bitmap_free_815978(file);
}
