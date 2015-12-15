//*****************************************************************************
// Filename : 'slider.c'
// Title    : Animation code for MONOCHRON slider clock
//*****************************************************************************

#ifdef EMULIN
#include "../emulator/stub.h"
#endif
#ifndef EMULIN
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
static void sliderElementValueSet(u08 x, u08 y, u08 oldVal, u08 newVal, u08 init);

//
// Function: sliderCycle
//
// Update the LCD display of a very simple slider clock
//
void sliderCycle(void)
{
  // Update alarm info in clock
  sliderAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Slider");

  // Verify changes in hour
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
  {
    sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_HOUR_Y_START, mcClockOldTH,
      mcClockNewTH, mcClockInit);
  }
  // Verify changes in min
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
  {
    sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_MIN_Y_START, mcClockOldTM,
      mcClockNewTM, mcClockInit);
  }
  // Verify changes in sec
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
  {
    sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_SEC_Y_START, mcClockOldTS,
      mcClockNewTS, mcClockInit);
  }

  // Verify changes in day
  if (mcClockNewDD != mcClockOldDD || mcClockInit == GLCD_TRUE)
  {
    sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_DAY_Y_START, mcClockOldDD,
      mcClockNewDD, mcClockInit);
  }
  // Verify changes in month
  if (mcClockNewDM != mcClockOldDM || mcClockInit == GLCD_TRUE)
  {
    sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_MON_Y_START, mcClockOldDM,
      mcClockNewDM, mcClockInit);
  }
  // Verify changes in year
  if (mcClockNewDY != mcClockOldDY || mcClockInit == GLCD_TRUE)
  {
    sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_YEAR_Y_START, mcClockOldDY,
      mcClockNewDY, mcClockInit);
  }
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
    {
      glcdDot(markerX + SLIDER_MARKER_WIDTH / 2, markerY + SLIDER_MARKER_HEIGHT / 2,
        mcFgColor);
    }
    // Draw marker on bottom row
    glcdDot(markerX + SLIDER_MARKER_WIDTH / 2,
      markerY + SLIDER_MARKER_HEIGHT + 1 + SLIDER_MARKER_HEIGHT / 2,
      mcFgColor);
    markerX = markerX + SLIDER_MARKER_WIDTH + 1;
  }
}

//
// Function: sliderElementValueSet
//
// Set the value markers for a time/date/alarm element
//
static void sliderElementValueSet(u08 x, u08 y, u08 oldVal, u08 newVal,
  u08 init)
{
  u08 valLowOld = oldVal % 10;
  u08 valHighOld = oldVal / 10;
  u08 valLowNew = newVal % 10;
  u08 valHighNew = newVal / 10;

  if (valHighOld != valHighNew || init == GLCD_TRUE)
  {
    // Replace old high value with new one
    if (init == GLCD_FALSE)
    {
      // Restore previous marker
      glcdFillRectangle2(x + SLIDER_MARKER_X_OFFSET +
        valHighOld * (SLIDER_MARKER_WIDTH + 1),
        y + SLIDER_MARKER_Y_OFFSET, SLIDER_MARKER_WIDTH, SLIDER_MARKER_HEIGHT,
        ALIGN_AUTO, FILL_INVERSE, mcBgColor);
    }
    // Draw new marker
    glcdFillRectangle2(x + SLIDER_MARKER_X_OFFSET +
      valHighNew * (SLIDER_MARKER_WIDTH + 1),
      y + SLIDER_MARKER_Y_OFFSET, SLIDER_MARKER_WIDTH, SLIDER_MARKER_HEIGHT,
      ALIGN_AUTO, FILL_INVERSE, mcFgColor);
  }

  if (valLowOld != valLowNew || init == GLCD_TRUE)
  {
    // Replace old low value with new one
    if (init == GLCD_FALSE)
    {
      // Restore previous marker
      glcdFillRectangle2(x + SLIDER_MARKER_X_OFFSET +
        valLowOld * (SLIDER_MARKER_WIDTH + 1),
        y + SLIDER_MARKER_Y_OFFSET + 1 + SLIDER_MARKER_HEIGHT,
        SLIDER_MARKER_WIDTH, SLIDER_MARKER_HEIGHT, ALIGN_AUTO, FILL_INVERSE,
        mcBgColor);
    }
    // Draw new marker
    glcdFillRectangle2(x + SLIDER_MARKER_X_OFFSET +
      valLowNew * (SLIDER_MARKER_WIDTH + 1),
      y + SLIDER_MARKER_Y_OFFSET + 1 + SLIDER_MARKER_HEIGHT,
      SLIDER_MARKER_WIDTH, SLIDER_MARKER_HEIGHT, ALIGN_AUTO, FILL_INVERSE,
      mcFgColor);
  }
}

//
// Function: sliderInit
//
// Initialize the LCD display of a clock with slider value elements
//
void sliderInit(u08 mode)
{
  u08 i;
  char val[2];

  DEBUGP("Init Slider");

  // Start from scratch
  glcdClearScreen(mcBgColor);

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
  
  // Force the alarm info area to init itself
  mcAlarmSwitch = ALARM_SWITCH_NONE;
  mcU8Util1 = GLCD_FALSE;
}

//
// Function: sliderAlarmAreaUpdate
//
// Draw update in slider clock alarm area
//
static void sliderAlarmAreaUpdate(void)
{
  u08 inverseAlarmArea = GLCD_FALSE;
  u08 newAlmDisplayState = GLCD_FALSE;

  if ((mcCycleCounter & 0x0F) >= 8)
    newAlmDisplayState = GLCD_TRUE;

  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Show alarm text and alarm time elements
      sliderElementInit(SLIDER_LEFT_X_START, SLIDER_ALARM_Y_START, 2, animHour);
      sliderElementInit(SLIDER_RIGHT_X_START, SLIDER_ALARM_Y_START, 5, animMin);

      // Set the alarm element values
      sliderElementValueSet(SLIDER_LEFT_X_START, SLIDER_ALARM_Y_START, mcAlarmH,
        mcAlarmH, GLCD_TRUE);
      sliderElementValueSet(SLIDER_RIGHT_X_START, SLIDER_ALARM_Y_START, mcAlarmM,
        mcAlarmM, GLCD_TRUE);
    }
    else
    {
      // Clear area (remove alarm time elements)
      glcdFillRectangle(SLIDER_LEFT_X_START - 1,
        SLIDER_ALARM_Y_START + SLIDER_MARKER_Y_OFFSET, 128 - SLIDER_LEFT_X_START,
        SLIDER_MARKER_HEIGHT * 2 + 1, mcBgColor);
      mcU8Util1 = GLCD_FALSE;
    }
  }

  if (mcAlarming == GLCD_TRUE)
  {
    // Blink alarm text when we're alarming or snoozing
    if (newAlmDisplayState != mcU8Util1)
    {
      inverseAlarmArea = GLCD_TRUE;
      mcU8Util1 = newAlmDisplayState;
    }
  }
  else
  {
    // Reset inversed alarm text when alarming has stopped
    if (mcU8Util1 == GLCD_TRUE)
    {
      inverseAlarmArea = GLCD_TRUE;
      mcU8Util1 = GLCD_FALSE;
    }
  }

  // Inverse the alarm text if needed
  if (inverseAlarmArea == GLCD_TRUE)
  {
    glcdFillRectangle2(SLIDER_LEFT_X_START - 1, SLIDER_ALARM_Y_START - 1, 17, 7,
      ALIGN_AUTO, FILL_INVERSE, mcBgColor);
    glcdFillRectangle2(SLIDER_RIGHT_X_START - 1, SLIDER_ALARM_Y_START - 1, 14, 7,
      ALIGN_AUTO, FILL_INVERSE, mcBgColor);
  }
}

