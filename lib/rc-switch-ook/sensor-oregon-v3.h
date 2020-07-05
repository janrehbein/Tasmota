#ifndef _SENSOR_OREGON_V3_H
#define _SENSOR_OREGON_V3_H

#include "sensor-common.h"

#define THGN801 0xF824  // Outdoor Temp Sensor
#define THGN801x 0x1D20 // Outdoor Temp Sensor
#define THGN801y 0xF8B4 // Outdoor Temp Sensor

#define WGR800 0x11994        // Anemometer
#define WGR800_NO_TEMP 0x1984 // Anemometer

#define PCR800 0x2914 // Rain Gauge

// Grab nibbile from packet and reverse it
byte getNibble(byte buffer[], int nibble)
{
    int pos = nibble / 2;
    int nib = nibble % 2;
    // byte b = Reverse(packet[pos]);
    byte b = buffer[pos];

    if (nib == 1)
        b = (byte)((byte)(b) >> 4);
    else
        b = (byte)((byte)(b) & (byte)(0x0f));

    return b;
}

// http://www.lostbyte.com/Arduino-OSV3/ (9) brian@lostbyte.com
// Directly lifted, then modified from Brian's work, due to nybbles bits now in correct order MSNybble->LSNybble
// CS = the sum of nybbles, 1 to (CSpos-1), compared to CSpos byte (LSNybble) and CSpos+1 byte (MSNybble);
// This sums the nybbles in the packet and creates a 1 byte number, and then compared to the two nybbles beginning at CSpos
// Note that Temp and anemometer uses only 10 bytes but rainfall use 11 bytes per packet. (Rainfall CS spans a byte boundary)
bool ovr3_validateChecksum(byte buffer[], int checksumPos)
{
    byte checksum = 0;
    for (int x = 1; x < checksumPos; x++)
    {
        byte test = getNibble(buffer, x);
        checksum += test;
    }

    byte check1 = getNibble(buffer, checksumPos);
    byte check2 = getNibble(buffer, checksumPos + 1);
    byte check = (check2 << 4) + check1;

    // Serial.print(check1, HEX); //dump out the LSNybble Checksum
    // Serial.print("(LSB), ");
    // Serial.print(check2, HEX); //dump out the MSNybble Checksum
    // Serial.print("(MSB), ");
    // Serial.print(check, HEX); //dump out the Rx'ed predicted byte Checksum
    // Serial.print("(combined),  calculated = ");
    // Serial.print(cs, HEX); //dump out the calculated byte Checksum
    // Serial.print("   ");   //Space it out for the next printout

    if (checksum == check)
    {
        return true;
    }
    return false;
}

uint8_t ovr3_getSync(byte buffer[])
{
    return getNibble(buffer, 0);
}

uint16_t ovr3_getSenderId(byte buffer[])
{
    return (getNibble(buffer, 1) << 12) |
           (getNibble(buffer, 2) << 8) |
           (getNibble(buffer, 3) << 4) |
           getNibble(buffer, 4);
}

uint8_t ovr3_getChannel(byte buffer[])
{
    return getNibble(buffer, 5);
}

uint8_t ovr3_getRollingCode(byte buffer[])
{
    return (getNibble(buffer, 6) << 4) |
           getNibble(buffer, 7);
}

uint8_t ovr3_getFlags(byte buffer[])
{
    return getNibble(buffer, 8);
}

/** °C */
float ovr3_getTemperature(byte buffer[])
{
    float temp = 0.0;
    int value = (getNibble(buffer, 11) * 100) + (getNibble(buffer, 10) * 10) + getNibble(buffer, 9);
    temp = (float)value / 10;

    if (getNibble(buffer, 12) == 1)
        temp *= -1;

    return temp;
}

/** Percent */
int ovr3_getHumidity(byte buffer[])
{
    return (getNibble(buffer, 14) * 10) + getNibble(buffer, 13);
}

/** INCH */
float ovr3_getRainTotalIN(byte buffer[])
{
    float rain_total = 0;
    for (int i = 0; i < 6; i++)
    {
        rain_total = rain_total * 10;
        rain_total += getNibble(buffer, 18 - i);
    }
    return rain_total / 1000.0f;
}

float ovr3_getRainTotal(byte buffer[])
{
    float rain_total = 0;
    for (int i = 0; i < 6; i++)
    {
        rain_total = rain_total * 10;
        rain_total += getNibble(buffer, 18 - i);
    }
    return rain_total / 1000.0f;
}

/** INCH */
float ovr3_getRainRateIN(byte buffer[])
{

    float rain_rate = 0;
    for (int i = 0; i < 5; i++)
    {
        rain_rate = rain_rate * 10;
        rain_rate += getNibble(buffer, 12 - i);
    }
    return rain_rate * .001;
}

int ovr3_getWindDirectionRaw(byte buffer[])
{
    return getNibble(buffer, 9);
}

float ovr3_getWindDirectionDegree(byte buffer[])
{
    return (float)ovr3_getWindDirectionRaw(buffer) * (float)22.5;
}

float ovr3_getWindGust(byte buffer[])
{
    // return ((getNibble(buffer, 13) * 100) + getNibble(buffer, 12)) * .01;
    return ((getNibble(buffer, 13) * 10) + getNibble(buffer, 12)) * 3.6 / 10;
}

float ovr3_getWindAvarage(byte buffer[])
{
    // return ((getNibble(buffer, 16) * 100) + getNibble(buffer, 15)) * .01;
    return ((getNibble(buffer, 16) * 10) + getNibble(buffer, 15)) * 3.6 / 10;
}

boolean ovr3_getSensorMeta(byte buffer[], SensorMeta &meta)
{
    meta.channel = ovr3_getChannel(buffer);
    meta.senderId = ovr3_getSenderId(buffer);
    meta.lowBat = ovr3_getFlags(buffer);

    return true;
}

boolean ovr3_getTempHumiditySensor(byte buffer[], TempHumiditySensor &data)
{
    uint16_t senderId = ovr3_getSenderId(buffer);
    if (senderId == THGN801 || senderId == THGN801x || senderId == THGN801y)
    {
        if (ovr3_validateChecksum(buffer, 16))
        {
            if (ovr3_getSensorMeta(buffer, data.meta))
            {
                data.temperature = ovr3_getTemperature(buffer);
                data.humidity = ovr3_getHumidity(buffer);

                if (BETWEEN(data.temperature, -40, 60) && BETWEEN(data.humidity, 0, 100))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

boolean ovr3_getAnemometerSensor(byte buffer[], AnemometerSensor &data)
{
    uint16_t senderId = ovr3_getSenderId(buffer);
    if (senderId == WGR800 || senderId == WGR800_NO_TEMP)
    {
        if (ovr3_validateChecksum(buffer, 18))
        {
            if (ovr3_getSensorMeta(buffer, data.meta))
            {
                data.gust = ovr3_getWindGust(buffer);
                data.average = ovr3_getWindAvarage(buffer);
                data.direction = ovr3_getWindDirectionDegree(buffer);

                if (BETWEEN(data.gust, 0, 50) && BETWEEN(data.average, 0, 50) && BETWEEN(data.direction, 0, 360))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

boolean ovr3_getRainGaugeSensor(byte buffer[], RainGaugeSensor &data)
{
    uint16_t senderId = ovr3_getSenderId(buffer);
    if (ovr3_validateChecksum(buffer, 16))
    {
        if (senderId == PCR800)
        {
            if (ovr3_getSensorMeta(buffer, data.meta))
            {
                data.bucket_tips = ovr3_getRainTotal(buffer);
            }
        }
    }
    return false;
}

#endif