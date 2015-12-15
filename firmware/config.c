//*****************************************************************************
// Filename : 'config.c'
// Title    : Configuration menu handling
//*****************************************************************************

#ifndef EMULIN
#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#endif
#include "monomain.h"
#include "ks0108.h"
#include "glcd.h"
#include "config.h"

// How many seconds to wait before turning off menus
#define INACTIVITYTIMEOUT	10

// How many hold increases to pass prior to increasing increase value
#define HOLD_SPEED_INCREASE	10

// Instructions
#define ACTION_ADVANCE		0
#define ACTION_EXIT		1
#define ACTION_CHANGE		2
#define ACTION_SET		3
#define ACTION_SAVE		4
#define ACTION_NONE		5

// Several fixed substring instructions
#define INSTR_PREFIX_CHANGE	"Press + to change "
#define INSTR_PREFIX_SET	"Press SET to set "

// Several fixed complete instructions
#define INSTR_ADVANCE		"Press MENU to advance"
#define INSTR_EXIT		"Press MENU to exit   "
#define INSTR_CHANGE		"Press + to change    "
#define INSTR_SET		"Press SET to set     "
#define INSTR_SAVE		"Press SET to save    "

// External data
extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t alarmSelect;
extern volatile uint8_t just_pressed, pressed;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t displaymode;
extern volatile uint8_t hold_release_req;
extern volatile uint8_t hold_release_conf;

// This variable keeps track of whether we have not pressed any
// buttons in a few seconds, and turns off the menu display
volatile uint8_t timeoutcounter = 0;

// The screenmutex attempts to prevent printing the time in the
// configuration menu when other display activities are going on.
// It does not always work due to race conditions.
volatile uint8_t screenmutex = 0;

// A shortcut to the active alarm clock
uint8_t almPageDataId;

// This variable administers the consecutive button hold events
static uint8_t holdcounter = 0;

// The months in a year
char *months[12] =
{
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// The days in a week
char *days[7] =
{
  "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat "
};

// Local function prototypes
static void menu_alarm(void);
static void display_alarm_menu(void);
static void display_main_menu(void);
static void enter_alarm_menu(void);
static uint8_t next_value(uint8_t value, uint8_t maxVal);
static void print_arrow(u08 y);
static void print_date(u08 month, u08 day, u08 year, u08 mode);
static void print_display_setting(u08 color);
static void print_instructions(char *line1, char *line2);
static void print_instructions2(char *line1b, char *line2b);
static void set_alarm(void);
static void set_backlight(void);
static void set_date(void);
static void set_display(void);
static void set_time(void);

//
// Function: menu_alarm
//
// This is the state-event machine for the alarm configuration page
//
static void menu_alarm(void)
{
  // Set parameters for alarm time/selector
  switch (displaymode)
  {
  case SET_ALARM:
    // Alarm -> Set Alarm 1
    DEBUGP("Set alarm 1");
    displaymode = SET_ALARM_1;
    almPageDataId = 0;
    break;
  case SET_ALARM_1:
    // Alarm 1 -> Set Alarm 2
    DEBUGP("Set alarm 2");
    displaymode = SET_ALARM_2;
    almPageDataId = 1;
    break;
  case SET_ALARM_2:
    // Alarm 2 -> Set Alarm 3
    DEBUGP("Set alarm 3");
    displaymode = SET_ALARM_3;
    almPageDataId = 2;
    break;
  case SET_ALARM_3:
    // Alarm 3 -> Set Alarm 4
    DEBUGP("Set alarm 4");
    displaymode = SET_ALARM_4;
    almPageDataId = 3;
    break;
  case SET_ALARM_4:
    // Alarm 4 -> Set Alarm Id
    DEBUGP("Set alarm Id");
    displaymode = SET_ALARM_ID;
    almPageDataId = 4;
    break;
  default:
    // Switch back to main menu
    DEBUGP("Return to config menu");
    displaymode = SET_ALARM;
    return;
  }

  // Set the requested alarm/selector
  set_alarm();
}

//
// Function: menu_main
//
// This is the state-event machine for the main configuration page
//
void menu_main(void)
{
  // If we enter menu mode clear the screen
  if (displaymode == SHOW_TIME)
  {
    screenmutex++;
    glcdClearScreen(mcBgColor);
    screenmutex--;
  }

  switch (displaymode)
  {
  case SHOW_TIME:
    // Clock -> Set Alarm
    DEBUGP("Set alarm");
    displaymode = SET_ALARM;
    enter_alarm_menu();
    break;
  case SET_ALARM:
    // Set Alarm -> Set Time
    DEBUGP("Set time");
    displaymode = SET_TIME;
    set_time();
    break;
  case SET_TIME:
    // Set Time -> Set Date
    DEBUGP("Set date");
    displaymode = SET_DATE;
    set_date();
    break;
  case SET_DATE:
    // Set Date -> Set Display
    DEBUGP("Set display");
    displaymode = SET_DISPLAY;
    set_display();
    break;
#ifdef BACKLIGHT_ADJUST
  case SET_DISPLAY:
    // Set Display -> Set Brightness
    DEBUGP("Set brightness");
    displaymode = SET_BRIGHTNESS;
    set_backlight();
    break;
#endif
  default:
    // Switch back to Clock
    DEBUGP("Exit config menu");
    displaymode = SHOW_TIME;
  }
}

//
// Function: display_alarm_menu
//
// Display the alarm menu page
//
static void display_alarm_menu(void)
{
  volatile u08 alarmH, alarmM;
  u08 i;

  DEBUGP("Display alarm menu");
  
  screenmutex++;
  glcdSetAddress(0, 0);
  glcdPutStrFg("Alarm Setup Menu");

  // Print the four alarm times
  for (i = 0; i < 4; i++)
  {
    glcdSetAddress(MENU_INDENT, i + 1);
    glcdPutStrFg("Alarm ");
    glcdPrintNumberFg(i + 1);
    glcdPutStrFg(":      ");
    alarmTimeGet(i, &alarmH, &alarmM);
    glcdPrintNumberFg(alarmH);
    glcdWriteCharFg(':');
    glcdPrintNumberFg(alarmM);
  }

  // Print the selected alarm
  glcdSetAddress(MENU_INDENT, 5);
  glcdPutStrFg("Select Alarm:     ");
  glcdPrintNumberFg(alarmSelect + 1);

  // Print the button functions
  if (displaymode != SET_ALARM_ID)
    print_instructions(INSTR_ADVANCE, INSTR_SET);
  else
    print_instructions(INSTR_EXIT, INSTR_SET);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, 7, 40, ALIGN_TOP, FILL_FULL, mcBgColor);
  screenmutex--;
}

//
// Function: display_main_menu
//
// Display the main menu page
//
static void display_main_menu(void)
{
  DEBUGP("Display menu");

  screenmutex++;
  glcdSetAddress(0, 0);
  glcdPutStrFg("Configuration Menu   ");
  glcdFillRectangle2(126, 0, 2, 8, ALIGN_TOP, FILL_FULL, mcBgColor);

  glcdSetAddress(MENU_INDENT, 1);
  glcdPutStrFg("Alarm:         Setup");
  
  glcdSetAddress(MENU_INDENT, 2);
  glcdPutStrFg("Time:       ");
  glcdPrintNumberFg(time_h);
  glcdWriteCharFg(':');
  glcdPrintNumberFg(time_m);
  glcdWriteCharFg(':');
  glcdPrintNumberFg(time_s);
  
  print_date(date_m, date_d, date_y, SET_DATE);
  print_display_setting(mcFgColor);

#ifdef BACKLIGHT_ADJUST
  glcdSetAddress(MENU_INDENT, 5);
  glcdPutStrFg("Backlight:        ");
  glcdPrintNumberFg(OCR2B >> OCR2B_BITSHIFT);
#endif
  
  print_instructions(INSTR_ADVANCE, INSTR_SET);
  glcdFillRectangle2(126, 48, 2, 16, ALIGN_TOP, FILL_FULL, mcBgColor);

  // Clear the arrow area
  glcdFillRectangle2(0, 8, 8, 40, ALIGN_TOP, FILL_FULL, mcBgColor);
  screenmutex--;
}

//
// Function: enter_alarm_menu
//
// Enter the alarm setup configuration page
//
static void enter_alarm_menu(void)
{
  uint8_t mode = SET_ALARM;

  display_main_menu();
  screenmutex++;
  // Put a small arrow next to 'Alarm'
  print_arrow(11);
  screenmutex--;
  timeoutcounter = INACTIVITYTIMEOUT;  

  while (1)
  {
#ifdef EMULIN
    stubEventGet();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_ALARM)
      {
        DEBUG(putstring_nl("Go to alarm setup"));
        glcdClearScreen(mcBgColor);
        do
        {
          menu_alarm();
        }
        while (displaymode != SET_ALARM && displaymode != SHOW_TIME);
      }
      if (displaymode == SET_ALARM)
        just_pressed = just_pressed | BTTN_MENU;
      screenmutex--;
      return;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
    }
  }
}

//
// Function: next_value
//
// Returns the next value for an item based on single keypress, initial
// press-hold and long duration press-hold and the upper limit value
//
static uint8_t next_value(uint8_t value, uint8_t maxVal)
{
  // Reset fast increase upon hold release confirmation
  if (hold_release_conf == 1)
  {
    if (DEBUGGING && holdcounter == HOLD_SPEED_INCREASE)
      DEBUGP("+1");
    holdcounter = 0;
    hold_release_conf = 0;
  }

  if (pressed & BTTN_PLUS)
  {
    // Press-hold: normal or fast increase

    // Request a confirmation on hold release
    if (DEBUGGING && hold_release_req == 0)
      DEBUGP("rlr");
    hold_release_req = 1;

    if (holdcounter < HOLD_SPEED_INCREASE)
    {
      // Not too long press-hold: single increase
      holdcounter++;
      value++;
      if (DEBUGGING && holdcounter == HOLD_SPEED_INCREASE)
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
    holdcounter = 0;
    value++;
  }

  // Check on overflow
  if (value >= maxVal)
    value = value % maxVal;

  return value;
}

//
// Function: print_arrow
//
// Print an arrow in front of a menu item
//
static void print_arrow(u08 y)
{
  glcdFillRectangle(0, y, MENU_INDENT - 1, 1, mcFgColor);
  glcdRectangle(MENU_INDENT - 3, y - 1, 1, 3, mcFgColor);
  glcdRectangle(MENU_INDENT - 4, y - 2, 1, 5, mcFgColor);
}

//
// Function: print_date
//
// Print the date (dow+mon+day+year) with optional highlighted item
//
static void print_date(u08 month, u08 day, u08 year, u08 mode)
{
  glcdSetAddress(MENU_INDENT, 3);
  glcdPutStrFg("Date:");
  glcdPutStrFg(days[dotw(month, day, year)]);
  glcdPutStr(months[month - 1], (mode == SET_MONTH) ? mcBgColor : mcFgColor);
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
// Function: print_display_setting
//
// Print the display setting
//
static void print_display_setting(u08 color)
{
  glcdSetAddress(MENU_INDENT, 4);
  glcdPutStrFg("Display:     ");
  if (mcBgColor == OFF)
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
// Function: print_instructions
//
// Print instructions at bottom of screen
//
static void print_instructions(char *line1, char *line2)
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
// Function: print_instructions2
//
// Print instructions at bottom of screen
//
static void print_instructions2(char *line1b, char *line2b)
{
  glcdSetAddress(0, 6);
  glcdPutStrFg(INSTR_PREFIX_CHANGE);
  glcdPutStrFg(line1b);
  glcdSetAddress(0, 7);
  glcdPutStrFg(INSTR_PREFIX_SET);
  glcdPutStrFg(line2b);
}

//
// Function: set_alarm
//
// Set an alarm time and alarm selector by processing button presses
//
static void set_alarm(void)
{
  uint8_t mode = SET_ALARM;
  uint8_t newAlarmSelect = alarmSelect;
  volatile uint8_t newHour, newMin;

  display_alarm_menu();
  screenmutex++;
  // Put a small arrow next to proper line
  print_arrow(11 + 8 * almPageDataId);
  screenmutex--;
  timeoutcounter = INACTIVITYTIMEOUT;  

  // Get current alarm
  if (almPageDataId != 4)
    alarmTimeGet(almPageDataId, &newHour, &newMin);

  // Clear any hold data
  holdcounter = 0;

  while (1)
  {
#ifdef EMULIN
    stubEventGet();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      just_pressed = 0;
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      holdcounter = 0;
      screenmutex++;
      if (mode == SET_ALARM)
      {
        if (almPageDataId == 4)
        {
          DEBUG(putstring_nl("Set selected alarm"));
          // Now it is selected
          mode = SET_ALARM_ID;
          // Print the alarm Id inverted
          glcdSetAddress(MENU_INDENT + 18 * 6, 5);
          glcdPrintNumberBg(newAlarmSelect + 1);
          // Display instructions below
          print_instructions2("alm", "alm ");
        }
        else
        {
          DEBUG(putstring_nl("Set alarm hour"));
          // Now it is selected
          mode = SET_HOUR;
          // Print the hour inverted
          glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
          glcdPrintNumberBg(newHour);
          // Display instructions below
          print_instructions2("hr.", "hour");
        }
      }
      else if (mode == SET_HOUR)
      {
        DEBUG(putstring_nl("Set alarm min"));
        mode = SET_MIN;
        // Print the hour normal
        glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
        glcdPrintNumberFg(newHour);
        // and the minutes inverted
        glcdSetAddress(MENU_INDENT + 18 * 6, 1 + almPageDataId);
        glcdPrintNumberBg(newMin);
        // Display instructions below
        print_instructions2("min", "min ");
      }
      else
      {
        // Clear edit mode
        if (mode == SET_ALARM_ID)
        {
          // Print the alarm Id normal
          glcdSetAddress(MENU_INDENT + 18 * 6, 5);
          glcdPrintNumberFg(newAlarmSelect + 1);
          // Save and sync the new alarm Id
          eeprom_write_byte((uint8_t *)EE_ALARM_SELECT, newAlarmSelect);
          alarmSelect = newAlarmSelect;
        }
        else
        {
          // Print the hour and minutes normal
          glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
          glcdPrintNumberFg(newHour);
          glcdSetAddress(MENU_INDENT + 18 * 6, 1 + almPageDataId);
          glcdPrintNumberFg(newMin);
          // Save the new alarm time
          alarmTimeSet(almPageDataId, newHour, newMin);
        }

        mode = SET_ALARM;
        // Sync alarm time in case the time or the alarm id was changed
        alarmTimeGet(newAlarmSelect, &mcAlarmH, &mcAlarmM);
        // Display instructions below
        if (displaymode != SET_ALARM_ID)
          print_instructions(INSTR_ADVANCE, INSTR_SET);
        else
          print_instructions(INSTR_EXIT, INSTR_SET);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_ALARM_ID)
      {
        newAlarmSelect++;
        if (newAlarmSelect >= 4)
          newAlarmSelect = 0;
        // Print the alarm Id inverted
        glcdSetAddress(MENU_INDENT + 18 * 6, 5);
        glcdPrintNumberBg(newAlarmSelect + 1);
        DEBUG(putstring("New alarm Id -> "));
        DEBUG(uart_putw_dec(newAlarmSelect + 1));
        DEBUG(putstring_nl(""));
      }
      else if (mode == SET_HOUR)
      {
        newHour++;
        if (newHour >= 24)
          newHour = 0;
        // Print the hour inverted
        glcdSetAddress(MENU_INDENT + 15 * 6, 1 + almPageDataId);
        glcdPrintNumberBg(newHour);
        DEBUG(putstring("New alarm hour -> "));
        DEBUG(uart_putw_dec(newHour));
        DEBUG(putstring_nl(""));
      }
      else if (mode == SET_MIN)
      {
        newMin = next_value(newMin, 60);
        // Print the minutes inverted
        glcdSetAddress(MENU_INDENT + 18 * 6, 1 + almPageDataId);
        glcdPrintNumberBg(newMin);
        DEBUG(putstring("New alarm min -> "));
        DEBUG(uart_putw_dec(newMin));
        DEBUG(putstring_nl(""));
      }
      screenmutex--;
      if (pressed & BTTN_PLUS)
	_delay_ms(KEYPRESS_DLY_1);
    }
  }
}

#ifdef BACKLIGHT_ADJUST
//
// Function: set_backlight
//
// Set display backlight brightness by processing button presses
//
static void set_backlight(void)
{
  uint8_t mode = SET_BRIGHTNESS;

  display_main_menu();
  screenmutex++;

  // Last item before leaving setup page
  print_instructions(INSTR_EXIT, 0);

  // Put a small arrow next to 'Backlight'
  print_arrow(43);
  screenmutex--;

  timeoutcounter = INACTIVITYTIMEOUT;  

  while (1)
  {
#ifdef EMULIN
    stubEventGet();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2B);
      return;
    }

    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      eeprom_write_byte((uint8_t *)EE_BRIGHT, OCR2B);
      displaymode = SHOW_TIME;     
      return;
    }
  
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_BRIGHTNESS)
      {
        DEBUG(putstring_nl("Setting backlight"));
        // Now it is selected
        mode = SET_BRT;
        // Print the brightness 
        glcdSetAddress(MENU_INDENT + 18 * 6, 5);
        glcdPrintNumberBg(OCR2B >> OCR2B_BITSHIFT);	
        // Display instructions below
        print_instructions(INSTR_CHANGE, INSTR_SAVE);
      }
      else
      {
        mode = SET_BRIGHTNESS;
        // Print the brightness normal
        glcdSetAddress(MENU_INDENT + 18 * 6, 5);
        glcdPrintNumberFg(OCR2B >> OCR2B_BITSHIFT);
        // Display instructions below
        print_instructions(INSTR_EXIT, INSTR_SET);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
      if (mode == SET_BRT)
      {
        OCR2B += OCR2B_PLUS;
        if(OCR2B > OCR2A_VALUE)
          OCR2B = 0;
        screenmutex++;
        display_main_menu();
        // Display instructions below
        print_instructions(INSTR_CHANGE, INSTR_SAVE);
        // Put a small arrow next to 'Backlight'
        print_arrow(43);
        glcdSetAddress(MENU_INDENT + 18 * 6, 5);
        glcdPrintNumberBg(OCR2B >> OCR2B_BITSHIFT);
        screenmutex--;
      }
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);
    }
  }
}
#endif

//
// Function: set_date
//
// Set a data by setting all individual items of a date by processing
// button presses
//
static void set_date(void)
{
  uint8_t mode = SET_DATE;
  uint8_t day, month, year;
    
  day = date_d;
  month = date_m;
  year = date_y;

  display_main_menu();

  // Put a small arrow next to 'Date'
  screenmutex++;
  print_arrow(27);
  screenmutex--;
  
  timeoutcounter = INACTIVITYTIMEOUT;  

  // Clear any hold data
  holdcounter = 0;

  while (1)
  {
#ifdef EMULIN
    stubEventGet();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      holdcounter = 0;
      screenmutex++;

      if (mode == SET_DATE)
      {
        DEBUG(putstring_nl("Set date month"));
        // Now it is selected
        mode = SET_MONTH;
        // Print the month inverted
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions2("mon", "mon ");
      }
      else if (mode == SET_MONTH)
      {
        DEBUG(putstring_nl("Set date day"));
        mode = SET_DAY;
        // Print the day inverted
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions2("day", "day ");
      }
      else if ((mode == SET_DAY))
      {
        DEBUG(putstring_nl("Set year"));
        mode = SET_YEAR;
        // Print the year inverted
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions2("yr.", "year");
      }
      else
      {
        // Done!
        DEBUG(putstring_nl("Done setting date"));
        mode = SET_DATE;
        // Print the date normal
        print_date(month, day, year, mode);
        // Display instructions below
        print_instructions(INSTR_ADVANCE, INSTR_SET);
	// Update date
        date_y = year;
        date_m = month;
        date_d = day;
        writei2ctime(time_s, time_m, time_h, day, month, year);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      // Increment the date element currently in edit mode
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_MONTH)
      {
        month++;
        if (month >= 13)
        {
          month = 1;
        }
        else if (month == 2)
        {
          if (day > 29)
            day = 29;
          if (!leapyear(year) && (day > 28))
            day = 28;
        }
        else if (month == 4 || month == 6 || month == 9 || month == 11)
        {
          if (day > 30)
      	    day = 30;
        }
      }
      if (mode == SET_DAY)
      {
        day++;
        if (day > 31)
        {
          day = 1;
        }
        else if (month == 2)
        {
          if (day > 29)
            day = 1;
          else if (!leapyear(year) && (day > 28))
            day = 1;
        }
        else if (month == 4 || month == 6 || month == 9 || month == 11)
        {
          if (day > 30)
      	    day = 1;
        }
      }
      if (mode == SET_YEAR)
      {
        year = next_value(year, 100);
        if (!leapyear(year) && month == 2 && day > 28)
          day = 28;
      }
      print_date(month, day, year, mode);
      screenmutex--;
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);  
    }
  }
}

//
// Function: set_display
//
// Set the display type by processing button presses
//
static void set_display(void)
{
  uint8_t mode = SET_DISPLAY;

  display_main_menu();

  screenmutex++;
#ifndef BACKLIGHT_ADJUST
  print_instructions(INSTR_EXIT, 0);
#endif
  // Put a small arrow next to 'Display'
  print_arrow(35);
  screenmutex--;

  timeoutcounter = INACTIVITYTIMEOUT;  
  while (1)
  {
#ifdef EMULIN
    stubEventGet();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Menu change
      eeprom_write_byte((uint8_t *)EE_BGCOLOR, mcBgColor);
      return;
    }

    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      eeprom_write_byte((uint8_t *)EE_BGCOLOR, mcBgColor);
      displaymode = SHOW_TIME;     
      return;
    }
  
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_DISPLAY)
      {
        // In set mode: display value inverse
        DEBUG(putstring_nl("Setting display"));
        mode = SET_DSP;
        print_display_setting(mcBgColor);
        // Display instructions below
        print_instructions(INSTR_CHANGE, INSTR_SAVE);
      }
      else
      {
        // In select mode: display value normal
        mode = SET_DISPLAY;
        print_display_setting(mcFgColor);
        // Display instructions below
#ifdef BACKLIGHT_ADJUST
        print_instructions(INSTR_ADVANCE, INSTR_SET);
#else
        print_instructions(INSTR_EXIT, INSTR_SET);
#endif
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      just_pressed = 0;
      if (mode == SET_DSP)
      {
        // Toggle display mode
        if (mcBgColor == OFF)
        {
          mcBgColor = ON;
          mcFgColor = OFF;
        }
        else
        {
          mcBgColor = OFF;
          mcFgColor = ON;
        }

        // Inverse and rebuild screen
        screenmutex++;
        display_main_menu();
        // Display instructions below
        print_instructions(INSTR_CHANGE, INSTR_SAVE);
        // Put a small arrow next to 'Display'
        print_arrow(35);
        print_display_setting(mcBgColor);
        screenmutex--;
        DEBUG(putstring("New display type -> "));
        DEBUG(uart_putw_dec(mcBgColor));
        DEBUG(putstring_nl(""));
      }
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);
    }
  }
}

//
// Function: set_time
//
// Set the system time by processing button presses
//
static void set_time(void)
{
  uint8_t mode = SET_TIME;
  uint8_t hour, min, sec;
    
  hour = time_h;
  min = time_m;
  sec = time_s;

  display_main_menu();
  
  screenmutex++;
  // Put a small arrow next to 'Time'
  print_arrow(19);
  screenmutex--;
 
  timeoutcounter = INACTIVITYTIMEOUT;  

  // Clear any hold data
  holdcounter = 0;

  while (1) {
#ifdef EMULIN
    stubEventGet();
#endif
    if (just_pressed & BTTN_MENU)
    {
      // Mode change
      return;
    }
    if (just_pressed || pressed)
    {
      // Button pressed so reset timoutcounter
      timeoutcounter = INACTIVITYTIMEOUT;  
    }
    else if (!timeoutcounter)
    {
      // Timed out!
      displaymode = SHOW_TIME;     
      return;
    }
    if (just_pressed & BTTN_SET)
    {
      just_pressed = 0;
      holdcounter = 0;
      screenmutex++;
      if (mode == SET_TIME)
      {
        DEBUG(putstring_nl("Set time hour"));
        // Now it is selected
        mode = SET_HOUR;
        // Print the hour inverted
        glcdSetAddress(MENU_INDENT + 12 * 6, 2);
        glcdPrintNumberBg(hour);
        // Display instructions below
        print_instructions2("hr.", "hour");
      }
      else if (mode == SET_HOUR)
      {
        DEBUG(putstring_nl("Set time min"));
        mode = SET_MIN;
        // Print the hour normal
        glcdSetAddress(MENU_INDENT + 12 * 6, 2);
        glcdPrintNumberFg(hour);
        // and the minutes inverted
        glcdWriteCharFg(':');
        glcdPrintNumberBg(min);
        // Display instructions below
        print_instructions2("min", "min ");
      }
      else if (mode == SET_MIN)
      {
        DEBUG(putstring_nl("Set time sec"));
        mode = SET_SEC;
        // Print the minutes normal
        glcdSetAddress(MENU_INDENT + 15 * 6, 2);
        glcdPrintNumberFg(min);
        glcdWriteCharFg(':');
        // and the seconds inverted
        glcdPrintNumberBg(sec);
        // Display instructions below
        print_instructions2("sec", "sec ");
      }
      else
      {
        // done!
        DEBUG(putstring_nl("Done setting time"));
        mode = SET_TIME;
        // Print the seconds normal
        glcdSetAddress(MENU_INDENT + 18 * 6, 2);
        glcdPrintNumberFg(sec);
        // Display instructions below
        print_instructions(INSTR_ADVANCE, INSTR_SET);
        // Update time
        time_h = hour;
        time_m = min;
        time_s = sec;
        writei2ctime(sec, min, hour, date_d, date_m, date_y);
      }
      screenmutex--;
    }
    if ((just_pressed & BTTN_PLUS) || (pressed & BTTN_PLUS))
    {
      // Increment the time element currently in edit mode
      just_pressed = 0;
      screenmutex++;
      if (mode == SET_HOUR)
      {
        hour++;
        if (hour >= 24)
          hour = 0;
        glcdSetAddress(MENU_INDENT + 12 * 6, 2);
        glcdPrintNumberBg(hour);
      }
      if (mode == SET_MIN)
      {
        min = next_value(min, 60);
        glcdSetAddress(MENU_INDENT + 15 * 6, 2);
        glcdPrintNumberBg(min);
      }
      if (mode == SET_SEC)
      {
        sec = next_value(sec, 60);
        glcdSetAddress(MENU_INDENT + 18 * 6, 2);
        glcdPrintNumberBg(sec);
      }
      screenmutex--;
      if (pressed & BTTN_PLUS)
        _delay_ms(KEYPRESS_DLY_1);
    }
  }
}

