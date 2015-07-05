//*****************************************************************************
// Filename : 'spotfire.c'
// Title    : Generic drawing code for MONOCHRON Spotfire clocks
//*****************************************************************************

#include <stdlib.h>
#ifdef EMULIN
#include "../emulator/stub.h"
#endif
#ifndef EMULIN
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../ratt.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"

// Specifics for filter panel
// FP = Filter Panel
// RF = Range Filter
// RS = Range Slider
#define FP_X_START 	105
#define FP_Y_START	18
#define FP_Y_OFFSET_SIZE 15
#define FP_HOUR_MAX	23
#define FP_MIN_MAX	59
#define FP_SEC_MAX	59
#define FP_RF_X_OFFSET	-1
#define FP_RF_Y_OFFSET	6
#define FP_RF_WIDTH	22
#define FP_RF_HEIGHT	7
#define FP_RS_X_OFFSET	1
#define FP_RS_Y_OFFSET	9
#define FP_RS_WIDTH	18
#define FP_RS_HEIGHT	1

// Specifics for visualization header alarm/date info
#define VIZ_AD_X_START	50
#define VIZ_AD_Y_START	9
#define VIZ_AD_WIDTH	24

// Specifics for the menu bar layout
#define MBAR_TXT_LEFT	0
#define MBAR_TXT_CENTER	1

// Structure defining the several layouts for a Spotfire menu bar
typedef struct
{
  uint8_t barText;	// Alignment: left or center
  uint8_t day;		// Message date day
  uint8_t month;	// Message date month
  char *msg1;		// First message
  char *msg2;		// Optional message added after first
} menuBarDriver_t;

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcU8Util1;
extern volatile uint8_t mcUpdAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcBgColor, mcFgColor;
extern unsigned char *months[12];

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];

// The several menu bar templates we're going to use
char barNewYear[] = "*** happy new year ***";
char barAprFool[] = "*** happy april fool's day ***";
char barBirthday[] = "** happy birthday ";
char barMsgDflt[] = "FILE  EDIT  VIEW  INSERT  TOOLS  HELP";

// The menuBarDriver array defines the possible Spotfire menu bars.
// The last entry in the array is considered the default.
menuBarDriver_t menuBarDriver[] =
{
  {MBAR_TXT_CENTER,  1,  1, barNewYear,  0},
  {MBAR_TXT_CENTER,  1,  4, barAprFool,  0},
  {MBAR_TXT_CENTER, 14,  3, barBirthday, "albert einstein **"},
  {MBAR_TXT_LEFT,    0,  0, barMsgDflt,  0}
};

// Contains the current message of the Spotfire menu bar
u08 menuBarId;

// Local function prototypes
void spotMenuBarUpdate(void);
void spotRangeSliderUpdate(u08 y, u08 maxVal, u08 oldVal, u08 newVal);

//
// Function: spotAlarmAreaUpdate
//
// Draw update in a Spotfire clock alarm area
//
void spotAlarmAreaUpdate(void)
{
  u08 inverseAlarmArea = GLCD_FALSE;
  u08 newAlmDisplayState = GLCD_FALSE;
  u08 pxDone = 0;
  char msg[7];

  if ((mcCycleCounter & 0x0F) >= 8)
    newAlmDisplayState = GLCD_TRUE;
    
  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    msg[0] = ' ';
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Show alarm time in visualization header
      animValToStr(mcAlarmH, &(msg[1]));
      msg[3] = ':';
      animValToStr(mcAlarmM, &(msg[4]));
      pxDone = glcdPutStr2(VIZ_AD_X_START, VIZ_AD_Y_START, FONT_5X5P, msg, mcFgColor);
    }
    else
    {
      // Prior to showing the date clear border of inverse alarm area (if needed)
      if (mcU8Util1 == GLCD_TRUE)
      {
        glcdRectangle(VIZ_AD_X_START + 1, VIZ_AD_Y_START - 1, 19, 7, mcBgColor);
        mcU8Util1 = GLCD_FALSE;
      }

      // Draw date
      pxDone = glcdPutStr2(VIZ_AD_X_START, VIZ_AD_Y_START, FONT_5X5P,
        (char *)months[mcClockNewDM - 1], mcFgColor) + VIZ_AD_X_START;
      animValToStr(mcClockNewDD, &(msg[1]));
      pxDone = pxDone + glcdPutStr2(pxDone, VIZ_AD_Y_START, FONT_5X5P, msg, mcFgColor) -
        VIZ_AD_X_START;
    }

    // Clean up any trailing remnants of previous text
    if (pxDone < VIZ_AD_WIDTH)
      glcdFillRectangle(VIZ_AD_X_START + pxDone, VIZ_AD_Y_START,
        VIZ_AD_WIDTH - pxDone, 5, mcBgColor);
  }

  if (mcAlarming == GLCD_TRUE)
  {
    // Blink alarm area when we're alarming or snoozing
    if (newAlmDisplayState != mcU8Util1)
    {
      inverseAlarmArea = GLCD_TRUE;
      mcU8Util1 = newAlmDisplayState;
    }
  }
  else
  {
    // Reset inversed alarm area when alarming has stopped
    if (mcU8Util1 == GLCD_TRUE)
    {
      inverseAlarmArea = GLCD_TRUE;
      mcU8Util1 = GLCD_FALSE;
    }
  }

  // Inverse the alarm area if needed
  if (inverseAlarmArea == GLCD_TRUE)
    glcdFillRectangle2(VIZ_AD_X_START + 1, VIZ_AD_Y_START - 1, 19, 7,
      ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

//
// Function: spotAxisInit
//
// Paint x/y-axis lines and labels in a Spotfire clock
//
void spotAxisInit(u08 clockId)
{
  u08 i;
  u08 xs, xh, y;

  if (clockId == CHRON_CASCADE)
  {
    // Draw x/y-axis lines
    glcdFillRectangle(8, 23, 1, 34, mcFgColor);
    glcdFillRectangle(9, 56, 83, 1, mcFgColor);

    // Draw y-axis value 10 markers
    for (i = 24; i <= 54; i = i + 5)
    {
      glcdDot(7, i, mcFgColor);
    }
  }

  // Draw clock dependent things and setup coordinates for axis labels
  if (clockId == CHRON_CASCADE)
  {
    // Cascade
    xs = 75; xh = 13; y = 58;
  }
  else if (clockId == CHRON_TRAFLIGHT)
  {
    // Trafficlight
    xs = 78; xh = 10; y = 58;
  }
  else // clockId == CHRON_SPEEDDIAL
  {
    // Speeddial
    xs = 78; xh = 10; y = 54;
  }

  // Draw the axis labels
  glcdPutStr2(xs, y, FONT_5X5P, animSec, mcFgColor);
  glcdPutStr2(44, y, FONT_5X5P, animMin, mcFgColor);
  glcdPutStr2(xh, y, FONT_5X5P, animHour, mcFgColor);
}

//
// Function: spotBarUpdate
//
// Update a single bar (used in Spotfire bar chart and cascade)
//
void spotBarUpdate(u08 x, u08 y, u08 maxVal, u08 maxHeight, u08 width,
  u08 oldVal, u08 newVal, s08 valXOffset, s08 valYOffset, u08 fillType)
{
  u08 oldBarHeight;
  u08 newBarHeight;
  char barValue[3];
  
  // Get height of old bar and new bar
  oldBarHeight = (u08)((maxHeight / (float)maxVal) * oldVal + 0.5);
  newBarHeight = (u08)((maxHeight / (float)maxVal) * newVal + 0.5);

  // If there are no changes in barheight there's no need to repaint the bar
  if (oldBarHeight != newBarHeight || mcClockInit == GLCD_TRUE)
  {
    // Paint new bar
    if (fillType == FILL_BLANK)
    {
      // A FILL_BLANK is in fact drawing the outline of the bar first and
      // then fill it with blank
      glcdRectangle(x, y - newBarHeight, width, newBarHeight + 1, mcFgColor);
      if (newBarHeight > 1)
        glcdFillRectangle2(x + 1, y - newBarHeight + 1, width - 2, newBarHeight - 1,
          ALIGN_TOP, fillType, mcFgColor);
    }
    else
    {
      glcdFillRectangle2(x, y - newBarHeight, width, newBarHeight + 1, ALIGN_BOTTOM,
        fillType, mcFgColor);
    }
  }

  // Add the bar value (depending on bar value font size)
  animValToStr(newVal, barValue);
  glcdPutStr2(x + valXOffset, y - newBarHeight + valYOffset, FONT_5X7N, barValue,
    mcFgColor);

  // Clear the first line between the bar and the bar value
  glcdFillRectangle(x, y - newBarHeight - 1, width, 1, mcBgColor);

  // Clear the space left and right of the bar value
  glcdFillRectangle(x, y - newBarHeight + valYOffset, valXOffset, -valYOffset - 1,
    mcBgColor);
  glcdFillRectangle(x + width - valXOffset + 1, y - newBarHeight + valYOffset,
    valXOffset - 1, -valYOffset - 1, mcBgColor);

  // Clear what was above it (if any)
  if (oldBarHeight > newBarHeight)
  {
    glcdFillRectangle(x, y - oldBarHeight + valYOffset, width,
      oldBarHeight - newBarHeight, mcBgColor);
  }
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
    glcdFillRectangle(0, 16, 100, 47, mcBgColor);

    // Visualization title bar
    pxDone = glcdPutStr2(2, 9, FONT_5X5P, label, mcFgColor);
    if (pxDone + 2 < VIZ_AD_X_START)
      glcdFillRectangle(pxDone + 2, 9, VIZ_AD_X_START - pxDone - 2, 5, mcBgColor);
  }
  else
  {
    // Full init: start from scratch
    glcdClearScreen(mcBgColor);

    // Draw main lines for menu bar, vis title bar and filter panel
    glcdFillRectangle(0, 7, GLCD_XPIXELS, 1, mcFgColor);
    glcdFillRectangle(0, 15, GLCD_XPIXELS, 1, mcFgColor);
    glcdFillRectangle(101, 7, 1, GLCD_YPIXELS - 7, mcFgColor);

    // Init the Menu bar
    menuBarId = -1;
    spotMenuBarUpdate();

    // Init the visualization Title bar label and icons
    glcdPutStr2(2, 9, FONT_5X5P, label, mcFgColor);
    glcdPutStr2(86, 9, FONT_5X5P, "@&*", mcFgColor);

    // Filter panel label
    glcdPutStr2(104, 9, FONT_5X5P, "FILTERS", mcFgColor);

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
        sliderLabel, mcFgColor);
      glcdRectangle(FP_X_START + FP_RF_X_OFFSET,
        FP_Y_START + i * FP_Y_OFFSET_SIZE + FP_RF_Y_OFFSET,
        FP_RF_WIDTH, FP_RF_HEIGHT, mcFgColor);
      glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET,
        FP_Y_START + i * FP_Y_OFFSET_SIZE + FP_RS_Y_OFFSET,
        FP_RS_WIDTH, FP_RS_HEIGHT, mcFgColor);
    }

    // Force the alarm info area to init itself in the Title bar
    mcAlarmSwitch = ALARM_SWITCH_NONE;
    mcU8Util1 = GLCD_FALSE;
  }
}

//
// Function: spotCommonUpdate
//
// Update common parts used by all Spotfire clocks
//
void spotCommonUpdate(void)
{
  // Verify changes in day and month for the menu bar
  spotMenuBarUpdate();

  // Update the filter panel range sliders
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
    spotRangeSliderUpdate(FP_Y_START + 2 * FP_Y_OFFSET_SIZE, FP_SEC_MAX, mcClockOldTS,
      mcClockNewTS);
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
    spotRangeSliderUpdate(FP_Y_START + FP_Y_OFFSET_SIZE, FP_MIN_MAX, mcClockOldTM,
      mcClockNewTM);
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
    spotRangeSliderUpdate(FP_Y_START, FP_HOUR_MAX, mcClockOldTH,
      mcClockNewTH);
}

//
// Function: spotMenuBarUpdate
//
// Put a (not so special) header in a Spotfire clock menu bar
//
void spotMenuBarUpdate(void)
{
  // Only get a new menu bar when the date has changed or when we're initializing
  if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
      mcClockInit == GLCD_TRUE)
  {
    uint8_t i = 1;
    uint8_t posX;
    menuBarDriver_t *mbDriver = menuBarDriver;

    // Find the new menu bar
    while (i < sizeof(menuBarDriver) / sizeof(menuBarDriver_t))
    {
      if (mcClockNewDD == mbDriver->day &&
          mcClockNewDM == mbDriver->month)
      {
        break;
      }
      i++;
      mbDriver++;
    }

    // Only update the menu bar if it has changed
    if (menuBarId != i)
    {
      DEBUG(putstring("Menu bar Id -> "));
      DEBUG(uart_putw_dec(i));
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
      glcdFillRectangle(0, 0, GLCD_XPIXELS, 7, mcBgColor);

      // Print the first and optionally second msg string
      posX = posX + glcdPutStr2(posX, 1, FONT_5X5P, mbDriver->msg1, mcFgColor);
      if (mbDriver->msg2 != 0)
        glcdPutStr2(posX, 1, FONT_5X5P, mbDriver->msg2, mcFgColor);
    }
  }
}

//
// Function: spotRangeSliderUpdate
//
// Update a single filter panel range slider
//
void spotRangeSliderUpdate(u08 y, u08 maxVal, u08 oldVal, u08 newVal)
{
  u08 sliderXPosOld;
  u08 sliderXPosNew;

  // Get xpos of old and new marker
  sliderXPosOld = (u08)(((FP_RS_WIDTH - 2) / (float) maxVal) * oldVal + 0.5);
  sliderXPosNew = (u08)(((FP_RS_WIDTH - 2) / (float) maxVal) * newVal + 0.5);

  // Only update if there's a need to
  if (sliderXPosOld != sliderXPosNew || mcClockInit == GLCD_TRUE)
  {
    // Remove old range slider location marker
    glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET + sliderXPosOld,
      y + FP_RS_Y_OFFSET - 1, 2, 1, mcBgColor);
    glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET + sliderXPosOld,
      y + FP_RS_Y_OFFSET + 1, 2, 1, mcBgColor);

    // Add new range slider location marker
    glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET + sliderXPosNew,
      y + FP_RS_Y_OFFSET - 1, 2, 1, mcFgColor);
    glcdFillRectangle(FP_X_START + FP_RS_X_OFFSET + sliderXPosNew,
      y + FP_RS_Y_OFFSET + 1, 2, 1, mcFgColor);
  }
}

