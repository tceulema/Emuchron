//*****************************************************************************
// Filename : 'trafficlight.c'
// Title    : Animation code for MONOCHRON traffic light clock
//*****************************************************************************

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"
#include "trafficlight.h"

// Specifics for traffic light clock
#define TRAF_BOX_X_START	9
#define TRAF_BOX_X_OFFSET_SIZE	33
#define TRAF_BOX_Y_START	18
#define TRAF_BOX_WIDTH		17
#define TRAF_BOX_LENGTH		39
#define TRAF_SEG_X_OFFSET	8
#define TRAF_SEG_Y_OFFSET	7
#define TRAF_SEG_Y_OFFSET_SIZE	12
#define TRAF_SEG_RADIUS		5

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;

// Local function prototypes
static void spotTrafSegmentUpdate(u08 x, u08 y, u08 segmentFactor, u08 oldVal,
  u08 newVal);

//
// Function: spotTrafLightCycle
//
// Update the QuintusVisuals traffic light and filter panel
//
void spotTrafLightCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == MC_FALSE)
    return;

  DEBUGP("Update TrafficLight");

  // Verify changes in sec + min + hour
  spotTrafSegmentUpdate(
    TRAF_BOX_X_START + 2 * TRAF_BOX_X_OFFSET_SIZE + TRAF_SEG_X_OFFSET,
    TRAF_BOX_Y_START + TRAF_SEG_Y_OFFSET, 20, mcClockOldTS, mcClockNewTS);
  spotTrafSegmentUpdate(
    TRAF_BOX_X_START + TRAF_BOX_X_OFFSET_SIZE + TRAF_SEG_X_OFFSET,
    TRAF_BOX_Y_START + TRAF_SEG_Y_OFFSET, 20, mcClockOldTM, mcClockNewTM);
  spotTrafSegmentUpdate(TRAF_BOX_X_START + TRAF_SEG_X_OFFSET,
    TRAF_BOX_Y_START + TRAF_SEG_Y_OFFSET, 8, mcClockOldTH, mcClockNewTH);
}

//
// Function: spotTrafLightInit
//
// Initialize the lcd display of a QuintusVisuals traffic light
//
void spotTrafLightInit(u08 mode)
{
  u08 i, j, x;

  DEBUGP("Init TrafficLight");

  // Draw Spotfire form layout
  spotCommonInit("traffic light", mode);

  // Draw static part of three traffic lights
  for (i = 0; i <= 2; i++)
  {
    x = TRAF_BOX_X_START + i * TRAF_BOX_X_OFFSET_SIZE;
    glcdRectangle(x, TRAF_BOX_Y_START, TRAF_BOX_WIDTH, TRAF_BOX_LENGTH);

    // Each traffic light has three segments
    for (j = 0; j <= 2; j++)
      glcdCircle2(x + TRAF_SEG_X_OFFSET,
        TRAF_BOX_Y_START + TRAF_SEG_Y_OFFSET + j * TRAF_SEG_Y_OFFSET_SIZE,
        TRAF_SEG_RADIUS, CIRCLE_FULL);
  }

  // Draw static axis part of trafficlight
  spotAxisInit(CHRON_TRAFLIGHT);
}

//
// Function: spotTrafSegmentUpdate
//
// Update a single traffic light
//
static void spotTrafSegmentUpdate(u08 x, u08 y, u08 segmentFactor, u08 oldVal,
  u08 newVal)
{
  u08 segmentOld, segmentNew;
  u08 fillType;

  // See if we need to update the traffic light
  segmentOld = oldVal / segmentFactor;
  segmentNew = newVal / segmentFactor;
  if (segmentOld == segmentNew && mcClockInit == MC_FALSE)
    return;

  // Clear old segment (clear content and redraw circle outline)
  glcdFillCircle2(x, y + segmentOld * TRAF_SEG_Y_OFFSET_SIZE, TRAF_SEG_RADIUS,
    FILL_BLANK);
  glcdCircle2(x, y + segmentOld * TRAF_SEG_Y_OFFSET_SIZE, TRAF_SEG_RADIUS,
    CIRCLE_FULL);

  // Define filltype and colordraw for new segment
  if (segmentNew == 0)
  {
    fillType = FILL_THIRDDOWN;
  }
  else if (segmentNew == 1)
  {
    fillType = FILL_HALF;
    if (x % 2 == 0)
      glcdColorSetBg();
  }
  else
  {
    fillType = FILL_THIRDUP;
  }

  // Fill new segment (fill content and redraw circle outline)
  glcdFillCircle2(x, y + segmentNew * TRAF_SEG_Y_OFFSET_SIZE, TRAF_SEG_RADIUS,
    fillType);
  glcdColorSetFg();
  glcdCircle2(x, y + segmentNew * TRAF_SEG_Y_OFFSET_SIZE, TRAF_SEG_RADIUS,
    CIRCLE_FULL);
}
