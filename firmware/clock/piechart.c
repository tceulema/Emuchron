//*****************************************************************************
// Filename : 'piechart.c'
// Title    : Animation code for MONOCHRON pie chart clock
//*****************************************************************************

#include <math.h>
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
#include "piechart.h"

// Specifics for pie chart clock
#define PIE_SEC_X_START		83
#define PIE_MIN_X_START		50
#define PIE_HOUR_X_START	17
#define PIE_Y_START		36
#define PIE_RADIUS		15
#define PIE_LINE_RADIUS		(PIE_RADIUS - 1)
#define PIE_LINE_RADIAL_STEPS	60L
#define PIE_LINE_RADIAL_START	0L
#define PIE_LINE_RADIAL_SIZE	(2L * M_PI)
#define PIE_VALUE_X_OFFSET	-3
#define PIE_VALUE_Y_OFFSET	-2
#define PIE_VALUE_RADIUS	(PIE_RADIUS - 6)
#define PIE_VALUE_ELLIPS_Y	1.1L

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcBgColor, mcFgColor;

// Global variables to save huge function parameter lists
static u08 centerX;	// Pie center X
static s08 startX;	// Arc startpoint X relative to center
static s08 startY;	// Arc startpoint Y relative to center
static u08 startQ;	// Arc startpoint quadrant
static s08 endX;	// Arc endpoint X relative to center
static s08 endY;	// Arc endpoint Y relative to center
static u08 endQ;	// Arc endpoint quadrant

// Local function prototypes
static void pieArc(void);
static void pieArcPoint(s08 deltaX, s08 deltaY);
static void pieLineUpdate(u08 x, u08 oldVal, u08 newVal);

//
// Function: spotPieChartCycle
//
// Update the Spotfire pie chart and filter panel
//
void spotPieChartCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == GLCD_FALSE)
    return;

  DEBUGP("Update PieChart");

  // Verify changes in sec + min + hour
  pieLineUpdate(PIE_SEC_X_START, mcClockOldTS, mcClockNewTS);
  pieLineUpdate(PIE_MIN_X_START, mcClockOldTM, mcClockNewTM);
  pieLineUpdate(PIE_HOUR_X_START, mcClockOldTH, mcClockNewTH);
}

//
// Function: spotPieChartInit
//
// Initialize the lcd display of a Spotfire pie chart
//
void spotPieChartInit(u08 mode)
{
  DEBUGP("Init PieChart");

  // Draw Spotfire form layout
  spotCommonInit("pie chart", mode);

  // Draw static part of pie dial
  glcdCircle2(PIE_SEC_X_START, PIE_Y_START, PIE_RADIUS, CIRCLE_THIRD,
    mcFgColor);
  glcdCircle2(PIE_MIN_X_START, PIE_Y_START, PIE_RADIUS, CIRCLE_THIRD,
    mcFgColor);
  glcdCircle2(PIE_HOUR_X_START, PIE_Y_START, PIE_RADIUS, CIRCLE_THIRD,
    mcFgColor);

  // Draw static axis part of piechart
  spotAxisInit(CHRON_PIECHART);
}

//
// Function: pieArc
//
// Draw an arc between two circle points. It is basically the method to oaint a
// circle but with additional functionality to determine for each individual
// point whether it is to be drawn or not.
//
static void pieArc(void)
{
  s08 tswitch, y;
  s08 x = 0;
  s08 d;

  d = PIE_Y_START - centerX;
  y = PIE_RADIUS;
  tswitch = 3 - 2 * y;
  while (x <= y)
  {
    pieArcPoint(x, y);
    pieArcPoint(x, -y);
    pieArcPoint(-x, y);
    pieArcPoint(-x, -y);
    pieArcPoint(centerX - (PIE_Y_START + y - d), x);
    pieArcPoint(centerX - (PIE_Y_START + y - d), -x);
    pieArcPoint(centerX - (PIE_Y_START - y - d), x);
    pieArcPoint(centerX - (PIE_Y_START - y - d), -x);
    if (tswitch < 0)
    {
      tswitch = tswitch + 4 * x + 6;
    }
    else
    {
      tswitch = tswitch + 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

//
// Function: pieArcPoint
//
// Draw a single arc point when within a circle arc range
//
static void pieArcPoint(s08 deltaX, s08 deltaY)
{
  s08 quadrant;
  s08 drawPoint = GLCD_FALSE;
  s08 startOk = GLCD_FALSE;

  // Get the quadrant of the point to draw
  if (deltaX >= 0 && deltaY < 0)
    quadrant = 0;
  else if (deltaX > 0 && deltaY >= 0)
    quadrant = 1;
  else if (deltaX <=0 && deltaY > 0)
    quadrant = 2;
  else
    quadrant = 3;

  // Are we in the quadrant range of the arc
  if (quadrant > startQ && quadrant < endQ)
  {
    // We're guaranteed within the arc range
    drawPoint = GLCD_TRUE;
  }
  else
  {
    // Find out if the point is between the arc startpoint and endpoint.
    // We'll do this per quadrant.
    if (quadrant > startQ)
    {
      // The point quadrant is bigger than the start quadrant -> ok
      startOk = GLCD_TRUE;
    }
    else if (quadrant == startQ)
    {
      // The point must be located after the arc startpoint
      if ((((quadrant == 0 || quadrant == 3) && deltaX >= startX) ||
          ((quadrant == 1 || quadrant == 2) && deltaX <= startX)) &&
          ((quadrant < 2 && deltaY >= startY) ||
          (quadrant > 1 && deltaY <= startY)))
        startOk = GLCD_TRUE;
    }

    // It only makes sense to check the endpos if the startpos is ok
    if (startOk == GLCD_TRUE)
    {
      if (quadrant < endQ)
      {
        // The point quadrant is less than the end quadrant -> ok
        drawPoint = GLCD_TRUE;
      }
      else if (quadrant == endQ)
      {
        // The point must be located before the arc endpoint
        if ((((quadrant == 0 || quadrant == 3) && deltaX <= endX) ||
            ((quadrant == 1 || quadrant == 2) && deltaX >= endX)) &&
            ((quadrant < 2 && deltaY <= endY) ||
            (quadrant > 1 && deltaY >= endY)))
          drawPoint = GLCD_TRUE;
      }
    }
  }

  // If we're between the piechart startline and endline draw the dot
  if (drawPoint == GLCD_TRUE)
    glcdDot(centerX + deltaX, PIE_Y_START + deltaY, mcFgColor);
}

//
// Function: pieLineUpdate
//
// Update a single pie chart line
//
static void pieLineUpdate(u08 x, u08 oldVal, u08 newVal)
{
  s08 oldLineDx, newLineDx, oldLineDy, newLineDy;
  float arcLineOld, arcLineNew;
  s08 oldValDx, newValDx, oldValDy, newValDy;
  float arcValOld, arcValNew;
  char pieValue[3];

  // See if we need to update the time element
  if (oldVal == newVal && mcClockInit == GLCD_FALSE)
    return;

  // Calculate changes in pie line
  arcLineOld = PIE_LINE_RADIAL_SIZE / PIE_LINE_RADIAL_STEPS * oldVal +
    PIE_LINE_RADIAL_START;
  oldLineDx = (s08)(sin(arcLineOld) * (PIE_LINE_RADIUS + 0.5L));
  oldLineDy = (s08)(-cos(arcLineOld) * (PIE_LINE_RADIUS + 0.5L));
  arcLineNew = PIE_LINE_RADIAL_SIZE / PIE_LINE_RADIAL_STEPS * newVal +
    PIE_LINE_RADIAL_START;
  newLineDx = (s08)(sin(arcLineNew) * (PIE_LINE_RADIUS + 0.5L));
  newLineDy = (s08)(-cos(arcLineNew) * (PIE_LINE_RADIUS + 0.5L));

  // Calculate changes in pie value
  arcValOld = (arcLineOld - PIE_LINE_RADIAL_START)/2L + PIE_LINE_RADIAL_START;
  oldValDx = (s08)(sin(arcValOld) * (PIE_VALUE_RADIUS + 0.5L));
  oldValDy =
    (s08)(-cos(arcValOld) * (PIE_VALUE_RADIUS + 0.5L) * PIE_VALUE_ELLIPS_Y);
  arcValNew = (arcLineNew - PIE_LINE_RADIAL_START)/2L + PIE_LINE_RADIAL_START;
  newValDx = (s08)(sin(arcValNew) * (PIE_VALUE_RADIUS + 0.5L));
  newValDy =
    (s08)(-cos(arcValNew) * (PIE_VALUE_RADIUS + 0.5L) * PIE_VALUE_ELLIPS_Y);

  // Remove old pie line
  glcdLine(x, PIE_Y_START, x + oldLineDx, PIE_Y_START + oldLineDy, mcBgColor);

  // Remove old pie value only when its location is changed
  if (oldValDx != newValDx || oldValDy != newValDy)
    glcdFillRectangle(x + PIE_VALUE_X_OFFSET + oldValDx,
      PIE_Y_START + PIE_VALUE_Y_OFFSET + oldValDy, 7, 5, mcBgColor);

  // Clear the circle outline if needed
  if (mcClockInit == GLCD_TRUE)
  {
    arcLineOld = PIE_LINE_RADIAL_START;
  }
  else if (newVal < oldVal)
  {
    // Mostly used when we're moving from 59 or 23 to 0.
    // Reset the circle outline.
    glcdCircle2(x, PIE_Y_START, PIE_RADIUS, CIRCLE_FULL, mcBgColor);
    glcdCircle2(x, PIE_Y_START, PIE_RADIUS, CIRCLE_THIRD, mcFgColor);
    arcLineOld = PIE_LINE_RADIAL_START;
  }

  // Repaint the 0-value line since removing the old needle and pie value
  // may cause it to (partly) disappear.
  glcdLine(x, PIE_Y_START,
    x + (s08)(sin(PIE_LINE_RADIAL_START) * (PIE_RADIUS + 0.5L)),
    PIE_Y_START + (s08)(-cos(PIE_LINE_RADIAL_START) * (PIE_RADIUS + 0.5L)),
    mcFgColor);

  // Add new pie line
  glcdLine(x, PIE_Y_START, x + newLineDx, PIE_Y_START + newLineDy, mcFgColor);

  // Add new pie value
  animValToStr(newVal, pieValue);
  glcdPutStr2(x + PIE_VALUE_X_OFFSET + newValDx,
    PIE_Y_START + PIE_VALUE_Y_OFFSET + newValDy, FONT_5X5P, pieValue,
    mcFgColor);
  glcdRectangle(x + PIE_VALUE_X_OFFSET + newValDx - 1,
    PIE_Y_START + PIE_VALUE_Y_OFFSET + newValDy - 1, 9, 7, mcBgColor);

  // Update the global pie arc info that is used to draw the arc
  centerX = x;
  startX = (s08)(sin(arcLineOld) * (PIE_RADIUS + 0.5L));
  startY = (s08)(-cos(arcLineOld) * (PIE_RADIUS + 0.5L));
  startQ = (u08)(arcLineOld * 4 / PIE_LINE_RADIAL_SIZE);
  endX = (s08)(sin(arcLineNew) * (PIE_RADIUS + 0.5L));
  endY = (s08)(-cos(arcLineNew) * (PIE_RADIUS + 0.5L));
  endQ = (u08)(arcLineNew * 4 / PIE_LINE_RADIAL_SIZE);
  pieArc();
}
