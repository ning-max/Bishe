#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H__

#include <stdint.h>

uint32_t Time_CompileUnix(void);
void     Time_UnixToStr(uint32_t ts, char *buf, uint8_t buf_sz);
void     Time_UnixToCalendar(uint32_t ts, uint16_t *year, uint8_t *mon,
                             uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec);
uint32_t Time_CalendarToUnix(uint16_t year, uint8_t mon, uint8_t day,
                             uint8_t hour, uint8_t min, uint8_t sec);

#endif
