#include "sensor_storage.h"
#include "w25q64.h"
#include "time_util.h"
#include "retarget.h"

#define ADDR        W25Q64_STORAGE_ADDR   /* Flash存储区起始地址（最后1个扇区） */
#define HDR_SZ      4                     /* 头部大小：1字节计数+3字节保留 */
#define REC_SZ      sizeof(SensorRecord)  /* 每条记录大小（packed，14字节） */

static SensorRecord buf[MAX_RECORDS];     /* RAM环形缓冲区 */
static uint8_t      count;                /* 当前记录数 */

/* 校验单条记录物理范围是否合法（防止Flash位翻转导致异常数据） */
static uint8_t record_valid(const SensorRecord *r)
{
    /* 气压：300~2000 hPa */
    if (r->press != 0 && (r->press < 30000 || r->press > 200000))
        return 0;
    /* 温度：-40~85 ℃ */
    if (r->temp != 0 && (r->temp < -4000 || r->temp > 8500))
        return 0;
    /* 湿度：0~100% */
    if (r->hum > 1000)
        return 0;
    return 1;
}

/* 从Flash恢复历史记录到RAM缓冲区 */
void Storage_Init(void)
{
    uint8_t hdr[HDR_SZ];
    uint32_t jedec = W25Q64_ReadJEDEC();

    /* Flash芯片不存在则跳过 */
    if (jedec == 0xFFFFFF || jedec == 0x000000) { count = 0; return; }

    /* 读取头部，获取记录数 */
    W25Q64_ReadData(ADDR, hdr, HDR_SZ);
    count = hdr[0];
    if (count > MAX_RECORDS) { count = 0; return; }   /* 异常值保护 */

    /* 读取全部记录并逐条校验 */
    if (count > 0) {
        W25Q64_ReadData(ADDR + HDR_SZ, (uint8_t *)buf, count * REC_SZ);
        uint8_t i;
        for (i = 0; i < count; i++) {
            if (!record_valid(&buf[i])) {
                /* 发现损坏记录 → 清空全部存储 */
                uart_puts("  [!] Corrupted flash records, clearing...\r\n");
                count = 0;
                hdr[0] = 0;
                W25Q64_SectorErase(ADDR);
                W25Q64_PageProgram(ADDR, hdr, HDR_SZ);
                break;
            }
        }
    }
}

/* 返回当前缓冲区记录数 */
uint8_t Storage_Count(void) { return count; }

/*
 * 保存一条新记录（环形缓冲区+写入Flash）
 * 满时丢弃最旧记录，左移数组
 */
void Storage_Save(SensorRecord *rec)
{
    /* 环形缓冲区：满时丢弃最旧记录 */
    if (count >= MAX_RECORDS) {
        uint8_t i;
        for (i = 0; i < MAX_RECORDS - 1; i++) buf[i] = buf[i + 1];
        count = (uint8_t)(MAX_RECORDS - 1);
    }
    buf[count++] = *rec;

    /* 写入Flash：先擦扇区，再写头部+全部数据 */
    uint8_t hdr[HDR_SZ] = {count, 0, 0, 0};
    W25Q64_SectorErase(ADDR);
    W25Q64_PageProgram(ADDR, hdr, HDR_SZ);
    W25Q64_PageProgram(ADDR + HDR_SZ, (uint8_t *)buf, count * REC_SZ);
}

/* 读取全部记录到外部数组 */
void Storage_ReadAll(SensorRecord *recs, uint8_t *cnt)
{
    *cnt = count;
    uint8_t i;
    for (i = 0; i < count; i++) recs[i] = buf[i];
}

/* 串口打印全部记录（格式化输出） */
void Storage_PrintAll(void)
{
    if (count == 0) {
        uart_puts("  (empty)\r\n");
        return;
    }

    uart_puts("  === Records (");
    uart_putu(count);
    uart_puts(") ===\r\n");

    uint8_t i;
    for (i = 0; i < count; i++) {
        char ts[20];
        int32_t t;
        uint32_t p;

        /* Unix时间戳 → "YYYY-MM-DD HH:MM:SS" */
        Time_UnixToStr(buf[i].timestamp, ts, sizeof(ts));

        /* 序号（两位补零） */
        uart_puts("  #");
        if (i + 1 < 10) uart_putc('0');
        uart_putu(i + 1);
        uart_putc(' ');

        /* 时间戳 */
        uart_puts(ts);
        uart_puts("  Lux:");

        /* 光照强度 */
        uart_putu(buf[i].light);

        /* 温度：带符号，整数一位小数 */
        uart_puts(" T:");
        t = buf[i].temp;
        if (t < 0) { uart_putc('-'); t = -t; }
        uart_putu(t / 100);
        uart_putc('.');
        uart_putu((t % 100) / 10);
        uart_puts("C H:");

        /* 湿度：整数一位小数 */
        uart_putu(buf[i].hum / 10);
        uart_putc('.');
        uart_putu(buf[i].hum % 10);
        uart_puts("% P:");

        /* 气压：整数一位小数 */
        p = buf[i].press;
        uart_putu(p / 100);
        uart_putc('.');
        uart_putu((p % 100) / 10);
        uart_puts("hPa\r\n");
    }
}
