#include "bme280.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

static I2C_HandleTypeDef *i2c = NULL;
static uint8_t  dev_addr;

static uint16_t dig_T1, dig_P1;
static int16_t  dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int8_t   dig_H2, dig_H4, dig_H5, dig_H6;
static int32_t  t_fine;
static uint8_t  initialized;

static uint8_t bme280_rd(uint8_t reg, uint8_t *buf, uint8_t len)
{
    HAL_StatusTypeDef st;
    st = HAL_I2C_Master_Transmit(i2c, dev_addr, &reg, 1, 100);
    if (st != HAL_OK) return 0;
    return HAL_I2C_Master_Receive(i2c, dev_addr, buf, len, 200) == HAL_OK;
}

static void bme280_wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    HAL_I2C_Master_Transmit(i2c, dev_addr, buf, 2, 100);
}

static void i2c_scan(I2C_HandleTypeDef *hi2c, const char *name)
{
    uint8_t addr;
    printf(" [DBG] %s scan:", name);
    for (addr = 1; addr < 127; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, addr << 1, 1, 5) == HAL_OK)
            printf(" 0x%02X", addr);
    }
    printf("\r\n");
}

static uint8_t try_bme280_at(I2C_HandleTypeDef *hi2c, uint8_t addr7bit)
{
    uint8_t id = 0;
    int retry;

    i2c = hi2c;
    dev_addr = (addr7bit << 1);

    for (retry = 10; retry > 0; retry--) {
        if (bme280_rd(BME280_REG_ID, &id, 1) && id == BME280_CHIP_ID)
            break;
        HAL_Delay(1);
    }
    return (id == BME280_CHIP_ID);
}

uint8_t BME280_Init(void)
{
    uint8_t calib[26], calib_h[7];
    const char *bus_name = NULL;

    initialized = 0;

    HAL_Delay(50);

    i2c_scan(&hi2c1, "I2C1");
    i2c_scan(&hi2c2, "I2C2");

    printf(" [DBG] BME280 search:\r\n");
    if (try_bme280_at(&hi2c1, 0x76)) {
        bus_name = "I2C1";
    } else if (try_bme280_at(&hi2c1, 0x77)) {
        bus_name = "I2C1";
    } else if (try_bme280_at(&hi2c2, 0x76)) {
        bus_name = "I2C2";
    } else if (try_bme280_at(&hi2c2, 0x77)) {
        bus_name = "I2C2";
    } else {
        printf(" [DBG] BME280 not found on either bus\r\n");
        i2c = NULL;
        return 0;
    }

    printf(" [DBG] BME280 found on %s at 0x%02X\r\n",
           bus_name, (unsigned int)(dev_addr >> 1));

    /* Soft reset */
    bme280_wr(BME280_REG_RESET, BME280_RESET_CMD);
    HAL_Delay(10);

    /* Temperature & pressure calibration: 0x88-0xA1 (26 bytes) */
    if (!bme280_rd(BME280_T1_LSB_ADDR, calib, 26)) return 0;

    dig_T1 = (uint16_t)(calib[0]  | (calib[1]  << 8));
    dig_T2 = (int16_t) (calib[2]  | (calib[3]  << 8));
    dig_T3 = (int16_t) (calib[4]  | (calib[5]  << 8));
    dig_P1 = (uint16_t)(calib[6]  | (calib[7]  << 8));
    dig_P2 = (int16_t) (calib[8]  | (calib[9]  << 8));
    dig_P3 = (int16_t) (calib[10] | (calib[11] << 8));
    dig_P4 = (int16_t) (calib[12] | (calib[13] << 8));
    dig_P5 = (int16_t) (calib[14] | (calib[15] << 8));
    dig_P6 = (int16_t) (calib[16] | (calib[17] << 8));
    dig_P7 = (int16_t) (calib[18] | (calib[19] << 8));
    dig_P8 = (int16_t) (calib[20] | (calib[21] << 8));
    dig_P9 = (int16_t) (calib[22] | (calib[23] << 8));
    dig_H1 = calib[25];

    /* Humidity calibration: 0xE1-0xE7 (7 bytes) */
    if (bme280_rd(BME280_H2_LSB_ADDR, calib_h, 7)) {
        dig_H2 = (int16_t)(calib_h[0]  | (calib_h[1] << 8));
        dig_H3 = calib_h[2];
        dig_H4 = (int8_t)((calib_h[3] << 4) | (calib_h[4] & 0x0F));
        dig_H5 = (int8_t)((calib_h[5] << 4) | ((calib_h[4] >> 4) & 0x0F));
        dig_H6 = (int8_t)calib_h[6];
    }

    /* Config: 0.5ms standby, no filter */
    bme280_wr(BME280_REG_CONFIG, BME280_STANDBY_0_5MS | BME280_FILTER_OFF);
    /* Humidity oversampling x1 */
    bme280_wr(BME280_REG_CTRL_HUM, BME280_OSRS_H_X1);

    initialized = 1;
    return 1;
}

void BME280_StartMeasurement(void)
{
    if (!initialized) return;
    bme280_wr(BME280_REG_CTRL_MEAS,
              BME280_OSRS_T_X1 | BME280_OSRS_P_X1 | BME280_MODE_FORCED);
}

/* ---- Exact Bosch Sensortec compensation formulas ---- */

static int32_t comp_temp(int32_t adc_T)
{
    int32_t var1, var2;

    var1 = (((adc_T >> 3) - ((int32_t)dig_T1 << 1)) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1))
            * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;

    return (t_fine * 5 + 128) >> 8;  /* C * 100 */
}

static uint32_t comp_press(int32_t adc_P)
{
    int64_t var1, var2, var3, var4;
    uint32_t pressure;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) * 131072);
    var2 = var2 + (((int64_t)dig_P4) * 34359738368);
    var1 = ((var1 * var1 * (int64_t)dig_P3) / 256)
         + ((var1 * ((int64_t)dig_P2) * 4096));
    var3 = ((int64_t)1) * 140737488355328;
    var1 = (var3 + var1) * ((int64_t)dig_P1) / 8589934592;

    if (var1 != 0) {
        var4 = 1048576 - adc_P;
        var4 = (((var4 * 2147483648) - var2) * 3125) / var1;
        var1 = (((int64_t)dig_P9) * (var4 / 8192) * (var4 / 8192)) / 33554432;
        var2 = (((int64_t)dig_P8) * var4) / 524288;
        var4 = ((var4 + var1 + var2) / 256) + (((int64_t)dig_P7) * 16);
        pressure = (uint32_t)(((var4 / 2) * 100) / 128);
    } else {
        pressure = 3000000;
    }

    return pressure;  /* Pa */
}

static uint32_t comp_hum(int32_t adc_H)
{
    int32_t var1, var2, var3, var4, var5;
    uint32_t humidity;

    var1 = t_fine - ((int32_t)76800);
    var2 = (int32_t)(adc_H * 16384);
    var3 = (int32_t)(((int32_t)dig_H4) * 1048576);
    var4 = ((int32_t)dig_H5) * var1;
    var5 = (((var2 - var3) - var4) + (int32_t)16384) >> 15;
    var2 = (var1 * ((int32_t)dig_H6)) / 1024;
    var3 = (var1 * ((int32_t)dig_H3)) / 2048;
    var4 = ((var2 * (var3 + (int32_t)32768)) / 1024) + (int32_t)2097152;
    var2 = ((var4 * ((int32_t)dig_H2)) + 8192) / 16384;
    var3 = var5 * var2;
    var4 = ((var3 / 32768) * (var3 / 32768)) / 128;
    var5 = var3 - ((var4 * ((int32_t)dig_H1)) / 16);
    var5 = (var5 < 0 ? 0 : var5);
    var5 = (var5 > 419430400 ? 419430400 : var5);
    humidity = (uint32_t)(var5 / 4096);  /* %RH * 1024 */

    /* Convert to %RH * 100 for our application */
    return (humidity * 100) / 1024;
}

uint8_t BME280_ReadData(BME280_Data *data)
{
    uint8_t raw[8];
    int32_t aT, aP, aH;

    if (!initialized) return 0;
    if (!bme280_rd(BME280_REG_PRESS_MSB, raw, 8)) return 0;

    aP = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    aT = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    aH = ((int32_t)raw[6] << 8)  | (int32_t)raw[7];

    data->temp  = comp_temp(aT);
    data->press = comp_press(aP);
    data->hum   = comp_hum(aH);
    return 1;
}
