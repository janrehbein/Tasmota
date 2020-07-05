#ifndef _SENSOR_KW9010_H
#define _SENSOR_KW9010_H

#include "sensor-common.h"

struct KW9015Sensor
{
    SensorMeta meta;
    float temperature = 0;
    TREND temperatureTrend = TEMP_UNKNOWN;
    byte rainFlaps = 0;
};

// check if received crc is correct for received value
boolean kw9010_isValid(unsigned long value, byte checksum)
{
    byte calculatedChecksum = 0;
    for (int i = 0; i < 8; i++)
    {
        calculatedChecksum += (byte)(value >> (i * 4));
    }

    calculatedChecksum &= 0xF;
    return calculatedChecksum == checksum;
}

float kw9010_getTemperature(unsigned long value)
{
    // Temperature
    int temperature = value >> 12 & 0b111111111111; // bit 12..23
    // if sign bit is set, adjust two's complement to fit a 16-bit number
    if (bitRead(temperature, 11))
    {
        temperature = temperature | 0b1111000000000000;
    }
    return temperature / 10.0f;
}

byte kw9010_getSensorId(unsigned long value)
{
    // bit 0, 1, 2, 3, 6, 7, random change bits when battery is changed
    return value & 0b11001111;
}

byte kw9010_getChannel(unsigned long value)
{
    // bit 4, 5 are channel number
    return 2 * bitRead(value, 4) + bitRead(value, 5);
}

TREND kw9010_getTemperatureTrend(unsigned long value)
{
    // temperature tendency
    byte trend = (value >> 9 & 0b11); // bit 9, 10

    if (trend == 0)
        return TEMP_STEADY;
    else if (trend == 1)
        return TEMP_FALLING;
    else if (trend == 2)
        return TEMP_RISING;

    return TEMP_UNKNOWN;
}

byte kw9010_getHumidity(unsigned long value)
{
    return (value >> 24 & 0b11111111) - 156; // bit 24..31
}

bool kw9010_isLowBat(unsigned long value)
{
    return value >> 8 & 0b1; // bit 8 is set if battery voltage is low
}

bool kw9010_isForceSend(unsigned long value)
{
    return value >> 11 & 0b1; // bit 11 is set if manual send button was pressed
}

byte kw9010_getRainFlaps(unsigned long value)
{
    return (value >> 24 & 0b1111111); // bit 24..31
}

// void printResultsX(byte buffer[])
// {

//     unsigned long value = (unsigned long)buffer[3] << 24 | (unsigned long)buffer[2] << 16;
//     value |= (unsigned long)buffer[1] << 8 | buffer[0];

//     if (!crcValidX(value, buffer[4]))
//     {
//         Serial.println("INVALUDVALID +++++++++++++++++++");
//         return;
//     }

//     // Sensor ID
//     byte id = value & 0b11001111; // bit 0, 1, 2, 3, 6, 7, random change bits when battery is changed
//     Serial.print('\t');
//     Serial.print(id);

//     // Channel (as set on sensor)
//     byte channel = 2 * bitRead(value, 4) + bitRead(value, 5); // bit 4, 5 are channel number
//     Serial.print('\t');
//     Serial.print(channel);

//     // Temperature
//     int temperature = value >> 12 & 0b111111111111; // bit 12..23
//     // if sign bit is set, adjust two's complement to fit a 16-bit number
//     if (bitRead(temperature, 11))
//         temperature = temperature | 0b1111000000000000;
//     Serial.print('\t');
//     Serial.print(temperature / 10.0, 1);

//     // temperature tendency
//     byte trend = value >> 9 & 0b11; // bit 9, 10
//     Serial.print('\t');
//     if (trend == 0)
//         Serial.print('='); // temp tendency steady
//     else if (trend == 1)
//         Serial.print('-'); // temp tendency falling
//     else if (trend == 2)
//         Serial.print('+'); // temp tendency rising

//     // Humidity
//     byte humidity = (value >> 24 & 0b11111111) - 156; // bit 24..31
//     Serial.print('\t');
//     Serial.print(humidity);

//     // Flaps
//     byte flaps = (value >> 24 & 0b1111111); // bit 24..31
//     Serial.print('\t');
//     Serial.print(flaps);

//     // Battery State
//     bool lowBat = value >> 8 & 0b1; // bit 8 is set if battery voltage is low
//     Serial.print('\t');
//     Serial.print(lowBat);

//     // Trigger
//     bool forcedSend = value >> 11 & 0b1; // bit 11 is set if manual send button was pressed
//     Serial.print('\t');
//     Serial.print(forcedSend);

//     // if (lastFlapCount != 0 && lastMillis != 0)
//     // {

//     //     byte lastMillisDelta = (millis() - lastMillis) / 1000;

//     //     Serial.print('\t');
//     //     Serial.print(lastMillisDelta);

//     //     // check overflow
//     //     byte flapsDelta = flaps - lastFlapCount;
//     //     if (flaps < lastFlapCount)
//     //     {
//     //         flapsDelta = (128 - lastFlapCount) + flaps;
//     //     }

//     //     float currentRain = flapsDelta * 0.26f;

//     //     Serial.print('\t');
//     //     Serial.print(flapsDelta);

//     //     Serial.print('\t');
//     //     Serial.print(currentRain);
//     //     Serial.print("mm");

//     //     if (lastMillisDelta > 0)
//     //     {
//     //         float xxx = (currentRain / lastMillisDelta) * 3600;

//     //         Serial.print('\t');
//     //         Serial.print(xxx);
//     //     }
//     // }

//     // // update
//     // lastMillis = millis();
//     // lastFlapCount = flaps;
// }

boolean kw9010_getKW9010Sensor(byte buffer[], TempHumiditySensor &data)
{
    unsigned long value = (unsigned long)buffer[3] << 24 | (unsigned long)buffer[2] << 16 | (unsigned long)buffer[1] << 8 | buffer[0];

    if (!kw9010_isValid(value, buffer[4]))
    {
        // Serial.println("INVALUDVALID +++++++++++++++++++");
        return false;
    }

    data.meta.channel = kw9010_getChannel(value);
    data.meta.senderId = kw9010_getSensorId(value);
    data.meta.lowBat = kw9010_isLowBat(value);
    data.meta.forceSend = kw9010_isForceSend(value);

    data.temperature = kw9010_getTemperature(value);
    data.temperatureTrend = kw9010_getTemperatureTrend(value);
    data.humidity = kw9010_getHumidity(value);

    if(data.temperature >= -40 && data.temperature < 50) {
        if(data.humidity >= 0 && data.humidity < 100) {
            return true;
        }
    }

    return false;
}

boolean kw9010_getKW9015Sensor(byte buffer[], byte bufferSize, KW9015Sensor &data)
{

    if(bufferSize != 5) {
        return false;
    }

    unsigned long value = (unsigned long)buffer[3] << 24 | (unsigned long)buffer[2] << 16 | (unsigned long)buffer[1] << 8 | buffer[0];

    if (!kw9010_isValid(value, buffer[4]))
    {
        return false;
    }

    data.meta.channel = kw9010_getChannel(value);
    data.meta.senderId = kw9010_getSensorId(value);
    data.meta.lowBat = kw9010_isLowBat(value);
    data.meta.forceSend = kw9010_isForceSend(value);

    data.temperature = kw9010_getTemperature(value);
    data.temperatureTrend = kw9010_getTemperatureTrend(value);
    data.rainFlaps = kw9010_getRainFlaps(value);

    if (data.temperature >= -40 && data.temperature < 60)
    {
        return true;
    }

    return false;
}

#endif