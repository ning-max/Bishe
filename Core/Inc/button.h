#ifndef __BUTTON_H__
#define __BUTTON_H__

#include "main.h"

void    Button_Init(void);
uint8_t Button_IsPressed(void);
uint8_t Button_ReadDebounced(void);  /* returns 1 on press, auto-clears */

#endif
