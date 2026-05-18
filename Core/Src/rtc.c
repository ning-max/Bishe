#include "rtc.h"
#include "main.h"

/*
 * RTC register-level driver.
 *
 * We avoid the HAL RTC driver (stm32l0xx_hal_rtc.c) because it was not
 * included when CubeMX generated this project.  The RTC operations needed
 * for periodic wakeup are simple enough for direct register access.
 */

#define RTC_WAKEUP_COUNTER  20480U  /* LSE/16 = 2048 Hz × 10 s = 20480 ticks */

static void rtc_unlock(void)
{
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
}

static void rtc_lock(void)
{
    RTC->WPR = 0xFF;
}

void RTC_Init(void)
{
    RCC_OscInitTypeDef       osc  = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    /* 1. Enable LSE (32.768 kHz external crystal on PC14/PC15) */
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    /* 2. Route LSE to RTC */
    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pclk.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK)
        Error_Handler();

    __HAL_RCC_RTC_ENABLE();

    HAL_PWR_EnableBkUpAccess();

    /*
     * 3. Enter init mode, set prescalers, exit.
     *    LSE = 32768 Hz
     *    async = 127 → ck_apre = 256 Hz
     *    sync  = 255 → 1 Hz  (not used for wakeup timer, but required)
     */
    rtc_unlock();
    RTC->ISR |= RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_INITF)) {}

    RTC->PRER = (127U << 16) | 255U;

    RTC->ISR &= ~RTC_ISR_INIT;
    while (!(RTC->ISR & RTC_ISR_RSF)) {}
    rtc_lock();

    /*
     * 4. Enable EXTI line 20 (RTC wakeup timer) so it can wake us from Stop.
     */
    EXTI->IMR  |= (1U << 20);
    EXTI->RTSR |= (1U << 20);

    HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
}

void RTC_Set10sWakeup(void)
{
    rtc_unlock();

    /* Disable wakeup timer, wait for write-protect release */
    RTC->CR &= ~RTC_CR_WUTE;
    while (!(RTC->ISR & RTC_ISR_WUTWF)) {}

    /* Clear wakeup flag + pending EXTI */
    RTC->ISR &= ~RTC_ISR_WUTF;
    EXTI->PR = (1U << 20);

    /* Write reload value */
    RTC->WUTR = RTC_WAKEUP_COUNTER;

    /* Clock = RTCCLK / 16 (WUCKSEL = 00), enable wakeup timer + its interrupt */
    RTC->CR &= ~RTC_CR_WUCKSEL;           /* 00 = RTC/16 */
    RTC->CR |= RTC_CR_WUTIE | RTC_CR_WUTE;

    /* Clear PWR wakeup flag so we can enter Stop cleanly */
    PWR->CR |= PWR_CR_CWUF;

    rtc_lock();
}

void RTC_EnterStop(void)
{
    extern UART_HandleTypeDef huart1;

    /* Wait until UART TX shift register is empty */
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {}

    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    /*
     * Woken by RTC wakeup timer.
     * System clock is MSI (~2.1 MHz). Restore HSE+PLL → 32 MHz.
     */
    SystemClock_Config();
    HAL_ResumeTick();
}

void RTC_IRQHandler(void)
{
    /* Clear RTC wakeup flag — prevents interrupt from firing again */
    RTC->ISR &= ~RTC_ISR_WUTF;
    EXTI->PR = (1U << 20);
}
