//*****************************************************************************
// Filename : 'ratt.h'
// Title    : Defines for the main clock engine for MONOCHRON
//*****************************************************************************

#ifndef RATT_H
#define RATT_H

#include "global.h"

#define halt(x)		while (1)

// Debugging macros.
// Note that DEBUGGING is the master switch for generating debug output.
// 0 = OFF, 1 = ON
#define DEBUGGING	0
#define DEBUG(x)	if (DEBUGGING) { x; }
#define DEBUGP(x)	DEBUG(putstring_nl(x))

// Software options. Uncomment to enable.
// BACKLIGHT_ADJUST - Allows software control of backlight, assuming
// you mounted your 100ohm resistor in R2.
#define BACKLIGHT_ADJUST 1

// Define loop timer for animation and keypress handling. Note that
// redrawing takes some time too so you don't want this too small or
// your clock will 'hiccup' and appear jittery.
#define ANIMTICK_MS	75

// Two-tone alarm beep
#define ALARM_FREQ_1	4000
#define ALARM_FREQ_2	3750
#define ALARMTICK_MS	325

// Set timeouts for snooze and alarm (in seconds)
#ifndef EMULIN
#define MAXSNOOZE	600
#define MAXALARM	1800
#else
// In our emulator we don't want to wait that long
#define MAXSNOOZE	25
#define MAXALARM	65
#endif

// The Monochron buttons
#define BTTN_MENU	0x01
#define BTTN_SET	0x02
#define BTTN_PLUS	0x04

// Pin definitions
// Note: there's more in KS0108.h for the display

#define ALARM_DDR	DDRB
#define ALARM_PIN	PINB
#define ALARM_PORT	PORTB
#define ALARM		6

#define PIEZO_PORT	PORTC
#define PIEZO_PIN	PINC
#define PIEZO_DDR	DDRC
#define PIEZO		3

// Enums

// Constants for how to display time & date
// Those commented out are no longer supported
//#define REGION_US	0
//#define REGION_EU	1
//#define DOW_REGION_US	2
//#define DOW_REGION_EU	3
//#define DATELONG	4
#define DATELONG_DOW	5
//#define TIME_12H	0
#define TIME_24H	1

// Constants for calculating the Timer2 interrupt return rate.
// Make the i2ctime readout at a certain number of times a 
// second and a few other values about once a second.
// The default readout rate was ~5.7Hz that has been increased
// to ~8.5Hz. This was done to detect changes in seconds faster,
// leading to a more smooth 'seconds tick' animation in clocks.
#define OCR2B_BITSHIFT	0
#define OCR2B_PLUS	1
#define OCR2A_VALUE	16
// Uncomment to implement i2ctime readout @ ~5.7Hz
//#define TIMER2_RETURN_1	80
//#define TIMER2_RETURN_2	6
// Uncomment to implement i2ctime readout @ ~8.5Hz
#define TIMER2_RETURN_1	53
#define TIMER2_RETURN_2	9

// DO NOT set EE_INITIALIZED to 0xFF / 255, as that is
// the state the eeprom will be in when totally erased.
#define EE_INITIALIZED	0xC3
#define EE_INIT		0
#define EE_ALARM_HOUR	1
#define EE_ALARM_MIN	2
#define EE_BRIGHT	3
#define EE_VOLUME	4
#define EE_REGION	5
#define EE_TIME_FORMAT	6
#define EE_SNOOZE	7
#define EE_BGCOLOR	8
#define EE_ALARM_HOUR2	9
#define EE_ALARM_MIN2	10
#define EE_ALARM_HOUR3	11
#define EE_ALARM_MIN3	12
#define EE_ALARM_HOUR4	13
#define EE_ALARM_MIN4	14
#define EE_ALARM_SELECT 15
// Set EE_MAX to the highest value in use above
#define EE_MAX		15

// Function prototypes
void alarmStateSet(void);
void alarmTimeGet(uint8_t alarmId, volatile uint8_t *hour, volatile uint8_t *min);
void alarmTimeSet(uint8_t alarmId, volatile uint8_t hour, volatile uint8_t min);
void beep(uint16_t freq, uint8_t duration);
uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr);
uint8_t i2bcd(uint8_t x);
void init_eeprom(void);
uint8_t leapyear(uint16_t y);
void mchronTimeInit(void);
uint8_t readi2ctime(void);
void writei2ctime(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
  uint8_t date, uint8_t mon, uint8_t yr);
#endif
