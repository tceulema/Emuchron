//*****************************************************************************
// Filename : 'barchart.c'
// Title    : Animation code for MONOCHRON bar chart clock
//*****************************************************************************

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"
#include "barchart.h"

// Specifics for bar chart clock
#define BAR_SEC_X_START		69
#define BAR_MIN_X_START		42
#define BAR_HOUR_X_START	15
#define BAR_BAR_WIDTH		17
#define BAR_VALUE_X_OFFSET	3

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;

//
// Function: spotBarChartCycle
//
// Update the Spotfire bar chart and filter panel
//
void spotBarChartCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == MC_FALSE)
    return;

  DEBUGP("Update BarChart");

  // Verify changes in sec + min + hour
  spotBarUpdate(BAR_SEC_X_START, BAR_BAR_WIDTH, mcClockOldTS, mcClockNewTS,
    BAR_VALUE_X_OFFSET, FILL_BLANK);
  spotBarUpdate(BAR_MIN_X_START, BAR_BAR_WIDTH, mcClockOldTM, mcClockNewTM,
    BAR_VALUE_X_OFFSET, FILL_HALF);
  spotBarUpdate(BAR_HOUR_X_START, BAR_BAR_WIDTH, mcClockOldTH, mcClockNewTH,
    BAR_VALUE_X_OFFSET, FILL_FULL);
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

  // Draw static axis part of bar chart
  spotAxisInit(CHRON_BARCHART);
}
