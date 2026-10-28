#ifndef OPENICESTAR_KERNEL_JPEG_SCAN_H
#define OPENICESTAR_KERNEL_JPEG_SCAN_H
#include <stdint.h>
uint32_t kernel_jpeg_scan_83af27(uint32_t context);
uint32_t kernel_jpeg_prepare_83b233(uint32_t context);
uint32_t kernel_jpeg_symbol_83b351(uint32_t context, uint32_t table_class,
                                    uint32_t table_index);
#endif
