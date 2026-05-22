#include "time_util.h"

static uint8_t is_leap(uint16_t y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static uint8_t month_num(const char *m)
{
    if (m[0] == 'J' && m[1] == 'a') return 1;
    if (m[0] == 'F') return 2;
    if (m[0] == 'M' && m[2] == 'r') return 3;
    if (m[0] == 'A' && m[1] == 'p') return 4;
    if (m[0] == 'M' && m[2] == 'y') return 5;
    if (m[0] == 'J' && m[2] == 'n') return 6;
    if (m[0] == 'J' && m[2] == 'l') return 7;
    if (m[0] == 'A' && m[1] == 'u') return 8;
    if (m[0] == 'S') return 9;
    if (m[0] == 'O') return 10;
    if (m[0] == 'N') return 11;
    if (m[0] == 'D') return 12;
    return 1;
}

static const uint8_t mdays[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

uint32_t Time_CompileUnix(void)
{
    const char *date = __DATE__;
    const char *time = __TIME__;

    uint8_t mon = month_num(date);
    uint8_t day;
    if (date[4] == ' ')
        day = date[5] - '0';
    else
        day = (date[4] - '0') * 10 + (date[5] - '0');
    uint16_t year = (date[7] - '0') * 1000
                  + (date[8] - '0') * 100
                  + (date[9] - '0') * 10
                  + (date[10] - '0');
    uint8_t hour = (time[0] - '0') * 10 + (time[1] - '0');
    uint8_t min  = (time[3] - '0') * 10 + (time[4] - '0');
    uint8_t sec  = (time[6] - '0') * 10 + (time[7] - '0');

    return Time_CalendarToUnix(year, mon, day, hour, min, sec);
}

void Time_UnixToCalendar(uint32_t ts, uint16_t *year, uint8_t *mon,
                         uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec)
{
    uint32_t days = ts / 86400;
    uint32_t sod  = ts % 86400;

    *year = 1970;
    while (1) {
        uint16_t dy = 365 + is_leap(*year);
        if (days < dy) break;
        days -= dy;
        (*year)++;
    }

    uint8_t leap = is_leap(*year);
    *mon = 1;
    for (; *mon <= 12; (*mon)++) {
        uint8_t md = mdays[*mon];
        if (*mon == 2 && leap) md = 29;
        if (days < md) break;
        days -= md;
    }
    *day = (uint8_t)days + 1;

    *hour = sod / 3600;
    *min  = (sod % 3600) / 60;
    *sec  = sod % 60;
}

uint32_t Time_CalendarToUnix(uint16_t year, uint8_t mon, uint8_t day,
                             uint8_t hour, uint8_t min, uint8_t sec)
{
    uint32_t days = 0;
    uint16_t y;
    for (y = 1970; y < year; y++)
        days += 365 + is_leap(y);

    uint8_t leap = is_leap(year);
    uint8_t m;
    for (m = 1; m < mon; m++) {
        days += mdays[m];
        if (m == 2 && leap) days++;
    }
    days += day - 1;

    return days * 86400UL + hour * 3600UL + min * 60UL + sec;
}

void Time_UnixToStr(uint32_t ts, char *buf, uint8_t buf_sz)
{
    (void)buf_sz;

    uint16_t year;
    uint8_t  mon, day, hour, min, sec;
    Time_UnixToCalendar(ts, &year, &mon, &day, &hour, &min, &sec);

    /* "YYYY-MM-DD HH:MM:SS" */
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
