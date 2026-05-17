#include "led.h"

void LED_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = LED_R_Pin | LED_G_Pin | LED_B_Pin;
    HAL_GPIO_Init(LED_R_GPIO_Port, &gpio);

    LED_AllOff();
}

void LED_R(uint8_t on) { HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET); }
void LED_G(uint8_t on) { HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET); }
void LED_B(uint8_t on) { HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET); }
void LED_AllOff(void)   { LED_R(0); LED_G(0); LED_B(0); }

void LED_RGB(uint8_t r, uint8_t g, uint8_t b) { LED_R(r); LED_G(g); LED_B(b); }
