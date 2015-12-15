//*****************************************************************************
// Filename : 'spiderplot.c'
// Title    : Animation code for MONOCHRON spiderplot clock
//*****************************************************************************

#include <math.h>
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
#include "spiderplot.h"

// Specifics for QV spider plot
#define SQRT2			(sqrt(2.0))
#define SINPI3			(sin(M_PI/3))
#define COSPI3			(cos(M_PI/3))
#define SPDR_AXIS_SEC		0
#define SPDR_AXIS_MIN		1
#define SPDR_AXIS_HOUR		2
#define SPDR_X_START		52
#define SPDR_Y_START		39
#define SPDR_RADIUS		22
#define SPDR_AXIS_MS_STEPS	60L
#define SPDR_AXIS_H_STEPS	24L
#define SPDR_SEC_VAL_X_START	79
#define SPDR_SEC_VAL_Y_START	33
#define SPDR_MIN_VAL_X_START	16
#define SPDR_MIN_VAL_Y_START	49
#define SPDR_HOUR_VAL_X_START	16
#define SPDR_HOUR_VAL_Y_START	17
#define SPDR_SEC_LABEL_X_START	79
#define SPDR_SEC_LABEL_Y_START	41
#define SPDR_MIN_LABEL_X_START	15
#define SPDR_MIN_LABEL_Y_START	57
#define SPDR_HOUR_LABEL_X_START	14
#define SPDR_HOUR_LABEL_Y_START 25
#define SPDR_AXIS_VAL_BEGIN	4
#define SPDR_AXIS_VAL_END	(SPDR_RADIUS)

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];

// Local function prototypes
static void spotSpiderAxisConnUpdate(u08 axisStart, u08 axisEnd, u08 valStart,
  u08 valEnd, u08 color);

//
// Function: spotSpiderPlotCycle
//
// Update the Spotfire QuintusVisuals Spider Plot and filter panel
//
void spotSpiderPlotCycle(void)
{
  char newVal[3];

  // Update alarm info in clock
  spotAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update SpiderPlot");

  // Update common parts of a Spotfire clock
  spotCommonUpdate();

  // Verify changes in time and update axis value
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
  {
    // Second
    animValToStr(mcClockNewTS, newVal);
    glcdPutStr2(SPDR_SEC_VAL_X_START, SPDR_SEC_VAL_Y_START, FONT_5X7N,
      newVal, mcFgColor);
  }
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
  {
    // Minute
    animValToStr(mcClockNewTM, newVal);
    glcdPutStr2(SPDR_MIN_VAL_X_START, SPDR_MIN_VAL_Y_START, FONT_5X7N,
      newVal, mcFgColor);
  }
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
  {
    // Hour
    animValToStr(mcClockNewTH, newVal);
    glcdPutStr2(SPDR_HOUR_VAL_X_START, SPDR_HOUR_VAL_Y_START, FONT_5X7N,
      newVal, mcFgColor);
  }

  // If only the seconds have changed verify if it impacts the Sec axis.
  // If not, then the plot remains untouched and we don't have to (re)paint
  // anything. Repainting (=remove and paint) an unchanged plot can be seen
  // on the LCD by the lines faintly dis/reappearing; we want to avoid that.
  if (mcClockNewTS != mcClockOldTS && mcClockNewTM == mcClockOldTM && 
    mcClockNewTH == mcClockOldTH && mcClockInit == GLCD_FALSE)
  {
    if ((u08)((float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / SPDR_AXIS_MS_STEPS *
         mcClockOldTS) ==
         (u08)((float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / SPDR_AXIS_MS_STEPS *
         mcClockNewTS))
      return;
  }

  // Repaint all spiderplot connector and axis lines and the inner circles

  // First remove the 'old' connector and axis lines
  spotSpiderAxisConnUpdate(SPDR_AXIS_SEC, SPDR_AXIS_MIN, mcClockOldTS, 
    mcClockOldTM, mcBgColor);
  spotSpiderAxisConnUpdate(SPDR_AXIS_MIN, SPDR_AXIS_HOUR, mcClockOldTM,
    mcClockOldTH, mcBgColor);
  spotSpiderAxisConnUpdate(SPDR_AXIS_HOUR, SPDR_AXIS_SEC, mcClockOldTH,
    mcClockOldTS, mcBgColor);

  // Then draw the 'new' connector and axis lines
  spotSpiderAxisConnUpdate(SPDR_AXIS_SEC, SPDR_AXIS_MIN, mcClockNewTS, 
    mcClockNewTM, mcFgColor);
  spotSpiderAxisConnUpdate(SPDR_AXIS_MIN, SPDR_AXIS_HOUR, mcClockNewTM,
    mcClockNewTH, mcFgColor);
  spotSpiderAxisConnUpdate(SPDR_AXIS_HOUR, SPDR_AXIS_SEC, mcClockNewTH,
    mcClockNewTS, mcFgColor);

  // Repaint the dotted inner circles at logical position 20 and 40 in
  // case they got distorted by updating the connector and axis lines
  glcdCircle2(SPDR_X_START, SPDR_Y_START,
    (u08)((float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / 3L + SPDR_AXIS_VAL_BEGIN),
    CIRCLE_THIRD, mcFgColor);
  glcdCircle2(SPDR_X_START, SPDR_Y_START,
    (u08)((float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / 3L * 2L + SPDR_AXIS_VAL_BEGIN),
    CIRCLE_HALF_E, mcFgColor);   
}

//
// Function: spotSpiderPlotInit
//
// Initialize the LCD display for use as a Spotfire QuintusVisuals Spider Plot
//
void spotSpiderPlotInit(u08 mode)
{
  DEBUGP("Init SpiderPlot");

  // Draw Spotfire form layout
  spotCommonInit("spider plot", mode);

  // Draw static part of spider plot
  glcdCircle2(SPDR_X_START, SPDR_Y_START, SPDR_RADIUS, CIRCLE_FULL, mcFgColor);
  glcdPutStr2(SPDR_SEC_LABEL_X_START, SPDR_SEC_LABEL_Y_START, FONT_5X5P,
    animSec, mcFgColor);
  glcdPutStr2(SPDR_MIN_LABEL_X_START, SPDR_MIN_LABEL_Y_START, FONT_5X5P,
    animMin, mcFgColor);
  glcdPutStr2(SPDR_HOUR_LABEL_X_START, SPDR_HOUR_LABEL_Y_START, FONT_5X5P,
    animHour, mcFgColor);
  glcdDot(SPDR_X_START, SPDR_Y_START, mcFgColor);
}

//
// Function: spotSpiderAxisConnUpdate
//
// Draw a connector line between two axis and an axis line in a Spotfire
// QuintusVisuals Spider Plot. The color parameter will either remove or add
// lines. 
//
static void spotSpiderAxisConnUpdate(u08 axisStart, u08 axisEnd, u08 valStart,
  u08 valEnd, u08 color)
{
  s08 startX, startY, endX, endY;
  float tmp;

  // Get the x/y position of the axisStart value
  if (axisStart == SPDR_AXIS_SEC || axisStart == SPDR_AXIS_MIN)
    tmp = (float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / SPDR_AXIS_MS_STEPS *
      valStart + SPDR_AXIS_VAL_BEGIN;
  else
    tmp = (float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / SPDR_AXIS_H_STEPS *
      valStart + SPDR_AXIS_VAL_BEGIN;
  if (axisStart == SPDR_AXIS_SEC)
  {
    startX = SPDR_X_START + (u08)(tmp);
    startY = SPDR_Y_START;
  }
  else
  {
    startX = SPDR_X_START - (u08)(tmp * COSPI3);
    if (axisStart == SPDR_AXIS_MIN)
    {
      startY = SPDR_Y_START + (u08)(tmp * SINPI3);
    }
    else
    {
      startY = SPDR_Y_START - (u08)(tmp * SINPI3);
    }
  }
  // Get the x/y position of the axisEnd value
  if (axisEnd == SPDR_AXIS_SEC || axisEnd == SPDR_AXIS_MIN)
    tmp = (float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / SPDR_AXIS_MS_STEPS *
      valEnd + SPDR_AXIS_VAL_BEGIN;
  else
    tmp = (float)(SPDR_AXIS_VAL_END - SPDR_AXIS_VAL_BEGIN) / SPDR_AXIS_H_STEPS *
      valEnd + SPDR_AXIS_VAL_BEGIN;
  if (axisEnd == SPDR_AXIS_SEC)
  {
    endX = SPDR_X_START + (u08)(tmp);
    endY = SPDR_Y_START;
  }
  else
  {
    endX = SPDR_X_START - (u08)(tmp * COSPI3);
    if (axisEnd == SPDR_AXIS_MIN)
    {
      endY = SPDR_Y_START + (u08)(tmp * SINPI3);
    }
    else
    {
      endY = SPDR_Y_START - (u08)(tmp * SINPI3);
    }
  }

  // Remove/add the connector line.
  // Note: two connector lines are drawn involving the Sec axis. Make sure
  // that both lines are drawn towards the Sec axis, thus showing identical
  // line pixel behavior.
  if (axisStart == SPDR_AXIS_SEC || axisStart == SPDR_AXIS_MIN)
    glcdLine(endX, endY, startX, startY, color);
  else
    glcdLine(startX, startY, endX, endY, color);

  // Remove/add the axis line
  if (axisStart == SPDR_AXIS_SEC)
    glcdLine(startX, startY, SPDR_X_START + SPDR_RADIUS, startY, color);
  else if (axisStart == SPDR_AXIS_MIN)
    glcdLine(startX, startY, SPDR_X_START - (u08)(SPDR_RADIUS * COSPI3),
      SPDR_Y_START + (u08)(SPDR_RADIUS * SINPI3), color);
  else
    glcdLine(startX, startY, SPDR_X_START - (u08)(SPDR_RADIUS * COSPI3),
      SPDR_Y_START - (u08)(SPDR_RADIUS * SINPI3), color);
}

