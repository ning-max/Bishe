#include "button.h"

static uint32_t last_check = 0;

void Button_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Pin  = BTN_Pin;
    HAL_GPIO_Init(BTN_GPIO_Port, &gpio);
}

uint8_t Button_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET);
}

uint8_t Button_ReadDebounced(void)
{
    if (Button_IsPressed()) {
        if (HAL_GetTick() - last_check < 300) return 0;
        last_check = HAL_GetTick();
        HAL_Delay(20);
        if (Button_IsPressed()) {
            {
                uint32_t t0 = HAL_GetTick();
                while (Button_IsPressed() && HAL_GetTick() - t0 < 2000) {}
            }
            return 1;
        }
    }
    return 0;
}
