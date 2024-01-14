#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <RTClib.h>
#include <ESP32Time.h>

bool setSystemTimeFromRTC()
{
    RTC_DS3231 _rtc;
    if (!_rtc.begin())
        return false;

    DateTime now = _rtc.now();
    struct tm tmStruct;

    tmStruct.tm_year = now.year() - 1900;
    tmStruct.tm_mon = now.month() - 1;
    tmStruct.tm_mday = now.day();
    tmStruct.tm_hour = now.hour();
    tmStruct.tm_min = now.minute();
    tmStruct.tm_sec = now.second();

    time_t time = mktime(&tmStruct);
    struct timeval tv = {time, 0};

    settimeofday(&tv, nullptr);


    // rtc.setTime(_rtc.now().unixtime());
    return true; 
}

bool setRTCTimeFromNTP(ESP32Time rtc)
{
    RTC_DS3231 _rtc;
    
    if (!_rtc.begin())
        return false;

    configTime(19800, 0, "pool.ntp.org");
    struct tm timeinfo;

    if (getLocalTime(&timeinfo))
    {
        rtc.setTimeStruct(timeinfo);
    }

    tm now = rtc.getTimeStruct();

    // Adjust the RTC time
    _rtc.adjust(DateTime(now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec));

    return true;
}

void setSystemTimeFromNTP(ESP32Time rtc)
{
    configTime(19800, 0, "pool.ntp.org");
    delay(700);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        rtc.setTimeStruct(timeinfo);
    }
}