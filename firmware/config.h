//*****************************************************************************
// Filename : 'config.h'
// Title    : Definitions for MONOCHRON configuration menu
//*****************************************************************************

#ifndef CONFIG_H
#define CONFIG_H

// Config display mode.
// Note that some items are no longer supported. They are retained for backward
// compatibility.
#define SHOW_NONE	99
#define SHOW_TIME	0
#define SHOW_DATE	1
#define SHOW_ALARM	2
#define SHOW_SNOOZE	3
#define SET_TIME	4
#define SET_ALARM	5
#define SET_ALARMS	6
#define SET_DATE	7
#define SET_BRIGHTNESS	8
#define SET_VOLUME	9
#define SET_REGION	10
#define SET_SNOOZE	11
#define SET_MONTH	12
#define SET_DAY		13
#define SET_YEAR	14
#define SET_DISPLAY	15
#define SET_ALARM_1	16
#define SET_ALARM_2	17
#define SET_ALARM_3	18
#define SET_ALARM_4	19
#define SET_ALARM_ID	20
#define SET_HOUR	101
#define SET_MIN		102
#define SET_SEC		103
#define SET_REG		104
#define SET_BRT		105
#define SET_DSP		106

// How many pixels to indent the menu items
#define MENU_INDENT	8

// Button keypress delay (msec)
#define KEYPRESS_DLY_1	150

void cfgMenuMain(void);
void cfgMenuTimeShow(void);
#endif
