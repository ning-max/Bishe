#include "rtc.h"
#include "main.h"
#include "time_util.h"

/* RTC唤醒计数器：LSE/16=2048Hz × 10秒 = 20480个计数 */
#define RTC_WAKEUP_COUNTER  20480U

static RTC_HandleTypeDef hrtc;

/*
 * RTC初始化
 * 参数 unix_time：首次上电时的初始Unix时间戳（用编译时间）
 * - 使能LSE 32.768kHz晶振，路由到RTC
 * - 异步预分频127、同步预分频255 → 1Hz日历时钟
 * - 首次上电（备份域丢失）：写入初始日历时间
 * - 后续复位（备份域保留）：日历保持继续走
 * - 配置EXTI线20用于RTC唤醒定时器
 */
void RTC_Init(uint32_t unix_time)
{
    RCC_OscInitTypeDef       osc  = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    /* 使能LSE（32.768kHz外部低速晶振） */
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    /* 将LSE路由为RTC时钟源 */
    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pclk.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK)
        Error_Handler();

    __HAL_RCC_RTC_ENABLE();              /* 使能RTC外设时钟 */
    HAL_PWR_EnableBkUpAccess();          /* 使能备份域访问 */

    /* 记录RTC是否已经初始化过（INITS标志在备份域，复位不丢失） */
    uint8_t was_init = (RTC->ISR & RTC_ISR_INITS) ? 1 : 0;

    /* 配置RTC预分频器和输出 */
    hrtc.Instance          = RTC;
    hrtc.Init.HourFormat   = RTC_HOURFORMAT_24;     /* 24小时制 */
    hrtc.Init.AsynchPrediv = 127;                   /* 32768/(127+1)=256Hz */
    hrtc.Init.SynchPrediv  = 255;                   /* 256/(255+1)=1Hz */
    hrtc.Init.OutPut       = RTC_OUTPUT_DISABLE;    /* 不需要RTC输出引脚 */
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType   = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
        Error_Handler();

    /* 首次上电：用编译时间初始化RTC日历 */
    if (!was_init) {
        uint16_t year;
        uint8_t  mon, day, hour, min, sec;

        /* Unix时间戳 → 日历分量 */
        Time_UnixToCalendar(unix_time, &year, &mon, &day, &hour, &min, &sec);

        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};

        sTime.Hours   = hour;
        sTime.Minutes = min;
        sTime.Seconds = sec;
        sDate.Date    = day;
        sDate.Month   = mon;
        sDate.Year    = (uint8_t)(year - 2000);     /* RTC年份以2000为基准 */

        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    }

    /* 配置EXTI线20：RTC唤醒定时器，用于从Stop模式唤醒 */
    __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_RISING_EDGE();
    __HAL_RTC_WAKEUPTIMER_EXTI_ENABLE_IT();

    HAL_NVIC_SetPriority(RTC_IRQn, 1, 0);    /* 抢占优先级1，子优先级0 */
    HAL_NVIC_EnableIRQ(RTC_IRQn);
}

/* 设置RTC唤醒定时器为10秒后触发 */
void RTC_Set10sWakeup(void)
{
    /* 唤醒时钟=LSE/16，计数20480 → 正好10秒 */
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, RTC_WAKEUP_COUNTER,
                                RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);       /* 清除PWR唤醒标志 */
}

/*
 * Stop模式唤醒后恢复系统时钟
 * 唤醒时系统跑在MSI(~2.1MHz)，需要重新启用HSE+PLL切回32MHz
 * 注意：外设时钟选择（USART1,I2C1）在Stop期间保留，无需重新配置
 */
static void clock_restore(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* 重新使能HSE+PLL */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLLMUL_8;
    osc.PLL.PLLDIV     = RCC_PLLDIV_2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        Error_Handler();

    /* 系统时钟源切回PLL */
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK)
        Error_Handler();

    /* 恢复SysTick 1ms滴答 */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/*
 * 进入Stop低功耗模式
 * - 等待UART发送完成
 * - 挂起SysTick
 * - 进入Stop（WFI，低功耗调压器开启）
 * - RTC唤醒后调用clock_restore()恢复时钟
 */
void RTC_EnterStop(void)
{
    extern UART_HandleTypeDef huart1;

    /* 等待UART移位寄存器完全空闲，防止丢最后一个字节 */
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {}

    HAL_SuspendTick();                                               /* 挂起SysTick */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); /* 进入Stop */

    clock_restore();                                                 /* 唤醒后恢复32MHz */
    HAL_ResumeTick();                                                /* 恢复SysTick */
}

/* 从RTC日历读取当前Unix时间戳 */
uint32_t RTC_GetUnixTime(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    /* 日历分量 → Unix时间戳 */
    return Time_CalendarToUnix(2000 + (uint16_t)sDate.Year,
                               sDate.Month, sDate.Date,
                               sTime.Hours, sTime.Minutes, sTime.Seconds);
}

/* RTC中断服务函数：处理唤醒定时器事件 */
void RTC_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);  /* HAL内部清除WUTF标志 */
}
