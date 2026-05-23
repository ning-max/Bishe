#include "button.h"

static uint32_t last_check = 0;           /* 上次检测时间（去抖用） */

/* 按键初始化：上拉输入 */
void Button_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;              /* 上拉，按下=低电平 */
    gpio.Pin  = BTN_Pin;
    HAL_GPIO_Init(BTN_GPIO_Port, &gpio);
}

/* 读取按键当前状态（未去抖，1=按下） */
uint8_t Button_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin) == GPIO_PIN_RESET);
}

/*
 * 带消抖的按键读取
 * 流程：检测按下 → 间隔>300ms？ → 等20ms再确认 → 按下保持2秒内不重复触发
 * 返回：1=有效按下，0=无效或抖动脉冲
 */
uint8_t Button_ReadDebounced(void)
{
    if (Button_IsPressed()) {
        /* 两次触发间隔<300ms → 视为抖动或重复触发，忽略 */
        if (HAL_GetTick() - last_check < 300) return 0;
        last_check = HAL_GetTick();

        HAL_Delay(20);                     /* 等待20ms，跨越机械抖动期 */
        if (Button_IsPressed()) {          /* 再次确认仍然按下 */
            {
                /* 等待按键释放，但最多等2秒防止死等 */
                uint32_t t0 = HAL_GetTick();
                while (Button_IsPressed() && HAL_GetTick() - t0 < 2000) {}
            }
            return 1;                      /* 有效按键 */
        }
    }
    return 0;
}
