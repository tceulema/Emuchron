//*****************************************************************************
// Filename : 'anim.c'
// Title    : Main animation and drawing code driver for MONOCHRON clocks
//*****************************************************************************

#ifndef EMULIN
#include <util/delay.h>
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/controller.h"
#endif
#include "ks0108.h"
#include "monomain.h"
#include "glcd.h"
#include "anim.h"

// Include the supported clocks
#include "clock/analog.h"
#include "clock/barchart.h"
#include "clock/bigdigit.h"
#include "clock/cascade.h"
#include "clock/crosstable.h"
#include "clock/digital.h"
#include "clock/example.h"
#include "clock/linechart.h"
#include "clock/mosquito.h"
#include "clock/nerd.h"
#include "clock/perftest.h"
#include "clock/piechart.h"
#include "clock/pong.h"
#include "clock/puzzle.h"
#include "clock/qr.h"
#include "clock/slider.h"
#include "clock/speeddial.h"
#include "clock/spiderplot.h"
#include "clock/thermometer.h"
#include "clock/trafficlight.h"

// The following monomain.c global variables are used to transfer the hardware
// state to a stable functional Monochron clock state.
// They are not to be used in individual Monochron clocks as their contents are
// considered unstable.
extern volatile uint8_t almSwitchOn, almAlarming;
extern volatile rtcDateTime_t rtcDateTimeNext;
extern volatile uint8_t rtcTimeEvent;

// The following mcXYZ global variables are for use in any Monochron clock.
// In a Monochron clock its contents are considered stable.

// Previous and new date/time (also ref mcClockTimeEvent)
volatile uint8_t mcClockOldTS = 0, mcClockOldTM = 0, mcClockOldTH = 0;
volatile uint8_t mcClockNewTS = 0, mcClockNewTM = 0, mcClockNewTH = 0;
volatile uint8_t mcClockOldDD = 0, mcClockOldDM = 0, mcClockOldDY = 0;
volatile uint8_t mcClockNewDD = 0, mcClockNewDM = 0, mcClockNewDY = 0;

// Indicates whether real time clock has changed since last check.
// Note the following:
// - mcClockTimeEvent turns true when the time or date has changed since last
//   check
// - mcClockDateEvent turns true when the date has changed since last check
volatile uint8_t mcClockTimeEvent = GLCD_FALSE;
volatile uint8_t mcClockDateEvent = GLCD_FALSE;

// Indicates whether the alarm switch is On or Off
volatile uint8_t mcAlarmSwitch;

// Indicates whether the alarm switch has changed since last check
volatile uint8_t mcUpdAlarmSwitch = GLCD_FALSE;

// Indicates whether the clock is alarming/snoozing or not
volatile uint8_t mcAlarming = GLCD_FALSE;

// The alarm time
volatile uint8_t mcAlarmH, mcAlarmM;

// Clock cycle ticker
volatile uint8_t mcCycleCounter = 0;

// Flag to force clocks to paint clock values
volatile uint8_t mcClockInit = GLCD_FALSE;

// The foreground and background colors of the b/w lcd display.
// Their values must be mutual exclusive.
// Upon changing the display mode the values are swapped.
// GLCD_OFF = 0 = black color (=0x0 bit value in lcd memory)
// GLCD_ON  = 1 = white color (=0x1 bit value in lcd memory)
volatile uint8_t mcFgColor;
volatile uint8_t mcBgColor;

// Free for use variables for clocks
volatile uint8_t mcU8Util1;
volatile uint8_t mcU8Util2;
volatile uint8_t mcU8Util3;
volatile uint8_t mcU8Util4;
volatile uint16_t mcU16Util1;
volatile uint16_t mcU16Util2;
volatile uint16_t mcU16Util3;
volatile uint16_t mcU16Util4;

// Common labels for time/date elements
const char animHour[] = "Hour";
const char animMin[] = "Min";
const char animSec[] = "Sec";
const char animDay[] = "Day";
const char animMonth[] = "Mon";
const char animYear[] = "Year";

// Common labels for the months in a year
const char *animMonths[12] =
{
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// Common labels for the days in a week
const char *animDays[7] =
{
  "Sun ", "Mon ", "Tue ", "Wed ", "Thu ", "Fri ", "Sat "
};

// The monochron array defines the clocks and their round-robin sequence as
// supported in the application.
// Based on the selection of clocks in monochron[], configure the source files
// in SRC in Makefile [firmware].
clockDriver_t monochron[] =
{
   //{CHRON_PERFTEST,    DRAW_INIT_FULL,    perfInit,           perfCycle,           0}
   {CHRON_CASCADE,     DRAW_INIT_FULL,    spotCascadeInit,    spotCascadeCycle,    0}
  ,{CHRON_SPEEDDIAL,   DRAW_INIT_PARTIAL, spotSpeedDialInit,  spotSpeedDialCycle,  0}
  ,{CHRON_SPIDERPLOT,  DRAW_INIT_PARTIAL, spotSpiderPlotInit, spotSpiderPlotCycle, 0}
  //,{CHRON_THERMOMETER, DRAW_INIT_PARTIAL, spotThermInit,      spotThermCycle,      0}
  ,{CHRON_TRAFLIGHT,   DRAW_INIT_PARTIAL, spotTrafLightInit,  spotTrafLightCycle,  0}
  //,{CHRON_BARCHART,    DRAW_INIT_PARTIAL, spotBarChartInit,   spotBarChartCycle,   0}
  //,{CHRON_CROSSTABLE,  DRAW_INIT_PARTIAL, spotCrossTableInit, spotCrossTableCycle, 0}
  //,{CHRON_PIECHART,    DRAW_INIT_PARTIAL, spotPieChartInit,   spotPieChartCycle,   0}
  //,{CHRON_LINECHART,   DRAW_INIT_PARTIAL, spotLineChartInit,  spotLineChartCycle,  0}
  ,{CHRON_ANALOG_HMS,  DRAW_INIT_FULL,    analogHmsInit,      analogCycle,         0}
  ,{CHRON_ANALOG_HM,   DRAW_INIT_PARTIAL, analogHmInit,       analogCycle,         0}
  ,{CHRON_DIGITAL_HMS, DRAW_INIT_FULL,    digitalHmsInit,     digitalCycle,        0}
  ,{CHRON_DIGITAL_HM,  DRAW_INIT_PARTIAL, digitalHmInit,      digitalCycle,        0}
  //,{CHRON_MOSQUITO,    DRAW_INIT_FULL,    mosquitoInit,       mosquitoCycle,       0}
  //,{CHRON_NERD,        DRAW_INIT_FULL,    nerdInit,           nerdCycle,           0}
  //,{CHRON_PONG,        DRAW_INIT_FULL,    pongInit,           pongCycle,           pongButton}
  ,{CHRON_PUZZLE,      DRAW_INIT_FULL,    puzzleInit,         puzzleCycle,         puzzleButton}
  //,{CHRON_SLIDER,      DRAW_INIT_FULL,    sliderInit,         sliderCycle,         0}
  //,{CHRON_BIGDIG_TWO,  DRAW_INIT_FULL,    bigDigInit,         bigDigCycle,         bigDigButton}
  //,{CHRON_BIGDIG_ONE,  DRAW_INIT_PARTIAL, bigDigInit,         bigDigCycle,         bigDigButton}
  //,{CHRON_QR_HMS,      DRAW_INIT_FULL,    qrInit,             qrCycle,             0}
  //,{CHRON_QR_HM,       DRAW_INIT_PARTIAL, qrInit,             qrCycle,             0}
  //,{CHRON_EXAMPLE,     DRAW_INIT_FULL,    exampleInit,        exampleCycle,        0}
};

// Runtime pointer to active clockdriver array and the index in the array
// pointing to the active clock
clockDriver_t *mcClockPool = monochron;
volatile uint8_t mcMchronClock = 0;

// The alarm blink state
static u08 almDisplayState;

// Local function prototypes
static void animAlarmSwitchCheck(void);
static void animDatePrint(u08 x, u08 y);
static void animDateTimeCopy(void);

//
// Function: animADAreaUpdate
//
// Draw update in clock alarm/date area. It supports generic alarm/date area
// functionality used in many clocks. The type defines whether the area is used
// for displaying the alarm time only, date only or a combination of both,
// depending on whether the alarm switch is on or off.
// NOTE: A single clock can implement multiple date-only areas but only a
// single area that includes the alarm. This restriction is due to the method
// used to administer the blinking state of the alarm time while alarming,
// using static variable almDisplayState.
//
void animADAreaUpdate(u08 x, u08 y, u08 type)
{
  u08 newAlmDisplayState = GLCD_FALSE;
  u08 pxDone = 0;
  char msg[6];

  if (type == AD_AREA_DATE_ONLY)
  {
    // Only the date is shown
    if (mcClockDateEvent == GLCD_TRUE || mcClockInit == GLCD_TRUE)
      animDatePrint(x, y);
    return;
  }

  // Detect change in displaying alarm or date
  if (mcUpdAlarmSwitch == GLCD_TRUE || mcClockDateEvent == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      if (mcUpdAlarmSwitch == GLCD_TRUE)
      {
        // Show alarm time
        animValToStr(mcAlarmH, msg);
        msg[2] = ':';
        animValToStr(mcAlarmM, &msg[3]);
        pxDone = glcdPutStr2(x, y, FONT_5X5P, msg, mcFgColor);

        // Clean up any trailing remnants of previous text
        if (type == AD_AREA_ALM_DATE)
          glcdFillRectangle(x + pxDone, y, AD_AREA_AD_WIDTH - pxDone, 5,
            mcBgColor);
      }
    }
    else
    {
      // Remove potential alarm time that is potentially inverted
      glcdFillRectangle(x - 1, y - 1, 19, 7, mcBgColor);
      almDisplayState = GLCD_FALSE;

      // Show date if requested
      if (type == AD_AREA_ALM_DATE)
        animDatePrint(x, y);
    }
  }

  // Set alarm blinking state in case we're alarming
  if (mcAlarming == GLCD_TRUE && (mcCycleCounter & 0x08) == 8)
    newAlmDisplayState = GLCD_TRUE;

  // Make alarm area blink during alarm or cleanup after end of alarm
  if (newAlmDisplayState != almDisplayState)
  {
    // Inverse the alarm area
    almDisplayState = newAlmDisplayState;
    glcdFillRectangle2(x - 1, y - 1, 19, 7, ALIGN_AUTO, FILL_INVERSE,
      mcBgColor);
  }
}

//
// Function: animAlarmSwitchCheck
//
// Check the position of the alarm switch versus the software state of the
// alarm info, resulting in a flag indicating whether the alarm info area must
// be updated
//
static void animAlarmSwitchCheck(void)
{
  // Reset pending functional alarm switch change
  mcUpdAlarmSwitch = GLCD_FALSE;

  if (almSwitchOn == GLCD_TRUE)
  {
    if (mcAlarmSwitch != ALARM_SWITCH_ON)
    {
      // Init alarm switch value, or the alarm switch has been switched on
      DEBUGP("Alarm info -> Alarm");
      mcAlarmSwitch = ALARM_SWITCH_ON;
      mcUpdAlarmSwitch = GLCD_TRUE;
      almDisplayState = GLCD_FALSE;
    }
  }
  else
  {
    // Show the current date
    if (mcAlarmSwitch != ALARM_SWITCH_OFF)
    {
      // Init alarm switch value, or he alarm switch has been switched off
      DEBUGP("Alarm info -> Other");
      mcAlarmSwitch = ALARM_SWITCH_OFF;
      mcUpdAlarmSwitch = GLCD_TRUE;
    }
  }
}

//
// Function: animClockButton
//
// Wrapper function for clocks to react to the Set and/or Plus button (when
// supported).
// Returns GLCD_TRUE when a button handler is configured for the clock.
//
u08 animClockButton(u08 pressedButton)
{
  clockDriver_t *clockDriver = &mcClockPool[mcMchronClock];

  if (clockDriver->button != 0)
  {
    // Execute the configured button function
    clockDriver->button(pressedButton);
    return GLCD_TRUE;
  }
  return GLCD_FALSE;
}

//
// Function: animClockDraw
//
// Wrapper function for clocks to draw themselves
//
void animClockDraw(u08 mode)
{
  clockDriver_t *clockDriver = &mcClockPool[mcMchronClock];

  // Sync alarming state and time event for clock
  mcAlarming = almAlarming;
  mcClockTimeEvent = rtcTimeEvent;

  // If there's a time event, sync Monochron time with RTC
  if (mcClockTimeEvent == GLCD_TRUE)
  {
    DEBUGTP("Update by time event");
    mcClockNewTS = rtcDateTimeNext.timeSec;
    mcClockNewTM = rtcDateTimeNext.timeMin;
    mcClockNewTH = rtcDateTimeNext.timeHour;
    mcClockNewDD = rtcDateTimeNext.dateDay;
    mcClockNewDM = rtcDateTimeNext.dateMon;
    mcClockNewDY = rtcDateTimeNext.dateYear;
    if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
        mcClockNewDY != mcClockOldDY)
      mcClockDateEvent = GLCD_TRUE;
  }

  // Have the clock initialize or update itself
  if (clockDriver->clockId != CHRON_NONE)
  {
    if (mode == DRAW_CYCLE)
    {
      // Update clock and sync old date/time to current for next comparison
      animAlarmSwitchCheck();
      clockDriver->cycle();
      if (mcClockTimeEvent == GLCD_TRUE)
        animDateTimeCopy();

      // Clear init flag (if set anyway) in case of first clock draw
      mcClockInit = GLCD_FALSE;
    }
    else // DRAW_INIT_FULL or DRAW_INIT_PARTIAL
    {
      if (mode == DRAW_INIT_FULL)
      {
        // Full init: force alarm area to update and clear the screen
        mcAlarmSwitch = ALARM_SWITCH_NONE;
        glcdClearScreen(mcBgColor);
      }

      // Init the clock
      animDateTimeCopy();
      mcClockInit = GLCD_TRUE;
      clockDriver->init(mode);
    }
  }
  else
  {
    DEBUGP("Bad clock in animClockDraw()");
  }

  // Clear a time event when set
  if (mcClockTimeEvent == GLCD_TRUE)
  {
    DEBUGTP("Clear time event");
    mcClockTimeEvent = GLCD_FALSE;
    mcClockDateEvent = GLCD_FALSE;
    rtcTimeEvent = GLCD_FALSE;
  }
}

//
// Function: animClockNext
//
// Get next clock based on the current one and return its init type
//
u08 animClockNext(void)
{
  // Select the next clock
  mcMchronClock++;

  // When end of clock pool is reached continue at beginning
  if (mcMchronClock >= sizeof(monochron) / sizeof(clockDriver_t))
    mcMchronClock = 0;

  return mcClockPool[mcMchronClock].initType;
}

//
// Function: animDatePrint
//
// Prints the current date at location px[x,y]. It takes care of removing any
// date remnants when the new printed date is smaller than the max date print
// size.
//
static void animDatePrint(u08 x, u08 y)
{
  u08 pxDone = 0;
  char msg[4];

  // Print the date
  pxDone = glcdPutStr2(x, y, FONT_5X5P, (char *)animMonths[mcClockNewDM - 1],
    mcFgColor);
  msg[0] = ' ';
  animValToStr(mcClockNewDD, &msg[1]);
  pxDone = pxDone + glcdPutStr2(pxDone + x, y, FONT_5X5P, msg, mcFgColor);

  // Clean up any trailing remnants of previous date
  if (pxDone < AD_AREA_AD_WIDTH)
    glcdFillRectangle(x + pxDone, y, AD_AREA_AD_WIDTH - pxDone, 5, mcBgColor);
}

//
// Function: animDateTimeCopy
//
// Copy new date/time to old date/time
//
static void animDateTimeCopy(void)
{
  mcClockOldTS = mcClockNewTS;
  mcClockOldTM = mcClockNewTM;
  mcClockOldTH = mcClockNewTH;
  mcClockOldDD = mcClockNewDD;
  mcClockOldDM = mcClockNewDM;
  mcClockOldDY = mcClockNewDY;
}

//
// Function: animValToStr
//
// Translate a value into a two-digit character string
//
void animValToStr(u08 value, char valString[])
{
  valString[0] = (char)(value / 10 + '0');
  valString[1] = (char)(value % 10 + '0');
  valString[2] = '\0';
}

//
// Function: animWelcome
//
// Give Monochron startup message
//
void animWelcome(void)
{
  // Give startup welcome message and (optionally) firmware version
  glcdPutStr2(33, 14, FONT_5X7N, "Welcome to", mcFgColor);
  glcdPutStr2(18, 30, FONT_5X7N, "-- T1 clocks --", mcFgColor);
  //glcdPutStr2(1, 58, FONT_5X5P, EMUCHRON_VERSION, mcFgColor);

#ifdef EMULIN
  ctrlLcdFlush();
  _delay_ms(1000);
#else
  _delay_ms(3000);
#endif
  beep(3750, 100);
  beep(4000, 100);
}
