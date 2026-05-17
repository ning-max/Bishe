#include "main.h"
#include "usart.h"
#include <stdio.h>

extern UART_HandleTypeDef huart1;

int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}
