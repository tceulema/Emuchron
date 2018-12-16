//*****************************************************************************
// Filename : 'slider.c'
// Title    : Animation code for MONOCHRON slider clock
//*****************************************************************************

#ifdef EMULIN
#include "../emulator/stub.h"
#else
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"
#include "slider.h"

// Specifics for slider clock
#define SLIDER_LEFT_X_START	1
#define SLIDER_RIGHT_X_START	69
#define SLIDER_MARKER_X_OFFSET	19
#define SLIDER_MARKER_Y_OFFSET	-3

#define SLIDER_NUMBER_Y_START	1

#define SLIDER_SEC_Y_START	38
#define SLIDER_MIN_Y_START	25
#define SLIDER_HOUR_Y_START	12
#define SLIDER_DAY_Y_START	12
#define SLIDER_MON_Y_START	25
#define SLIDER_YEAR_Y_START	38

#define SLIDER_ALARM_Y_START	54

#define SLIDER_MARKER_WIDTH	3
#define SLIDER_MARKER_HEIGHT	5

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcU8Util1;
extern volatile uint8_t mcUpdAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];
extern char animDay[];
extern char animMonth[];
extern char animYear[];

// Local function prototypes
static void sliderAlarmAreaUpdate(void);
static void sliderElementInit(u08 x, u08 y, u08 factor, char *label);
static void sliderElementInvert(u08 x, u08 y, u08 element);
static void sliderElementValueSet(u08 x, u08 y, u08 oldVal, u08 newVal,
  u08 init);

//
// Function: sliderCycle
//
// Update the lcd display of a very simple slider clock
//
void sliderCycle(void)
{
  // Update alarm info in clock
  sliderAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Slider");

  // Verify changes in hour + min + sec
  sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_HOUR_Y_START, mcClockOldTH,
    mcClockNewTH, mcClockInit);
  sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_MIN_Y_START, mcClockOldTM,
    mcClockNewTM, mcClockInit);
  sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_SEC_Y_START, mcClockOldTS,
    mcClockNewTS, mcClockInit);

  // Verify changes in day + mon + year
  sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_DAY_Y_START, mcClockOldDD,
    mcClockNewDD, mcClockInit);
  sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_MON_Y_START, mcClockOldDM,
    mcClockNewDM, mcClockInit);
  sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_YEAR_Y_START, mcClockOldDY,
    mcClockNewDY, mcClockInit);
}

//
// Function: sliderElementInit
//
// Draw the value label and markers for a time/date/alarm element
//
static void sliderElementInit(u08 x, u08 y, u08 factor, char *label)
{
  u08 i;
  u08 markerX = x + SLIDER_MARKER_X_OFFSET;
  u08 markerY = y + SLIDER_MARKER_Y_OFFSET;

  glcdPutStr2(x, y, FONT_5X5P, label, mcFgColor);

  for (i = 0; i <= 9; i++)
  {
    // Draw marker on top row only if in range
    if (i <= factor)
      glcdDot(markerX + SLIDER_MARKER_WIDTH / 2,
        markerY + SLIDER_MARKER_HEIGHT / 2, mcFgColor);
    // Draw marker on bottom row
    glcdDot(markerX + SLIDER_MARKER_WIDTH / 2,
      markerY + SLIDER_MARKER_HEIGHT + 1 + SLIDER_MARKER_HEIGHT / 2,
      mcFgColor);
    markerX = markerX + SLIDER_MARKER_WIDTH + 1;
  }
}

//
// Function: sliderElementInvert
//
// Invert a value marker for a time/date/alarm element
//
static void sliderElementInvert(u08 x, u08 y, u08 element)
{
  glcdFillRectangle2(x + SLIDER_MARKER_X_OFFSET +
    element * (SLIDER_MARKER_WIDTH + 1),
    y + SLIDER_MARKER_Y_OFFSET, SLIDER_MARKER_WIDTH, SLIDER_MARKER_HEIGHT,
    ALIGN_AUTO, FILL_INVERSE, mcFgColor);
}

//
// Function: sliderElementValueSet
//
// Set the value markers for a time/date/alarm element
//
static void sliderElementValueSet(u08 x, u08 y, u08 oldVal, u08 newVal,
  u08 init)
{
  u08 valLowOld;
  u08 valHighOld;
  u08 valLowNew;
  u08 valHighNew;

  // See if we need to update the time element
  if (oldVal == newVal && init == GLCD_FALSE)
    return;

  valLowOld = oldVal % 10;
  valHighOld = oldVal / 10;
  valLowNew = newVal % 10;
  valHighNew = newVal / 10;

  if (valHighOld != valHighNew || init == GLCD_TRUE)
  {
    // Replace old high value with new one
    if (init == GLCD_FALSE)
    {
      // Restore previous marker
      sliderElementInvert(x, y, valHighOld);
    }
    // Draw new marker
    sliderElementInvert(x, y, valHighNew);
  }

  if (valLowOld != valLowNew || init == GLCD_TRUE)
  {
    // Replace old low value with new one
    if (init == GLCD_FALSE)
    {
      // Restore previous marker
      sliderElementInvert(x, y + 1 + SLIDER_MARKER_HEIGHT, valLowOld);
    }
    // Draw new marker
    sliderElementInvert(x, y + 1 + SLIDER_MARKER_HEIGHT, valLowNew);
  }
}

//
// Function: sliderInit
//
// Initialize the lcd display of a clock with slider value elements
//
void sliderInit(u08 mode)
{
  u08 i;
  char val[2];

  DEBUGP("Init Slider");

  // Draw the top row numbers
  val[0] = '0';
  val[1] = '\0';
  for (i = 0; i <= 9; i++)
  {
    glcdPutStr2(SLIDER_LEFT_X_START + SLIDER_MARKER_X_OFFSET + i * 4,
      SLIDER_NUMBER_Y_START, FONT_5X5P, val, mcFgColor);
    glcdPutStr2(SLIDER_RIGHT_X_START + SLIDER_MARKER_X_OFFSET + i * 4,
      SLIDER_NUMBER_Y_START, FONT_5X5P, val, mcFgColor);
    val[0] = val[0] + 1;
  }

  // Draw separator between date/time and alarm and the alarm text
  glcdRectangle(0, 48, 128, 1, mcFgColor);

  // Draw the date and time elements
  sliderElementInit(SLIDER_LEFT_X_START, SLIDER_HOUR_Y_START, 2, animHour);
  sliderElementInit(SLIDER_LEFT_X_START, SLIDER_MIN_Y_START, 5, animMin);
  sliderElementInit(SLIDER_LEFT_X_START, SLIDER_SEC_Y_START, 5, animSec);
  sliderElementInit(SLIDER_RIGHT_X_START, SLIDER_DAY_Y_START, 3, animDay);
  sliderElementInit(SLIDER_RIGHT_X_START, SLIDER_MON_Y_START, 1, animMonth);
  sliderElementInit(SLIDER_RIGHT_X_START, SLIDER_YEAR_Y_START, 9, animYear);

  // Init alarm blink state
  mcU8Util1 = GLCD_FALSE;
}

//
// Function: sliderAlarmAreaUpdate
//
// Draw update in slider clock alarm area
//
static void sliderAlarmAreaUpdate(void)
{
  u08 newAlmDisplayState = GLCD_FALSE;

  // Detect change in displaying alarm
  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Show alarm text and alarm time elements
      sliderElementInit(SLIDER_LEFT_X_START, SLIDER_ALARM_Y_START, 2,
        animHour);
      sliderElementInit(SLIDER_RIGHT_X_START, SLIDER_ALARM_Y_START, 5,
        animMin);

      // Set the alarm element values
      sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_ALARM_Y_START,
        mcAlarmH, mcAlarmH, GLCD_TRUE);
      sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_ALARM_Y_START,
        mcAlarmM, mcAlarmM, GLCD_TRUE);
    }
    else
    {
      // Clear area (remove alarm time elements)
      glcdFillRectangle(SLIDER_LEFT_X_START - 1,
        SLIDER_ALARM_Y_START + SLIDER_MARKER_Y_OFFSET,
        128 - SLIDER_LEFT_X_START, SLIDER_MARKER_HEIGHT * 2 + 1, mcBgColor);
      mcU8Util1 = GLCD_FALSE;
    }
  }

  // Set alarm blinking state in case we're alarming
  if (mcAlarming == GLCD_TRUE && (mcCycleCounter & 0x08) == 8)
    newAlmDisplayState = GLCD_TRUE;

  // Make alarm area blink during alarm or cleanup after end of alarm
  if (newAlmDisplayState != mcU8Util1)
  {
    // Inverse the alarm area
    mcU8Util1 = newAlmDisplayState;
    glcdFillRectangle2(SLIDER_LEFT_X_START - 1, SLIDER_ALARM_Y_START - 1, 17,
      7, ALIGN_AUTO, FILL_INVERSE, mcBgColor);
    glcdFillRectangle2(SLIDER_RIGHT_X_START - 1, SLIDER_ALARM_Y_START - 1, 14,
      7, ALIGN_AUTO, FILL_INVERSE, mcBgColor);
  }
}
