//*****************************************************************************
// Filename : 'config.c'
// Title    : Configuration menu handling
//*****************************************************************************

#ifndef EMULIN
#include <util/delay.h>
#include <avr/eeprom.h>
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#endif
#include "monomain.h"
#include "buttons.h"
#include "ks0108.h"
#include "glcd.h"
#include "config.h"

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

// How many hold increases to pass prior to increasing increase value
#define CFG_BTN_HOLD_COUNT	10

// How many pixels to indent the menu items
#define CFG_MENU_INDENT		8

// Several fixed substring instructions
#define CFG_INSTR_PREFIX_CHANGE	"Press + to change "
#define CFG_INSTR_PREFIX_SET	"Press SET to set "

// Several fixed complete instructions
#define CFG_INSTR_ADVANCE	"Press MENU to advance"
#define CFG_INSTR_EXIT		"Press MENU to exit   "
#define CFG_INSTR_CHANGE	"Press + to change    "
#define CFG_INSTR_SET		"Press SET to set     "
#define CFG_INSTR_SAVE		"Press SET to save    "

// External data
extern volatile uint8_t almAlarmSelect;
extern volatile uint8_t btnPressed, btnHold;
extern volatile uint8_t btnHoldRelReq, btnHoldRelCfm;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile rtcDateTime_t rtcDateTime;
extern char *animMonths[12];
extern char *animDays[7];

// Variables that control updating the time in the main config menu
static uint8_t cfgScreenLock;
static uint8_t cfgTimeSec;
static uint8_t cfgTimeUpdate;

// This variable keeps track of whether we did not press any buttons in x
// seconds, signaling to exit the config menu
volatile uint8_t cfgTickerActivity = 0;

// This variable administers the consecutive button hold events allowing to
// increase the button hold increments
static uint8_t cfgCounterHold = 0;

// A copy of the buttom just pressed
static uint8_t cfgButtonPressed;

// Local function prototypes

// Main driver for the alarm config page
static void cfgMenuAlarm(void);

// Show a config menu page
static void cfgMenuAlarmShow(void);
static void cfgMenuMainShow(char *line1, char *line2);

// Generic pre- and postevent handling for buttons in a menu item
static void cfgEventPost(void);
static uint8_t cfgEventPre(void);
static void cfgMenuTimeShow(void);

// Generate next item value based on current one
static void cfgNextDate(uint8_t *year, uint8_t *month, uint8_t *day,
  uint8_t mode);
static uint8_t cfgNextNumber(uint8_t value, uint8_t maxVal);

// Print menu item value on a prefixed location
static void cfgPrintAlarm(uint8_t line, uint8_t hour, uint8_t min,
  uint8_t mode);
static void cfgPrintArrow(u08 y);
static void cfgPrintDate(uint8_t year, uint8_t month, uint8_t day,
  uint8_t mode);
static void cfgPrintDisplay(uint8_t color);
static void cfgPrintInstruct1(char *line1, char *line2);
static void cfgPrintInstruct2(char *line1b, char *line2b);
static void cfgPrintTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t mode);

// The event handlers for a menu item
static void cfgSetAlarm(uint8_t line);
static void cfgSetAlarmMenu(void);
static void cfgSetBacklight(void);
static void cfgSetDate(void);
static void cfgSetDisplay(void);
static void cfgSetTime(void);

//
// Function: cfgEventPost
//
// Generic button and event postprocessing in a menu item
//
static void cfgEventPost(void)
{
  if (btnHold)
    _delay_ms(KEYPRESS_DLY_1);
}

//
// Function: cfgEventPre
//
// Generic button and event preprocessing in a menu item
//
static uint8_t cfgEventPre(void)
{
#ifdef EMULIN
  if (stubEventGet() == 'q')
    cfgTickerActivity = 0;
#endif
  // Copy current button and clear for next background button press
  cfgButtonPressed = btnPressed;
  btnPressed = BTN_NONE;

  if (cfgButtonPressed & BTN_MENU)
  {
    // Menu button: move to next menu item
    return GLCD_TRUE;
  }
  else if (cfgButtonPressed & BTN_SET)
  {
    // Button is not '+': clear '+' button hold counter
    cfgCounterHold = 0;
  }

  if (cfgButtonPressed || btnHold)
  {
    // Button pressed or hold: reset inactivity timout
    cfgTickerActivity = CFG_TICK_ACTIVITY_SEC;
  }
  else if (cfgTickerActivity == 0)
  {
    // Timed out in menu item: exit config module
    return GLCD_TRUE;
  }

  // Signal if a new time is present
  if (cfgTimeSec != rtcDateTime.timeSec)
  {
    cfgTimeUpdate = GLCD_TRUE;
    cfgTimeSec = rtcDateTime.timeSec;
  }
  // Update the time in the menu when allowed and needed
  if (cfgScreenLock == GLCD_FALSE && cfgTimeUpdate == GLCD_TRUE)
  {
    cfgTimeUpdate = GLCD_FALSE;
    cfgMenuTimeShow();
  }

  return GLCD_FALSE;
}

//
// Function: cfgMenuAlarm
//
// This is the state-event machine for the alarm configuration page
//
static void cfgMenuAlarm(void)
{
  uint8_t mode = SET_ALARM;
  uint8_t line;

  // Set parameters for alarm time/selector.
  // Only when all menu items are passed or when a no-press timeout occurs
  // return to caller.
  cfgScreenLock = GLCD_TRUE;
  do
  {
    cfgTickerActivity = CFG_TICK_ACTIVITY_SEC;
    cfgCounterHold = 0;

    switch (mode)
    {
    case SET_ALARM:
      // Alarm -> Set Alarm 1
      DEBUGP("Set alarm 1");
      mode = SET_ALARM_1;
      glcdClearScreen(mcBgColor);
      line = 0;
      break;
    case SET_ALARM_1:
      // Alarm 1 -> Set Alarm 2
      DEBUGP("Set alarm 2");
      mode = SET_ALARM_2;
      line = 1;
      break;
    case SET_ALARM_2:
      // Alarm 2 -> Set Alarm 3
      DEBUGP("Set alarm 3");
      mode = SET_ALARM_3;
      line = 2;
      break;
    case SET_ALARM_3:
      // Alarm 3 -> Set Alarm 4
      DEBUGP("Set alarm 4");
      mode = SET_ALARM_4;
      line = 3;
      break;
    case SET_ALARM_4:
      // Alarm 4 -> Set Alarm Id
      DEBUGP("Set alarm Id");
      mode = SET_ALARM_ID;
      line = 4;
      break;
    default:
      // Switch back to main menu
      DEBUGP("Return to config menu");
      cfgScreenLock = GLCD_FALSE;
      return;
    }

    // Set the requested alarm/selector
    cfgSetAlarm(line);
  } while (cfgTickerActivity != 0);

  // Switch back to clock due to timeout
}

//
// Function: cfgMenuAlarmShow
//
// Display the alarm menu page
//
static void cfgMenuAlarmShow(void)
{
  volatile u08 alarmH, alarmM;
  uint8_t i;

  DEBUGP("Display alarm menu");
  glcdSetAddress(0, 0);
  glcdPutStrFg("Alarm Setup Menu");

  // Print the four alarm times
  for (i = 1; i < 5; i++)
  {
    glcdSetAddress(CFG_MENU_INDENT, i);
    glcdPutStrFg("Alarm ");
    glcdPrintNumberFg(i);
    glcdPutStrFg(":      ");
    almTimeGet(i - 1, &alarmH, &alarmM);
    glcdPrintNumberFg(alarmH);
    glcdWriteCharFg(':');
    glcdPrintNumberFg(alarmM);
  }

  // Print the selected alarm
  glcdSetAddress(CFG_MENU_INDENT, 5);
  glcdPutStrFg("Select Alarm:     ");
  glcdPrintNumberFg(almAlarmSelect + 1);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, CFG_MENU_INDENT - 1, 40, ALIGN_AUTO, FILL_FULL,
    mcBgColor);
}

//
// Function: cfgMenuMain
//
// This is the state-event machine for the main configuration page
//
void cfgMenuMain(void)
{
  uint8_t mode = SHOW_TIME;

  glcdClearScreen(mcBgColor);
  cfgTimeSec = rtcDateTime.timeSec;
  cfgTimeUpdate = GLCD_FALSE;
  cfgScreenLock = GLCD_FALSE;

  // Only when all menu items are passed or when a no-press timeout occurs
  // return to caller
  do
  {
    // (Re)paint main menu except when we're exiting the menu
    if (mode != SET_BRIGHTNESS)
      cfgMenuMainShow(CFG_INSTR_ADVANCE, CFG_INSTR_SET);

    cfgTickerActivity = CFG_TICK_ACTIVITY_SEC;
    btnPressed = BTN_NONE;
    cfgCounterHold = 0;

    switch (mode)
    {
    case SHOW_TIME:
      // Clock -> Set Alarm
      DEBUGP("Set alarm");
      mode = SET_ALARM;
      cfgSetAlarmMenu();
      break;
    case SET_ALARM:
      // Set Alarm -> Set Time
      DEBUGP("Set time");
      mode = SET_TIME;
      cfgSetTime();
      break;
    case SET_TIME:
      // Set Time -> Set Date
      DEBUGP("Set date");
      mode = SET_DATE;
      cfgSetDate();
      break;
    case SET_DATE:
      // Set Date -> Set Display
      DEBUGP("Set display");
      mode = SET_DISPLAY;
      cfgSetDisplay();
      break;
#ifdef BACKLIGHT_ADJUST
    case SET_DISPLAY:
      // Set Display -> Set Brightness
      DEBUGP("Set brightness");
      mode = SET_BRIGHTNESS;
      cfgSetBacklight();
      break;
#endif
    default:
      // Switch back to Clock
      DEBUGP("Exit config menu");
      return;
    }
  } while (cfgTickerActivity != 0);

  // Switch back to clock due to timeout
  DEBUGP("Timeout -> resume to clock");
}

//
// Function: cfgMenuMainShow
//
// Display the main menu page
//
static void cfgMenuMainShow(char *line1, char *line2)
{
  DEBUGP("Display menu");

  glcdSetAddress(0, 0);
  glcdPutStrFg("Configuration Menu   ");
  glcdFillRectangle2(126, 0, 2, 8, ALIGN_AUTO, FILL_FULL, mcBgColor);
  glcdSetAddress(CFG_MENU_INDENT, 1);
  glcdPutStrFg("Alarm:         Setup");
  glcdSetAddress(CFG_MENU_INDENT, 2);
  glcdPutStrFg("Time:       ");
  cfgMenuTimeShow();
  cfgPrintDate(rtcDateTime.dateYear, rtcDateTime.dateMon, rtcDateTime.dateDay,
    SET_DATE);
  cfgPrintDisplay(mcFgColor);
#ifdef BACKLIGHT_ADJUST
  glcdSetAddress(CFG_MENU_INDENT, 5);
  glcdPutStrFg("Backlight:        ");
  glcdPrintNumberFg(OCR2B >> OCR2B_BITSHIFT);
#endif
  cfgPrintInstruct1(line1, line2);
  glcdFillRectangle2(126, 48, 2, 16, ALIGN_AUTO, FILL_FULL, mcBgColor);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, CFG_MENU_INDENT, 40, ALIGN_AUTO, FILL_FULL,
    mcBgColor);
}

//
// Function: cfgMenuTimeShow
//
// Print the updated time on the config main menu page
//
void cfgMenuTimeShow(void)
{
  glcdSetAddress(CFG_MENU_INDENT + 12 * 6, 2);
  glcdPrintNumberFg(rtcDateTime.timeHour);
  glcdWriteCharFg(':');
  glcdPrintNumberFg(rtcDateTime.timeMin);
  glcdWriteCharFg(':');
  glcdPrintNumberFg(rtcDateTime.timeSec);
}

//
// Function: cfgNextDate
//
// Returns the next date based on the one provided. Increment by either day,
// month or year.
//
static void cfgNextDate(uint8_t *year, uint8_t *month, uint8_t *day,
  uint8_t mode)
{
  uint8_t newYear = *year;
  uint8_t newMonth = *month;
  uint8_t newDay = *day;

  if (mode == SET_YEAR)
  {
    // Increment year
    newYear = cfgNextNumber(newYear, 100);
    if (!rtcLeapYear(newYear) && newMonth == 2 && newDay > 28)
      newDay = 28;
  }
  else if (mode == SET_MONTH)
  {
    // Increment month
    newMonth++;
    if (newMonth >= 13)
    {
      newMonth = 1;
    }
    else if (newMonth == 2)
    {
      if (newDay > 29)
        newDay = 29;
      if (!rtcLeapYear(newYear) && newDay > 28)
        newDay = 28;
    }
    else if (newMonth == 4 || newMonth == 6 || newMonth == 9 || newMonth == 11)
    {
      if (newDay > 30)
        newDay = 30;
    }
  }
  else if (mode == SET_DAY)
  {
    // Increment day
    newDay++;
    if (newDay > 31)
    {
      newDay = 1;
    }
    else if (newMonth == 2)
    {
      if (newDay > 29)
        newDay = 1;
      else if (!rtcLeapYear(newYear) && newDay > 28)
        newDay = 1;
    }
    else if (newMonth == 4 || newMonth == 6 || newMonth == 9 || newMonth == 11)
    {
      if (newDay > 30)
        newDay = 1;
    }
  }

  // Return new date
  *year = newYear;
  *month = newMonth;
  *day = newDay;
}

//
// Function: cfgNextNumber
//
// Returns the next value for an item based on single keypress, initial
// press-hold and long duration press-hold and the upper limit value
//
static uint8_t cfgNextNumber(uint8_t value, uint8_t maxVal)
{
  // Reset fast increase upon hold release confirmation
  if (btnHoldRelCfm == GLCD_TRUE)
  {
    if (DEBUGGING && cfgCounterHold == CFG_BTN_HOLD_COUNT)
      DEBUGP("+1");
    cfgCounterHold = 0;
    btnHoldRelCfm = GLCD_FALSE;
  }

  if (btnHold)
  {
    // Press-hold: normal or fast increase

    // Request a confirmation on hold release
    if (DEBUGGING && btnHoldRelReq == GLCD_FALSE)
      DEBUGP("rlr");
    btnHoldRelReq = GLCD_TRUE;

    if (cfgCounterHold < CFG_BTN_HOLD_COUNT)
    {
      // Not too long press-hold: single increase
      cfgCounterHold++;
      value++;
      if (DEBUGGING && cfgCounterHold == CFG_BTN_HOLD_COUNT)
        DEBUGP("+2");
    }
    else
    {
      // Long press-hold; double increase
      value = value + 2;
    }
  }
  else
  {
    // Single press: single increase
    cfgCounterHold = 0;
    value++;
  }

  // Beware of overflow
  value = value % maxVal;

  return value;
}

//
// Function: cfgPrintAlarm
//
// Print the alarm (hh:mm) with optional highlighted item
//
static void cfgPrintAlarm(uint8_t line, uint8_t hour, uint8_t min,
  uint8_t mode)
{
  glcdSetAddress(CFG_MENU_INDENT + 15 * 6, 1 + line);
  if (mode == SET_HOUR)
    glcdPrintNumberBg(hour);
  else
    glcdPrintNumberFg(hour);
  glcdWriteCharFg(':');
  if (mode == SET_MIN)
    glcdPrintNumberBg(min);
  else
    glcdPrintNumberFg(min);
}

//
// Function: cfgPrintArrow
//
// Print an arrow in front of a menu item
//
static void cfgPrintArrow(u08 y)
{
  glcdFillRectangle(0, y, CFG_MENU_INDENT - 1, 1, mcFgColor);
  glcdRectangle(CFG_MENU_INDENT - 3, y - 1, 1, 3, mcFgColor);
  glcdRectangle(CFG_MENU_INDENT - 4, y - 2, 1, 5, mcFgColor);
}

//
// Function: cfgPrintDate
//
// Print the date (dow+mon+day+year) with optional highlighted item
//
static void cfgPrintDate(uint8_t year, uint8_t month, uint8_t day,
  uint8_t mode)
{
  glcdSetAddress(CFG_MENU_INDENT, 3);
  glcdPutStrFg("Date:");
  glcdPutStrFg(animDays[rtcDotw(month, day, year)]);
  glcdPutStr(animMonths[month - 1],
    (mode == SET_MONTH) ? mcBgColor : mcFgColor);
  glcdWriteCharFg(' ');
  if (mode == SET_DAY)
    glcdPrintNumberBg(day);
  else
    glcdPrintNumberFg(day);
  glcdWriteCharFg(',');
  if (mode == SET_YEAR)
  {
    glcdPrintNumberBg(20);
    glcdPrintNumberBg(year);
  }
  else
  {
    glcdPrintNumberFg(20);
    glcdPrintNumberFg(year);
  }
}

//
// Function: cfgPrintDisplay
//
// Print the display setting
//
static void cfgPrintDisplay(uint8_t color)
{
  glcdSetAddress(CFG_MENU_INDENT, 4);
  glcdPutStrFg("Display:     ");
  if (mcBgColor == GLCD_OFF)
  {
    glcdPutStrFg(" ");
    glcdPutStr("Normal", color);
  }
  else
  {
    glcdPutStr("Inverse", color);
  }
}

//
// Function: cfgPrintInstruct1
//
// Print full instructions at bottom of screen
//
static void cfgPrintInstruct1(char *line1, char *line2)
{
  glcdSetAddress(0, 6);
  glcdPutStrFg(line1);
  if (line2 != 0)
  {
    glcdSetAddress(0, 7);
    glcdPutStrFg(line2);
  }
}

//
// Function: cfgPrintInstruct2
//
// Print detail instructions for change and set at bottom of screen
//
static void cfgPrintInstruct2(char *line1b, char *line2b)
{
  glcdSetAddress(0, 6);
  glcdPutStrFg(CFG_INSTR_PREFIX_CHANGE);
  glcdPutStrFg(line1b);
  glcdSetAddress(0, 7);
  glcdPutStrFg(CFG_INSTR_PREFIX_SET);
  glcdPutStrFg(line2b);
  if (line2b[3] == '\0')
    glcdPutStrFg(" ");
}

//
// Function: cfgPrintTime
//
// Print the time (hh:mm:ss) with optional highlighted item
//
static void cfgPrintTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t mode)
{
  glcdSetAddress(CFG_MENU_INDENT + 12 * 6, 2);
  if (mode == SET_HOUR)
    glcdPrintNumberBg(hour);
  else
    glcdPrintNumberFg(hour);
  glcdWriteCharFg(':');
  if (mode == SET_MIN)
    glcdPrintNumberBg(min);
  else
    glcdPrintNumberFg(min);
  glcdWriteCharFg(':');
  if (mode == SET_SEC)
    glcdPrintNumberBg(sec);
  else
    glcdPrintNumberFg(sec);
}

//
// Function: cfgSetAlarm
//
// Set alarm time or alarm selector by processing button presses
//
static void cfgSetAlarm(uint8_t line)
{
  uint8_t mode = SET_ALARM;
  uint8_t newAlarmSelect = almAlarmSelect;
  volatile uint8_t newHour, newMin;

  // Print alarm menu and put a small arrow next to proper line
  cfgMenuAlarmShow();
  if (line != 4)
    cfgPrintInstruct1(CFG_INSTR_ADVANCE, CFG_INSTR_SET);
  else
    cfgPrintInstruct1(CFG_INSTR_EXIT, CFG_INSTR_SET);
  cfgPrintArrow(11 + 8 * line);

  // Get current alarm
  if (line != 4)
    almTimeGet(line, &newHour, &newMin);

  while (1)
  {
    if (cfgEventPre() == GLCD_TRUE)
      return;

    if (cfgButtonPressed & BTN_SET)
    {
      if (mode == SET_ALARM)
      {
        if (line == 4)
        {
          // Select alarm number item
          DEBUGP("Set selected alarm");
          mode = SET_ALARM_ID;
          glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
          glcdPrintNumberBg(newAlarmSelect + 1);
          cfgPrintInstruct2("alm", "alm");
        }
        else
        {
          // Select hour item
          DEBUGP("Set alarm hour");
          mode = SET_HOUR;
          cfgPrintInstruct2("hr.", "hour");
        }
      }
      else if (mode == SET_HOUR)
      {
        // Select minute item
        DEBUGP("Set alarm min");
        mode = SET_MIN;
        cfgPrintInstruct2("min", "min");
      }
      else
      {
        // Deselect item
        if (mode == SET_ALARM_ID)
        {
          // Save alarm number item
          glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
          glcdPrintNumberFg(newAlarmSelect + 1);
          eeprom_write_byte((uint8_t *)EE_ALARM_SELECT, newAlarmSelect);
          almAlarmSelect = newAlarmSelect;
          cfgPrintInstruct1(CFG_INSTR_EXIT, CFG_INSTR_SET);
        }
        else
        {
          // Save alarm time item
          almTimeSet(line, newHour, newMin);
          cfgPrintInstruct1(CFG_INSTR_ADVANCE, CFG_INSTR_SET);
        }
        mode = SET_ALARM;

        // Sync new settings with Monochron alarm time
        almTimeGet(newAlarmSelect, &mcAlarmH, &mcAlarmM);
      }
    }
    if (cfgButtonPressed & BTN_PLUS || btnHold)
    {
      if (mode == SET_ALARM_ID)
      {
        // Increment alarm number item
        newAlarmSelect++;
        if (newAlarmSelect >= 4)
          newAlarmSelect = 0;
        glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
        glcdPrintNumberBg(newAlarmSelect + 1);
        DEBUG(putstring("New alarm Id -> "));
        DEBUG(uart_putw_dec(newAlarmSelect + 1));
        DEBUG(putstring_nl(""));
      }
      else if (mode == SET_HOUR)
      {
        // Increment hour item
        newHour++;
        if (newHour >= 24)
          newHour = 0;
        DEBUG(putstring("New alarm hour -> "));
        DEBUG(uart_putw_dec(newHour));
        DEBUG(putstring_nl(""));
      }
      else if (mode == SET_MIN)
      {
        // Increment minute item
        newMin = cfgNextNumber(newMin, 60);
        DEBUG(putstring("New alarm min -> "));
        DEBUG(uart_putw_dec(newMin));
        DEBUG(putstring_nl(""));
      }
    }
    if (cfgButtonPressed || btnHold)
    {
      // Update display in case alarm time was changed
      if (line != 4)
        cfgPrintAlarm(line, newHour, newMin, mode);
    }

    cfgEventPost();
  }
}

//
// Function: cfgSetAlarmMenu
//
// Enter the alarm setup configuration page
//
static void cfgSetAlarmMenu(void)
{
  // Put a small arrow next to 'Alarm'
  cfgPrintArrow(11);

  while (1)
  {
    if (cfgEventPre() == GLCD_TRUE)
      return;

    if (cfgButtonPressed & BTN_SET)
    {
      // Execute the alarm config menu
      DEBUGP("Go to alarm setup");
      cfgMenuAlarm();
      return;
    }
  }
}

#ifdef BACKLIGHT_ADJUST
//
// Function: cfgSetBacklight
//
// Set display backlight brightness by processing button presses
//
static void cfgSetBacklight(void)
{
  uint8_t mode = SET_BRIGHTNESS;

  // Print instructions and put a small arrow next to 'Backlight'
  cfgPrintInstruct1(CFG_INSTR_EXIT, 0);
  cfgPrintArrow(43);

  while (1)
  {
    if (cfgEventPre() == GLCD_TRUE)
    {
      eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2B >> OCR2B_BITSHIFT);
      return;
    }

    if (cfgButtonPressed & BTN_SET)
    {
      glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
      if (mode == SET_BRIGHTNESS)
      {
        // Select backlight item
        DEBUGP("Setting backlight");
        mode = SET_BRT;
        cfgPrintInstruct1(CFG_INSTR_CHANGE, CFG_INSTR_SAVE);
      }
      else
      {
        // Deselect backlight item
        mode = SET_BRIGHTNESS;
        cfgPrintInstruct1(CFG_INSTR_EXIT, CFG_INSTR_SET);
      }
    }
    if (cfgButtonPressed & BTN_PLUS || btnHold)
    {
      // Increment brightness
      if (mode == SET_BRT)
      {
        if (OCR2B >= OCR2A_VALUE)
          OCR2B = 0;
        else
          OCR2B += OCR2B_PLUS;
      }
    }
    if (cfgButtonPressed || btnHold)
    {
      // Update display
      glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
      if (mode == SET_BRT)
        glcdPrintNumberBg(OCR2B >> OCR2B_BITSHIFT);
      else
        glcdPrintNumberFg(OCR2B >> OCR2B_BITSHIFT);
    }

    cfgEventPost();
  }
}
#endif

//
// Function: cfgSetDate
//
// Set a date by setting all individual items of a date by processing button
// presses
//
static void cfgSetDate(void)
{
  uint8_t mode = SET_DATE;
  uint8_t newDay = rtcDateTime.dateDay;
  uint8_t newMonth = rtcDateTime.dateMon;
  uint8_t newYear = rtcDateTime.dateYear;

  // Put a small arrow next to 'Date'
  cfgPrintArrow(27);

  while (1)
  {
    if (cfgEventPre() == GLCD_TRUE)
      return;

    if (cfgButtonPressed & BTN_SET)
    {
      if (mode == SET_DATE)
      {
        // Select month item
        DEBUGP("Set date month");
        mode = SET_MONTH;
        cfgPrintInstruct2("mon", "mon");
      }
      else if (mode == SET_MONTH)
      {
        // Select month day
        DEBUGP("Set date day");
        mode = SET_DAY;
        cfgPrintInstruct2("day", "day");
      }
      else if ((mode == SET_DAY))
      {
        // Select year item
        DEBUGP("Set year");
        mode = SET_YEAR;
        cfgPrintInstruct2("yr.", "year");
      }
      else
      {
        // Deselect
        DEBUGP("Done setting date");
        mode = SET_DATE;
        cfgPrintInstruct1(CFG_INSTR_ADVANCE, CFG_INSTR_SET);
        rtcDateTime.dateYear = newYear;
        rtcDateTime.dateMon = newMonth;
        rtcDateTime.dateDay = newDay;
        rtcTimeWrite();
      }
      cfgPrintDate(newYear, newMonth, newDay, mode);
    }
    if (cfgButtonPressed & BTN_PLUS || btnHold)
    {
      // Increment the date element currently in edit mode
      cfgNextDate(&newYear, &newMonth, &newDay, mode);
      cfgPrintDate(newYear, newMonth, newDay, mode);
    }

    cfgEventPost();
  }
}

//
// Function: cfgSetDisplay
//
// Set the display type by processing button presses
//
static void cfgSetDisplay(void)
{
  uint8_t mode = SET_DISPLAY;

  // Print instructions and put a small arrow next to 'Display'
#ifndef BACKLIGHT_ADJUST
  cfgPrintInstruct1(CFG_INSTR_EXIT, 0);
#endif
  cfgPrintArrow(35);

  while (1)
  {
    if (cfgEventPre() == GLCD_TRUE)
    {
      eeprom_write_byte((uint8_t *)EE_BGCOLOR, mcBgColor);
      return;
    }

    if (cfgButtonPressed & BTN_SET)
    {
      if (mode == SET_DISPLAY)
      {
        // Select display item
        DEBUGP("Setting display");
        mode = SET_DSP;
        cfgPrintDisplay(mcBgColor);
        cfgPrintInstruct1(CFG_INSTR_CHANGE, CFG_INSTR_SAVE);
      }
      else
      {
        // Deselect display item
        mode = SET_DISPLAY;
        cfgPrintDisplay(mcFgColor);
#ifdef BACKLIGHT_ADJUST
        cfgPrintInstruct1(CFG_INSTR_ADVANCE, CFG_INSTR_SET);
#else
        cfgPrintInstruct1(CFG_INSTR_EXIT, CFG_INSTR_SET);
#endif
      }
    }
    if (cfgButtonPressed & BTN_PLUS || btnHold)
    {
      if (mode == SET_DSP)
      {
        // Toggle display mode
        uint8_t temp = mcBgColor;
        mcBgColor = mcFgColor;
        mcFgColor = temp;

        // Inverse and rebuild display
        cfgMenuMainShow(CFG_INSTR_CHANGE, CFG_INSTR_SAVE);
        cfgPrintArrow(35);
        cfgPrintDisplay(mcBgColor);
        DEBUG(putstring("New display type -> "));
        DEBUG(uart_putw_dec(mcBgColor));
        DEBUG(putstring_nl(""));
      }
    }

    cfgEventPost();
  }
}

//
// Function: cfgSetTime
//
// Set the system time by processing button presses
//
static void cfgSetTime(void)
{
  uint8_t mode = SET_TIME;
  uint8_t newHour = 0;
  uint8_t newMin = 0;
  uint8_t newSec = 0;

  // Put a small arrow next to 'Time'
  cfgPrintArrow(19);

  while (1)
  {
    if (cfgEventPre() == GLCD_TRUE)
    {
      cfgScreenLock = GLCD_FALSE;
      return;
    }

    if (cfgButtonPressed & BTN_SET)
    {
      cfgScreenLock = GLCD_TRUE;
      if (mode == SET_TIME)
      {
        // Fixate time to work on
        newHour = rtcDateTime.timeHour;
        newMin = rtcDateTime.timeMin;
        newSec = rtcDateTime.timeSec;
        // Select hour item
        DEBUGP("Set time hour");
        mode = SET_HOUR;
        cfgPrintInstruct2("hr.", "hour");
      }
      else if (mode == SET_HOUR)
      {
        // Select minute item
        DEBUGP("Set time min");
        mode = SET_MIN;
        cfgPrintInstruct2("min", "min");
      }
      else if (mode == SET_MIN)
      {
        // Select second item
        DEBUGP("Set time sec");
        mode = SET_SEC;
        cfgPrintInstruct2("sec", "sec");
      }
      else
      {
        // Deselect, save time and resume updating time
        DEBUGP("Done setting time");
        mode = SET_TIME;
        cfgPrintInstruct1(CFG_INSTR_ADVANCE, CFG_INSTR_SET);
        rtcDateTime.timeHour = newHour;
        rtcDateTime.timeMin = newMin;
        rtcDateTime.timeSec = newSec;
        rtcTimeWrite();
        cfgScreenLock = GLCD_FALSE;
      }
    }
    if (cfgButtonPressed & BTN_PLUS || btnHold)
    {
      // Increment the time element currently in edit mode
      if (mode == SET_HOUR)
      {
        newHour++;
        if (newHour >= 24)
          newHour = 0;
      }
      else if (mode == SET_MIN)
      {
        newMin = cfgNextNumber(newMin, 60);
      }
      else if (mode == SET_SEC)
      {
        newSec = cfgNextNumber(newSec, 60);
      }
    }
    if (cfgButtonPressed || btnHold)
    {
      // Update display
      cfgPrintTime(newHour, newMin, newSec, mode);

      // When Menu is pressed resume updating time as we'll leave this menu
      if (cfgButtonPressed & BTN_MENU)
        cfgScreenLock = GLCD_FALSE;
   }

    cfgEventPost();
  }
}
