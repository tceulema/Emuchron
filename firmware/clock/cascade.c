//*****************************************************************************
// Filename : 'cascade.c'
// Title    : Animation code for MONOCHRON cascade clock
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
#include "spotfire.h"
#include "cascade.h"

// Specifics for cascade clock
#define CASC_SEC_X_START	73
#define CASC_MIN_X_START	43
#define CASC_HOUR_X_START	13
#define CASC_SNAPSHOT_WIDTH	15
#define CASC_DELTA_X_OFFSET	(CASC_SNAPSHOT_WIDTH + 1)
#define CASC_DELTA_WIDTH	13
#define CASC_VALUE_X_OFFSET	2
#define CASC_DELTA_VALUE_Y_OFFSET -6

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcBgColor, mcFgColor;

// Local function prototypes
static void spotCascadeDeltaUpdate(u08 x, u08 oldValLeft, u08 newValLeft,
  u08 oldValRight, u08 newValRight);

//
// Function: spotCascadeCycle
//
// Update the QuintusVisuals cascade and filter panel
//
void spotCascadeCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == GLCD_FALSE)
    return;

  DEBUGP("Update Cascade");

  // Verify changes in sec
  spotBarUpdate(CASC_SEC_X_START, CASC_SNAPSHOT_WIDTH, mcClockOldTS,
    mcClockNewTS, CASC_VALUE_X_OFFSET, FILL_BLANK);

  // Verify changes in delta min to sec
  spotCascadeDeltaUpdate(CASC_MIN_X_START + CASC_DELTA_X_OFFSET,
    mcClockOldTM, mcClockNewTM, mcClockOldTS, mcClockNewTS);

  // Verify changes in min
  spotBarUpdate(CASC_MIN_X_START, CASC_SNAPSHOT_WIDTH, mcClockOldTM,
    mcClockNewTM, CASC_VALUE_X_OFFSET, FILL_BLANK);

  // Verify changes in delta hour to min
  spotCascadeDeltaUpdate(CASC_HOUR_X_START + CASC_DELTA_X_OFFSET,
    mcClockOldTH, mcClockNewTH, mcClockOldTM, mcClockNewTM);

  // Verify changes in hour
  spotBarUpdate(CASC_HOUR_X_START, CASC_SNAPSHOT_WIDTH, mcClockOldTH,
    mcClockNewTH, CASC_VALUE_X_OFFSET, FILL_BLANK);
}

//
// Function: spotCascadeInit
//
// Initialize the lcd display of a QuintusVisuals cascade
//
void spotCascadeInit(u08 mode)
{
  DEBUGP("Init Cascade");

  // Draw Spotfire form layout
  spotCommonInit("cascade", mode);

  // Draw static axis part of cascade
  spotAxisInit(CHRON_CASCADE);
}

//
// Function: spotCascadeDeltaUpdate
//
// Update a single QuintusVisuals cascade delta bar
//
static void spotCascadeDeltaUpdate(u08 x, u08 oldValLeft, u08 newValLeft,
  u08 oldValRight, u08 newValRight)
{
  u08 oldDeltaHeight;
  u08 newDeltaHeight;
  u08 oldDeltaYStart;
  u08 newDeltaYStart;
  u08 oldLeftHeight;
  u08 newLeftHeight;
  u08 oldRightHeight;
  u08 newRightHeight;
  u08 clearStart;
  u08 clearHeight;
  char barValue[4];
  u08 barValLen = 0;
  s08 barVal;
  u08 alignWidth;

  // See if there's any need to update a delta bar
  if (oldValLeft == newValLeft && oldValRight == newValRight &&
      mcClockInit == GLCD_FALSE)
    return;

  // Get height of old and new bar height on left and right side
  oldLeftHeight =
    (u08)((SPOT_BAR_HEIGHT_MAX / (float)SPOT_BAR_VAL_STEPS) * oldValLeft + 0.5);
  newLeftHeight =
    (u08)((SPOT_BAR_HEIGHT_MAX / (float)SPOT_BAR_VAL_STEPS) * newValLeft + 0.5);
  oldRightHeight =
    (u08)((SPOT_BAR_HEIGHT_MAX / (float)SPOT_BAR_VAL_STEPS) * oldValRight + 0.5);
  newRightHeight =
    (u08)((SPOT_BAR_HEIGHT_MAX / (float)SPOT_BAR_VAL_STEPS) * newValRight + 0.5);

  // Get height and y-start of old delta bar
  if (oldLeftHeight < oldRightHeight)
  {
    oldDeltaHeight = oldRightHeight - oldLeftHeight;
    oldDeltaYStart = SPOT_BAR_Y_START - oldRightHeight;
  }
  else
  {
    oldDeltaHeight = oldLeftHeight - oldRightHeight;
    oldDeltaYStart = SPOT_BAR_Y_START - oldLeftHeight;
  }
  // A zero delta bar is size 1
  if (oldDeltaHeight == 0)
    oldDeltaHeight = 1;

  // Get height and y-start of new delta bar
  if (newLeftHeight < newRightHeight)
  {
    newDeltaHeight = newRightHeight - newLeftHeight;
    newDeltaYStart = SPOT_BAR_Y_START - newRightHeight;
  }
  else
  {
    newDeltaHeight = newLeftHeight - newRightHeight;
    newDeltaYStart = SPOT_BAR_Y_START - newLeftHeight;
  }
  // A zero delta bar is size 1
  if (newDeltaHeight == 0)
    newDeltaHeight = 1;

  // If there are no changes in barheights there's no need to repaint the bar
  if (oldLeftHeight != newLeftHeight || oldRightHeight != newRightHeight ||
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
    glcdFillRectangle2(x, newDeltaYStart, CASC_DELTA_WIDTH, newDeltaHeight,
      align, fillType, mcFgColor);
  }

  // Paint the new bar value
  barVal = (s08)newValRight - (s08)newValLeft;
  if (barVal < 0)
  {
    barValue[0] = '-';
    barValLen++;
    barVal = -barVal;
  }
  if (barVal > 9)
  {
     barValue[barValLen] = (char)(barVal / 10 + '0');
     barValLen++;
  }
  barValue[barValLen] = (char)(barVal % 10 + '0');
  barValLen++;
  barValue[barValLen] = '\0';
  barValLen = barValLen * 4 - 1; // Width in pixels
  alignWidth = CASC_DELTA_WIDTH - barValLen;
  glcdPutStr2(x + alignWidth / 2 + (CASC_DELTA_WIDTH % 2 == 0 ? 1 : 0),
    newDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET, FONT_5X5P, barValue,
    mcFgColor);

  // Clear the left side of the bar value
  glcdFillRectangle(x, newDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET,
    alignWidth / 2 + (CASC_DELTA_WIDTH % 2 == 0 ? 1 : 0),
    -CASC_DELTA_VALUE_Y_OFFSET, mcBgColor);

  // Clear the right side of the bar value
  glcdFillRectangle(x + alignWidth / 2 + barValLen,
    newDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET, alignWidth / 2,
    -CASC_DELTA_VALUE_Y_OFFSET, mcBgColor);

  // If there are no changes in barheights there's no need to clear anything
  if (oldLeftHeight != newLeftHeight || oldRightHeight != newRightHeight)
  {
    // Clear the first line between the bar and the bar value
    glcdFillRectangle(x, newDeltaYStart - 1, CASC_DELTA_WIDTH, 1, mcBgColor);

    // Clear what was above it (if any)
    if (oldDeltaYStart < newDeltaYStart)
    {
      if (oldDeltaYStart + oldDeltaHeight < newDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET)
        clearHeight = oldDeltaHeight - CASC_DELTA_VALUE_Y_OFFSET;
      else
        clearHeight = newDeltaYStart - oldDeltaYStart;
      if (clearHeight != 0)
        glcdFillRectangle2(x, oldDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET,
          CASC_DELTA_WIDTH, clearHeight, ALIGN_AUTO, FILL_BLANK, mcFgColor);
    }

    // Clear a single line if the bars were equally high and we're moving up
    if (oldLeftHeight == oldRightHeight && newLeftHeight < newRightHeight &&
         oldLeftHeight == newLeftHeight)
      glcdFillRectangle2(x, oldDeltaYStart, CASC_DELTA_WIDTH, 1, ALIGN_AUTO,
        FILL_BLANK, mcFgColor);

    // Clear what was below it (if any)
    if (oldDeltaYStart + oldDeltaHeight > newDeltaYStart + newDeltaHeight)
    {
      if (oldDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET < newDeltaYStart + newDeltaHeight)
      {
        clearHeight = oldDeltaYStart + oldDeltaHeight -
          (newDeltaYStart + newDeltaHeight);
        clearStart = newDeltaYStart + newDeltaHeight;
      }
      else
      {
        clearHeight = oldDeltaHeight - CASC_DELTA_VALUE_Y_OFFSET;
        clearStart = oldDeltaYStart + CASC_DELTA_VALUE_Y_OFFSET;
      }
      if (clearHeight != 0)
        glcdFillRectangle2(x, clearStart, CASC_DELTA_WIDTH, clearHeight,
          ALIGN_AUTO, FILL_BLANK, mcFgColor);
    }
  }
}
