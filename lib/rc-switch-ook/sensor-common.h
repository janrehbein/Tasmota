#ifndef _SENSOR_COMMON_H
#define _SENSOR_COMMON_H

#include <Arduino.h>

#define BETWEEN(value, min, max) value >= min &&value < max

enum TREND
{
    TEMP_STEADY,
    TEMP_FALLING,
    TEMP_RISING,
    TEMP_UNKNOWN
};

enum RAIN_STATUS
{
    RAIN_STATUS_SKIP,
    RAIN_STATUS_PERIOD_OVER,
    RAIN_STATUS_OK
};

struct SensorMeta
{
    uint16_t senderId = 0xFFFF;
    uint8_t channel;
    uint8_t rollingCode;
    // uint8_t flags;

    uint8_t lowBat : 1;
    uint8_t forceSend : 1;
};

struct TempHumiditySensor
{
    SensorMeta meta;
    float temperature = 0;
    TREND temperatureTrend = TEMP_UNKNOWN;
    uint8_t humidity = 0;
};

struct AnemometerSensor
{
    SensorMeta meta;
    int direction;
    float gust;
    float average;
};

struct RainGaugeSensor
{
    SensorMeta meta;
    uint8_t bucket_tips;
    float rate;
    float total;
};

void printBuffer(uint8_t buffer[], int pos)
{
    char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    for (int i = 0; i < pos; i++)
    {
        Serial.print(hex[buffer[i] >> 4]);
        Serial.print(hex[buffer[i] & 15]);
    }
}

/**
 * RAIN CALCULATION
 * 
 * 
 **/

#define TEN_MINUTES 600000

struct RainPeriod
{
    // configuration part
    uint32_t windowDuration = TEN_MINUTES;

    // last update time
    uint32_t lastTipMillis;

    // last update value
    uint8_t lastTipValue;

    // End Time millis for this period
    uint32_t endTimeMillis = 0;

    // Total amount of bucket flips
    uint8_t total = 0;

    // Total amount of iterations
    uint8_t iterations = 0;
};

struct RainPeriodHistory
{
    uint8_t total = 0;
    uint8_t iterations = 0;
};

RAIN_STATUS calculateRainData(RainPeriod &period, RainPeriodHistory &historyPeriod, uint8_t value)
{
    uint32_t now = millis();
    uint8_t delta = 0;
    bool isPeriodOver = false;

    // Serial.println();

    // skip updates frequnce lower than 1 second
    if (period.lastTipMillis + 1000 > now)
    {
        // Serial.println("Skip multiple data");
        // return isPeriodOver;
        return RAIN_STATUS_SKIP;
    }

    // a fresh start of the MCU
    if (period.endTimeMillis == 0)
    {
        Serial.println("Fresh start ...");
        period.endTimeMillis = now + period.windowDuration;
        period.lastTipMillis = now;
        period.lastTipValue = value;
        return RAIN_STATUS_OK;
        // return isPeriodOver;
    }

    if (value < period.lastTipValue)
    {
        // overflow
        delta = (255 - period.lastTipValue) + value;
    }
    else
    {
        delta = value - period.lastTipValue;
    }

    Serial.print("Delta: ");
    Serial.println(delta);

    // set new values
    period.lastTipValue = value;
    period.lastTipMillis = now;

    if (period.endTimeMillis <= now)
    {

        Serial.print("Time window over, start new window! ");
        Serial.println(now);

        // transfer current values to history period
        historyPeriod.iterations = period.iterations;
        historyPeriod.total = period.total;

        period.endTimeMillis = now + period.windowDuration;
        period.total = 0;
        period.iterations = 0;

        isPeriodOver = true;
    }
    else
    {
        // Time window still valid
        period.total += delta;
        period.iterations++;

        Serial.print("Time Window total ");
        Serial.print(period.total);
        Serial.print(" in ");
        Serial.print(period.iterations);
        Serial.println(" iteration...");
    }

    float totalMm = period.total * 0.3f;
    float totalMmHrs = ((uint16_t)period.total * 6) * 0.3f;

    Serial.print("Rain Rate ");
    Serial.print(period.total);
    Serial.print(" buckets ");
    Serial.print(totalMm);
    Serial.print("mm/10min ");
    Serial.print(totalMmHrs);
    Serial.print("mm/h");

    return isPeriodOver ? RAIN_STATUS_PERIOD_OVER : RAIN_STATUS_OK;
    // return isPeriodOver;
}

#endif