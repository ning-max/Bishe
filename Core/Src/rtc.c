#include "rtc.h"
#include "main.h"
#include "time_util.h"

#define RTC_WAKEUP_COUNTER  20480U  /* LSE/16 = 2048 Hz * 10 s */

static RTC_HandleTypeDef hrtc;

void RTC_Init(uint32_t unix_time)
{
    RCC_OscInitTypeDef       osc  = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    /* Enable LSE (32.768 kHz) */
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    /* Route LSE to RTC */
    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pclk.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK)
        Error_Handler();

    __HAL_RCC_RTC_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* Check whether RTC was already initialized (backup domain preserved) */
    uint8_t was_init = (RTC->ISR & RTC_ISR_INITS) ? 1 : 0;

    hrtc.Instance          = RTC;
    hrtc.Init.HourFormat   = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv  = 255;
    hrtc.Init.OutPut       = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType   = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
        Error_Handler();

    if (!was_init) {
        /* First boot or backup domain lost — seed RTC calendar */
        uint16_t year;
        uint8_t  mon, day, hour, min, sec;

        Time_UnixToCalendar(unix_time, &year, &mon, &day, &hour, &min, &sec);

        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};

        sTime.Hours   = hour;
        sTime.Minutes = min;
        sTime.Seconds = sec;
        sDate.Date    = day;
        sDate.Month   = mon;
        sDate.Year    = (uint8_t)(year - 2000);

        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    }

    /* EXTI line 20 — RTC wakeup timer, wake from Stop */
    __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_RISING_EDGE();
    __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_IT();

    HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
}

void RTC_Set10sWakeup(void)
{
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_WAKEUP_COUNTER,
                                RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
}

static void clock_restore(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLLMUL_8;
    osc.PLL.PLLDIV     = RCC_PLLDIV_2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK)
        Error_Handler();

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

void RTC_EnterStop(void)
{
    extern UART_HandleTypeDef huart1;

    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {}

    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    clock_restore();
    HAL_ResumeTick();
}

uint32_t RTC_GetUnixTime(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    return Time_CalendarToUnix(2000 + (uint16_t)sDate.Year,
                               sDate.Month, sDate.Date,
                               sTime.Hours, sTime.Minutes, sTime.Seconds);
}

void RTC_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}
