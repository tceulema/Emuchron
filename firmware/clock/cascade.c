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
#include "../ratt.h"
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
void spotCascadeDeltaUpdate(u08 x, u08 y, u08 maxVal, u08 maxHeight, u08 width,
  u08 oldValLeft, u08 newValLeft, u08 oldValRight, u08 newValRight,
  s08 valYOffset, u08 fillType);

//
// Function: spotCascadeCycle
//
// Update the Spotfire QuintusVisuals Cascade and filter panel
//
void spotCascadeCycle(void)
{
  u08 fillType;

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
    if (mcClockNewTM > mcClockNewTS)
      fillType = FILL_THIRDDOWN;
    else
      fillType = FILL_THIRDUP;
    spotCascadeDeltaUpdate(CASC_MIN_X_START + CASC_DELTA_X_OFFSET, CASC_Y_START,
      CASC_VAL_STEPS, CASC_HEIGHT_MAX, CASC_DELTA_WIDTH, mcClockOldTM, mcClockNewTM,
      mcClockOldTS, mcClockNewTS, CASC_DELTA_VALUE_Y_OFFSET, fillType);
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
    if (mcClockNewTH > mcClockNewTM)
      fillType = FILL_THIRDDOWN;
    else
      fillType = FILL_THIRDUP;
    spotCascadeDeltaUpdate(CASC_HOUR_X_START + CASC_DELTA_X_OFFSET, CASC_Y_START,
      CASC_VAL_STEPS, CASC_HEIGHT_MAX, CASC_DELTA_WIDTH, mcClockOldTH, mcClockNewTH,
      mcClockOldTM, mcClockNewTM, CASC_DELTA_VALUE_Y_OFFSET, fillType);
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
void spotCascadeDeltaUpdate(u08 x, u08 y, u08 maxVal, u08 maxHeight, u08 width,
  u08 oldValLeft, u08 newValLeft, u08 oldValRight, u08 newValRight,
  s08 valYOffset, u08 fillType)
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
  oldLeftBarHeight = (u08)((maxHeight / (float)maxVal) * oldValLeft + 0.5);
  newLeftBarHeight = (u08)((maxHeight / (float)maxVal) * newValLeft + 0.5);
  oldRightBarHeight = (u08)((maxHeight / (float)maxVal) * oldValRight + 0.5);
  newRightBarHeight = (u08)((maxHeight / (float)maxVal) * newValRight + 0.5);

  // Get height and y-start of old delta bar
  if (oldLeftBarHeight < oldRightBarHeight)
  {
    oldDeltaBarHeight = oldRightBarHeight - oldLeftBarHeight;
    oldDeltaBarYStart = y - oldRightBarHeight;
  }
  else
  {
    oldDeltaBarHeight = oldLeftBarHeight - oldRightBarHeight;
    oldDeltaBarYStart = y - oldLeftBarHeight;
  }
  // A zero delta bar is size 1
  if (oldDeltaBarHeight == 0)
    oldDeltaBarHeight = 1;

  // Get height and y-start of new delta bar
  if (newLeftBarHeight < newRightBarHeight)
  {
    newDeltaBarHeight = newRightBarHeight - newLeftBarHeight;
    newDeltaBarYStart = y - newRightBarHeight;
  }
  else
  {
    newDeltaBarHeight = newLeftBarHeight - newRightBarHeight;
    newDeltaBarYStart = y - newLeftBarHeight;
  }
  // A zero delta bar is size 1
  if (newDeltaBarHeight == 0)
    newDeltaBarHeight = 1;

  // If there are no changes in barheights there's no need to repaint the bar
  if (oldLeftBarHeight != newLeftBarHeight || oldRightBarHeight != newRightBarHeight ||
      mcClockInit == GLCD_TRUE)
  {
    // Paint new bar
    if (newLeftBarHeight >= newRightBarHeight)
      glcdFillRectangle2(x, newDeltaBarYStart, width, newDeltaBarHeight,
        ALIGN_TOP, fillType, mcFgColor);
    else
      glcdFillRectangle2(x, newDeltaBarYStart, width, newDeltaBarHeight,
        ALIGN_BOTTOM, fillType, mcFgColor);
    if (fillType == FILL_BLANK)
    {
      glcdRectangle(x, newDeltaBarYStart, width, newDeltaBarHeight, mcFgColor);
    }
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
  glcdPutStr2(x + (width - barValLen) / 2 + (width % 2 == 0 ? 1 : 0),
    newDeltaBarYStart + valYOffset, FONT_5X5P, barValue, mcFgColor);

  // Clear the left side of the bar value
  glcdFillRectangle(x, newDeltaBarYStart + valYOffset,
    (width-barValLen) / 2 + (width % 2 == 0 ? 1 : 0), -valYOffset, mcBgColor);

  // Clear the right side of the bar value
  glcdFillRectangle(x + (width - barValLen) / 2 + barValLen,
    newDeltaBarYStart + valYOffset, (width - barValLen) / 2, -valYOffset, mcBgColor);

  // If there are no changes in barheights there's no need to clear anything
  if (oldLeftBarHeight != newLeftBarHeight || oldRightBarHeight != newRightBarHeight)
  {
    // Clear the first line between the bar and the bar value
    glcdFillRectangle(x, newDeltaBarYStart - 1, width, 1, mcBgColor);

    // Clear what was above it (if any)
    if (oldDeltaBarYStart < newDeltaBarYStart)
    {
      if (oldDeltaBarYStart + oldDeltaBarHeight < newDeltaBarYStart + valYOffset)
        clearHeight = oldDeltaBarHeight - valYOffset;
      else
        clearHeight = newDeltaBarYStart - oldDeltaBarYStart;
      if (clearHeight != 0)
        glcdFillRectangle2(x, oldDeltaBarYStart + valYOffset, width,
          clearHeight, ALIGN_AUTO, FILL_BLANK, mcFgColor);
    }

    // Clear a single line if the bars were equally high and we're moving up
    if (oldLeftBarHeight == oldRightBarHeight && newLeftBarHeight < newRightBarHeight &&
         oldLeftBarHeight == newLeftBarHeight)
      glcdFillRectangle2(x, oldDeltaBarYStart, width, 1, ALIGN_AUTO, FILL_BLANK,
        mcFgColor);

    // Clear what was below it (if any)
    if (oldDeltaBarYStart + oldDeltaBarHeight > newDeltaBarYStart + newDeltaBarHeight)
    {
      if (oldDeltaBarYStart + valYOffset < newDeltaBarYStart + newDeltaBarHeight)
      {
        clearHeight = oldDeltaBarYStart + oldDeltaBarHeight -
          (newDeltaBarYStart + newDeltaBarHeight);
        clearStart = newDeltaBarYStart + newDeltaBarHeight;
      }
      else
      {
        clearHeight = oldDeltaBarHeight - valYOffset;
        clearStart = oldDeltaBarYStart + valYOffset;
      }
      if (clearHeight != 0)
        glcdFillRectangle2(x, clearStart, width, clearHeight, ALIGN_AUTO, FILL_BLANK,
          mcFgColor);
    }
  }
}

