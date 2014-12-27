//*****************************************************************************
// Filename : 'bigdigit.c'
// Title    : Animation code for MONOCHRON bigdigit clock
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
#include "bigdigit.h"

// Specifics for bigdigit clock
#define BIGDIG_HMS_X_START	1
#define BIGDIG_HMS_Y_START	54
#define BIGDIG_DMY_X_START	126
#define BIGDIG_DMY_Y_START	2

#define BIGDIG_ONE_X_START	23
#define BIGDIG_ONE_Y_START	3
#define BIGDIG_ONE_X_SCALE	16
#define BIGDIG_ONE_Y_SCALE	8

#define BIGDIG_TWO_X_START	20
#define BIGDIG_TWO_Y_START	3
#define BIGDIG_TWO_X_SCALE	8
#define BIGDIG_TWO_Y_SCALE	8

#define BIGDIG_FONT_WIDTH	5
#define BIGDIG_FONT_HEIGHT	7

#define BIGDIG_ALARM_X_START	1
#define BIGDIG_ALARM_Y_START	58

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
extern volatile uint8_t mcMchronClock;
extern clockDriver_t *mcClockPool;

// Force digit draw
extern volatile uint8_t mcU8Util2;
// Active clock id
extern volatile uint8_t mcU8Util3;

// Labels for clock
char labelTime[] = "HH:MM:SS";
char labelDate[] = "DD:MM:YYYY";

// The y offsets for each of the 14 elements in time+date for the
// bigdigit clock.
// Yes, we can apply logic to calculate them at runtime but this
// cost us lots of code logic when compared to this very small array.
const unsigned char __attribute__ ((progmem)) bigdigYPos[] =
{
  /* H   X. */ BIGDIG_HMS_Y_START - 0 * (BIGDIG_FONT_WIDTH + 1),
  /* H   .X */ BIGDIG_HMS_Y_START - 1 * (BIGDIG_FONT_WIDTH + 1),
  /* M   X. */ BIGDIG_HMS_Y_START - 3 * (BIGDIG_FONT_WIDTH + 1),
  /* M   .X */ BIGDIG_HMS_Y_START - 4 * (BIGDIG_FONT_WIDTH + 1),
  /* S   X. */ BIGDIG_HMS_Y_START - 6 * (BIGDIG_FONT_WIDTH + 1),
  /* S   .X */ BIGDIG_HMS_Y_START - 7 * (BIGDIG_FONT_WIDTH + 1),
  /* D   X. */ BIGDIG_DMY_Y_START + 0 * (BIGDIG_FONT_WIDTH + 1),
  /* D   .X */ BIGDIG_DMY_Y_START + 1 * (BIGDIG_FONT_WIDTH + 1),
  /* M   X. */ BIGDIG_DMY_Y_START + 3 * (BIGDIG_FONT_WIDTH + 1),
  /* M   .X */ BIGDIG_DMY_Y_START + 4 * (BIGDIG_FONT_WIDTH + 1),
  /* Y X... */ BIGDIG_DMY_Y_START + 6 * (BIGDIG_FONT_WIDTH + 1),
  /* Y .X.. */ BIGDIG_DMY_Y_START + 7 * (BIGDIG_FONT_WIDTH + 1),
  /* Y ..X. */ BIGDIG_DMY_Y_START + 8 * (BIGDIG_FONT_WIDTH + 1),
  /* Y ...X */ BIGDIG_DMY_Y_START + 9 * (BIGDIG_FONT_WIDTH + 1)
};

// Store the item identifier per clock. This allows to re-init on the
// last active item upon re-initializing a big digit clock. You will
// appreciate it mostly when returning from the configuration menu.
uint8_t bigdigOneState = 0;
uint8_t bigdigTwoState = 0;

// Local function prototypes
void bigdigAlarmAreaUpdate(void);
void bigdigItemInvert(void);

//
// Function: bigdigButton
//
// Process pressed button for bigdigit clock
//
void bigdigButton(u08 pressedButton)
{
  // Unmark current item
  bigdigItemInvert();

  // Move to next item
  if (mcU8Util3 == CHRON_BIGDIG_ONE)
  {
    bigdigOneState = bigdigOneState + 1;
    if (bigdigOneState == (uint8_t)sizeof(bigdigYPos))
    {
      // Restart at beginning
      bigdigOneState = 0;
    }
  }
  else
  {
    bigdigTwoState = bigdigTwoState + 2;
    if (bigdigTwoState == (uint8_t)sizeof(bigdigYPos))
    {
      // Restart at beginning
      bigdigTwoState = 0;
    }
  }

  // Mark next item
  bigdigItemInvert();
}

//
// Function: bigdigCycle
//
// Update the LCD display of a bigdigit clock
//
void bigdigCycle(void)
{
  u08 oldVal = 0;
  u08 newVal = 0;
  uint8_t bigdigState;

  // Update alarm info in clock
  bigdigAlarmAreaUpdate();

  // Only if a time event or init or force (due to button press) is flagged
  // we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE &&
      mcU8Util2 == GLCD_FALSE)
    return;

  DEBUGP("Update BigDigit");

  // Get current state
  if (mcU8Util3 == CHRON_BIGDIG_ONE)
    bigdigState = bigdigOneState;
  else
    bigdigState = bigdigTwoState;

  // Get the digit(s) to update
  switch (bigdigState / 2)
  {
  case 0:
    oldVal = mcClockOldTH;
    newVal = mcClockNewTH;
    break;
  case 1:
    oldVal = mcClockOldTM;
    newVal = mcClockNewTM;
    break;
  case 2:
    oldVal = mcClockOldTS;
    newVal = mcClockNewTS;
    break;
  case 3:
    oldVal = mcClockOldDD;
    newVal = mcClockNewDD;
    break;
  case 4:
    oldVal = mcClockOldDM;
    newVal = mcClockNewDM;
    break;
  case 5:
    oldVal = 20;
    newVal = 20;
    break;
  case 6:
    oldVal = mcClockOldDY;
    newVal = mcClockNewDY;
    break;
  }
  if (mcU8Util3 == CHRON_BIGDIG_ONE)
  {
    if ((bigdigState & 0x1) == 0)
    {
      oldVal = oldVal / 10;
      newVal = newVal / 10;
    }
    else
    {
      oldVal = oldVal % 10;
      newVal = newVal % 10;
    }
  }

  // Draw digit(s) only when needed
  if (oldVal != newVal || mcClockInit == GLCD_TRUE || mcU8Util2 == GLCD_TRUE)
  {
    char digits[3];

    // Set the string to be drawn
    animValToStr(newVal, digits);

    if (mcU8Util3 == CHRON_BIGDIG_ONE)
    {
      // Draw the single big digit
      glcdPutStr3(BIGDIG_ONE_X_START, BIGDIG_ONE_Y_START, FONT_5X7N, &(digits[1]),
        BIGDIG_ONE_X_SCALE, BIGDIG_ONE_Y_SCALE, mcFgColor);
    }
    else
    {
      char *digitsPtr = digits;
      u08 addLocX = 0;

      // Check if only the right-most digit needs to be drawn; faster UI
      if (mcClockInit == GLCD_FALSE && mcU8Util2 == GLCD_FALSE && oldVal / 10 == newVal / 10)
      {
        // Draw only the right-most digit
        digitsPtr++;
        addLocX = (BIGDIG_FONT_WIDTH + 1) * BIGDIG_TWO_X_SCALE;
      }

      // Draw both big digits or only the right-most one
      glcdPutStr3(BIGDIG_TWO_X_START + addLocX, BIGDIG_TWO_Y_START, FONT_5X7N,
        digitsPtr, BIGDIG_TWO_X_SCALE, BIGDIG_TWO_Y_SCALE, mcFgColor);
    }
  }

  // Clear force flag (if set anyway)
  mcU8Util2 = GLCD_FALSE;
}

//
// Function: bigdigInit
//
// Initialize the LCD display of bigdigit clock
//
void bigdigInit(u08 mode)
{
  u08 labelLen;

  DEBUGP("Init Bigdigit");

  // Get the clockId
  mcU8Util3 = mcClockPool[mcMchronClock].clockId;

  // Draw static clock layout
  if (mode == DRAW_INIT_FULL)
  {
    // Full init so we start from scratch
    glcdClearScreen(mcBgColor);
  }
  else if (mcU8Util3 == CHRON_BIGDIG_ONE)
  {
    // Clear the most left part of the two digit area. The rest is
    // overwritten by the single digit clock
    glcdFillRectangle(BIGDIG_TWO_X_START, BIGDIG_TWO_Y_START,
      BIGDIG_ONE_X_START - BIGDIG_TWO_X_START, BIGDIG_FONT_HEIGHT * BIGDIG_TWO_Y_SCALE,
      mcBgColor);
  }

  // (Re)draw the labels. Redrawing is needed for a partial init to clear
  // an inverted clock item
  labelLen = glcdPutStr3v(BIGDIG_HMS_X_START, BIGDIG_HMS_Y_START, FONT_5X7N,
    ORI_VERTICAL_BU, labelTime, 1, 1, mcFgColor);
  if (mode == DRAW_INIT_PARTIAL)
  {
    // Clear the rim of any inverted HMS clock item
    glcdRectangle(BIGDIG_HMS_X_START - 1, BIGDIG_HMS_Y_START - labelLen,
      BIGDIG_FONT_HEIGHT + 2, labelLen + 2, mcBgColor);
  }
  labelLen = glcdPutStr3v(BIGDIG_DMY_X_START, BIGDIG_DMY_Y_START, FONT_5X7N,
    ORI_VERTICAL_TD, labelDate, 1, 1, mcFgColor);
  if (mode == DRAW_INIT_PARTIAL)
  {
    // Clear the rim of any inverted DMY clock item
    glcdRectangle(BIGDIG_DMY_X_START - BIGDIG_FONT_HEIGHT, BIGDIG_DMY_Y_START - 1,
      BIGDIG_FONT_HEIGHT + 2, labelLen + 2, mcBgColor);
  }

  // Invert the current selected item
  bigdigItemInvert();

  // Force the alarm info area to init itself
  mcAlarmSwitch = ALARM_SWITCH_NONE;
  mcU8Util1 = GLCD_FALSE;
}

//
// Function: bigdigAlarmAreaUpdate
//
// Draw update in bigdigit clock alarm area
//
void bigdigAlarmAreaUpdate(void)
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
      glcdPutStr2(BIGDIG_ALARM_X_START, BIGDIG_ALARM_Y_START, FONT_5X5P,
        msg, mcFgColor);
    }
    else
    {
      // Clear area (remove alarm time)
      glcdFillRectangle(BIGDIG_ALARM_X_START - 1, BIGDIG_ALARM_Y_START - 1,
        19, 7, mcBgColor);
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
    glcdFillRectangle2(BIGDIG_ALARM_X_START - 1, BIGDIG_ALARM_Y_START - 1,
      19, 7, ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

//
// Function: bigdigItemInvert
//
// Invert time/date item.
//
void bigdigItemInvert(void)
{
  u08 x;
  u08 y;
  u08 sizeAdd;
  uint8_t bigdigState;

  // Get pointer to current state and define extra size to (un)invert
  // per single or two digit clock
  if (mcU8Util3 == CHRON_BIGDIG_ONE)
  {
    bigdigState = bigdigOneState;
    sizeAdd = 0;
  }
  else
  {
    bigdigState = bigdigTwoState;
    sizeAdd = BIGDIG_FONT_WIDTH + 1;
  }

  // Get x and y location of item
  if (bigdigState < 6)
  {
    // HMS
    x = BIGDIG_HMS_X_START - 1;
    y = pgm_read_byte(&bigdigYPos[bigdigState]) - BIGDIG_FONT_WIDTH - sizeAdd;
  }
  else
  {
    // DMY
    x = BIGDIG_DMY_X_START - BIGDIG_FONT_HEIGHT;
    y = pgm_read_byte(&bigdigYPos[bigdigState]) - 1;
  }

  // Invert item
  glcdFillRectangle2(x, y, BIGDIG_FONT_HEIGHT + 2, BIGDIG_FONT_WIDTH + 2 + sizeAdd,
    ALIGN_AUTO, FILL_INVERSE, mcFgColor);

  // And force the digit to be drawn
  mcU8Util2 = GLCD_TRUE;
}

