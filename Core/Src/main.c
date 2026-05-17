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

#include <stdio.h>

static void SystemClock_Config(void);

static uint32_t last_tick, uptime;
static uint8_t  ok_bh1750, ok_bme280, ok_flash;

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

    /* Boot banner */
    printf("\r\n=================================\r\n");
    printf(" STM32L051 Environment Monitor\r\n");
    printf("=================================\r\n");

    /* ---- Reset cause diagnostic ---- */
    {
        uint32_t csr = RCC->CSR;
        printf(" [DBG] Reset flags:");
        if (csr & RCC_CSR_IWDGRSTF)  printf(" IWDG");
        if (csr & RCC_CSR_WWDGRSTF)  printf(" WWDG");
        if (csr & RCC_CSR_PORRSTF)   printf(" POR");
        if (csr & RCC_CSR_PINRSTF)   printf(" NRST");
        if (csr & RCC_CSR_SFTRSTF)   printf(" SFT");
        if (csr & RCC_CSR_OBLRSTF)   printf(" OBL");
        if (csr & RCC_CSR_LPWRRSTF)  printf(" LPWR");
        printf("\r\n");
        __HAL_RCC_CLEAR_RESET_FLAGS();
    }

    /* ---- W25Q64 Flash ---- */
    W25Q64_Init();
    uint32_t jedec = W25Q64_ReadJEDEC();
    if (jedec != 0xFFFFFF && jedec != 0x000000) {
        printf(" [OK] W25Q64  JEDEC=0x%06X\r\n", (unsigned int)jedec);
        ok_flash = 1;
        Storage_Init();
        printf(" [OK] Flash records: %d\r\n", Storage_Count());
        Storage_PrintAll();
    } else {
        printf(" [--] W25Q64  NOT FOUND\r\n");
        LED_R(1);
    }

    /* ---- BH1750 Light ---- */
    BH1750_Init();
    {
        uint16_t test_lux;
        if (BH1750_ReadLight(&test_lux)) {
            printf(" [OK] BH1750  I2C2 addr=0x23\r\n");
            ok_bh1750 = 1;
        } else {
            printf(" [--] BH1750  NOT FOUND\r\n");
            LED_R(1);
        }
    }

    /* ---- BME280 Env ---- */
    if (BME280_Init()) {
        printf(" [OK] BME280  I2C addr=0x76\r\n");
        ok_bme280 = 1;
    } else {
        printf(" [--] BME280  NOT FOUND\r\n");
        LED_R(1);
    }

    printf("=================================\r\n\r\n");

    /* Trigger first measurement immediately (not after 2s wait) */
    last_tick = HAL_GetTick() - 2000;

    while (1)
    {
        /* Kick the independent watchdog (IWDG) if enabled in option bytes */
        IWDG->KR = 0xAAAA;

        /* ---- Button: print stored records ---- */
        if (Button_ReadDebounced() && ok_flash) {
            LED_B(1);
            printf("\r\n--- Button: Dump Flash ---\r\n");
            Storage_PrintAll();
            printf("--- End ---\r\n\r\n");
            LED_B(0);
        }

        /* ---- 2-second sensor read ---- */
        if (HAL_GetTick() - last_tick >= 2000) {
            last_tick = HAL_GetTick();
            uptime += 2;

            LED_G(1);  /* Measure indicator ON */

            SensorRecord rec = {0};
            rec.timestamp = uptime;

            /* BH1750 */
            if (ok_bh1750) {
                uint16_t lux = 0;
                BH1750_ReadLight(&lux);
                rec.light = lux;
            }

            /* BME280 */
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

            /* ---- printf output ---- */
            printf("[%02u] ", (unsigned int)uptime);
            if (ok_bh1750)  printf("Lux:%-5u ", rec.light);
            else            printf("Lux:---   ");
            if (ok_bme280)  printf("T:%2d.%dC H:%2d.%d%% P:%uPa",
                                   rec.temp / 100,
                                   (rec.temp < 0 ? -rec.temp : rec.temp) % 100 / 10,
                                   rec.hum / 10, rec.hum % 10,
                                   (unsigned int)rec.press);
            else            printf("T:--- H:--- P:---");
            printf("\r\n");

            LED_G(0);  /* Measure indicator OFF */
        }
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSE=8MHz → PLL ×8 ÷2 = 32MHz (max for STM32L051) */
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

    /* Re-configure SysTick for 32MHz HCLK after clock switch */
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

void Error_Handler(void)
{
    __disable_irq();
    LED_RGB(1, 0, 0);  /* Red on error */
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { Error_Handler(); }
#endif
