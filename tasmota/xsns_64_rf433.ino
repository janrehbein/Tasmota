/*
  xsns_64_rf433.ino - La Crosse Tx20/Tx23 wind sensor support for Tasmota

  Copyright (C) 2020  Thomas Eckerstorfer, Norbert Richter and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define USE_RF433
#ifdef USE_RF433

#define XSNS_64 64

// #define RF433_DEBUG

// #warning **** XSNS_64 XSNS_64 XSNS_64 XSNS_64 XSNS_64 XSNS_64 ****

// The Arduino standard GPIO routines are not enough,
// must use some from the Espressif SDK as well
extern "C" {
#include "gpio.h"
}
#include <RcOok2.h>
#include <LightweightRingBuff.h>
#include <sensor-kw9010.h>
#include <sensor-oregon-v3.h>
// #include "tasmota.h";

RingBuff_t puleWideBuffer;
OregonDecoderV3 orscV3;
KW9019Decoder kw9010;

byte OokReceivedData[100];
byte OokReceivedDataSize;
bool OokAvailableCode = false;

RainPeriod currentRainPeriod;
RainPeriodHistory lastRainPeriod;

#ifndef ARDUINO_ESP8266_RELEASE_2_3_0      // Fix core 2.5.x ISR not in IRAM Exception
void Rf433StartRead(void) ICACHE_RAM_ATTR; // As iram is tight and it works this way too
#endif                                     // ARDUINO_ESP8266_RELEASE_2_3_0

const char S_JSON_RF433_WIND[] PROGMEM = "{\"Wind-%d\":{\"Guest\":%s,\"Average\":%s,\"Direction\":\"%s\",\"LowBattery\":%s}}";

const char S_JSON_RF433_TEMP_HUM[] PROGMEM =  "{\"TempHum-%d\":{\"" D_JSON_TEMPERATURE "\":%s,\"" D_JSON_HUMIDITY "\":%d,\"LowBattery\":%s}}";

const char S_JSON_RF433_RAIN[] PROGMEM =  "{\"Rain-%d\":{\"Buckets\":%d,\"" D_JSON_TEMPERATURE "\":%s,\"Trend\":\"%s\",\"RainTotal\":%d,\"LowBattery\":%s}}";

void Rf433StartRead(void)
{
  static unsigned int duration;
  static unsigned long lastTime;

  long time = micros();
  duration = time - lastTime;
  lastTime = time;

  word p = (unsigned short int)duration;

  RingBuffer_Insert(&puleWideBuffer, p);

  // Must clear this bit in the interrupt register,
  // it gets set even when interrupts are disabled
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << pin[GPIO_RF433_RX]);
}

AnemometerSensor windSensor;

void Rf433Read(void)
{
  if (RingBuffer_IsFull(&puleWideBuffer))
  {
#ifdef RF433_DEBUG
    Serial.print("puleWideBuffer FULL !!!!!!");
#endif
    RingBuffer_InitBuffer(&puleWideBuffer);
  }

  while (!RingBuffer_IsEmpty(&puleWideBuffer))
  {
    word p = RingBuffer_Remove(&puleWideBuffer);

    // Avoid re-entry
    if (!OokAvailableCode)
    { // avoid reentrance -- wait until data is read
      if (orscV3.nextPulse(p)){ OokAvailableCode = true; orscV3.transfer(OokReceivedData, OokReceivedDataSize);  orscV3.resetDecoder();}
      if (kw9010.nextPulse(p)){ OokAvailableCode = true; kw9010.transfer(OokReceivedData, OokReceivedDataSize); kw9010.resetDecoder(); }
    }
  }

  if (OokAvailableCode)
  {
#ifdef RF433_DEBUG
    Serial.print(" --->");

    printBuffer(OokReceivedData, OokReceivedDataSize);
    Serial.print(" : ");
#endif

    KW9015Sensor kw9015sensor;
    if (kw9010_getKW9015Sensor(OokReceivedData, OokReceivedDataSize, kw9015sensor))
    {
      OokAvailableCode = false;

#ifdef RF433_DEBUG
      Serial.print(" Temp: ");
      Serial.print(kw9015sensor.temperature);

      Serial.print(" Trend: ");
      Serial.print(kw9015sensor.temperatureTrend);
      Serial.print(
          kw9015sensor.temperatureTrend == TEMP_RISING ? "+" : kw9015sensor.temperatureTrend == TEMP_FALLING ? "-" : kw9015sensor.temperatureTrend == TEMP_STEADY ? "=" : "?");

      Serial.print(" Rain: ");
      Serial.print(kw9015sensor.rainFlaps);

      Serial.print(" LowBat: ");
      Serial.print(kw9015sensor.meta.lowBat);

      Serial.print(" senderId: ");
      Serial.print(kw9015sensor.meta.senderId);

      Serial.print(" channel: ");
      Serial.print(kw9015sensor.meta.channel);
#endif
      RAIN_STATUS status = calculateRainData(currentRainPeriod, lastRainPeriod, kw9015sensor.rainFlaps);
      if (status == RAIN_STATUS_PERIOD_OVER || status == RAIN_STATUS_OK)
      {

        if(status == RAIN_STATUS_PERIOD_OVER)
        {
          Serial.println("PERIOD OVER ...................");
        }

        char temperature[33];
        dtostrfd(kw9015sensor.temperature, 2, temperature);

        const char *trend = kw9015sensor.temperatureTrend == TEMP_RISING ? PSTR("+") : 
          kw9015sensor.temperatureTrend == TEMP_FALLING ? PSTR("-") : 
          kw9015sensor.temperatureTrend == TEMP_STEADY ? PSTR("=") : 
          PSTR("?");

        const char *lowBatt = windSensor.meta.lowBat ? PSTR("true") : PSTR("false");

        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_RF433_RAIN, 
          kw9015sensor.meta.senderId, kw9015sensor.rainFlaps, temperature, trend, currentRainPeriod.total, lowBatt );
        MqttPublishPrefixTopic_P(RESULT_OR_TELE, mqtt_data);
      }
    }

    TempHumiditySensor tempSensor;
    if (ovr3_getTempHumiditySensor(OokReceivedData, tempSensor))
    {
      OokAvailableCode = false;
#ifdef RF433_DEBUG
      Serial.print(" Temp: ");
      Serial.print(tempSensor.temperature);

      Serial.print(" Hum: ");
      Serial.print(tempSensor.humidity);

      Serial.print(" LowBat: ");
      Serial.print(tempSensor.meta.lowBat);

      Serial.print(" senderId: ");
      Serial.print(tempSensor.meta.senderId);

      Serial.print(" channel: ");
      Serial.print(tempSensor.meta.channel);
#endif

      char temperature[33];
      dtostrfd(tempSensor.temperature, 2, temperature);

      const char *lowBatt = windSensor.meta.lowBat ? PSTR("true") : PSTR("false");

      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_RF433_TEMP_HUM, 
        tempSensor.meta.senderId, temperature, tempSensor.humidity, lowBatt);
      MqttPublishPrefixTopic_P(RESULT_OR_TELE, mqtt_data);
    }

    
    if (ovr3_getAnemometerSensor(OokReceivedData, windSensor))
    {
      OokAvailableCode = false;

#ifdef RF433_DEBUG
      Serial.print(" Gust: ");
      Serial.print(windSensor.gust);

      Serial.print(" Avg: ");
      Serial.print(windSensor.average);

      Serial.print(" Direction: ");
      Serial.print(windSensor.direction);

      Serial.print(" LowBat: ");
      Serial.print(windSensor.meta.lowBat);

      Serial.print(" senderId: ");
      Serial.print(windSensor.meta.senderId);

      Serial.print(" channel: ");
      Serial.print(windSensor.meta.channel);
#endif
      char gust[33];
      dtostrfd(windSensor.gust, 2, gust);
      char average[33];
      dtostrfd(windSensor.average, 2, average);
      char direction[] = "SSW";

      const char *lowBatt = windSensor.meta.lowBat ? PSTR("true") : PSTR("false");

      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_RF433_WIND,
        windSensor.meta.senderId, gust, average, direction, lowBatt);
      MqttPublishPrefixTopic_P(RESULT_OR_TELE, mqtt_data);
    }
#ifdef RF433_DEBUG
    Serial.println();
#endif
    OokAvailableCode = false;
  }
}

void Rf433Init(void)
{
  pinMode(pin[GPIO_RF433_RX], INPUT);
  attachInterrupt(pin[GPIO_RF433_RX], Rf433StartRead, CHANGE);

  RingBuffer_InitBuffer(&puleWideBuffer);
}

// const char JSON_SNS_TEMPHUMX[] PROGMEM = ",\"%s\":{\"" D_JSON_TEMPERATURE "\":%s,\"" D_JSON_HUMIDITY "\":%s}";
// const char DOMOTICZ_MESSAGE[] PROGMEM = "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%s\",\"Battery\":%d,\"RSSI\":%d}";

// void Rf433ShowMeta(SensorMeta meta, bool json)
// {
//   if(json)
//   {
//     char senderId[10];
//     ultoa(windSensor.meta.senderId, senderId, 10);

//     char channel[10];
//     ultoa(windSensor.meta.channel, channel, 10);

//     ResponseAppend_P(PSTR(",\"" "meta" "\":{\"LowBattery\":%s,\"SenderId\":%s,\"Channel\":%s}"),
//       (windSensor.meta.lowBat ? PSTR("true") : PSTR("false")), senderId, channel);
//   }
// }

// void Rf433Show(bool json)
// {
//   if(json)
//   {

//     if(windSensor.meta.senderId != 0xFFFF) {

//     char gust[33];
//     dtostrfd(windSensor.gust, 1, gust);
//     char average[33];
//     dtostrfd(windSensor.average, 1, average);
//     char direction[] = "SSW";

//     // ResponseAppend_P(JSON_SNS_TEMPHUM, "abc", gust, average);

//     // char gust[33];
//     // dtostrfd(windSensor.gust, 1, gust);
//     // char average[33];
//     // dtostrfd(windSensor.average, 1, average);
//     // char direction[] = "SSW";

//     char senderId[10];
//     ultoa(windSensor.meta.senderId, senderId, 10);

//     char channel[10];
//     ultoa(windSensor.meta.channel, channel, 10);

//     ResponseAppend_P(PSTR(",\"" "meta" "\":{\"LowBattery\":%s,\"SenderId\":%s,\"Channel\":%s}"),
//       (windSensor.meta.lowBat ? PSTR("true") : PSTR("false")), senderId, channel);

//     ResponseAppend_P(PSTR(",\"" "wind" "\":{\"Guest\":%s,\"Average\":%s,\"Direction\":\"%s\"}"),
//       gust, average, direction);





//     }


//   }
// }

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns64(uint8_t function)
{
  bool result = false;

  if (pin[GPIO_RF433_RX] < 99)
  {
    switch (function)
    {
    case FUNC_INIT:
      Rf433Init();
      break;
    case FUNC_LOOP:
      Rf433Read();
      break;
    case FUNC_JSON_APPEND:
      // Rf433Show(1);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      // Rf433Show(0);
      break;
#endif // USE_WEBSERVER
    }
  }
  return result;
}

#endif // USE_TX20_WIND_SENSOR || USE_TX23_WIND_SENSOR