#include "bh1750.h"

extern I2C_HandleTypeDef hi2c2;

void BH1750_Init(void)
{
    uint8_t cmd = BH1750_POWER_ON;
    HAL_I2C_Master_Transmit(&hi2c2, BH1750_ADDR << 1, &cmd, 1, 100);
}

uint8_t BH1750_ReadLight(uint16_t *lux)
{
    uint8_t cmd = BH1750_HRES_MODE;
    uint8_t buf[2] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c2, BH1750_ADDR << 1, &cmd, 1, 100) != HAL_OK)
        return 0;
    HAL_Delay(180);
    if (HAL_I2C_Master_Receive(&hi2c2, BH1750_ADDR << 1, buf, 2, 200) != HAL_OK)
        return 0;

    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    *lux = raw * 10 / 12;
    return 1;
}
