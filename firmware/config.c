//*****************************************************************************
// Filename : 'config.c'
// Title    : Configuration menu handling
//*****************************************************************************

#ifndef EMULIN
#include <util/delay.h>
#include <avr/eeprom.h>
#endif
#include "global.h"
#include "glcd.h"
#include "anim.h"
#include "ks0108.h"
#include "monomain.h"
#include "buttons.h"
#include "config.h"

// Config display modes to navigate the menu and modify values
#define SET_NONE	255
#define SET_ALARM	0
#define SET_TIME	1
#define SET_DATE	2
#define SET_DISPLAY	3
#define SET_BRIGHTNESS	4
#define EDIT_ALARM_ID	51
#define EDIT_HOUR	52
#define EDIT_MIN	53
#define EDIT_SEC	54
#define EDIT_MONTH	55
#define EDIT_DAY	56
#define EDIT_YEAR	57
#define EDIT_DISPLAY	58
#define EDIT_BRIGHTNESS	59

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
  if (stubEventGet(MC_FALSE) == 'q')
    cfgTickerActivity = 0;
#endif
  // Copy current button and clear for next background button press
  cfgButtonPressed = btnPressed;
  btnPressed = BTN_NONE;

  if (cfgButtonPressed & BTN_MENU)
  {
    // Menu button: move to next menu item
    return MC_TRUE;
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
    return MC_TRUE;
  }

  // Signal if a new time is present
  if (cfgTimeSec != rtcDateTime.timeSec)
  {
    cfgTimeUpdate = MC_TRUE;
    cfgTimeSec = rtcDateTime.timeSec;
  }
  // Update the time in the menu when allowed and needed
  if (cfgScreenLock == MC_FALSE && cfgTimeUpdate == MC_TRUE)
  {
    cfgTimeUpdate = MC_FALSE;
    cfgMenuTimeShow();
  }

  return MC_FALSE;
}

//
// Function: cfgMenuAlarm
//
// This is the menu driver for the alarm configuration page
//
static void cfgMenuAlarm(void)
{
  uint8_t line = MAX_UINT8_T;

  // Set parameters for alarm time/selector.
  // Only when all menu items are passed or when a no-press timeout occurs
  // return to caller.
  cfgScreenLock = MC_TRUE;
  do
  {
    cfgTickerActivity = CFG_TICK_ACTIVITY_SEC;
    cfgCounterHold = 0;
    switch (line)
    {
    case MAX_UINT8_T:
      // Init -> Set Alarm 1
      DEBUGP("Alarm line 0");
      glcdClearScreen();
      line = 0;
      break;
    case 4:
      // Switch back to main menu
      DEBUGP("Return to config menu");
      cfgScreenLock = MC_FALSE;
      return;
    default:
      // Set Alarm 2..4 and Alarm Id
      line++;
      DEBUG(putstring("Alarm line "); uart_put_dec(line); putstring_nl(""));
      break;
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
  glcdColorSetFg();
  glcdPutStr("Alarm Setup Menu");

  // Print the four alarm times
  for (i = 1; i < 5; i++)
  {
    glcdSetAddress(CFG_MENU_INDENT, i);
    glcdPutStr("Alarm ");
    glcdPrintNumber(i);
    glcdPutStr(":      ");
    almTimeGet(i - 1, &alarmH, &alarmM);
    glcdPrintNumber(alarmH);
    glcdWriteChar(':');
    glcdPrintNumber(alarmM);
  }

  // Print the selected alarm
  glcdSetAddress(CFG_MENU_INDENT, 5);
  glcdPutStr("Select Alarm:     ");
  glcdPrintNumber(almAlarmSelect + 1);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, CFG_MENU_INDENT - 1, 40, ALIGN_AUTO, FILL_BLANK);
}

//
// Function: cfgMenuMain
//
// This is the menu driver for the main configuration page
//
void cfgMenuMain(void)
{
  uint8_t mode = SET_NONE;

  glcdClearScreen();
  cfgTimeSec = rtcDateTime.timeSec;
  cfgTimeUpdate = MC_FALSE;
  cfgScreenLock = MC_FALSE;

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
    case SET_NONE:
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
  glcdColorSetFg();
  glcdPutStr("Configuration Menu   ");
  glcdFillRectangle2(126, 0, 2, 8, ALIGN_AUTO, FILL_BLANK);
  glcdSetAddress(CFG_MENU_INDENT, 1);
  glcdPutStr("Alarm:         Setup");
  glcdSetAddress(CFG_MENU_INDENT, 2);
  glcdPutStr("Time:       ");
  cfgMenuTimeShow();
  cfgPrintDate(rtcDateTime.dateYear, rtcDateTime.dateMon, rtcDateTime.dateDay,
    SET_DATE);
  cfgPrintDisplay(mcFgColor);
#ifdef BACKLIGHT_ADJUST
  glcdSetAddress(CFG_MENU_INDENT, 5);
  glcdPutStr("Backlight:        ");
  glcdPrintNumber(OCR2B >> OCR2B_BITSHIFT);
#endif
  cfgPrintInstruct1(line1, line2);
  glcdFillRectangle2(126, 48, 2, 16, ALIGN_AUTO, FILL_BLANK);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, CFG_MENU_INDENT, 40, ALIGN_AUTO, FILL_BLANK);
}

//
// Function: cfgMenuTimeShow
//
// Print the updated time on the config main menu page
//
void cfgMenuTimeShow(void)
{
  glcdSetAddress(CFG_MENU_INDENT + 12 * 6, 2);
  glcdPrintNumber(rtcDateTime.timeHour);
  glcdWriteChar(':');
  glcdPrintNumber(rtcDateTime.timeMin);
  glcdWriteChar(':');
  glcdPrintNumber(rtcDateTime.timeSec);
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

  if (mode == EDIT_YEAR)
  {
    // Increment year
    newYear = cfgNextNumber(newYear, 100);
    if (!calLeapYear(newYear) && newMonth == 2 && newDay > 28)
      newDay = 28;
  }
  else if (mode == EDIT_MONTH)
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
      if (!calLeapYear(newYear) && newDay > 28)
        newDay = 28;
    }
    else if (newMonth == 4 || newMonth == 6 || newMonth == 9 || newMonth == 11)
    {
      if (newDay > 30)
        newDay = 30;
    }
  }
  else if (mode == EDIT_DAY)
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
      else if (!calLeapYear(newYear) && newDay > 28)
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
  if (btnHoldRelCfm == MC_TRUE)
  {
    if (DEBUGGING && cfgCounterHold == CFG_BTN_HOLD_COUNT)
      DEBUGP("+1");
    cfgCounterHold = 0;
    btnHoldRelCfm = MC_FALSE;
  }

  if (btnHold)
  {
    // Press-hold: normal or fast increase

    // Request a confirmation on hold release
    if (DEBUGGING && btnHoldRelReq == MC_FALSE)
      DEBUGP("rlr");
    btnHoldRelReq = MC_TRUE;

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
  if (mode == EDIT_HOUR)
    glcdPrintNumberBg(hour);
  else
    glcdPrintNumber(hour);
  glcdWriteChar(':');
  if (mode == EDIT_MIN)
    glcdPrintNumberBg(min);
  else
    glcdPrintNumber(min);
}

//
// Function: cfgPrintArrow
//
// Print an arrow in front of a menu item
//
static void cfgPrintArrow(u08 y)
{
  glcdFillRectangle(0, y, CFG_MENU_INDENT - 1, 1);
  glcdRectangle(CFG_MENU_INDENT - 3, y - 1, 1, 3);
  glcdRectangle(CFG_MENU_INDENT - 4, y - 2, 1, 5);
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
  glcdPutStr("Date:");
  glcdPutStr(animDays[calDotw(month, day, year)]);
  if (mode == EDIT_MONTH)
    glcdColorSetBg();
  glcdPutStr(animMonths[month - 1]);
  glcdColorSetFg();
  glcdWriteChar(' ');
  if (mode == EDIT_DAY)
    glcdPrintNumberBg(day);
  else
    glcdPrintNumber(day);
  glcdWriteChar(',');
  if (mode == EDIT_YEAR)
    glcdColorSetBg();
  glcdPrintNumber(20);
  glcdPrintNumber(year);
  glcdColorSetFg();
}

//
// Function: cfgPrintDisplay
//
// Print the display setting
//
static void cfgPrintDisplay(uint8_t color)
{
  glcdSetAddress(CFG_MENU_INDENT, 4);
  glcdPutStr("Display:     ");
  if (mcBgColor == GLCD_OFF)
  {
    glcdPutStr(" ");
    glcdColorSet(color);
    glcdPutStr("Normal");
  }
  else
  {
    glcdColorSet(color);
    glcdPutStr("Inverse");
  }
  glcdColorSetFg();
}

//
// Function: cfgPrintInstruct1
//
// Print full instructions at bottom of screen
//
static void cfgPrintInstruct1(char *line1, char *line2)
{
  glcdSetAddress(0, 6);
  glcdPutStr(line1);
  if (line2 != 0)
  {
    glcdSetAddress(0, 7);
    glcdPutStr(line2);
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
  glcdPutStr(CFG_INSTR_PREFIX_CHANGE);
  glcdPutStr(line1b);
  glcdSetAddress(0, 7);
  glcdPutStr(CFG_INSTR_PREFIX_SET);
  glcdPutStr(line2b);
  if (line2b[3] == '\0')
    glcdPutStr(" ");
}

//
// Function: cfgPrintTime
//
// Print the time (hh:mm:ss) with optional highlighted item
//
static void cfgPrintTime(uint8_t hour, uint8_t min, uint8_t sec, uint8_t mode)
{
  glcdSetAddress(CFG_MENU_INDENT + 12 * 6, 2);
  if (mode == EDIT_HOUR)
    glcdPrintNumberBg(hour);
  else
    glcdPrintNumber(hour);
  glcdWriteChar(':');
  if (mode == EDIT_MIN)
    glcdPrintNumberBg(min);
  else
    glcdPrintNumber(min);
  glcdWriteChar(':');
  if (mode == EDIT_SEC)
    glcdPrintNumberBg(sec);
  else
    glcdPrintNumber(sec);
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
    if (cfgEventPre() == MC_TRUE)
      return;

    if (cfgButtonPressed & BTN_SET)
    {
      if (mode == SET_ALARM)
      {
        if (line == 4)
        {
          // Select alarm number item
          DEBUGP("Set selected alarm");
          mode = EDIT_ALARM_ID;
          glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
          glcdPrintNumberBg(newAlarmSelect + 1);
          cfgPrintInstruct2("alm", "alm");
        }
        else
        {
          // Select hour item
          DEBUGP("Set alarm hour");
          mode = EDIT_HOUR;
          cfgPrintInstruct2("hr.", "hour");
        }
      }
      else if (mode == EDIT_HOUR)
      {
        // Select minute item
        DEBUGP("Set alarm min");
        mode = EDIT_MIN;
        cfgPrintInstruct2("min", "min");
      }
      else
      {
        // Deselect item
        if (mode == EDIT_ALARM_ID)
        {
          // Save alarm number item
          glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
          glcdPrintNumber(newAlarmSelect + 1);
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
      if (mode == EDIT_ALARM_ID)
      {
        // Increment alarm number item
        newAlarmSelect++;
        if (newAlarmSelect >= 4)
          newAlarmSelect = 0;
        glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
        glcdPrintNumberBg(newAlarmSelect + 1);
        DEBUG(putstring("New alarm Id -> "));
        DEBUG(uart_put_dec(newAlarmSelect + 1));
        DEBUG(putstring_nl(""));
      }
      else if (mode == EDIT_HOUR)
      {
        // Increment hour item
        newHour++;
        if (newHour >= 24)
          newHour = 0;
        DEBUG(putstring("New alarm hour -> "));
        DEBUG(uart_put_dec(newHour));
        DEBUG(putstring_nl(""));
      }
      else if (mode == EDIT_MIN)
      {
        // Increment minute item
        newMin = cfgNextNumber(newMin, 60);
        DEBUG(putstring("New alarm min -> "));
        DEBUG(uart_put_dec(newMin));
        DEBUG(putstring_nl(""));
      }
    }

    // Update display in case alarm time is (de-)edited
    if ((cfgButtonPressed || btnHold) && line != 4)
      cfgPrintAlarm(line, newHour, newMin, mode);

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
    if (cfgEventPre() == MC_TRUE)
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
    if (cfgEventPre() == MC_TRUE)
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
        mode = EDIT_BRIGHTNESS;
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
      if (mode == EDIT_BRIGHTNESS)
      {
        if (OCR2B >= OCR2A_VALUE)
          OCR2B = 0;
        else
          OCR2B = OCR2B + OCR2B_PLUS;
      }
    }
    if (cfgButtonPressed || btnHold)
    {
      // Update display
      glcdSetAddress(CFG_MENU_INDENT + 18 * 6, 5);
      if (mode == EDIT_BRIGHTNESS)
        glcdPrintNumberBg(OCR2B >> OCR2B_BITSHIFT);
      else
        glcdPrintNumber(OCR2B >> OCR2B_BITSHIFT);
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
    if (cfgEventPre() == MC_TRUE)
      return;

    if (cfgButtonPressed & BTN_SET)
    {
      if (mode == SET_DATE)
      {
        // Select month item
        DEBUGP("Set date month");
        mode = EDIT_MONTH;
        cfgPrintInstruct2("mon", "mon");
      }
      else if (mode == EDIT_MONTH)
      {
        // Select month day
        DEBUGP("Set date day");
        mode = EDIT_DAY;
        cfgPrintInstruct2("day", "day");
      }
      else if ((mode == EDIT_DAY))
      {
        // Select year item
        DEBUGP("Set year");
        mode = EDIT_YEAR;
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
    if (cfgEventPre() == MC_TRUE)
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
        mode = EDIT_DISPLAY;
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
      if (mode == EDIT_DISPLAY)
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
        DEBUG(uart_put_dec(mcBgColor));
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
    if (cfgEventPre() == MC_TRUE)
    {
      cfgScreenLock = MC_FALSE;
      return;
    }

    if (cfgButtonPressed & BTN_SET)
    {
      cfgScreenLock = MC_TRUE;
      if (mode == SET_TIME)
      {
        // Fixate time to work on
        newHour = rtcDateTime.timeHour;
        newMin = rtcDateTime.timeMin;
        newSec = rtcDateTime.timeSec;
        // Select hour item
        DEBUGP("Set time hour");
        mode = EDIT_HOUR;
        cfgPrintInstruct2("hr.", "hour");
      }
      else if (mode == EDIT_HOUR)
      {
        // Select minute item
        DEBUGP("Set time min");
        mode = EDIT_MIN;
        cfgPrintInstruct2("min", "min");
      }
      else if (mode == EDIT_MIN)
      {
        // Select second item
        DEBUGP("Set time sec");
        mode = EDIT_SEC;
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
        cfgScreenLock = MC_FALSE;
      }
    }
    if (cfgButtonPressed & BTN_PLUS || btnHold)
    {
      // Increment the time element currently in edit mode
      if (mode == EDIT_HOUR)
      {
        newHour++;
        if (newHour >= 24)
          newHour = 0;
      }
      else if (mode == EDIT_MIN)
      {
        newMin = cfgNextNumber(newMin, 60);
      }
      else if (mode == EDIT_SEC)
      {
        newSec = cfgNextNumber(newSec, 60);
      }
    }
    if (cfgButtonPressed || btnHold)
    {
      // Update display
      cfgPrintTime(newHour, newMin, newSec, mode);
   }

    cfgEventPost();
  }
}
