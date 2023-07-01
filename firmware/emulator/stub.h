//*****************************************************************************
// Filename : 'stub.h'
// Title    : Stub definitions for emuchron emulator
//*****************************************************************************

#ifndef STUB_H
#define STUB_H

#include <stddef.h>
#include "../avrlibtypes.h"

// Keyboard input mode
#define KB_MODE_LINE		0
#define KB_MODE_SCAN		1

// Instructions for setting mchron date/time
#define DT_DATE_KEEP		70	// Keep current date
#define DT_DATE_RESET		80	// Reset to system date
#define DT_TIME_KEEP		70	// Keep current time
#define DT_TIME_RESET		80	// Reset to system time

// Eeprom stubs
uint8_t eeprom_read_byte(uint8_t *eprombyte);
void eeprom_write_byte(uint8_t *eprombyte, uint8_t value);
void stubEepReset(void);

// Beep stub
void stubBeep(uint16_t hz, uint8_t msec);

// Button stub
void btnInit(void);

// Delay stub
void _delay_ms(int x);

// RTC stubs
void i2cInit(void);
u08 i2cMasterReceiveNI(u08 deviceAddr, u08 length, u08 *data);
u08 i2cMasterSendNI(u08 deviceAddr, u08 length, u08* data);
u08 stubTimeSet(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
  uint8_t mon, uint8_t yr);

// UART (debug) output stub
void stubUartPutChar(void);

// Below is emulator oriented stub functionality

// Monochron emulator stubs
char stubEventGet(u08 stats);
void stubEventInit(u08 startWait, u08 cfgTimeout,
  void (*stubHelpHandler)(void));
u08 stubEventQuitGet(void);

// Logfile stubs
void stubLogfileClose(void);
u08 stubLogfileOpen(char debugFile[]);

// Alarm sound and switch stubs
void alarmSoundReset(void);
void alarmSwitchSet(u08 on, u08 show);
void alarmSwitchShow(void);
void alarmSwitchToggle(u08 show);

// Keyboard input and keypress events
char kbKeypressScan(u08 quitFind);
u08 kbModeGet(void);
void kbModeSet(u08 mode);

// Statistics
void stubStatsPrint(void);
void stubStatsReset(void);

// Help pages when running the clock or Monochron emulator
void stubHelpClockFeed(void);
void stubHelpMonochron(void);
#endif
