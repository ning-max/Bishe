#include "main.h"
#include "usart.h"

extern UART_HandleTypeDef huart1;

void uart_putc(char c)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

void uart_putu(uint32_t n)
{
    char buf[11];
    uint8_t pos = sizeof(buf);
    buf[--pos] = '\0';
    if (n == 0) buf[--pos] = '0';
    else {
        while (n) {
            buf[--pos] = '0' + (n % 10);
            n /= 10;
        }
    }
    uart_puts(&buf[pos]);
}

void uart_puti(int32_t n)
{
    if (n < 0) { uart_putc('-'); n = -n; }
    uart_putu((uint32_t)n);
}
