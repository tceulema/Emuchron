//*****************************************************************************
// Filename : 'linechart.c'
// Title    : Animation code for MONOCHRON line chart clock
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
#include "linechart.h"

// Specifics for line chart clock
#define LINE_AXIS_SEC		0
#define LINE_AXIS_MIN		1
#define LINE_AXIS_HOUR		2
#define LINE_SEC_X_START	80
#define LINE_MIN_X_START	50
#define LINE_HOUR_X_START	20
#define LINE_Y_START		54
#define LINE_HEIGHT_MAX		29
#define LINE_VAL_STEPS		59
#define LINE_VALUE_X_OFFSET	-5
#define LINE_VALUE_Y_OFFSET	-8

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcBgColor, mcFgColor;

// Local function prototypes
static void spotLineUpdate(u08 axisEnd, u08 xLeft, u08 xRight, u08 oldValLeft,
  u08 newValLeft, u08 oldValRight, u08 newValRight);

//
// Function: spotLineChartCycle
//
// Update the Spotfire line chart and filter panel
//
void spotLineChartCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == GLCD_FALSE)
    return;

  DEBUGP("Update LineChart");

  // Verify changes in sec or min
  if (mcClockNewTS != mcClockOldTS || mcClockNewTM != mcClockOldTM ||
      mcClockInit == GLCD_TRUE)
  {
    // Replace line min->sec
    spotLineUpdate(LINE_AXIS_SEC, LINE_MIN_X_START, LINE_SEC_X_START,
      mcClockOldTM, mcClockNewTM, mcClockOldTS, mcClockNewTS);
  }

  // Verify changes in min or hour
  if (mcClockNewTM != mcClockOldTM || mcClockNewTH != mcClockOldTH ||
      mcClockInit == GLCD_TRUE)
  {
    // Replace line hour->min
    spotLineUpdate(LINE_AXIS_MIN, LINE_HOUR_X_START, LINE_MIN_X_START,
      mcClockOldTH, mcClockNewTH, mcClockOldTM, mcClockNewTM);
  }
}

//
// Function: spotLineChartInit
//
// Initialize the lcd display fof a Spotfire line chart
//
void spotLineChartInit(u08 mode)
{
  DEBUGP("Init LineChart");

  // Draw Spotfire form layout
  spotCommonInit("line chart", mode);

  // Draw static part of linechart
  spotAxisInit(CHRON_LINECHART);
}

//
// Function: spotLineUpdate
//
// Update a single Spotfire line chart line
//
static void spotLineUpdate(u08 axisEnd, u08 xLeft, u08 xRight, u08 oldValLeft,
  u08 newValLeft, u08 oldValRight, u08 newValRight)
{
  u08 oldLeftHeight;
  u08 newLeftHeight;
  u08 oldRightHeight;
  u08 newRightHeight;
  char lineValue[3];

  // Get height of old and new line height on left and right side
  oldLeftHeight =
    (u08)((LINE_HEIGHT_MAX / (float)LINE_VAL_STEPS) * oldValLeft + 0.5);
  newLeftHeight =
    (u08)((LINE_HEIGHT_MAX / (float)LINE_VAL_STEPS) * newValLeft + 0.5);
  oldRightHeight =
    (u08)((LINE_HEIGHT_MAX / (float)LINE_VAL_STEPS) * oldValRight + 0.5);
  newRightHeight =
    (u08)((LINE_HEIGHT_MAX / (float)LINE_VAL_STEPS) * newValRight + 0.5);

  // Check if we actually need to redraw lines
  if (oldLeftHeight != newLeftHeight || oldRightHeight != newRightHeight ||
      mcClockInit == GLCD_TRUE)
  {
    // Remove old line
    glcdLine(xLeft, LINE_Y_START - oldLeftHeight, xRight,
      LINE_Y_START - oldRightHeight, mcBgColor);
  }

  // Check if the new line will interfere with the value on the left side
  if (oldLeftHeight != newLeftHeight || mcClockInit == GLCD_TRUE)
  {
    // Remove old left line value
    glcdFillRectangle(xLeft + LINE_VALUE_X_OFFSET - 1,
      LINE_Y_START - oldLeftHeight + LINE_VALUE_Y_OFFSET - 1, 13, 9,
      mcBgColor);
  }

  // Check if the new line will interfere with the value on the right side
  if (oldRightHeight != newRightHeight || mcClockInit == GLCD_TRUE)
  {
    // Remove old right line value
    if (axisEnd == LINE_AXIS_SEC)
      glcdFillRectangle(xRight + LINE_VALUE_X_OFFSET,
        LINE_Y_START - oldRightHeight + LINE_VALUE_Y_OFFSET, 11, 7,
        mcBgColor);
  }

  // Add new line
  glcdLine(xLeft, LINE_Y_START - newLeftHeight, xRight,
    LINE_Y_START - newRightHeight, mcFgColor);

  // Add/repaint new left line value
  animValToStr(newValLeft, lineValue);
  glcdPutStr2(xLeft + LINE_VALUE_X_OFFSET,
    LINE_Y_START - newLeftHeight + LINE_VALUE_Y_OFFSET, FONT_5X7N, lineValue,
    mcFgColor);
  glcdRectangle(xLeft + LINE_VALUE_X_OFFSET - 1,
    LINE_Y_START - newLeftHeight + LINE_VALUE_Y_OFFSET - 1, 13, 9,
    mcBgColor);

  // Add/repaint new right line value
  animValToStr(newValRight, lineValue);
  glcdPutStr2(xRight + LINE_VALUE_X_OFFSET,
    LINE_Y_START - newRightHeight + LINE_VALUE_Y_OFFSET, FONT_5X7N, lineValue,
    mcFgColor);
  glcdRectangle(xRight + LINE_VALUE_X_OFFSET - 1,
    LINE_Y_START - newRightHeight + LINE_VALUE_Y_OFFSET - 1, 13, 9,
    mcBgColor);
}
