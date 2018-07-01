//*****************************************************************************
// Filename : 'barchart.c'
// Title    : Animation code for MONOCHRON bar chart clock
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
#include "barchart.h"

// Specifics for bar chart clock
#define BAR_SEC_X_START		69
#define BAR_MIN_X_START		42
#define BAR_HOUR_X_START	15
#define BAR_Y_START		54
#define BAR_HEIGHT_MAX		29
#define BAR_VAL_STEPS		59
#define BAR_BAR_WIDTH		17
#define BAR_VALUE_X_OFFSET	3
#define BAR_VALUE_Y_OFFSET	-8

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;

//
// Function: spotBarChartCycle
//
// Update the Spotfire bar chart and filter panel
//
void spotBarChartCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == GLCD_FALSE)
    return;

  DEBUGP("Update BarChart");

  // Verify changes in sec
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
    spotBarUpdate(BAR_SEC_X_START, BAR_Y_START, BAR_VAL_STEPS, BAR_HEIGHT_MAX,
      BAR_BAR_WIDTH, mcClockOldTS, mcClockNewTS, BAR_VALUE_X_OFFSET,
      BAR_VALUE_Y_OFFSET, FILL_BLANK);

  // Verify changes in min
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
    spotBarUpdate(BAR_MIN_X_START, BAR_Y_START, BAR_VAL_STEPS, BAR_HEIGHT_MAX,
      BAR_BAR_WIDTH, mcClockOldTM, mcClockNewTM, BAR_VALUE_X_OFFSET,
      BAR_VALUE_Y_OFFSET, FILL_HALF);

  // Verify changes in hour
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
    spotBarUpdate(BAR_HOUR_X_START, BAR_Y_START, BAR_VAL_STEPS, BAR_HEIGHT_MAX,
      BAR_BAR_WIDTH, mcClockOldTH, mcClockNewTH, BAR_VALUE_X_OFFSET,
      BAR_VALUE_Y_OFFSET, FILL_FULL);
}

//
// Function: spotBarChartInit
//
// Initialize the lcd display of a Spotfire bar chart
//
void spotBarChartInit(u08 mode)
{
  DEBUGP("Init BarChart");

  // Draw Spotfire form layout
  spotCommonInit("bar chart", mode);

  // Draw static part of bar chart
  spotAxisInit(CHRON_BARCHART);
}

