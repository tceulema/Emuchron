//*****************************************************************************
// Filename : 'digital.c'
// Title    : Animation code for MONOCHRON digital clock
//*****************************************************************************

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
#include "digital.h"

// Specifics for digital clock
#define DIGI_ALARM_X_START	2
#define DIGI_ALARM_Y_START	57

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
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;

extern unsigned char *months[12];
extern unsigned char *days[7];

// The following globals control the layout of the digital clock
u08 digiSecShow;
u08 digiTimeXScale, digiTimeYScale;
u08 digiTimeXStart, digiTimeYStart;
u08 digiDateXStart, digiDateYStart;

// Local function prototypes
void digitalInit(u08 mode);
void digitalAlarmAreaUpdate(void);

//
// Function: digitalCycle
//
// Update the LCD display of a very simple digital clock
//
void digitalCycle(void)
{
  char clockInfo[9];

  // Update alarm info in clock
  digitalAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Digital");

  // Verify changes in date
  if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
       mcClockNewDY != mcClockOldDY || mcClockInit == GLCD_TRUE)
  {
    glcdPutStr2(digiDateXStart, digiDateYStart, FONT_5X7N,
      (char *)days[dotw(mcClockNewDM, mcClockNewDD, mcClockNewDY)], mcFgColor);
    glcdPutStr2(digiDateXStart + 24, digiDateYStart, FONT_5X7N,
      (char *)months[mcClockNewDM - 1], mcFgColor);
    animValToStr(mcClockNewDD, clockInfo);
    clockInfo[2] = ',';
    clockInfo[3] = ' ';
    clockInfo[4] = '2';
    clockInfo[5] = '0';
    animValToStr(mcClockNewDY, &(clockInfo[6]));
    glcdPutStr2(digiDateXStart + 48, digiDateYStart, FONT_5X7N, clockInfo,
      mcFgColor);
  }

  // Verify changes in time
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
  {
    animValToStr(mcClockNewTH, clockInfo);
    glcdPutStr3(digiTimeXStart, digiTimeYStart, FONT_5X7N, clockInfo,
      digiTimeXScale, digiTimeYScale, mcFgColor);
  }
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
  {
    animValToStr(mcClockNewTM, clockInfo);
    glcdPutStr3(digiTimeXStart + 3 * 6 * digiTimeXScale + digiTimeXScale,
      digiTimeYStart, FONT_5X7N, clockInfo, digiTimeXScale, digiTimeYScale,
      mcFgColor);
  }
  if (digiSecShow == GLCD_TRUE &&
      (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE))
  {
    animValToStr(mcClockNewTS, clockInfo);
    glcdPutStr3(digiTimeXStart + 6 * 6 * digiTimeXScale + 2 * digiTimeXScale,
      digiTimeYStart, FONT_5X7N, clockInfo, digiTimeXScale, digiTimeYScale,
      mcFgColor);
  }
}

//
// Function: digitalHmInit
//
// Initialize the LCD display of a very simple digital clock with H+M
//
void digitalHmInit(u08 mode)
{
  // Setup the variables for the digital clock
  digiSecShow = GLCD_FALSE;
  digiTimeXStart = 4;
  digiTimeYStart = 2;
  digiTimeXScale = 4;
  digiTimeYScale = 5;
  digiDateXStart = 18;
  digiDateYStart = 44;

  // Do the actual initialization
  digitalInit(mode);
}

//
// Function: digitalHmsInit
//
// Initialize the LCD display of a very simple digital clock with H+M+S
//
void digitalHmsInit(u08 mode)
{
  // Setup the variables for the digital clock
  digiSecShow = GLCD_TRUE;
  digiTimeXStart = 16;
  digiTimeYStart = 12;
  digiTimeXScale = 2;
  digiTimeYScale = 2;
  digiDateXStart = 18;
  digiDateYStart = 37;

  // Do the actual initialization
  digitalInit(mode);
}

//
// Function: digitalInit
//
// Initialize the LCD display of a very simple digital clock
//
void digitalInit(u08 mode)
{
  DEBUGP("Init Digital");

  // Draw static clock layout
  if (mode == DRAW_INIT_FULL)
  {
    // Start from scratch
    glcdClearScreen(mcBgColor);
  }
  else
  {
    // Clear area with digital clock and leave alarm area unharmed
    glcdFillRectangle(0, 0, GLCD_XPIXELS, DIGI_ALARM_Y_START - 1, mcBgColor);
  }

  // Draw the ":" separators between hour:min(:sec)
  glcdPutStr3(digiTimeXStart + 2 * 6 * digiTimeXScale + digiTimeXScale,
    digiTimeYStart, FONT_5X7N, ":", digiTimeXScale, digiTimeYScale,
    mcFgColor);
  if (digiSecShow == GLCD_TRUE)
    glcdPutStr3(digiTimeXStart + 5 * 6 * digiTimeXScale + 2 * digiTimeXScale,
      digiTimeYStart, FONT_5X7N, ":", digiTimeXScale, digiTimeYScale,
      mcFgColor);
  
  if (mode == DRAW_INIT_FULL)
  {
    // Force the alarm info area to init itself
    mcAlarmSwitch = ALARM_SWITCH_NONE;
    mcU8Util1 = GLCD_FALSE;
  }
}

//
// Function: digitalAlarmAreaUpdate
//
// Draw update in digital clock alarm area
//
void digitalAlarmAreaUpdate(void)
{
  u08 inverseAlarmArea = GLCD_FALSE;
  u08 newAlmDisplayState = GLCD_FALSE;
  char msg[6];

  if ((mcCycleCounter & 0x0F) >= 8)
    newAlmDisplayState = GLCD_TRUE;

  if (mcUpdAlarmSwitch == GLCD_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Show alarm time
      animValToStr(mcAlarmH, msg);
      msg[2] = ':';
      animValToStr(mcAlarmM, &(msg[3]));
      glcdPutStr2(DIGI_ALARM_X_START, DIGI_ALARM_Y_START, FONT_5X5P, msg, mcFgColor);
    }
    else
    {
      // Clear area (remove alarm time)
      glcdFillRectangle(DIGI_ALARM_X_START - 1, DIGI_ALARM_Y_START - 1, 19, 7,
        mcBgColor);
      mcU8Util1 = GLCD_FALSE;
    }
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
    glcdFillRectangle2(DIGI_ALARM_X_START - 1, DIGI_ALARM_Y_START - 1, 19, 7,
      ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

