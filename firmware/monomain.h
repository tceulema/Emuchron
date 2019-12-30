//*****************************************************************************
// Filename : 'monomain.h'
// Title    : Defines for the main clock engine for MONOCHRON
//*****************************************************************************

#ifndef MONOMAIN_H
#define MONOMAIN_H

#include "global.h"

// Version of our emuchron code base
#define EMUCHRON_VERSION	"v5.1"

// Debugging macros.
// Note that DEBUGGING is the master switch for generating debug output.
// 0 = Off, 1 = On
#define DEBUGGING	0
#define DEBUG(x)	if (DEBUGGING) { x; }
#define DEBUGP(x)	DEBUG(putstring_nl(x))
// Select to report debug entries for time change events
#define DEBUGTIME	1
#define DEBUGT(x)	if (DEBUGGING && DEBUGTIME) { x; }
#define DEBUGTP(x)	DEBUGT(putstring_nl(x))

// Software options. Uncomment to enable.
// BACKLIGHT_ADJUST - Allows software control of backlight, assuming you
// mounted your 100ohm resistor in R2.
#define BACKLIGHT_ADJUST 1

// Define application clock cycle msec timer value for animation and keypress
// handling. Note that redrawing takes some time too so you don't want this
// too small or your clock will 'hiccup' and appear jittery.
#define ANIM_TICK_CYCLE_MS	75

// Lcd backlight brightness related constants
#define OCR2B_BITSHIFT	0
#define OCR2B_PLUS	1
#define OCR2A_VALUE	16

// Pin definitions for alarm switch and piezo speaker
// Note: there's more in ks0108.h [firmware] for the display
#define ALARM_PORT	PORTB
#define ALARM_PIN	PINB
#define ALARM_DDR	DDRB
#define ALARM		6

#define PIEZO_PORT	PORTC
#define PIEZO_PIN	PINC
#define PIEZO_DDR	DDRC
#define PIEZO		3

// Constants for how to display time & date.
// Those commented out are no longer supported.
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
uint8_t rtcDotw(uint8_t mon, uint8_t day, uint8_t year);
uint8_t rtcLeapYear(uint16_t year);
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
uint8_t bcdEncode(uint8_t x);
#endif
