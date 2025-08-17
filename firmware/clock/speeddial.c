//*****************************************************************************
// Filename : 'speeddial.c'
// Title    : Animation code for MONOCHRON speed dial clock
//*****************************************************************************

#include <math.h>
#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"
#include "speeddial.h"

// Specifics for speed dial clock
// NDL = Needle
#define SPEED_X_START		17
#define SPEED_X_OFFSET_SIZE	33
#define SPEED_Y_START		36
#define SPEED_RADIUS		15
#define SPEED_VALUE_X_OFFSET	-5
#define SPEED_VALUE_Y_OFFSET	6
#define SPEED_MARK_RADIUS	(SPEED_RADIUS - 1.6)
#define SPEED_NDL_RADIUS	(SPEED_RADIUS - 1.6)
#define SPEED_NDL_RADIAL_STEPS	60.0
#define SPEED_NDL_RADIAL_START	(-0.75 * M_PI)
#define SPEED_NDL_RADIAL_SIZE	(1.50 * M_PI)

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;

// Local function prototypes
static void spotSpeedDialMarkerUpdate(u08 x, u08 marker);
static void spotSpeedNeedleUpdate(u08 x, u08 oldVal, u08 newVal);

//
// Function: spotSpeedDialCycle
//
// Update the QuintusVisuals speed dial and filter panel
//
void spotSpeedDialCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == MC_FALSE)
    return;

  DEBUGP("Update SpeedDial");

  // Verify changes in sec + min + hour
  spotSpeedNeedleUpdate(SPEED_X_START + 2 * SPEED_X_OFFSET_SIZE, mcClockOldTS,
    mcClockNewTS);
  spotSpeedNeedleUpdate(SPEED_X_START + SPEED_X_OFFSET_SIZE, mcClockOldTM,
    mcClockNewTM);
  spotSpeedNeedleUpdate(SPEED_X_START, mcClockOldTH, mcClockNewTH);
}

//
// Function: spotSpeedDialInit
//
// Initialize the lcd display of a QuintusVisuals speed dial
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
      SPEED_RADIUS, CIRCLE_FULL);

    // Draw speed dial markers
    for (j = 0; j < 7; j++)
      spotSpeedDialMarkerUpdate(SPEED_X_START + i * SPEED_X_OFFSET_SIZE, j);
  }

  // Draw static axis part of speeddial
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

  // See if we need to update the needle
  if (oldVal == newVal && mcClockInit == MC_FALSE)
    return;

  // Calculate changes in needle
  tmp = sin(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * oldVal +
    SPEED_NDL_RADIAL_START);
  oldDx = (s08)(tmp * SPEED_NDL_RADIUS);
  tmp = -cos(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * oldVal +
    SPEED_NDL_RADIAL_START);
  oldDy = (s08)(tmp * SPEED_NDL_RADIUS);
  tmp = sin(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * newVal +
    SPEED_NDL_RADIAL_START);
  newDx = (s08)(tmp * SPEED_NDL_RADIUS);
  tmp = -cos(SPEED_NDL_RADIAL_SIZE / SPEED_NDL_RADIAL_STEPS * newVal +
    SPEED_NDL_RADIAL_START);
  newDy = (s08)(tmp * SPEED_NDL_RADIUS);

  // Only work on the needle when it has changed
  if (oldDx != newDx || oldDy != newDy || mcClockInit == MC_TRUE)
  {
    // Remove old needle
    glcdColorSetBg();
    glcdLine(x, SPEED_Y_START, x + oldDx, SPEED_Y_START + oldDy);
    glcdColorSetFg();

    // Add new needle
    glcdLine(x, SPEED_Y_START, x + newDx, SPEED_Y_START + newDy);

    // Repaint (potentially) erased speed dial marker
    if (oldVal % 10 == 0)
      spotSpeedDialMarkerUpdate(x, oldVal / 10);
  }

  // Update speed dial value
  animValToStr(newVal, needleValue);
  glcdPutStr2(x + SPEED_VALUE_X_OFFSET, SPEED_Y_START + SPEED_VALUE_Y_OFFSET,
    FONT_5X7M, needleValue);
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
  tmp = sin(SPEED_NDL_RADIAL_SIZE / 6.0 * marker + SPEED_NDL_RADIAL_START);
  dx = (s08)(tmp * SPEED_MARK_RADIUS);
  tmp = -cos(SPEED_NDL_RADIAL_SIZE / 6.0 * marker + SPEED_NDL_RADIAL_START);
  dy = (s08)(tmp * SPEED_MARK_RADIUS);
  glcdDot(x + dx, SPEED_Y_START + dy);
}
