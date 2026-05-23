#include "bme280.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;

static I2C_HandleTypeDef *i2c = NULL;     /* 选中的I2C总线 */
static uint8_t  dev_addr;                 /* 器件地址（含读写位） */

/* BME280校准参数（上电时从寄存器读取） */
static uint16_t dig_T1, dig_P1;
static int16_t  dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int8_t   dig_H2, dig_H4, dig_H5, dig_H6;
static int32_t  t_fine;                   /* 温度补偿中间值（给气压/湿度用） */
static uint8_t  initialized;             /* 初始化完成标志 */

/* 读BME280寄存器（先发寄存器地址，再接收数据） */
static uint8_t bme280_rd(uint8_t reg, uint8_t *buf, uint8_t len)
{
    HAL_StatusTypeDef st;
    st = HAL_I2C_Master_Transmit(i2c, dev_addr, &reg, 1, 100);
    if (st != HAL_OK) return 0;
    return HAL_I2C_Master_Receive(i2c, dev_addr, buf, len, 200) == HAL_OK;
}

/* 写BME280寄存器 */
static void bme280_wr(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    HAL_I2C_Master_Transmit(i2c, dev_addr, buf, 2, 100);
}

/* 尝试在指定I2C总线和地址探测BME280芯片ID */
static uint8_t try_bme280_at(I2C_HandleTypeDef *hi2c, uint8_t addr7bit)
{
    uint8_t id = 0;
    int retry;

    i2c = hi2c;
    dev_addr = (addr7bit << 1);           /* 7位地址 → 8位（含读写位） */

    for (retry = 10; retry > 0; retry--) {   /* 最多重试10次 */
        if (bme280_rd(BME280_REG_ID, &id, 1) && id == BME280_CHIP_ID)
            break;
        HAL_Delay(1);
    }
    return (id == BME280_CHIP_ID);
}

/*
 * BME280初始化
 * 自动探测I2C1/I2C2的0x76/0x77地址，读取校准参数
 * 配置：Standby=0.5ms，无滤波，温/湿/压过采样×1，强制测量模式
 */
uint8_t BME280_Init(void)
{
    uint8_t calib[26], calib_h[7];

    initialized = 0;

    HAL_Delay(50);                        /* 等待传感器上电稳定 */

    /* 自动探测：I2C1→I2C2，先0x76后0x77 */
    if (try_bme280_at(&hi2c1, 0x76)) {
    } else if (try_bme280_at(&hi2c1, 0x77)) {
    } else if (try_bme280_at(&hi2c2, 0x76)) {
    } else if (try_bme280_at(&hi2c2, 0x77)) {
    } else {
        i2c = NULL;
        return 0;                         /* 未找到BME280 */
    }

    /* 软件复位 */
    bme280_wr(BME280_REG_RESET, BME280_RESET_CMD);
    HAL_Delay(10);

    /* 读取温度/气压校准参数：寄存器0x88~0xA1（26字节） */
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

    /* 读取湿度校准参数：寄存器0xE1~0xE7（7字节） */
    if (bme280_rd(BME280_H2_LSB_ADDR, calib_h, 7)) {
        dig_H2 = (int16_t)(calib_h[0]  | (calib_h[1] << 8));
        dig_H3 = calib_h[2];
        dig_H4 = (int8_t)((calib_h[3] << 4) | (calib_h[4] & 0x0F));
        dig_H5 = (int8_t)((calib_h[5] << 4) | ((calib_h[4] >> 4) & 0x0F));
        dig_H6 = (int8_t)calib_h[6];
    }

    /* 配置寄存器：Standby=0.5ms，IIR滤波器关闭 */
    bme280_wr(BME280_REG_CONFIG, BME280_STANDBY_0_5MS | BME280_FILTER_OFF);
    /* 湿度控制寄存器：过采样×1 */
    bme280_wr(BME280_REG_CTRL_HUM, BME280_OSRS_H_X1);

    initialized = 1;
    return 1;
}

/* 启动一次强制测量（温/湿/压过采样×1） */
void BME280_StartMeasurement(void)
{
    if (!initialized) return;
    bme280_wr(BME280_REG_CTRL_MEAS,
              BME280_OSRS_T_X1 | BME280_OSRS_P_X1 | BME280_MODE_FORCED);
}

/* ---- Bosch官方补偿公式（16-bit精度版本） ---- */

/* 温度补偿：返回 ℃×100，同时保存t_fine供气压/湿度使用 */
static int32_t comp_temp(int32_t adc_T)
{
    int32_t var1, var2;

    var1 = (((adc_T >> 3) - ((int32_t)dig_T1 << 1)) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1))
            * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;

    return (t_fine * 5 + 128) >> 8;        /* ℃×100 */
}

/* 气压补偿：返回 Pa */
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
        pressure = pressure / 100;          /* Bosch 64-bit返回Pa×100，转为Pa */
    } else {
        pressure = 30000;                   /* 异常时返回300hPa */
    }

    return pressure;                        /* Pa */
}

/* 湿度补偿：返回 %RH×100 */
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
    var5 = (var5 < 0 ? 0 : var5);          /* 限制下限 */
    var5 = (var5 > 419430400 ? 419430400 : var5); /* 限制上限 */
    humidity = (uint32_t)(var5 / 4096);     /* %RH×1024 */

    return (humidity * 100) / 1024;         /* → %RH×100 */
}

/* 读取BME280测量数据（启动测量后需等待BME280_T_MEAS_MS） */
uint8_t BME280_ReadData(BME280_Data *data)
{
    uint8_t raw[8];
    int32_t aT, aP, aH;

    if (!initialized) return 0;
    if (!bme280_rd(BME280_REG_PRESS_MSB, raw, 8)) return 0;

    /* 提取20-bit ADC值（气压→温度→湿度） */
    aP = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    aT = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    aH = ((int32_t)raw[6] << 8)  | (int32_t)raw[7];

    /* 运行补偿公式（温度必须先补偿，因为t_fine是全局依赖） */
    data->temp  = comp_temp(aT);
    data->press = comp_press(aP);
    data->hum   = comp_hum(aH);
    return 1;
}
