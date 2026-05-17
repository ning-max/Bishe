#ifndef __LED_H__
#define __LED_H__

#include "main.h"

void LED_Init(void);
void LED_R(uint8_t on);
void LED_G(uint8_t on);
void LED_B(uint8_t on);
void LED_RGB(uint8_t r, uint8_t g, uint8_t b);
void LED_AllOff(void);

#endif
