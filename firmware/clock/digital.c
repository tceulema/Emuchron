//*****************************************************************************
// Filename : 'digital.c'
// Title    : Animation code for MONOCHRON digital clock
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
#include "digital.h"

// Uncomment if you want to apply a 'glitch' mode to the clock.
// Refer to digiPeriodSet() for setting glitch delay and duration.
//#define DIGI_GLITCH

// Specifics for digital clock
#define DIGI_ALARM_X_START	2
#define DIGI_ALARM_Y_START	57
#define DIGI_DATE_X_START	18

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
#ifdef DIGI_GLITCH
extern volatile uint8_t mcCycleCounter;
// mcU8Util1 = glitch sleep duration (sec)
// mcU8Util2 = glitch duration (sec)
// mcU8Util3 = glitch controller 0 display off duration (sec)
// mcU8Util4 = glitch controller 1 display off duration (sec)
extern volatile uint8_t mcU8Util1, mcU8Util2, mcU8Util3, mcU8Util4;
#endif

extern char *animMonths[12];
extern char *animDays[7];

#ifdef DIGI_GLITCH
// Random value for determining the occurrence and duration of glitch
static u16 digiRandBase = M_PI * M_PI * 1000;
static const float digiRandSeed = 3.9147258617;
static u16 digiRandVal = 0xA5C3;
#endif

// The following globals control the layout of the digital clock
static u08 digiSecShow;
static u08 digiTimeXScale, digiTimeYScale;
static u08 digiTimeXStart, digiTimeYStart;
static u08 digiDateYStart;

// Local function prototypes
static void digitalInit(u08 mode);
static void digitalTimeValDraw(u08 val, u08 factor);
#ifdef DIGI_GLITCH
static void digiPeriodSet(void);
static void digiRandGet(void);
#endif

//
// Function: digitalCycle
//
// Update the lcd display of a very simple digital clock
//
void digitalCycle(void)
{
  char clockInfo[9];

  // Update alarm info in clock
  animAlarmAreaUpdate(DIGI_ALARM_X_START, DIGI_ALARM_Y_START,
    ALARM_AREA_ALM_ONLY);

#ifdef DIGI_GLITCH
  u08 i;
  u08 payload;

  // Do a clock glitch when needed and only on every two clock cycles
  if (mcU8Util1 == 0 && mcU8Util2 > 0 && (mcCycleCounter & 0x1) == 0)
  {
    // Set new controller startline
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    {
      digiRandGet();
      payload = ((digiRandVal >> 5) & 0x3f);
      glcdControlWrite(i, GLCD_START_LINE | payload);
    }
  }
#endif

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Digital");

  // Verify changes in date
  if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
      mcClockNewDY != mcClockOldDY || mcClockInit == GLCD_TRUE)
  {
    glcdPutStr2(DIGI_DATE_X_START, digiDateYStart, FONT_5X7N,
      (char *)animDays[rtcDotw(mcClockNewDM, mcClockNewDD, mcClockNewDY)],
      mcFgColor);
    glcdPutStr2(DIGI_DATE_X_START + 24, digiDateYStart, FONT_5X7N,
      (char *)animMonths[mcClockNewDM - 1], mcFgColor);
    animValToStr(mcClockNewDD, clockInfo);
    clockInfo[2] = ',';
    clockInfo[3] = ' ';
    clockInfo[4] = '2';
    clockInfo[5] = '0';
    animValToStr(mcClockNewDY, &clockInfo[6]);
    glcdPutStr2(DIGI_DATE_X_START + 48, digiDateYStart, FONT_5X7N, clockInfo,
      mcFgColor);
  }

  // Verify changes in time
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
    digitalTimeValDraw(mcClockNewTH, 0);
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
    digitalTimeValDraw(mcClockNewTM, 1);
  if (digiSecShow == GLCD_TRUE &&
      (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE))
    digitalTimeValDraw(mcClockNewTS, 2);

#ifdef DIGI_GLITCH
  // Verify glitch parameters
  if (mcU8Util1 > 0)
  {
    // Counting down for next glitch cycle
    mcU8Util1--;
  }
  else
  {
    // Counting down when glitching
    mcU8Util2--;
    if (mcU8Util2 == 0)
    {
      // Reset to normal and define new glitch cycle
      glcdResetScreen();
      digiPeriodSet();
    }
    else
    {
      // See if controller 0 needs to be turned off with 3% chance per check
      if (mcU8Util3 == 0)
      {
        digiRandGet();
        payload = (digiRandVal >> 6) % 100;
        if (payload < 3)
        {
          // Three seconds of left blank screen
          glcdControlWrite(0, GLCD_ON_CTRL | 0);
          mcU8Util3 = 3;
        }
      }
      else
      {
        mcU8Util3--;
        // Switch display back on
        if (mcU8Util3 == 0)
          glcdControlWrite(0, GLCD_ON_CTRL | GLCD_ON_DISPLAY);
      }

      // See if controller 1 needs to be turned off with 3% chance per check
      if (mcU8Util4 == 0)
      {
        digiRandGet();
        payload = (digiRandVal >> 6) % 100;
        if (payload < 3)
        {
          // Three seconds of right blank screen
          glcdControlWrite(1, GLCD_ON_CTRL | 0);
          mcU8Util4 = 3;
        }
      }
      else
      {
        mcU8Util4--;
        // Switch display back on
        if (mcU8Util4 == 0)
          glcdControlWrite(1, GLCD_ON_CTRL | GLCD_ON_DISPLAY);
      }
    }
  }
#endif
}

//
// Function: digitalHmInit
//
// Initialize the lcd display of a very simple digital clock with H+M
//
void digitalHmInit(u08 mode)
{
  // Setup the variables for the digital clock in HH:MM format
  digiSecShow = GLCD_FALSE;
  digiTimeXStart = 4;
  digiTimeYStart = 2;
  digiTimeXScale = 4;
  digiTimeYScale = 5;
  digiDateYStart = 44;

  // Do the actual initialization
  digitalInit(mode);
}

//
// Function: digitalHmsInit
//
// Initialize the lcd display of a very simple digital clock with H+M+S
//
void digitalHmsInit(u08 mode)
{
  // Setup the variables for the digital clock in HH:MM:SS format
  digiSecShow = GLCD_TRUE;
  digiTimeXStart = 16;
  digiTimeYStart = 12;
  digiTimeXScale = 2;
  digiTimeYScale = 2;
  digiDateYStart = 37;

  // Do the actual initialization
  digitalInit(mode);
}

//
// Function: digitalInit
//
// Initialize the lcd display of a very simple digital clock
//
static void digitalInit(u08 mode)
{
  DEBUGP("Init Digital");

  // Draw static clock layout
  // On partial init clear digital clock area but leave alarm area unharmed
  if (mode == DRAW_INIT_PARTIAL)
    glcdFillRectangle(0, 0, GLCD_XPIXELS, DIGI_ALARM_Y_START - 1, mcBgColor);

  // Draw the ":" separators between hour:min(:sec)
  glcdPutStr3(digiTimeXStart + 2 * 6 * digiTimeXScale + digiTimeXScale,
    digiTimeYStart, FONT_5X7N, ":", digiTimeXScale, digiTimeYScale,
    mcFgColor);
  if (digiSecShow == GLCD_TRUE)
    glcdPutStr3(digiTimeXStart + 5 * 6 * digiTimeXScale + 2 * digiTimeXScale,
      digiTimeYStart, FONT_5X7N, ":", digiTimeXScale, digiTimeYScale,
      mcFgColor);

#ifdef DIGI_GLITCH
  // Reset lcd display and init the first glitch cycle
  glcdResetScreen();
  digiPeriodSet();
#endif
}

//
// Function: digitalTimeValDraw
//
// Draw a time element (hh, mm or ss)
//
static void digitalTimeValDraw(u08 val, u08 factor)
{
  char clockInfo[3];

  animValToStr(val, clockInfo);
  glcdPutStr3(digiTimeXStart + factor * 19 * digiTimeXScale, digiTimeYStart,
    FONT_5X7N, clockInfo, digiTimeXScale, digiTimeYScale, mcFgColor);
}

//
// Function: digiPeriodSet
//
// Set the glitch sleep and glitch duration
//
#ifdef DIGI_GLITCH
static void digiPeriodSet(void)
{
  // Get random number
  digiRandGet();

  // Uncomment one the following glitch sleep periods:
  // Set glitch sleep period between a range 100-227 seconds
  mcU8Util1 = 100 + ((digiRandVal >> 4) & 0x7f);
  // Set glitch sleep period between a range 25-40 seconds
  //mcU8Util1 = 25 + ((digiRandVal >> 4) & 0x0f);

  // Uncomment one the following glitch duration periods:
  // Set glitch duration between a range 5-12 seconds
  mcU8Util2 = 5 + ((digiRandVal >> 7) & 0x07) + 1;
  // Set glitch duration between a range 10-13 seconds
  //mcU8Util2 = 10 + ((digiRandVal >> 7) & 0x03) + 1;

  // Reset display off timers
  mcU8Util3 = 0;
  mcU8Util4 = 0;
}
#endif

//
// Function: digiRandGet
//
// Generate a random number of most likely abysmal quality
//
#ifdef DIGI_GLITCH
static void digiRandGet(void)
{
  digiRandBase = (int)(digiRandSeed * (digiRandVal + mcClockNewTM) * 213);
  digiRandVal = mcCycleCounter * digiRandSeed + digiRandBase;
}
#endif

