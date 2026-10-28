#ifndef OPENICESTAR_CMOS_FIELDS_H
#define OPENICESTAR_CMOS_FIELDS_H
#include <stdint.h>
uint32_t kernel_cmos_seconds(void);
uint32_t kernel_cmos_minutes(void);
uint32_t kernel_cmos_hours(void);
uint32_t kernel_cmos_weekday(void);
uint32_t kernel_cmos_day(void);
uint32_t kernel_cmos_month(void);
uint32_t kernel_cmos_year(void);
#endif
