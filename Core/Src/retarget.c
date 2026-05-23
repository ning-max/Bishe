#include "main.h"
#include "usart.h"

extern UART_HandleTypeDef huart1;

/* 发送单个字符（阻塞，直到发送完成） */
void uart_putc(char c)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

/* 发送字符串（逐字符发送，遇'\0'停止） */
void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

/* 发送无符号十进制整数（避免引入printf/newlib） */
void uart_putu(uint32_t n)
{
    char buf[11];               /* 最多10位+终止符 */
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

/* 发送有符号十进制整数 */
void uart_puti(int32_t n)
{
    if (n < 0) { uart_putc('-'); n = -n; }
    uart_putu((uint32_t)n);
}
