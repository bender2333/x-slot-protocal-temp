#ifndef __DIGITALINPUT_H_0484A059_08BE_4ea5_9168_773E218B16C1
#define __DIGITALINPUT_H_0484A059_08BE_4ea5_9168_773E218B16C1

#include "type.h"

typedef struct {
  int meta;
  char strName[16];
  BYTE channel;
} DIDATASHORT;

// YIN 3.0
typedef struct {
  BYTE index;
  BYTE state;
  BYTE alarm;
  BYTE offLatch;
  BYTE onLatch;

  BYTE alarmEnable;
  BYTE alarmMonitorState;
  BYTE alarmResetType;
  BYTE clearOffLatch;
  BYTE clearOnLatch;
  BYTE outOfService;
  BYTE polarity;
  BYTE resetAlarm;
  BYTE resetOffCounter;
  BYTE resetOnCounter;
  BYTE resetOffTimer;
  BYTE resetOnTimer;
  BYTE userSetState;

  short offCounter;
  short onCounter;
  short alarmDelayTime;

  u32_t offTimer;
  u32_t onTimer;

} DIDATA;

typedef struct {
  ////////Data//////////////
  DIDATA didata;
  ////////Operations////////
  s8_t m_DIReadCounter;
} DIGITALINPUTOBJECT;

#endif /* end of __DIGITALINPUT_H_0484A059_08BE_4ea5_9168_773E218B16C1 */
