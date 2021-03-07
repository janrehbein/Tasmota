// #include "aps_parser.h"

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

#define APS_OFFSET              -3                  // OFFSET for tasmota Zigbee Frame
#define APS_INV_ID              0    + APS_OFFSET       // 6 bytes
#define APS_TEMPERATURE         10   + APS_OFFSET       // 2 bytes
#define APS_FREQUENCY           12   + APS_OFFSET       // 3 bytes
#define APS_TIMESTAMP           15   + APS_OFFSET       // 2 bytes
#define APS_CURRENT_CH2         22   + APS_OFFSET       // 2.5 byte
#define APS_VOLTAGE_CH2         24   + APS_OFFSET       // 1 byte
#define APS_CURRENT_CH1         25   + APS_OFFSET       // 2.5 byte  
#define APS_VOLTAGE_CH1         27   + APS_OFFSET       // 1 byte
#define APS_VOLTAGE_AC          28   + APS_OFFSET       // 2 bytes
#define APS_TODAY_ENERGY_CH1    37   + APS_OFFSET       // x bytes
#define APS_TODAY_ENERGY_CH2    42   + APS_OFFSET       // x bytes

uint64_t getBigEndian(class SBuffer buff, size_t offset, size_t len)
{
    uint64_t value = 0;
    for (size_t i = 0; i < len; i++)
    {
        value |= ((uint64_t)buff.get8(offset+len -1 - i) <<  (8*i));
    }
    return value;
}

void Z_APS_Parser(const class SBuffer &buff) {
    AddLog(LOG_LEVEL_ERROR, PSTR(D_LOG_ZIGBEE "-------------------------> %s"), "xxxxxx");

    uint16_t timeStamp = buff.get32BigEndian(APS_TIMESTAMP);
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("TimeStamp %d"), timeStamp);

    float voltageAc = buff.get16BigEndian(APS_VOLTAGE_AC) * 0.1873f;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("AC Voltage %1_f V"), &voltageAc);

    // uint32_t tmp = getBigEndian(buff, APS_FREQUENCY, 3);
    float frequence = 12.3f;
    // if (tmp > 0) {
    //     frequence = 50000000.0f / tmp;
    // }
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Frequence %2_f Hz"), &frequence);

    float temperature = 0.2752f * buff.get16BigEndian(APS_TEMPERATURE) - 258.7f;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Temperature %1_f C"), &temperature);

    // Channel 1

    float voltageCh1 = buff.get8(APS_VOLTAGE_CH1) / 3;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Voltage CH1 %1_f V"), &voltageCh1);

    float todayEnergyCh1 = buff.get32BigEndian(APS_TODAY_ENERGY_CH1) * 8.311f / 3600 / 1000;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Today Energy CH1 %2_f kW"), &todayEnergyCh1);

    // Channel 2

    float voltageCh2 = buff.get8(APS_VOLTAGE_CH2) / 3;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Voltage CH2 %1_f V"), &voltageCh2);

    float todayEnergyCh2 = buff.get32BigEndian(APS_TODAY_ENERGY_CH2) * 8.311f / 3600 / 1000;
    AddLog_P(LOG_LEVEL_DEBUG_MORE, PSTR("Today Energy H2 %2_f kW"), &todayEnergyCh2);

    // send MQTT message
    Response_P(PSTR("{\"Timestamp\": %d, \"VoltageAC\": %1_f, \"Frequence\": %2_f, \"Temperature\": %1_f, \"voltage1\": %1_f, \"todayEnergy1\": %2_f, \"voltage2\": %1_f, \"todayEnergy2\": %2_f}"), 
        timeStamp, &voltageAc, &frequence, &temperature, &voltageCh1, &todayEnergyCh1, &voltageCh2, &todayEnergyCh2 );

    MqttPublishTeleSensor();
}