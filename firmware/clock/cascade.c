//*****************************************************************************
// Filename : 'cascade.c'
// Title    : Animation code for MONOCHRON cascade clock
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
#include "spotfire.h"
#include "cascade.h"

// Specifics for QV cascade
#define CASC_SEC_X_START	73
#define CASC_MIN_X_START	43
#define CASC_HOUR_X_START	13
#define CASC_Y_START		54
#define CASC_HEIGHT_MAX		29
#define CASC_VAL_STEPS		59
#define CASC_SNAPSHOT_WIDTH	15
#define CASC_DELTA_X_OFFSET	(CASC_SNAPSHOT_WIDTH + 1)
#define CASC_DELTA_WIDTH	13
#define CASC_VALUE_X_OFFSET	2
#define CASC_VALUE_Y_OFFSET	-8
#define CASC_DELTA_VALUE_Y_OFFSET -6

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;

// Local function prototypes
static void spotCascadeDeltaUpdate(u08 x, u08 oldValLeft, u08 newValLeft,
  u08 oldValRight, u08 newValRight);

//
// Function: spotCascadeCycle
//
// Update the Spotfire QuintusVisuals Cascade and filter panel
//
void spotCascadeCycle(void)
{
  // Update alarm info in clock
  spotAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Cascade");

  // Update common parts of a Spotfire clock
  spotCommonUpdate();

  // Verify changes in sec
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
  {
    spotBarUpdate(CASC_SEC_X_START, CASC_Y_START, CASC_VAL_STEPS, CASC_HEIGHT_MAX,
      CASC_SNAPSHOT_WIDTH, mcClockOldTS, mcClockNewTS, CASC_VALUE_X_OFFSET,
      CASC_VALUE_Y_OFFSET, FILL_BLANK);
  }

  // Verify changes in delta min to sec
  if (mcClockNewTM != mcClockOldTM || mcClockNewTS != mcClockOldTS || 
      mcClockInit == GLCD_TRUE)
  {
    spotCascadeDeltaUpdate(CASC_MIN_X_START + CASC_DELTA_X_OFFSET, 
      mcClockOldTM, mcClockNewTM, mcClockOldTS, mcClockNewTS);
  }  

  // Verify changes in min
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
  {
    spotBarUpdate(CASC_MIN_X_START, CASC_Y_START, CASC_VAL_STEPS, CASC_HEIGHT_MAX,
      CASC_SNAPSHOT_WIDTH, mcClockOldTM, mcClockNewTM, CASC_VALUE_X_OFFSET,
      CASC_VALUE_Y_OFFSET, FILL_BLANK);
  }

  // Verify changes in delta hour to min
  if (mcClockNewTH != mcClockOldTH || mcClockNewTM != mcClockOldTM ||
      mcClockInit == GLCD_TRUE)
  {
    spotCascadeDeltaUpdate(CASC_HOUR_X_START + CASC_DELTA_X_OFFSET, 
      mcClockOldTH, mcClockNewTH, mcClockOldTM, mcClockNewTM);
  }

  // Verify changes in hour
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
  {
    spotBarUpdate(CASC_HOUR_X_START, CASC_Y_START, CASC_VAL_STEPS, CASC_HEIGHT_MAX,
      CASC_SNAPSHOT_WIDTH, mcClockOldTH, mcClockNewTH, CASC_VALUE_X_OFFSET,
      CASC_VALUE_Y_OFFSET, FILL_BLANK);
  }
}

//
// Function: spotCascadeInit
//
// Initialize the LCD display for use as a Spotfire QuintusVisuals Cascade
//
void spotCascadeInit(u08 mode)
{
  DEBUGP("Init Cascade");

  // Draw Spotfire form layout
  spotCommonInit("cascade", mode);

  // Draw static part of cascade
  spotAxisInit(CHRON_CASCADE);
}

//
// Function: spotCascadeDeltaUpdate
//
// Update a single QV cascade delta bar
//
static void spotCascadeDeltaUpdate(u08 x, u08 oldValLeft, u08 newValLeft,
  u08 oldValRight, u08 newValRight)
{
  u08 oldDeltaBarHeight;
  u08 newDeltaBarHeight;
  u08 oldDeltaBarYStart;
  u08 newDeltaBarYStart;
  u08 oldLeftBarHeight;
  u08 newLeftBarHeight;
  u08 oldRightBarHeight;
  u08 newRightBarHeight;
  u08 clearStart;
  u08 clearHeight;
  char barValue[4];
  u08 barValLen = 0;
  s08 barVal;

  // Get height of old and new bar height on left and right side
  oldLeftBarHeight = (u08)((CASC_HEIGHT_MAX / (float)CASC_VAL_STEPS) * oldValLeft + 0.5);
  newLeftBarHeight = (u08)((CASC_HEIGHT_MAX / (float)CASC_VAL_STEPS) * newValLeft + 0.5);
  oldRightBarHeight = (u08)((CASC_HEIGHT_MAX / (float)CASC_VAL_STEPS) * oldValRight + 0.5);
  newRightBarHeight = (u08)((CASC_HEIGHT_MAX / (float)CASC_VAL_STEPS) * newValRight + 0.5);

  // Get height and y-start of old delta bar
  if (oldLeftBarHeight < oldRightBarHeight)
  {
    oldDeltaBarHeight = oldRightBarHeight - oldLeftBarHeight;
    oldDeltaBarYStart = CASC_Y_START - oldRightBarHeight;
  }
  else
  {
    oldDeltaBarHeight = oldLeftBarHeight - oldRightBarHeight;
    oldDeltaBarYStart = CASC_Y_START - oldLeftBarHeight;
  }
  // A zero delta bar is size 1
  if (oldDeltaBarHeight == 0)
    oldDeltaBarHeight = 1;

  // Get height and y-start of new delta bar
  if (newLeftBarHeight < newRightBarHeight)
  {
    newDeltaBarHeight = newRightBarHeight - newLeftBarHeight;
    newDeltaBarYStart = CASC_Y_START - newRightBarHeight;
  }
  else
  {
    newDeltaBarHeight = newLeftBarHeight - newRightBarHeight;
    newDeltaBarYStart = CASC_Y_START - newLeftBarHeight;
  }
  // A zero delta bar is size 1
  if (newDeltaBarHeight == 0)
    newDeltaBarHeight = 1;

  // If there are no changes in barheights there's no need to repaint the bar
  if (oldLeftBarHeight != newLeftBarHeight || oldRightBarHeight != newRightBarHeight ||
      mcClockInit == GLCD_TRUE)
  {
    u08 fillType;
    u08 align;

    // Depending on whether the left or right bar is bigger we need
    // to draw the delta bar differently
    if (newValLeft > newValRight)
    {
      fillType = FILL_THIRDDOWN;
      align = ALIGN_TOP;
    }
    else
    {
      fillType = FILL_THIRDUP;
      align = ALIGN_BOTTOM;
    }

    // Draw the delta bar
    glcdFillRectangle2(x, newDeltaBarYStart, CASC_DELTA_WIDTH, newDeltaBarHeight,
      align, fillType, mcFgColor);
  }

  // Paint the new bar value
  barVal = (s08)newValRight - (s08)newValLeft;
  if (barVal < 0)
  {
    barValue[0] = '-';
    barValLen++;
  }
  barVal = ABS(barVal);
  if (barVal > 9)
  {
     barValue[barValLen] = (char)(barVal / 10 + '0');
     barValLen++;
  }
  barValue[barValLen] = (char)(barVal % 10 + '0');
  barValLen++;
  barValue[barValLen] = '\0';  
  barValLen = barValLen * 4 - 1; // Width in pixels
  glcdPutStr2(x + (CASC_DELTA_WIDTH - barValLen) / 2 + (CASC_DELTA_WIDTH % 2 == 0 ? 1 : 0),
    newDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET, FONT_5X5P, barValue, mcFgColor);

  // Clear the left side of the bar value
  glcdFillRectangle(x, newDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET,
    (CASC_DELTA_WIDTH - barValLen) / 2 + (CASC_DELTA_WIDTH % 2 == 0 ? 1 : 0),
    -CASC_DELTA_VALUE_Y_OFFSET, mcBgColor);

  // Clear the right side of the bar value
  glcdFillRectangle(x + (CASC_DELTA_WIDTH - barValLen) / 2 + barValLen,
    newDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET, (CASC_DELTA_WIDTH - barValLen) / 2,
    -CASC_DELTA_VALUE_Y_OFFSET, mcBgColor);

  // If there are no changes in barheights there's no need to clear anything
  if (oldLeftBarHeight != newLeftBarHeight || oldRightBarHeight != newRightBarHeight)
  {
    // Clear the first line between the bar and the bar value
    glcdFillRectangle(x, newDeltaBarYStart - 1, CASC_DELTA_WIDTH, 1, mcBgColor);

    // Clear what was above it (if any)
    if (oldDeltaBarYStart < newDeltaBarYStart)
    {
      if (oldDeltaBarYStart + oldDeltaBarHeight < newDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET)
        clearHeight = oldDeltaBarHeight - CASC_DELTA_VALUE_Y_OFFSET;
      else
        clearHeight = newDeltaBarYStart - oldDeltaBarYStart;
      if (clearHeight != 0)
        glcdFillRectangle2(x, oldDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET, CASC_DELTA_WIDTH,
          clearHeight, ALIGN_AUTO, FILL_BLANK, mcFgColor);
    }

    // Clear a single line if the bars were equally high and we're moving up
    if (oldLeftBarHeight == oldRightBarHeight && newLeftBarHeight < newRightBarHeight &&
         oldLeftBarHeight == newLeftBarHeight)
      glcdFillRectangle2(x, oldDeltaBarYStart, CASC_DELTA_WIDTH, 1, ALIGN_AUTO, FILL_BLANK,
        mcFgColor);

    // Clear what was below it (if any)
    if (oldDeltaBarYStart + oldDeltaBarHeight > newDeltaBarYStart + newDeltaBarHeight)
    {
      if (oldDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET < newDeltaBarYStart + newDeltaBarHeight)
      {
        clearHeight = oldDeltaBarYStart + oldDeltaBarHeight -
          (newDeltaBarYStart + newDeltaBarHeight);
        clearStart = newDeltaBarYStart + newDeltaBarHeight;
      }
      else
      {
        clearHeight = oldDeltaBarHeight - CASC_DELTA_VALUE_Y_OFFSET;
        clearStart = oldDeltaBarYStart + CASC_DELTA_VALUE_Y_OFFSET;
      }
      if (clearHeight != 0)
        glcdFillRectangle2(x, clearStart, CASC_DELTA_WIDTH, clearHeight, ALIGN_AUTO, FILL_BLANK,
          mcFgColor);
    }
  }
}

