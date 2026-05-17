#ifndef __BME280_H__
#define __BME280_H__

#include "main.h"

/* I2C address (7-bit): SDO=GND → 0x76, SDO=VDD → 0x77 */
#define BME280_I2C_ADDR          0x76
/* HAL expects 7-bit address shifted left by 1 */
#define BME280_ADDR              (BME280_I2C_ADDR << 1)

/* Register map */
#define BME280_REG_ID            0xD0
#define BME280_REG_RESET         0xE0
#define BME280_REG_CTRL_HUM      0xF2
#define BME280_REG_STATUS        0xF3
#define BME280_REG_CTRL_MEAS     0xF4
#define BME280_REG_CONFIG        0xF5
#define BME280_REG_PRESS_MSB     0xF7

/* Calibration data ranges */
#define BME280_T1_LSB_ADDR       0x88
#define BME280_T1_MSB_ADDR       0x89
#define BME280_T2_LSB_ADDR       0x8A
#define BME280_T2_MSB_ADDR       0x8B
#define BME280_T3_LSB_ADDR       0x8C
#define BME280_T3_MSB_ADDR       0x8D
#define BME280_P1_LSB_ADDR       0x8E
#define BME280_P1_MSB_ADDR       0x8F
#define BME280_P2_LSB_ADDR       0x90
#define BME280_P2_MSB_ADDR       0x91
#define BME280_P3_LSB_ADDR       0x92
#define BME280_P3_MSB_ADDR       0x93
#define BME280_P4_LSB_ADDR       0x94
#define BME280_P4_MSB_ADDR       0x95
#define BME280_P5_LSB_ADDR       0x96
#define BME280_P5_MSB_ADDR       0x97
#define BME280_P6_LSB_ADDR       0x98
#define BME280_P6_MSB_ADDR       0x99
#define BME280_P7_LSB_ADDR       0x9A
#define BME280_P7_MSB_ADDR       0x9B
#define BME280_P8_LSB_ADDR       0x9C
#define BME280_P8_MSB_ADDR       0x9D
#define BME280_P9_LSB_ADDR       0x9E
#define BME280_P9_MSB_ADDR       0x9F
#define BME280_H1_ADDR           0xA1
#define BME280_H2_LSB_ADDR       0xE1
#define BME280_H2_MSB_ADDR       0xE2
#define BME280_H3_ADDR           0xE3
#define BME280_H4_MSB_LSB_ADDR   0xE4
#define BME280_H4_LSB_MSB_ADDR   0xE5  /* shared with H5 */
#define BME280_H5_MSB_LSB_ADDR   0xE5  /* shared with H4 */
#define BME280_H5_LSB_MSB_ADDR   0xE6
#define BME280_H6_ADDR           0xE7

/* Chip ID */
#define BME280_CHIP_ID           0x60

/* Modes */
#define BME280_MODE_SLEEP        0x00
#define BME280_MODE_FORCED       0x01
#define BME280_MODE_NORMAL       0x03

/* Oversampling */
#define BME280_OSRS_T_SKIP       0x00
#define BME280_OSRS_T_X1         0x20
#define BME280_OSRS_T_X2         0x40
#define BME280_OSRS_T_X16        0xE0

#define BME280_OSRS_P_SKIP       0x00
#define BME280_OSRS_P_X1         0x04
#define BME280_OSRS_P_X2         0x08
#define BME280_OSRS_P_X16        0x1C

#define BME280_OSRS_H_SKIP       0x00
#define BME280_OSRS_H_X1         0x01

/* Filter */
#define BME280_FILTER_OFF        0x00
#define BME280_FILTER_2          0x04
#define BME280_FILTER_16         0x14

/* Standby time in normal mode */
#define BME280_STANDBY_0_5MS     0x00

/* Soft reset command */
#define BME280_RESET_CMD         0xB6

/* Measurement time for forced mode:
 * t=1x, p=1x, h=1x → T_meas = 1.25 + 2.3*1 + 2.3*1 + 0.575 + 0.575*1 ≈ 9.3ms
 * Use generous margin */
#define BME280_T_MEAS_MS         15

/* ---- Public types ---- */
typedef struct {
    int32_t  temp;       /* C * 100    */
    uint32_t hum;        /* %RH * 100  */
    uint32_t press;      /* Pa         */
} BME280_Data;

/* ---- Public API ---- */
uint8_t BME280_Init(void);
void    BME280_StartMeasurement(void);
uint8_t BME280_ReadData(BME280_Data *data);

#endif
