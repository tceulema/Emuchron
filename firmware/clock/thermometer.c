//*****************************************************************************
// Filename : 'thermometer.c'
// Title    : Animation code for MONOCHRON thermometer clock
//*****************************************************************************

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"
#include "thermometer.h"

// Specifics for thermometer clock
#define THERM_BOX_X_START	14
#define THERM_BOX_X_OFFSET_SIZE	33
#define THERM_BOX_X_OFFSET_MID	3
#define THERM_BOX_Y_START	17
#define THERM_BOX_WIDTH		7
#define THERM_BOX_LENGTH	31
#define THERM_BULB_Y_START	52
#define THERM_BULB_RADIUS	5

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;

// Local function prototypes
static void spotThermUpdate(u08 x, u08 oldVal, u08 newVal);

//
// Function: spotThermCycle
//
// Update the QuintusVisuals thermometer and filter panel
//
void spotThermCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == MC_FALSE)
    return;

  DEBUGP("Update Thermometer");

  // Verify changes in sec + min + hour
  spotThermUpdate(THERM_BOX_X_START + 2 * THERM_BOX_X_OFFSET_SIZE,
    mcClockOldTS, mcClockNewTS);
  spotThermUpdate(THERM_BOX_X_START + THERM_BOX_X_OFFSET_SIZE,
    mcClockOldTM, mcClockNewTM);
  spotThermUpdate(THERM_BOX_X_START, mcClockOldTH, mcClockNewTH);
}

//
// Function: spotThermInit
//
// Initialize the lcd display of a QuintusVisuals thermometer
//
void spotThermInit(u08 mode)
{
  u08 i, x;

  DEBUGP("Init Thermometer");

  // Draw Spotfire form layout
  spotCommonInit("thermometer", mode);

  // Draw static part of three thermometers
  for (i = 0; i <= 2; i++)
  {
    x = THERM_BOX_X_START + i * THERM_BOX_X_OFFSET_SIZE;
    glcdRectangle(x, THERM_BOX_Y_START, THERM_BOX_WIDTH, THERM_BOX_LENGTH);
    glcdColorSetBg();
    glcdDot(x, THERM_BOX_Y_START);
    glcdDot(x + THERM_BOX_WIDTH - 1, THERM_BOX_Y_START);
    glcdColorSetFg();
    glcdFillCircle2(x + THERM_BOX_X_OFFSET_MID, THERM_BULB_Y_START,
      THERM_BULB_RADIUS, FILL_FULL);
    glcdCircle2(x + THERM_BOX_X_OFFSET_MID, THERM_BULB_Y_START,
      THERM_BULB_RADIUS, CIRCLE_FULL);
  }

  // Draw static axis part of thermometer
  spotAxisInit(CHRON_THERMOMETER);
}

//
// Function: spotThermUpdate
//
// Update a single thermometer
//
static void spotThermUpdate(u08 x, u08 oldVal, u08 newVal)
{
  u08 segmentOld, segmentNew;
  u08 fillOld, fillNew;
  u08 fillType;
  u08 vDraw;
  u08 height;
  u08 i;
  char thermValue[3];

  // See if we need to update the needle
  if (oldVal == newVal && mcClockInit == MC_FALSE)
    return;

  // Get thermometer 30-step fill level of old and new value
  if (mcClockInit == MC_TRUE)
    fillOld = 0;
  else
    fillOld = oldVal / 2;
  fillNew = newVal / 2;

  if (fillNew < fillOld && mcClockInit == MC_FALSE)
  {
    // Cleanup when new value is lower
    glcdColorSetBg();
    glcdFillRectangle(x + 1, THERM_BOX_Y_START + 30 - fillOld,
      THERM_BOX_WIDTH - 2, fillOld - fillNew);
    glcdColorSetFg();
  }
  else if (fillNew > fillOld || mcClockInit == MC_TRUE)
  {
    // A single thermometer is painted in three segments with decreasing
    // fill intensity. Determine what to draw per segment.
    segmentOld = fillOld / 10;
    segmentNew = fillNew / 10;
    for (i = segmentOld; i <= segmentNew; i++)
    {
      // Do not rebuild top old segment when it is already full
      if (i == segmentOld && fillOld % 10 == 9)
        continue;

      // Determine how to draw
      if (i == 0 || (i == 1 && (x & 0x1) == 1))
        glcdColorSetBg();
      else
        glcdColorSetFg();
      if (i == 1)
        fillType = FILL_HALF;
      else
        fillType = FILL_THIRDDOWN;

      // Determine how much to draw
      if (i != segmentNew)
      {
        // Draw full segment between old and new value
        vDraw = 10;
        height = 10;
      }
      else
      {
        vDraw = fillNew % 10 + 1;
        if (segmentOld == segmentNew)
        {
          // Draw delta between old and new value in segment
          height = fillNew - fillOld;
          if (mcClockInit == MC_TRUE)
            height++;
        }
        else
        {
          // Draw new value in segment only in use by new value
          height = vDraw;
        }
      }

      // (Re)draw segment
      glcdFillRectangle2(x + 1,
        THERM_BOX_Y_START + 1 + 20 - 10 * i + (10 - vDraw),
        THERM_BOX_WIDTH - 2, height, ALIGN_AUTO, fillType);
    }
  }

  // Paint the thermometer value
  glcdColorSetBg();
  animValToStr(newVal, thermValue);
  glcdPutStr2(x, THERM_BULB_Y_START - 2, FONT_5X5P, thermValue);
  glcdColorSetFg();
}
