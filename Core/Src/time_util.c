#include "time_util.h"

/* 判断是否为闰年 */
static uint8_t is_leap(uint16_t y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

/* 月份缩写 → 数字（1-12） */
static uint8_t month_num(const char *m)
{
    if (m[0] == 'J' && m[1] == 'a') return 1;   /* Jan */
    if (m[0] == 'F') return 2;                   /* Feb */
    if (m[0] == 'M' && m[2] == 'r') return 3;    /* Mar */
    if (m[0] == 'A' && m[1] == 'p') return 4;    /* Apr */
    if (m[0] == 'M' && m[2] == 'y') return 5;    /* May */
    if (m[0] == 'J' && m[2] == 'n') return 6;    /* Jun */
    if (m[0] == 'J' && m[2] == 'l') return 7;    /* Jul */
    if (m[0] == 'A' && m[1] == 'u') return 8;    /* Aug */
    if (m[0] == 'S') return 9;                   /* Sep */
    if (m[0] == 'O') return 10;                  /* Oct */
    if (m[0] == 'N') return 11;                  /* Nov */
    if (m[0] == 'D') return 12;                  /* Dec */
    return 1;                                     /* 默认1月 */
}

/* 每月天数（平年，2月28天） */
static const uint8_t mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

/*
 * 编译期Unix时间戳
 * 解析 __DATE__ 和 __TIME__ 宏，转为1970-01-01起的秒数
 */
uint32_t Time_CompileUnix(void)
{
    const char *date = __DATE__;     /* 例："May 23 2026" */
    const char *time = __TIME__;     /* 例："17:05:42" */

    uint8_t mon = month_num(date);
    uint8_t day;
    if (date[4] == ' ')
        day = date[5] - '0';                          /* " 1" → 1 */
    else
        day = (date[4] - '0') * 10 + (date[5] - '0'); /* "23" → 23 */
    uint16_t year = (date[7] - '0') * 1000
                  + (date[8] - '0') * 100
                  + (date[9] - '0') * 10
                  + (date[10] - '0');
    uint8_t hour = (time[0] - '0') * 10 + (time[1] - '0');
    uint8_t min  = (time[3] - '0') * 10 + (time[4] - '0');
    uint8_t sec  = (time[6] - '0') * 10 + (time[7] - '0');

    return Time_CalendarToUnix(year, mon, day, hour, min, sec);
}

/*
 * Unix时间戳 → 日历分量
 * 输入：1970-01-01 00:00:00 起的秒数
 * 输出：年/月/日/时/分/秒
 */
void Time_UnixToCalendar(uint32_t ts, uint16_t *year, uint8_t *mon,
                         uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec)
{
    uint32_t days = ts / 86400;      /* 总天数 */
    uint32_t sod  = ts % 86400;      /* 当天秒数 */

    /* 从1970年起逐年减天数 */
    *year = 1970;
    while (1) {
        uint16_t dy = 365 + is_leap(*year);
        if (days < dy) break;
        days -= dy;
        (*year)++;
    }

    /* 逐月减天数 */
    uint8_t leap = is_leap(*year);
    *mon = 1;
    for (; *mon <= 12; (*mon)++) {
        uint8_t md = mdays[*mon];
        if (*mon == 2 && leap) md = 29;   /* 闰年2月29天 */
        if (days < md) break;
        days -= md;
    }
    *day = (uint8_t)days + 1;

    /* 秒数转时分秒 */
    *hour = sod / 3600;
    *min  = (sod % 3600) / 60;
    *sec  = sod % 60;
}

/*
 * 日历分量 → Unix时间戳
 * 输入：年/月/日/时/分/秒
 * 输出：1970-01-01 00:00:00 起的秒数
 */
uint32_t Time_CalendarToUnix(uint16_t year, uint8_t mon, uint8_t day,
                             uint8_t hour, uint8_t min, uint8_t sec)
{
    uint32_t days = 0;
    uint16_t y;

    /* 累加1970到year-1的每年天数 */
    for (y = 1970; y < year; y++)
        days += 365 + is_leap(y);

    /* 累加当年1月到mon-1的每月天数 */
    uint8_t leap = is_leap(year);
    uint8_t m;
    for (m = 1; m < mon; m++) {
        days += mdays[m];
        if (m == 2 && leap) days++;       /* 闰年2月加1天 */
    }
    days += day - 1;

    return days * 86400UL + hour * 3600UL + min * 60UL + sec;
}

/*
 * Unix时间戳 → 格式化字符串 "YYYY-MM-DD HH:MM:SS"
 * 用于串口打印时间
 */
void Time_UnixToStr(uint32_t ts, char *buf, uint8_t buf_sz)
{
    (void)buf_sz;   /* 信任调用方传20字节缓冲区 */

    uint16_t year;
    uint8_t  mon, day, hour, min, sec;
    Time_UnixToCalendar(ts, &year, &mon, &day, &hour, &min, &sec);

    buf[0]  = '0' + year / 1000;
    buf[1]  = '0' + (year / 100) % 10;
    buf[2]  = '0' + (year / 10) % 10;
    buf[3]  = '0' + year % 10;
    buf[4]  = '-';
    buf[5]  = '0' + mon / 10;
    buf[6]  = '0' + mon % 10;
    buf[7]  = '-';
    buf[8]  = '0' + day / 10;
    buf[9]  = '0' + day % 10;
    buf[10] = ' ';
    buf[11] = '0' + hour / 10;
    buf[12] = '0' + hour % 10;
    buf[13] = ':';
    buf[14] = '0' + min / 10;
    buf[15] = '0' + min % 10;
    buf[16] = ':';
    buf[17] = '0' + sec / 10;
    buf[18] = '0' + sec % 10;
    buf[19] = '\0';
}
