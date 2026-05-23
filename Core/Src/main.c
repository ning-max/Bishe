#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

#include "led.h"
#include "button.h"
#include "bh1750.h"
#include "bme280.h"
#include "w25q64.h"
#include "sensor_storage.h"
#include "time_util.h"
#include "rtc.h"
#include "retarget.h"

void SystemClock_Config(void);

/* 外设就绪标志：1=正常, 0=未检测到或初始化失败 */
static uint8_t ok_bh1750, ok_bme280, ok_flash;

int main(void)
{
    /* ---- 系统与时钟初始化 ---- */
    HAL_Init();                    /* HAL库初始化 */
    SystemClock_Config();          /* HSE 8MHz → PLL ×8 ÷2 → 32MHz */

    /* ---- 外设初始化 ---- */
    MX_GPIO_Init();                /* GPIO端口时钟与引脚 */
    LED_Init();                    /* RGB LED */
    Button_Init();                 /* 用户按键 */
    MX_I2C1_Init();               /* I2C1: 给BME280 */
    MX_I2C2_Init();               /* I2C2: 给BH1750 */
    MX_SPI2_Init();               /* SPI2: 给W25Q64 Flash */
    MX_USART1_UART_Init();        /* USART1: 115200bps 串口输出 */

    /* ---- W25Q64 Flash 初始化 ---- */
    W25Q64_Init();
    uint32_t jedec = W25Q64_ReadJEDEC();        /* 读芯片ID验证焊接是否正常 */
    if (jedec != 0xFFFFFF && jedec != 0x000000) {
        ok_flash = 1;                           /* Flash正常 */
        Storage_Init();                         /* 从Flash恢复历史记录 */
    } else {
        LED_R(1);                               /* 红灯告警 */
    }

    /* ---- BH1750 光照传感器初始化 ---- */
    BH1750_Init();
    {
        uint16_t test_lux;
        if (BH1750_ReadLight(&test_lux))
            ok_bh1750 = 1;                      /* BH1750正常 */
        else
            LED_R(1);
    }

    /* ---- BME280 环境传感器初始化 ---- */
    if (BME280_Init())
        ok_bme280 = 1;                          /* BME280正常 */
    else
        LED_R(1);

    /* ---- RTC 初始化（LSE 32.768kHz晶振）---- */
    /* 首次上电：用编译时间初始化日历；后续复位：日历保持不覆盖 */
    RTC_Init(Time_CompileUnix());

#ifdef DEBUG
    /* 调试模式：Stop模式下保持SWD时钟，防止OpenOCD断连 */
    __HAL_RCC_DBGMCU_CLK_ENABLE();
    DBGMCU->CR |= DBGMCU_CR_DBG_STOP;
#endif

    HAL_Delay(100);               /* 等待UART稳定 */
    uart_puts("\r\n");            /* 输出换行，确认串口工作 */

    /* ---- 主循环：每10秒采集→存储→串口输出→进入Stop ---- */
    while (1)
    {
        LED_G(1);                 /* 绿灯亮：工作中 */

        SensorRecord rec = {0};
        rec.timestamp = RTC_GetUnixTime();      /* 从RTC日历读真实Unix时间戳 */

        /* 采集光照（BH1750） */
        if (ok_bh1750) {
            uint16_t lux = 0;
            BH1750_ReadLight(&lux);
            rec.light = lux;
        }

        /* 采集温度/湿度/气压（BME280） */
        if (ok_bme280) {
            BME280_Data d;
            BME280_StartMeasurement();          /* 触发一次强制测量 */
            HAL_Delay(BME280_T_MEAS_MS);        /* 等待转换完成 */
            if (BME280_ReadData(&d)) {          /* 读取并运行Bosch补偿公式 */
                rec.temp  = (int16_t)d.temp;    /* ℃×100 */
                rec.hum   = (uint16_t)((d.hum + 5) / 10);  /* %RH×10，四舍五入 */
                rec.press = d.press;            /* Pa */
            }
        }

        /* 存储到Flash并串口输出全部记录 */
        if (ok_flash) {
            Storage_Save(&rec);                 /* 写入环形缓冲区+Flash */
            Storage_PrintAll();                 /* 串口打印全部记录 */
        }

        LED_G(0);                 /* 绿灯灭：工作完成 */

        /* 设置10秒后RTC唤醒，进入Stop低功耗模式 */
        RTC_Set10sWakeup();
        RTC_EnterStop();
        /* 唤醒后clock_restore()已恢复32MHz，回到循环顶部 */
    }
}

/*
 * 系统时钟配置
 * HSE 8MHz → PLL ×8 ÷2 → HCLK=SYSCLK=PCLK1=PCLK2=32MHz
 * 外设时钟：USART1 用 PCLK2，I2C1 用 PCLK1
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    /* 调压器设为最高性能档（Range 1，支持32MHz） */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE=8MHz → PLL ×8 ÷2 = 32MHz */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLLMUL_8;
    osc.PLL.PLLDIV = RCC_PLLDIV_2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    /* 系统时钟源选PLL，AHB/APB1/APB2 均不分频 */
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                    | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

    /* USART1时钟源=PCLK2，I2C1时钟源=PCLK1 */
    pclk.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_I2C1;
    pclk.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    pclk.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) Error_Handler();

    /* SysTick = HCLK / 1000 = 1ms 滴答 */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

/* 致命错误处理：关中断，亮红灯，死循环 */
void Error_Handler(void)
{
    __disable_irq();
    LED_RGB(1, 0, 0);
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { Error_Handler(); }
#endif
