#pragma once

#include <stdint.h>

void     RTC_Init(uint32_t unix_time);
void     RTC_Set10sWakeup(void);
void     RTC_EnterStop(void);
uint32_t RTC_GetUnixTime(void);
