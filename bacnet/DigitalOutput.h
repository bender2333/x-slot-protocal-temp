#ifndef __DIGITALOUTPUT_H_F5C8FBE4_249F_4b3e_B1EB_5DDBB072F945
#define __DIGITALOUTPUT_H_F5C8FBE4_249F_4b3e_B1EB_5DDBB072F945

#include "type.h"

typedef struct {
  int meta;
  char strName[16];
  BYTE channel;
} DODATASHORT;

// YIN 3.0
typedef struct {
  BYTE index;
  BYTE out;

  BYTE in1;
  BYTE in2;
  BYTE in3;
  BYTE in4;
  BYTE in5;
  BYTE in6;
  BYTE in7;
  BYTE in8;
  BYTE in9;
  BYTE in10;
  BYTE in11;
  BYTE in12;
  BYTE in13;
  BYTE in14;
  BYTE in15;
  BYTE in16;
  BYTE setState;

  BYTE outOfService;
  BYTE polarity;
  BYTE minOffOnStart;

  BYTE resetOffCounter;
  BYTE resetOnCounter;
  BYTE resetOffTimer;
  BYTE resetOnTimer;

  short interOutputDelay;
  short minOffTime;
  short minOnTime;

  short offCounter;
  short onCounter;

  u32_t offTimer;
  u32_t onTimer;
} DODATAV2;

typedef struct {
  //////Data///////////////////////
  DODATAV2 dodata;
  //////Operations/////////////////
  u8_t DO_ManualOverrideOff;
  u8_t DO_ManualOverrideOn;
  u8_t DO_ManualEmergencyOff;
  u8_t DO_ManualEmergencyOn;
  u8_t DO_PriorityArray[16];
} DIGITALOUTPUTOBJECT;

#endif /* end of __DIGITALOUTPUT_H_F5C8FBE4_249F_4b3e_B1EB_5DDBB072F945 */
