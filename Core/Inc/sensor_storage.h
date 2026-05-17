#ifndef __SENSOR_STORAGE_H__
#define __SENSOR_STORAGE_H__

#include "main.h"

#define MAX_RECORDS 10

#pragma pack(push, 1)
typedef struct {
    uint16_t light;
    int16_t  temp;        /* C * 100   */
    uint16_t hum;         /* %RH * 10  */
    uint32_t press;       /* Pa        */
    uint32_t timestamp;   /* seconds   */
} SensorRecord;
#pragma pack(pop)

void    Storage_Init(void);
uint8_t Storage_Count(void);
void    Storage_Save(SensorRecord *rec);
void    Storage_ReadAll(SensorRecord *recs, uint8_t *cnt);
void    Storage_PrintAll(void);

#endif
