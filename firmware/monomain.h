//*****************************************************************************
// Filename : 'monomain.h'
// Title    : Defines for the main clock engine for MONOCHRON
//*****************************************************************************

#ifndef MONOMAIN_H
#define MONOMAIN_H

#include "avrlibtypes.h"

// Define application clock cycle msec timer value for animation and keypress
// handling. Note that redrawing takes some time too so you don't want this too
// small or your clock will 'hiccup' and appear jittery.
#define ANIM_TICK_CYCLE_MS	75

// Constants for how to display time & date. Those commented out are no longer
// supported, and related code has been removed from the code base.
//#define REGION_US	0
//#define REGION_EU	1
//#define DOW_REGION_US	2
//#define DOW_REGION_EU	3
//#define DATELONG	4
#define DATELONG_DOW	5
//#define TIME_12H	0
#define TIME_24H	1

// Every time a final change is made to a value in one of the config pages,
// except for date/time, the end result is written back to eeprom. Each eeprom
// location in an atmega328p lasts for ~100k reset/write cycles. This should be
// enough for our Monochron application for many years. However, by relocating
// the eeprom addresses using address offset EE_OFFSET for the configurable
// Monochron items we can create a new batch of 100k reset/write cycles. An
// atmega328p has 1 KB of eeprom, so we have plenty of relocation space.
// To check the integrity of the eeprom we look for a specific initialization
// value EE_INITIALIZED at address EE_INIT. When not found, Monochron will
// reset the eeprom with default values from eepInit[] [monomain.c].
// Warning: Do not set EE_INITIALIZED to 0xff/255, as that is the state the
//          eeprom will be in when totally erased.
#define EE_SIZE		1024
#define EE_OFFSET	0
#define EE_INITIALIZED	0x5a

// The configuration items below are stored in eeprom. An atmega328p has 1 KB
// of eeprom available.
// Instructions for adding a new entry/entries:
// 1 - Add new define(s) at bottom of the list.
// 2 - Add default value(s) in eepDefault[] in monomain.c [firmware].
// 3 - Add new define(s) in eepDict[] in mchronutil.c [firmware/emulator].
// Warning: Keep defines EE_ALARM_HOUR1..EE_ALARM_MIN4 together in a single
//          range block and in sequential order.
#define EE_INIT		(EE_OFFSET + 0)
#define EE_BRIGHT	(EE_OFFSET + 1)
#define EE_VOLUME	(EE_OFFSET + 2)
#define EE_REGION	(EE_OFFSET + 3)
#define EE_TIME_FORMAT	(EE_OFFSET + 4)
#define EE_SNOOZE	(EE_OFFSET + 5)
#define EE_BGCOLOR	(EE_OFFSET + 6)
#define EE_ALARM_SELECT (EE_OFFSET + 7)
#define EE_ALARM_HOUR1	(EE_OFFSET + 8)
#define EE_ALARM_MIN1	(EE_OFFSET + 9)
#define EE_ALARM_HOUR2	(EE_OFFSET + 10)
#define EE_ALARM_MIN2	(EE_OFFSET + 11)
#define EE_ALARM_HOUR3	(EE_OFFSET + 12)
#define EE_ALARM_MIN3	(EE_OFFSET + 13)
#define EE_ALARM_HOUR4	(EE_OFFSET + 14)
#define EE_ALARM_MIN4	(EE_OFFSET + 15)

// Structure that defines the date and time
typedef struct _rtcDateTime_t
{
  uint8_t timeSec;
  uint8_t timeMin;
  uint8_t timeHour;
  uint8_t dateDay;
  uint8_t dateMon;
  uint8_t dateYear;
} rtcDateTime_t;

// Function prototypes for managing alarms
void almStateSet(void);
void almTimeGet(uint8_t alarmId, volatile uint8_t *hour,
  volatile uint8_t *min);
void almTimeSet(uint8_t alarmId, volatile uint8_t hour, volatile uint8_t min);

// Function prototypes for the real time clock
void rtcMchronTimeInit(void);
void rtcTimeInit(void);
uint8_t rtcTimeRead(void);
void rtcTimeWrite(void);

#ifdef EMULIN
// Function prototypes for Emuchron renamed functions and interrupt handlers
int monoMain(void);
void monoTimer(void);
#endif

// Miscellaneous
void eepInit(void);
void beep(uint16_t freq, uint8_t duration);
uint8_t bcdDecode(uint8_t x, uint8_t nibbleMask);
uint8_t bcdEncode(uint8_t x);
#endif
