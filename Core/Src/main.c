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

static uint8_t ok_bh1750, ok_bme280, ok_flash;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    LED_Init();
    Button_Init();
    MX_I2C1_Init();
    MX_I2C2_Init();
    MX_SPI2_Init();
    MX_USART1_UART_Init();

    /* W25Q64 Flash */
    W25Q64_Init();
    uint32_t jedec = W25Q64_ReadJEDEC();
    if (jedec != 0xFFFFFF && jedec != 0x000000) {
        ok_flash = 1;
        Storage_Init();
    } else {
        LED_R(1);
    }

    /* BH1750 Light */
    BH1750_Init();
    {
        uint16_t test_lux;
        if (BH1750_ReadLight(&test_lux))
            ok_bh1750 = 1;
        else
            LED_R(1);
    }

    /* BME280 Env */
    if (BME280_Init())
        ok_bme280 = 1;
    else
        LED_R(1);

    /* RTC with LSE (32.768 kHz) for 10 s periodic Stop wakeup.
       Seed with compile time on first boot; calendar persists through resets. */
    RTC_Init(Time_CompileUnix());

#ifdef DEBUG
    __HAL_RCC_DBGMCU_CLK_ENABLE();
    DBGMCU->CR |= DBGMCU_CR_DBG_STOP;
#endif

    HAL_Delay(100);
    uart_puts("\r\n");

    while (1)
    {
        LED_G(1);

        SensorRecord rec = {0};
        rec.timestamp = RTC_GetUnixTime();

        if (ok_bh1750) {
            uint16_t lux = 0;
            BH1750_ReadLight(&lux);
            rec.light = lux;
        }

        if (ok_bme280) {
            BME280_Data d;
            BME280_StartMeasurement();
            HAL_Delay(BME280_T_MEAS_MS);
            if (BME280_ReadData(&d)) {
                rec.temp  = (int16_t)d.temp;
                rec.hum   = (uint16_t)((d.hum + 5) / 10);
                rec.press = d.press;
            }
        }

        if (ok_flash) {
            Storage_Save(&rec);
            Storage_PrintAll();
        }

        LED_G(0);

        RTC_Set10sWakeup();
        RTC_EnterStop();
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE=8MHz -> PLL x8 /2 = 32MHz */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLLMUL_8;
    osc.PLL.PLLDIV = RCC_PLLDIV_2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                    | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) Error_Handler();

    pclk.PeriphClockSelection = RCC_PERIPHCLK_USART1 | RCC_PERIPHCLK_I2C1;
    pclk.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    pclk.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&pclk) != HAL_OK) Error_Handler();

    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

void Error_Handler(void)
{
    __disable_irq();
    LED_RGB(1, 0, 0);
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { Error_Handler(); }
#endif
