//*****************************************************************************
// Filename : 'thermometer.c'
// Title    : Animation code for MONOCHRON thermometer clock
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
extern volatile uint8_t mcBgColor, mcFgColor;

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
  if (spotCommonUpdate() == GLCD_FALSE)
    return;

  DEBUGP("Update Thermometer");

  // Verify changes in sec
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
    spotThermUpdate(THERM_BOX_X_START + 2 * THERM_BOX_X_OFFSET_SIZE,
      mcClockOldTS, mcClockNewTS);

  // Verify changes in min
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
    spotThermUpdate(THERM_BOX_X_START + THERM_BOX_X_OFFSET_SIZE,
      mcClockOldTM, mcClockNewTM);

  // Verify changes in hour
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
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

  // Draw static part of thermometer.
  // There are three thermometers.
  for (i = 0; i <= 2; i++)
  {
    x = THERM_BOX_X_START + i * THERM_BOX_X_OFFSET_SIZE;
    glcdRectangle(x, THERM_BOX_Y_START, THERM_BOX_WIDTH, THERM_BOX_LENGTH,
      mcFgColor);
    glcdDot(x, THERM_BOX_Y_START, mcBgColor);
    glcdDot(x + THERM_BOX_WIDTH - 1, THERM_BOX_Y_START, mcBgColor);
    glcdFillCircle2(x + THERM_BOX_X_OFFSET_MID, THERM_BULB_Y_START,
      THERM_BULB_RADIUS, FILL_FULL, mcFgColor);
    glcdCircle2(x + THERM_BOX_X_OFFSET_MID, THERM_BULB_Y_START,
      THERM_BULB_RADIUS, CIRCLE_FULL, mcFgColor);
  }
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
  u08 color;
  u08 vDraw;
  u08 height;
  u08 i;
  char thermValue[3];

  // Get thermometer 30-step fill level of old and new value
  if (mcClockInit == GLCD_TRUE)
    fillOld = 0;
  else
    fillOld = oldVal / 2;
  fillNew = newVal / 2;

  if (fillNew < fillOld && mcClockInit == GLCD_FALSE)
  {
    // Cleanup when new value is lower
    glcdFillRectangle(x + 1, THERM_BOX_Y_START + 30 - fillOld,
      THERM_BOX_WIDTH - 2, fillOld - fillNew, mcBgColor);
  }
  else if (fillNew > fillOld || mcClockInit == GLCD_TRUE)
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
      if (i == 0 || (i == 1 && x % 2 == 1))
        color = mcBgColor;
      else
        color = mcFgColor;
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
          if (mcClockInit == GLCD_TRUE)
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
        THERM_BOX_WIDTH - 2, height, ALIGN_AUTO, fillType, color);
    }
  }

  // Paint the thermometer value
  animValToStr(newVal, thermValue);
  glcdPutStr2(x, THERM_BULB_Y_START - 2, FONT_5X5P, thermValue, mcBgColor);
}
