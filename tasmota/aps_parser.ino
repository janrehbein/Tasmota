// I use this for coding, it resolves all visual studio code issue
// #define ZB_COMILE_TIME_HELPERS

#ifdef ZB_COMILE_TIME_HELPERS
#include "stdio.h"
#include "tasmota.h"
#include "tasmota_globals.h"
#include "i18n.h"
#include "support.ino"
#include "SBuffer.hpp"
#include "xdrv_02_mqtt.ino"
#include "xdrv_23_zigbee_5_converters.ino"
#include "xdrv_23_zigbee_0_constants.ino"
#include "xdrv_23_zigbee_1_headers.ino"
#include "xdrv_23_zigbee_8_parsers.ino"
#include "xdrv_23_zigbee_9_serial.ino"
#include "my_user_config.h"
#endif

#define APS_OFFSET 19 // OFFSET for raw AfIncomingMessage

#define APS_INV_ID 0 + APS_OFFSET       // 6 bytes
#define APS_TEMPERATURE 10 + APS_OFFSET // 2 bytes
#define APS_FREQUENCY 12 + APS_OFFSET   // 3 bytes
#define APS_VOLTAGE_AC 28 + APS_OFFSET  // 2 bytes

//YC600 Byteoffset
#define APS_YC600_TIMESTAMP 15 + APS_OFFSET        // 2 bytes
#define APS_YC600_CURRENT_CH1 22 + APS_OFFSET      // 1.5 byte
#define APS_YC600_VOLTAGE_CH1 24 + APS_OFFSET      // 1 byte
#define APS_YC600_CURRENT_CH2 25 + APS_OFFSET      // 1.5 byte
#define APS_YC600_VOLTAGE_CH2 27 + APS_OFFSET      // 1 byte
#define APS_YC600_TODAY_ENERGY_CH1 42 + APS_OFFSET // 3 bytes
#define APS_YC600_TODAY_ENERGY_CH2 37 + APS_OFFSET // 3 bytes

//QS1 Byteoffset
#define APS_OFFSET 19 // OFFSET for tasmota Zigbee Frame
#define APS_QS1_TIMESTAMP 30 + APS_OFFSET   // 2 bytes
#define APS_QS1_CURRENT_CH1 25 + APS_OFFSET // 1.5 byte
#define APS_QS1_VOLTAGE_CH1 15 + APS_OFFSET // 1 byte
#define APS_QS1_CURRENT_CH2 22 + APS_OFFSET // 1.5 byte
#define APS_QS1_VOLTAGE_CH2 24 + APS_OFFSET // 1 byte
#define APS_QS1_CURRENT_CH3 19 + APS_OFFSET // 1.5 byte
#define APS_QS1_VOLTAGE_CH3 21 + APS_OFFSET // 1 byte
#define APS_QS1_CURRENT_CH4 16 + APS_OFFSET // 1.5 byte
#define APS_QS1_VOLTAGE_CH4 18 + APS_OFFSET // 1 byte
#define APS_QS1_TODAY_ENERGY_CH1 39 + APS_OFFSET // 3 bytes
#define APS_QS1_TODAY_ENERGY_CH2 44 + APS_OFFSET // 3 bytes
#define APS_QS1_TODAY_ENERGY_CH3 49 + APS_OFFSET // 3 bytes
#define APS_QS1_TODAY_ENERGY_CH4 54 + APS_OFFSET // 3 bytes

// int timeLastTelegram = 0;
uint16_t lastTimeStamp;
uint32_t lastTodayEnergyCh1Raw;
uint32_t lastTodayEnergyCh2Raw;
uint32_t lastTodayEnergyCh3Raw;
uint32_t lastTodayEnergyCh4Raw;

uint64_t getBigEndian(const class SBuffer &buff, size_t offset, size_t len)
{
  uint64_t value = 0;
  for (size_t i = 0; i < len; i++)
  {
    value |= ((uint64_t)buff.get8(offset + len - 1 - i) << (8 * i));
  }
  return value;
}

boolean Z_APS_Parser(const class SBuffer &buff, const uint16_t srcaddr)
{

  float powerCh1 = 0;
  float powerCh2 = 0;
  float powerCh3 = 0;
  float powerCh4 = 0;

  float calcpowerCh1 = 0;
  float calcpowerCh2 = 0;
  float calcpowerCh3 = 0;
  float calcpowerCh4 = 0;

  boolean isQs1 = false;
  uint16_t inverterIDPre = buff.get16BigEndian(APS_INV_ID);

  if (inverterIDPre == 0x8010)
  {
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Inverter ID-Pre (QS1): 0x%04X"), inverterIDPre);
    isQs1 = true;
  }
  else if (inverterIDPre == 0x4080)
  {
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Inverter ID-Pre (YC600): 0x%04X"), inverterIDPre);
  }
  else
  {
    // not a QS1 or YC600 telegram, end with false
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Inverter ID-Pre (invalid): 0x%04X"), inverterIDPre);
    return false;
  }

  uint16_t timeStamp = buff.get32BigEndian(isQs1 ? APS_QS1_TIMESTAMP : APS_YC600_TIMESTAMP);
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("TimeStamp %d"), timeStamp);

  float voltageAc = buff.get16BigEndian(APS_VOLTAGE_AC) * 0.1873f;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("AC Voltage %1_f V"), &voltageAc);

  uint32_t tmp = getBigEndian(buff, APS_FREQUENCY, 3);
  float frequence = .0f;
  if (tmp > 0)
  {
    frequence = 50000000.0f / tmp;
  }
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Frequence %2_f Hz"), &frequence);

  float temperature = 0.2752f * buff.get16BigEndian(APS_TEMPERATURE) - 258.7f;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Temperature %1_f C"), &temperature);

  // Channel 1

  float voltageCh1 = buff.get8(isQs1 ? APS_QS1_VOLTAGE_CH1 : APS_YC600_VOLTAGE_CH1) / 3.0f;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Voltage CH1 %1_f V"), &voltageCh1);

  float currentCh1 = (((buff.get8(isQs1 ? APS_QS1_CURRENT_CH1 : APS_YC600_CURRENT_CH1 + 1) & 0x0F) << 8) | buff.get8(isQs1 ? APS_QS1_CURRENT_CH1 : APS_YC600_CURRENT_CH1)) / 160.0f;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Current CH1 %4_f A"), &currentCh1);

  uint32_t todayEnergyCh1Raw = getBigEndian(buff, isQs1 ? APS_QS1_TODAY_ENERGY_CH1 : APS_YC600_TODAY_ENERGY_CH1, 3);
  float todayEnergyCh1 = todayEnergyCh1Raw * 8.311f / 3600 / 1000;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Today Energy CH1 %3_f kW"), &todayEnergyCh1);

  // Channel 2

  float voltageCh2 = buff.get8(isQs1 ? APS_QS1_VOLTAGE_CH2 : APS_YC600_VOLTAGE_CH2) / 3.0f;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Voltage CH2 %1_f V"), &voltageCh2);

  float currentCh2 = (((buff.get8(isQs1 ? APS_QS1_CURRENT_CH2 : APS_YC600_CURRENT_CH2 + 1) & 0x0F) << 8) | buff.get8(isQs1 ? APS_QS1_CURRENT_CH2 : APS_YC600_CURRENT_CH2)) / 160.0f;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Current CH2 %4_f A"), &currentCh2);

  uint32_t todayEnergyCh2Raw = getBigEndian(buff, isQs1 ? APS_QS1_TODAY_ENERGY_CH2 : APS_YC600_TODAY_ENERGY_CH2, 3);
  float todayEnergyCh2 = todayEnergyCh2Raw * 8.311f / 3600 / 1000;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Today Energy CH2 %3_f kW"), &todayEnergyCh2);

  uint32_t timeDiff = lastTimeStamp == 0 ? 0 : timeStamp - lastTimeStamp;
  AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Time Diff %d"), timeDiff);

  if (timeDiff > 0)
  {
    // Channel 1

    powerCh1 = ((todayEnergyCh1Raw - lastTodayEnergyCh1Raw) * 8.311f) / timeDiff;
    calcpowerCh1 = voltageCh1 * currentCh1;
    lastTodayEnergyCh1Raw = todayEnergyCh1Raw;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("CalcPower CH1 %2_f W"), &calcpowerCh1);
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Power CH1 %2_f W"), &powerCh1);

    // Channel 2

    powerCh2 = ((todayEnergyCh2Raw - lastTodayEnergyCh2Raw) * 8.311f) / timeDiff;
    calcpowerCh2 = voltageCh2 * currentCh2;
    lastTodayEnergyCh2Raw = todayEnergyCh2Raw;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("CalcPower CH2 %2_f W"), &calcpowerCh2);
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Power CH2 %2_f W"), &powerCh2);
  }

  if (isQs1)
  {
    // Channel 3

    float voltageCh3 = buff.get8(APS_QS1_VOLTAGE_CH3) / 3.0f;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Voltage CH3 %1_f V"), &voltageCh3);

    float currentCh3 = (((buff.get8(APS_QS1_CURRENT_CH3 + 1) & 0x0F) << 8) | buff.get8(APS_QS1_CURRENT_CH3)) / 160.0f;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Current CH3 %4_f A"), &currentCh3);

    uint32_t todayEnergyCh3Raw = getBigEndian(buff, APS_QS1_TODAY_ENERGY_CH3, 3);
    float todayEnergyCh3 = todayEnergyCh3Raw * 8.311f / 3600 / 1000;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Today Energy CH3 %3_f kW"), &todayEnergyCh3);

    // Channel 4

    float voltageCh4 = buff.get8(APS_QS1_VOLTAGE_CH4) / 3.0f;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Voltage CH4 %1_f V"), &voltageCh4);

    float currentCh4 = (((buff.get8(APS_QS1_CURRENT_CH4 + 1) & 0x0F) << 8) | buff.get8(APS_QS1_CURRENT_CH4)) / 160.0f;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Current CH4 %4_f A"), &currentCh4);

    uint32_t todayEnergyCh4Raw = getBigEndian(buff, APS_QS1_TODAY_ENERGY_CH4, 3);
    float todayEnergyCh4 = todayEnergyCh4Raw * 8.311f / 3600 / 1000;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Today Energy CH4 %3_f kW"), &todayEnergyCh4);

    if (timeDiff > 0)
    {
      powerCh3 = ((todayEnergyCh1Raw - lastTodayEnergyCh1Raw) * 8.311f) / timeDiff;
      calcpowerCh3 = voltageCh3 * currentCh3;
      lastTodayEnergyCh3Raw = todayEnergyCh3Raw;
      AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("CalcPower CH3 %2_f W"), &calcpowerCh3);
      AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Power CH3 %2_f W"), &powerCh3);

      calcpowerCh4 = voltageCh4 * currentCh4;
      powerCh4 = ((todayEnergyCh2Raw - lastTodayEnergyCh2Raw) * 8.311f) / timeDiff;
      lastTodayEnergyCh4Raw = todayEnergyCh4Raw;
      AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("CalcPower CH4 %2_f W"), &calcpowerCh4);
      AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Power CH4 %2_f W"), &powerCh4);
    }

    Response_P(PSTR("{\"0x%04X\": {\"timestamp\": %d, \"voltageAC\": %1_f, \"frequence\": %2_f, \"temperature\": %1_f, \"voltageCh1\": %1_f, \"currentCh1\": %3_f,\"powerCh1\": %3_f, \"todayEnergyCh1\": %3_f, \"voltageCh2\": %1_f,\"currentCh2\": %3_f,\"powerCh2\": %3_f, \"todayEnergyCh2\": %3_f, \"voltageCh3\": %1_f,\"currentCh3\": %3_f,\"powerCh3\": %3_f, \"todayEnergyCh3\": %3_f, \"voltageCh4\": %1_f, \"currentCh4\": %3_f,\"powerCh4\": %3_f, \"todayEnergyCh4\": %3_f}}"),
               srcaddr, timeStamp, &voltageAc, &frequence, &temperature,
               &voltageCh1, &currentCh1, &powerCh1, &todayEnergyCh1,
               &voltageCh2, &currentCh2, &powerCh2, &todayEnergyCh2,
               &voltageCh3, &currentCh3, &powerCh3, &todayEnergyCh3,
               &voltageCh4, &currentCh4, &powerCh4, &todayEnergyCh4);
  }
  else
  {

    Response_P(PSTR("{\"0x%04X\": {\"timestamp\": %d, \"voltageAC\": %1_f, \"frequence\": %2_f, \"temperature\": %1_f, \"voltageCh1\": %1_f, \"currentCh1\": %3_f,\"powerCh1\": %3_f, \"todayEnergyCh1\": %3_f, \"voltageCh2\": %1_f,\"currentCh2\": %3_f,\"powerCh2\": %3_f, \"todayEnergyCh2\": %3_f}}"),
               srcaddr, timeStamp, &voltageAc, &frequence, &temperature,
               &voltageCh1, &currentCh1, &powerCh1, &todayEnergyCh1,
               &voltageCh2, &currentCh2, &powerCh2, &todayEnergyCh2);
  }

  // update global data for next time
  lastTimeStamp = timeStamp;

  MqttPublishTeleSensor();

  return true;
}
