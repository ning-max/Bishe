#ifndef __BH1750_H__
#define __BH1750_H__

#include "main.h"

#define BH1750_ADDR       0x23
#define BH1750_POWER_ON   0x01
#define BH1750_HRES_MODE  0x20

void    BH1750_Init(void);
uint8_t BH1750_ReadLight(uint16_t *lux);

#endif
