#include "sensor_storage.h"
#include "w25q64.h"
#include "time_util.h"
#include <stdio.h>

#define ADDR        W25Q64_STORAGE_ADDR
#define HDR_SZ      4
#define REC_SZ      sizeof(SensorRecord)

static SensorRecord buf[MAX_RECORDS];
static uint8_t      count;

static uint8_t record_valid(SensorRecord *r)
{
    if (r->press != 0 && (r->press < 30000 || r->press > 200000))
        return 0;
    if (r->temp != 0 && (r->temp < -4000 || r->temp > 8500))
        return 0;
    if (r->hum > 1000)
        return 0;
    return 1;
}

void Storage_Init(void)
{
    uint8_t hdr[HDR_SZ];
    uint32_t jedec = W25Q64_ReadJEDEC();

    if (jedec == 0xFFFFFF || jedec == 0x000000) { count = 0; return; }

    W25Q64_ReadData(ADDR, hdr, HDR_SZ);
    count = hdr[0];
    if (count > MAX_RECORDS) { count = 0; return; }
    if (count > 0) {
        W25Q64_ReadData(ADDR + HDR_SZ, (uint8_t *)buf, count * REC_SZ);
        uint8_t i;
        for (i = 0; i < count; i++) {
            if (!record_valid(&buf[i])) {
                printf("  [!] Corrupted flash records detected, clearing...\r\n");
                count = 0;
                hdr[0] = 0;
                W25Q64_SectorErase(ADDR);
                W25Q64_PageProgram(ADDR, hdr, HDR_SZ);
                break;
            }
        }
    }
}

uint8_t Storage_Count(void) { return count; }

void Storage_Save(SensorRecord *rec)
{
    if (count >= MAX_RECORDS) {
        uint8_t i;
        for (i = 0; i < MAX_RECORDS - 1; i++) buf[i] = buf[i + 1];
        count = (uint8_t)(MAX_RECORDS - 1);
    }
    buf[count++] = *rec;

    uint8_t hdr[HDR_SZ] = {count, 0, 0, 0};
    W25Q64_SectorErase(ADDR);
    W25Q64_PageProgram(ADDR, hdr, HDR_SZ);
    W25Q64_PageProgram(ADDR + HDR_SZ, (uint8_t *)buf, count * REC_SZ);
}

void Storage_ReadAll(SensorRecord *recs, uint8_t *cnt)
{
    *cnt = count;
    uint8_t i;
    for (i = 0; i < count; i++) recs[i] = buf[i];
}

void Storage_PrintAll(void)
{
    if (count == 0) { printf("  (empty)\r\n"); return; }
    printf("  === Records (%d) ===\r\n", count);
    uint8_t i;
    for (i = 0; i < count; i++) {
        char ts[20];
        Time_UnixToStr(buf[i].timestamp, ts, sizeof(ts));
        printf("  #%02d %s  Lux:%-5u T:%2d.%dC H:%2d.%d%% P:%u.%uhPa\r\n",
               i + 1, ts, buf[i].light,
               buf[i].temp / 100,
               (buf[i].temp < 0 ? -buf[i].temp : buf[i].temp) % 100 / 10,
               buf[i].hum / 10, buf[i].hum % 10,
               (unsigned int)(buf[i].press / 100),
               (unsigned int)((buf[i].press % 100) / 10));
    }
}
