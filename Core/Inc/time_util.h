#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H__
#include <stdint.h>

uint32_t Time_CompileUnix(void);
void     Time_UnixToStr(uint32_t ts, char *buf, uint8_t buf_sz);

#endif
