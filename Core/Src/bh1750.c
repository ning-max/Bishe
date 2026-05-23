#include "bh1750.h"

extern I2C_HandleTypeDef hi2c2;

/* BH1750上电初始化 */
void BH1750_Init(void)
{
    uint8_t cmd = BH1750_POWER_ON;
    HAL_I2C_Master_Transmit(&hi2c2, BH1750_ADDR << 1, &cmd, 1, 100);
}

/*
 * 读取光照强度
 * 使用高分辨率模式（测量时间约180ms），单位：lux
 * 返回值：0=失败，1=成功
 */
uint8_t BH1750_ReadLight(uint16_t *lux)
{
    uint8_t cmd = BH1750_HRES_MODE;       /* 高分辨率模式指令 */
    uint8_t buf[2] = {0};

    /* 启动测量 */
    if (HAL_I2C_Master_Transmit(&hi2c2, BH1750_ADDR << 1, &cmd, 1, 100) != HAL_OK)
        return 0;
    HAL_Delay(180);                        /* 等待高分辨率转换完成 */
    /* 读取16-bit原始值 */
    if (HAL_I2C_Master_Receive(&hi2c2, BH1750_ADDR << 1, buf, 2, 200) != HAL_OK)
        return 0;

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *lux = raw * 10 / 12;                 /* 官方转换公式：raw / 1.2 = raw × 10 / 12 */
    return 1;
}
