//*****************************************************************************
// Filename : 'speeddial.c'
// Title    : Animation code for MONOCHRON speeddial clock
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
#include "speeddial.h"

// Specifics for QV speeddial
// NDL = Needle
#define SPEED_X_START		17
#define SPEED_X_OFFSET_SIZE	33
#define SPEED_Y_START		36
#define SPEED_RADIUS		15
#define SPEED_VALUE_X_OFFSET	-5
#define SPEED_VALUE_Y_OFFSET	6
#define SPEED_NDL_RADIUS	(SPEED_RADIUS - 2)
#define SPEED_NDL_RADIAL_STEPS	60L
#define SPEED_NDL_RADIAL_START	(-0.75L * M_PI)
#define SPEED_NDL_RADIAL_SIZE	(1.50L * M_PI)

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;

// Local function prototypes
static void spotSpeedDialMarkerUpdate(u08 x, u08 marker);
static void spotSpeedNeedleUpdate(u08 x, u08 oldVal, u08 newVal);

//
// Function: spotSpeedDialCycle
//
// Update the Spotfire QuintusVisuals Speed Dial and filter panel
//
void spotSpeedDialCycle(void)
{
  // Update alarm info in clock
  spotAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update SpeedDial");

  // Update common parts of a Spotfire clock
  spotCommonUpdate();

  // Verify changes in sec
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
    spotSpeedNeedleUpdate(SPEED_X_START + 2 * SPEED_X_OFFSET_SIZE, mcClockOldTS,
      mcClockNewTS);

  // Verify changes in min
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
    spotSpeedNeedleUpdate(SPEED_X_START + SPEED_X_OFFSET_SIZE, mcClockOldTM,
      mcClockNewTM);

  // Verify changes in hour
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
    spotSpeedNeedleUpdate(SPEED_X_START, mcClockOldTH, mcClockNewTH);
}

//
// Function: spotSpeedDialInit
//
// Initialize the LCD display for use as a Spotfire QuintusVisuals Speed Dial
//
void spotSpeedDialInit(u08 mode)
{
  u08 i,j;

  DEBUGP("Init SpeedDial");

  // Draw Spotfire form layout
  spotCommonInit("speed dial", mode);

  // Draw static part of three speed dials
  for (i = 0; i <= 2; i++)
  {
    // Draw the speed dial
    glcdCircle2(SPEED_X_START + i * SPEED_X_OFFSET_SIZE, SPEED_Y_START,
      SPEED_RADIUS, CIRCLE_FULL, mcFgColor);

    // Draw speed dial markers
    for (j = 0; j < 7; j++)
    {
       spotSpeedDialMarkerUpdate(SPEED_X_START + i * SPEED_X_OFFSET_SIZE, j);
    }
  }
  spotAxisInit(CHRON_SPEEDDIAL);
}

//
// Function: spotSpeedNeedleUpdate
//
// Update a single speed dial needle
//
static void spotSpeedNeedleUpdate(u08 x, u08 oldVal, u08 newVal)
{
  s08 oldDx, newDx, oldDy, newDy;
  float tmp;
  char needleValue[3];

  // Calculate changes in needle
  tmp = sin(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * oldVal +
          SPEED_NDL_RADIAL_START);
  oldDx = (s08)(tmp * (SPEED_NDL_RADIUS + 0.4L));
  tmp = -cos(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * oldVal +
          SPEED_NDL_RADIAL_START);
  oldDy = (s08)(tmp * (SPEED_NDL_RADIUS + 0.4L));
  tmp = sin(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * newVal +
          SPEED_NDL_RADIAL_START);
  newDx = (s08)(tmp * (SPEED_NDL_RADIUS + 0.4L));
  tmp = -cos(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * newVal +
          SPEED_NDL_RADIAL_START);
  newDy = (s08)(tmp * (SPEED_NDL_RADIUS + 0.4L));

  // Only work on the needle when it has changed
  if (oldDx != newDx || oldDy != newDy || mcClockInit == GLCD_TRUE)
  {
    // Remove old needle
    glcdLine(x, SPEED_Y_START, x + oldDx, SPEED_Y_START + oldDy, mcBgColor);

    // Add new needle
    glcdLine(x, SPEED_Y_START, x + newDx, SPEED_Y_START + newDy, mcFgColor);

    // Repaint (potentially) erased speed dial marker
    if (oldVal % 10 == 0)
      spotSpeedDialMarkerUpdate(x, oldVal / 10);
  }

  // Update speed dial value
  animValToStr(newVal, needleValue);
  glcdPutStr2(x + SPEED_VALUE_X_OFFSET, SPEED_Y_START + SPEED_VALUE_Y_OFFSET,
    FONT_5X7N, needleValue, mcFgColor);
}

//
// Function: spotSpeedDialMarkerUpdate
//
// Paint a 10-minute marker in a Spotfire QuintusVisuals Speed Dial
//
static void spotSpeedDialMarkerUpdate(u08 x, u08 marker)
{
  s08 dx, dy;
  float tmp;

  // Paint 10-minute marker in speed dial
  tmp = sin(SPEED_NDL_RADIAL_SIZE / 6.00 * marker + SPEED_NDL_RADIAL_START);
  dx = (s08)(tmp * (SPEED_RADIUS - 2 + 0.4L));
  tmp = -cos(SPEED_NDL_RADIAL_SIZE / 6.00 * marker + SPEED_NDL_RADIAL_START);
  dy = (s08)(tmp * (SPEED_RADIUS - 2 + 0.4L));
  glcdDot(x + dx, SPEED_Y_START + dy, mcFgColor);
}

