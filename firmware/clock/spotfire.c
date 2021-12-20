//*****************************************************************************
// Filename : 'spotfire.c'
// Title    : Generic drawing code for MONOCHRON spotfire/quintusvisuals clocks
//*****************************************************************************

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../ks0108conf.h"
#include "spotfire.h"

// Specifics for filter panel
// FP = Filter Panel
// RS = Range Slider bar
// RF = Range Filter box
#define FP_X_START 	105
#define FP_Y_START	18
#define FP_Y_OFFSET_SIZE 15
#define FP_HOUR_MAX	23
#define FP_MIN_MAX	59
#define FP_SEC_MAX	59
#define FP_RS_X_OFFSET	1
#define FP_RS_Y_OFFSET	9
#define FP_RS_WIDTH	18
#define FP_RS_HEIGHT	1
#define FP_RF_X_OFFSET	-1
#define FP_RF_Y_OFFSET	6
#define FP_RF_WIDTH	FP_RS_WIDTH + 4
#define FP_RF_HEIGHT	FP_RS_HEIGHT + 6

// Specifics for the menu bar layout
#define MBAR_TXT_LEFT	0
#define MBAR_TXT_CENTER	1

// Position of the visualization header alarm/date area
#define AD_X_START	51
#define AD_Y_START	9

// Structure defining the several layouts for a Spotfire menu bar
typedef struct
{
  u08 barText;	// Alignment: left or center
  u08 day;	// Message date day
  u08 month;	// Message date month
  char *msg1;	// First message
  char *msg2;	// Optional message added after first
} menuBarDriver_t;

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockNewDD, mcClockNewDM;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent, mcClockDateEvent;
extern volatile uint8_t mcAlarmSwitch;

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];

// The several menu bar templates we're going to use
static char barNewYear[] = "*** happy new year ***";
static char barAprFool[] = "*** happy april fool's day ***";
static char barBirthday[] = "** happy birthday ";
static char barMsgDflt[] = "FILE  EDIT  DATA  VISUALIZATIONS  VIEW";

// The menuBarDriver array defines the possible Spotfire menu bars.
// The last entry in the array is considered the default.
static menuBarDriver_t menuBarDriver[] =
{
  {MBAR_TXT_CENTER,  1,  1, barNewYear,  0},
  {MBAR_TXT_CENTER,  1,  4, barAprFool,  0},
  {MBAR_TXT_CENTER, 14,  3, barBirthday, "albert einstein **"},
  {MBAR_TXT_LEFT,    0,  0, barMsgDflt,  0}
};

// Contains the current message of the Spotfire menu bar
static u08 menuBarId;

// Local function prototypes
static void spotMenuBarUpdate(void);
static void spotRangeSliderUpdate(u08 y, u08 maxVal, u08 oldVal, u08 newVal);

//
// Function: spotAxisInit
//
// Paint x/y-axis lines and labels in a Spotfire clock
//
void spotAxisInit(u08 clockId)
{
  u08 i;
  u08 xs, xh, y;

  if (clockId == CHRON_BARCHART || clockId == CHRON_CASCADE ||
      clockId == CHRON_LINECHART)
  {
    // Draw x/y-axis lines
    glcdFillRectangle(8, 23, 1, 34);
    glcdFillRectangle(9, 56, 83, 1);

    // Draw y-axis value 10 markers
    for (i = 24; i <= 54; i = i + 5)
      glcdDot(7, i);
  }

  // Draw clock dependent things and setup coordinates for axis labels
  if (clockId == CHRON_BARCHART)
  {
    // Barchart
    glcdDot(37, 57);
    glcdDot(64, 57);
    xs = 72; xh = 16; y = 58;
  }
  else if (clockId == CHRON_CASCADE || clockId == CHRON_LINECHART)
  {
    // Cascade or linechart
    xs = 75; xh = 13; y = 58;
  }
  else if (clockId == CHRON_TRAFLIGHT)
  {
    // Trafficlight
    xs = 78; xh = 10; y = 58;
  }
  else if (clockId == CHRON_THERMOMETER)
  {
    // Thermometer
    xs = 78; xh = 10; y = 59;
  }
  else // clockId == CHRON_SPEEDDIAL || clockId == CHRON_PIECHART
  {
    // Speeddial or piechart
    xs = 78; xh = 10; y = 54;
  }

  // Draw the axis labels
  glcdPutStr2(xs, y, FONT_5X5P, animSec);
  glcdPutStr2(44, y, FONT_5X5P, animMin);
  glcdPutStr2(xh, y, FONT_5X5P, animHour);
}

//
// Function: spotBarUpdate
//
// Update a single bar (used in Spotfire bar chart and cascade)
//
void spotBarUpdate(u08 x, u08 width, u08 oldVal, u08 newVal, s08 valXOffset,
  u08 fillType)
{
  u08 oldBarHeight;
  u08 newBarHeight;
  char barValue[3];

  // See if there's any need to update a bar
  if (oldVal == newVal && mcClockInit == MC_FALSE)
    return;

  // Get height of old bar and new bar
  oldBarHeight =
    (u08)((SPOT_BAR_HEIGHT_MAX / (float)SPOT_BAR_VAL_STEPS) * oldVal + 0.5);
  newBarHeight =
    (u08)((SPOT_BAR_HEIGHT_MAX / (float)SPOT_BAR_VAL_STEPS) * newVal + 0.5);

  // If there are no changes in barheight there's no need to repaint the bar
  if (oldBarHeight != newBarHeight || mcClockInit == MC_TRUE)
  {
    // Paint new bar
    if (fillType == FILL_BLANK)
    {
      // A FILL_BLANK is in fact drawing the outline of the bar first and
      // then fill it with blank
      glcdRectangle(x, SPOT_BAR_Y_START - newBarHeight, width,
        newBarHeight + 1);
      if (newBarHeight > 1)
        glcdFillRectangle2(x + 1, SPOT_BAR_Y_START - newBarHeight + 1,
          width - 2, newBarHeight - 1, ALIGN_TOP, fillType);
    }
    else
    {
      glcdFillRectangle2(x, SPOT_BAR_Y_START - newBarHeight, width,
        newBarHeight + 1, ALIGN_BOTTOM, fillType);
    }
  }

  // Add the bar value (depending on bar value font size)
  animValToStr(newVal, barValue);
  glcdPutStr2(x + valXOffset, SPOT_BAR_Y_START - newBarHeight +
    SPOT_BAR_VAL_Y_OFFSET, FONT_5X7M, barValue);

  glcdColorSetBg();
  // Clear the first line between the bar and the bar value
  glcdFillRectangle(x, SPOT_BAR_Y_START - newBarHeight - 1, width, 1);

  // Clear the space left and right of the bar value
  glcdFillRectangle(x, SPOT_BAR_Y_START - newBarHeight + SPOT_BAR_VAL_Y_OFFSET,
    valXOffset, -SPOT_BAR_VAL_Y_OFFSET - 1);
  glcdFillRectangle(x + width - valXOffset + 1,
    SPOT_BAR_Y_START - newBarHeight + SPOT_BAR_VAL_Y_OFFSET, valXOffset - 1,
    -SPOT_BAR_VAL_Y_OFFSET - 1);

  // Clear what was above it (if any)
  if (oldBarHeight > newBarHeight)
    glcdFillRectangle(x, SPOT_BAR_Y_START - oldBarHeight +
      SPOT_BAR_VAL_Y_OFFSET, width, oldBarHeight - newBarHeight);
  glcdColorSetFg();
}

//
// Function: spotCommonInit
//
// Draw static Spotfire form visualization layout template
//
void spotCommonInit(char *label, u08 mode)
{
  u08 i;
  char *sliderLabel;

  // Either clear everything or only the chart area
  if (mode == DRAW_INIT_PARTIAL)
  {
    u08 pxDone;

    // Partial init: clear only the chart area
    glcdColorSetBg();
    glcdFillRectangle(0, 16, 100, 48);
    glcdColorSetFg();

    // Visualization title bar
    pxDone = glcdPutStr2(2, 9, FONT_5X5P, label);
    if (pxDone + 2 < AD_X_START)
    {
      glcdColorSetBg();
      glcdFillRectangle(pxDone + 2, 9, AD_X_START - pxDone - 2, 5);
      glcdColorSetFg();
    }
  }
  else
  {
    // Full init: start from scratch

    // Draw main lines for menu bar, vis title bar and filter panel
    glcdFillRectangle(0, 7, GLCD_XPIXELS, 1);
    glcdFillRectangle(0, 15, GLCD_XPIXELS, 1);
    glcdFillRectangle(101, 7, 1, GLCD_YPIXELS - 7);

    // Init the Menu bar
    menuBarId = 255;
    spotMenuBarUpdate();

    // Init the visualization title bar label
    glcdPutStr2(2, 9, FONT_5X5P, label);

    // Filter panel label
    glcdPutStr2(104, 9, FONT_5X5P, "FILTERS");

    // There are three filter sliders; hour + min + sec
    for (i = 0; i <= 2; i++)
    {
      if (i == 0)
        sliderLabel = animHour;
      else if (i == 1)
        sliderLabel = animMin;
      else
        sliderLabel = animSec;

      // Paint filter slider
      glcdPutStr2(FP_X_START, FP_Y_START + i * FP_Y_OFFSET_SIZE, FONT_5X5P,
        sliderLabel);
      glcdRectangle(FP_X_START + FP_RF_X_OFFSET,
        FP_Y_START + i * FP_Y_OFFSET_SIZE + FP_RF_Y_OFFSET, FP_RF_WIDTH,
        FP_RF_HEIGHT);
      glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET,
        FP_Y_START + i * FP_Y_OFFSET_SIZE + FP_RS_Y_OFFSET, FP_RS_WIDTH,
        FP_RS_HEIGHT);
    }
  }
}

//
// Function: spotCommonUpdate
//
// Update common parts used by all Spotfire clocks.
// Returns MC_TRUE when Spotfire clocks need to update itself.
//
u08 spotCommonUpdate(void)
{
  // Update alarm/date info in clock
  animADAreaUpdate(AD_X_START, AD_Y_START, AD_AREA_ALM_DATE);

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == MC_FALSE && mcClockInit == MC_FALSE)
    return MC_FALSE;

  // Verify changes in day and month for the menu bar
  spotMenuBarUpdate();

  // Update the filter panel range sliders
  if (mcClockNewTS != mcClockOldTS || mcClockInit == MC_TRUE)
    spotRangeSliderUpdate(FP_Y_START + 2 * FP_Y_OFFSET_SIZE, FP_SEC_MAX,
      mcClockOldTS, mcClockNewTS);
  if (mcClockNewTM != mcClockOldTM || mcClockInit == MC_TRUE)
    spotRangeSliderUpdate(FP_Y_START + FP_Y_OFFSET_SIZE, FP_MIN_MAX,
      mcClockOldTM, mcClockNewTM);
  if (mcClockNewTH != mcClockOldTH || mcClockInit == MC_TRUE)
    spotRangeSliderUpdate(FP_Y_START, FP_HOUR_MAX, mcClockOldTH,
      mcClockNewTH);

  return MC_TRUE;
}

//
// Function: spotMenuBarUpdate
//
// Put a (not so special) header in a Spotfire clock menu bar
//
static void spotMenuBarUpdate(void)
{
  // Only get a new menu bar when the date has changed or when we're
  // initializing
  if (mcClockDateEvent == MC_TRUE || mcClockInit == MC_TRUE)
  {
    u08 i;
    u08 posX;
    menuBarDriver_t *mbDriver = menuBarDriver;

    // Find the new menu bar
    for (i = 1; i < sizeof(menuBarDriver) / sizeof(menuBarDriver_t); i++)
    {
      if (mcClockNewDD == mbDriver->day && mcClockNewDM == mbDriver->month)
        break;
      mbDriver++;
    }

    // Only update the menu bar if it has changed
    if (menuBarId != i)
    {
      DEBUG(putstring("Menu bar Id -> "));
      DEBUG(uart_put_dec(i));
      DEBUG(putstring_nl(""));

      // Sync new menu bar
      menuBarId = i;

      // Get starting position on x axis
      if (mbDriver->barText == MBAR_TXT_LEFT)
      {
        // Text is to be started at left (with a small align indent)
        posX = 2;
      }
      else
      {
        // Text is to be centered
        posX = glcdGetWidthStr(FONT_5X5P, mbDriver->msg1);
        if (mbDriver->msg2 != 0)
          posX = posX + glcdGetWidthStr(FONT_5X5P, mbDriver->msg2);
        posX = (GLCD_XPIXELS - posX + 1) / 2;
      }

      // Clear the current bar
      glcdColorSetBg();
      glcdFillRectangle(0, 0, GLCD_XPIXELS, 7);
      glcdColorSetFg();

      // Print the first and optionally second msg string
      posX = posX + glcdPutStr2(posX, 1, FONT_5X5P, mbDriver->msg1);
      if (mbDriver->msg2 != 0)
        glcdPutStr2(posX, 1, FONT_5X5P, mbDriver->msg2);
    }
  }
}

//
// Function: spotRangeSliderUpdate
//
// Update a single filter panel range slider
//
static void spotRangeSliderUpdate(u08 y, u08 maxVal, u08 oldVal, u08 newVal)
{
  u08 sliderXPosOld;
  u08 sliderXPosNew;

  // Get xpos of old and new marker
  sliderXPosOld = (u08)(((FP_RS_WIDTH - 2) / (float) maxVal) * oldVal + 0.5);
  sliderXPosNew = (u08)(((FP_RS_WIDTH - 2) / (float) maxVal) * newVal + 0.5);

  // Only update if there's a need to
  if (sliderXPosOld != sliderXPosNew || mcClockInit == MC_TRUE)
  {
    // Clear range slider location marker area
    glcdColorSetBg();
    glcdRectangle(FP_X_START + FP_RS_X_OFFSET - 1, y + FP_RS_Y_OFFSET - 1,
      FP_RS_WIDTH + 2, FP_RS_HEIGHT + 2);
    glcdColorSetFg();

    // Add new range slider location markers
    glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET + sliderXPosNew,
      y + FP_RS_Y_OFFSET - 1, 2, FP_RS_HEIGHT + 2);
  }
}
