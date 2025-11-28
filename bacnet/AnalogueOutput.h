#ifndef __ANALOGUEOUTPUT_H_48C7536F_1E91_4cb6_8942_A8AA103CFB11
#define __ANALOGUEOUTPUT_H_48C7536F_1E91_4cb6_8942_A8AA103CFB11
#include "type.h"
#define AOTYPE_VOLTAGE 0
#define AOTYPE_CUR020 1
#define AOTYPE_CUR420 2
#define AOTYPE_VOLRAW 0x80
#define AOTYPE_CURRAW 0x81
#define AOTYPE_END 5
typedef unsigned char BYTE;
typedef struct {
  int meta;
  char strName[16];
  BYTE channel;
} AODATASHORT;

// YIN 3.0
typedef struct {
  BYTE index;
  BYTE clampingHighEnable;
  BYTE clampingLowEnable;
  BYTE outOfService;
  BYTE resetMinValue;
  BYTE resetMaxValue;
  BYTE reverseOutput;
  BYTE squarerootOutput;

  short type;

  float value;
  float rawValue;
  float minValue;
  float maxValue;

  float clampingHigh;
  float clampingLow;
  float scaleHigh;
  float scaleLow;
  float setValue;
  float in1;
  float in2;
  float in3;
  float in4;
  float in5;
  float in6;
  float in7;
  float in8;
  float in9;
  float in10;
  float in11;
  float in12;
  float in13;
  float in14;
  float in15;
  float in16;
} AODATAV2;

typedef struct {
  /////////////////Data/////////////////////////////
  AODATAV2 aodata;
  /////////////////Operations///////////////////////
  float AnalogueOutputEmergency;      /////////////Emergency
  float AnalogueOutputManualOverride; /////////////Manual Override
  uint16_t AnalogueOutputTypePre;
  float AOPriorityArray[16];
} ANALOGOUTPUTOBJECT;

#endif /* end of __ANALOGUEOUTPUT_H_48C7536F_1E91_4cb6_8942_A8AA103CFB11 */
