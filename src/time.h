#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_BASE 0x09010000
#define RTC_DR (*(volatile uint32_t*)(RTC_BASE + 0x000))
    
uint32_t rtc_time(void)
{
    return RTC_DR;
}

uint32_t rtc_time_tz(int timezone_hours)
{
    int32_t t = (int32_t)RTC_DR;

    t += timezone_hours * 3600;

    return (uint32_t)t;
}

void get_time(uint32_t* h, uint32_t* m, uint32_t* s)
{
    uint32_t t = rtc_time();

    *s = t % 60;
    t /= 60;

    *m = t % 60;
    t /= 60;

    *h = t % 24;
}

#ifdef __cplusplus
}
#endif