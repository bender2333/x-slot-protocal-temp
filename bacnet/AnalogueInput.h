#ifndef __ANALOGUEINPUT_H_E2B3F8CF_1D1D_4a52_A0DD_5A6563ABF0F3
#define __ANALOGUEINPUT_H_E2B3F8CF_1D1D_4a52_A0DD_5A6563ABF0F3

#define AITEMPDAMPINGFACTOR                                                    \
  128 // This is for Sensor Mode, decrease it based on speed requirement
#include "type.h"
typedef enum {
  AITYPE_VOLTAGE,
  AITYPE_CURRENT,
  AITYPE_RESISTANCE,
  AITYPE_THERMISTOR,
  AITYPE_DI,
} AI_TYPE;

#define AI_NO_FAULT 0
#define AI_OPEN 1
#define AI_SHORT 2
#define AI_OVERRANGE 3
#define AI_UNDERRANGE 4
#define AI_NOSENSOR 5

#define ALARMOFF 0
#define ALARMHIGH 0x01
#define ALARMLOW 0x02
#define ALARMONHIGH 0x10
#define ALARMONLOW 0x20
#define ALARMON 0x30 // ALARMONHIGH|ALARMONLOW

typedef struct {
  int meta;
  char strName[16];
  BYTE channel;
} UIDATASHORT;

// YIN 3.0
typedef struct {
  BYTE index;
  BYTE alarm;
  BYTE alarmType;

  BYTE alarmReset;
  BYTE alarmResetType;
  BYTE highAlarmEnable;
  BYTE lowAlarmEnable;
  BYTE linearization;
  BYTE lowCutoffEnable;
  BYTE outOfService;
  BYTE resetMinValue;
  BYTE resetMaxValue;

  short reliability;
  short alarmDelay;
  short type;
  short decimalPoint;
  short temperatureTable;

  float maxValue;
  float minValue;
  float rawValue;

  float value;

  float alarmDeadband;

  float alarmHighLimit;
  float alarmLowLimit;

  float digitalOffLevel;
  float digitalOnLevel;
  float lowCutoffValue;
  float offset;
  float scaleHighValue;
  float scaleLowValue;
  float userSetValue;
} UIDATA;

typedef struct {
  ////// Data ////////////////////////////////////////////////////
  UIDATA uidata;

  ////// Operations //////////////////////////////////////////////
  u16_t AnalogueInputTypePre; //+ 2018-02-09
  double AIAverageValue;
  BYTE AIAverageCount;
  float m_AIDecimal[5];
  u8_t m_AIAlarmStatus;
  u16_t m_AIAlarmTime;
  u8_t m_AIDigital;
  u8_t m_AIGain;
} ANALOGINPUTOBJECT;

#endif /* end of __ANALOGUEINPUT_H_E2B3F8CF_1D1D_4a52_A0DD_5A6563ABF0F3 */
