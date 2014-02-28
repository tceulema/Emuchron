//*****************************************************************************
// Filename : 'puzzle.c'
// Title    : Animation code for MONOCHRON puzzle clock
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
#include "puzzle.h"

// Specifics for puzzle clock
#define PUZZLE_MODE_CLOCK	0
#define PUZZLE_MODE_HELP	1

#define PUZZLE_NUMBER_X_START	12
#define PUZZLE_NUMBER_Y_START	1

#define PUZZLE_BULB_X_START	13
#define PUZZLE_BULB_Y_START	13
#define PUZZLE_BULB_RADIUS	5

#define PUZZLE_LBL_X_START	1
#define PUZZLE_LBL_TIME_Y_START	26
#define PUZZLE_LBL_DATE_Y_START	49

#define PUZZLE_ALARM_X_START	1
#define PUZZLE_ALARM_Y_START	58

#define PUZZLE_HELP_TIMEOUT	46
#define PUZZLE_HELP_LEFT_X	14
#define PUZZLE_HELP_RIGHT_X	70
#define PUZZLE_HELP_TEXT_OFFSET	11

typedef struct _bulbDriver_t
{
  u08 colorCode;	// draw color: 0 = foreground, 1 = background
  u08 fillType;		// circle fill type
} bulbDriver_t;

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
extern char animHour[];
extern char animMin[];
extern char animSec[];
extern char animDay[];
extern char animMonth[];
extern char animYear[];
// Display timer for help page
extern volatile uint8_t mcU8Util2;
// Display mode for clock
extern volatile uint8_t mcU8Util3;

// For each of the eight permutations of a bulb value specify the
// parameters for the fill circle draw
bulbDriver_t bulbDriver[] =
{
  { 0, FILL_BLANK },
  { 0, FILL_THIRDUP },
  { 0, FILL_THIRDDOWN },
  { 0, FILL_HALF },
  { 1, FILL_HALF },
  { 1, FILL_THIRDDOWN },
  { 1, FILL_THIRDUP },
  { 0, FILL_FULL }
};

// Local function prototypes
void puzzleAlarmAreaUpdate(void);
void puzzleBulbRowSet(u08 y, u08 oldVal1, u08 oldVal2, u08 oldVal3,
    u08 newVal1, u08 newVal2, u08 newVal3, u08 high, u08 init);
void puzzleHelp(void);

//
// Function: puzzleButton
//
// Process pressed button for puzzle clock
//
void puzzleButton(u08 pressedButton)
{
  // Provide help page or switch back to clock
  if (mcU8Util3 == PUZZLE_MODE_CLOCK)
  {
    // Provide help page
    DEBUGP("Clock -> Help");
    mcU8Util2 = PUZZLE_HELP_TIMEOUT;
    mcU8Util3 = PUZZLE_MODE_HELP;
    puzzleHelp();
  }
  else
  {
    // The mode change is processed in puzzleCycle()
    DEBUGP("Help -> Clock");
    mcU8Util3 = PUZZLE_MODE_CLOCK;
  }
}

//
// Function: puzzleCycle
//
// Update the LCD display of a puzzle clock
//
void puzzleCycle(void)
{
  if ((mcU8Util2 == 1 && mcClockTimeEvent == GLCD_TRUE) ||
      (mcU8Util2 > 0 && mcU8Util3 == PUZZLE_MODE_CLOCK))
  {
    // Switch back from help page to clock
    animClockDraw(DRAW_INIT_FULL);
  } 
  else if (mcU8Util3 == PUZZLE_MODE_HELP)
  {
    // We're in help mode so no screen updates, but decrease
    // helppage timeout counter when appropriate
    if (mcClockTimeEvent == GLCD_TRUE)
    {
      char counter[3];

      mcU8Util2--;
      animValToStr(mcU8Util2, counter);
      glcdPutStr2(120,  1, FONT_5X5P, counter, mcFgColor);
    }
    return;
  }

  // Update alarm info in clock
  puzzleAlarmAreaUpdate();

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Puzzle");

  // Time high part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START, mcClockOldTS, mcClockOldTM,
    mcClockOldTH, mcClockNewTS, mcClockNewTM, mcClockNewTH,
    GLCD_TRUE, mcClockInit);
  // Time low part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START + 12, mcClockOldTS, mcClockOldTM,
    mcClockOldTH, mcClockNewTS, mcClockNewTM, mcClockNewTH,
    GLCD_FALSE, mcClockInit);
  // Date high part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START + 24, mcClockOldDD, mcClockOldDM,
    mcClockOldDY, mcClockNewDD, mcClockNewDM, mcClockNewDY,
    GLCD_TRUE, mcClockInit);
  // Date low part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START + 36, mcClockOldDD, mcClockOldDM,
    mcClockOldDY, mcClockNewDD, mcClockNewDM, mcClockNewDY,
    GLCD_FALSE, mcClockInit);
}

//
// Function: puzzleInit
//
// Initialize the LCD display of puzzle clock
//
void puzzleInit(u08 mode)
{
  u08 x, y;
  char val[2];

  DEBUGP("Init Puzzle");

  // Draw static clock layout
  glcdClearScreen(mcBgColor);

  // Draw the top row numbers
  val[0] = '0';
  val[1] = '\0';
  for (x = 0; x <= 9; x++)
  {
    glcdPutStr2(PUZZLE_NUMBER_X_START + x * 12,
      PUZZLE_NUMBER_Y_START, FONT_5X5P, val, mcFgColor);
    val[0] = val[0] + 1;
  }

  // Draw the text labels
  glcdPutStr3v(PUZZLE_LBL_X_START, PUZZLE_LBL_TIME_Y_START, FONT_5X5P,
    ORI_VERTICAL_BU, "Time", 1, 1, mcFgColor);
  glcdPutStr3v(PUZZLE_LBL_X_START, PUZZLE_LBL_DATE_Y_START, FONT_5X5P,
    ORI_VERTICAL_BU, "Date", 1, 1, mcFgColor);

  // Draw the bulbs
  for (x = 0; x <= 9; x++)
  {
    for (y = 0; y <= 3; y++)
    {
      if (y > 0 || x <= 5)
        glcdCircle2(PUZZLE_BULB_X_START + x * 12, PUZZLE_BULB_Y_START + y * 12,
          PUZZLE_BULB_RADIUS, CIRCLE_FULL, mcFgColor);
    }
  }
  
  // Force the alarm info area to init itself
  mcAlarmSwitch = ALARM_SWITCH_NONE;
  mcU8Util1 = GLCD_FALSE;

  // Reset the parameters for the clock/help page
  mcU8Util2 = 0;
  mcU8Util3 = PUZZLE_MODE_CLOCK;
}

//
// Function: puzzleAlarmAreaUpdate
//
// Draw update in puzzle clock alarm area
//
void puzzleAlarmAreaUpdate(void)
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
      glcdPutStr2(PUZZLE_ALARM_X_START, PUZZLE_ALARM_Y_START, FONT_5X5P,
        msg, mcFgColor);
    }
    else
    {
      // Clear area (remove alarm time)
      glcdFillRectangle(PUZZLE_ALARM_X_START - 1, PUZZLE_ALARM_Y_START - 1,
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
    glcdFillRectangle2(PUZZLE_ALARM_X_START - 1, PUZZLE_ALARM_Y_START - 1,
      19, 7, ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

//
// Function: puzzleBulbRowSet
//
// Update a single bulb row of a puzzle clock
//
void puzzleBulbRowSet(u08 y, u08 oldVal1, u08 oldVal2, u08 oldVal3,
    u08 newVal1, u08 newVal2, u08 newVal3, u08 high, u08 init)
{
  u08 i;
  u08 bulbOld;
  u08 bulbNew;
  u08 color;
  u08 ov1, ov2, ov3, nv1, nv2, nv3;

  // Get the right bulb values based on whether we're dealing with
  // the factor 10 values (most significant digit) or the modulo 10
  // values (least significant digit)
  if (high == GLCD_TRUE)
  {
    ov1 = oldVal1 / 10;
    ov2 = oldVal2 / 10;
    ov3 = oldVal3 / 10;
    nv1 = newVal1 / 10;
    nv2 = newVal2 / 10;
    nv3 = newVal3 / 10;
  }
  else
  {
    ov1 = oldVal1 % 10;
    ov2 = oldVal2 % 10;
    ov3 = oldVal3 % 10;
    nv1 = newVal1 % 10;
    nv2 = newVal2 % 10;
    nv3 = newVal3 % 10;
  }

  // Verify if anything needs to be done at all
  if (ov1 == nv1 && ov2 == nv2 && ov3 == nv3 && init == GLCD_FALSE)
    return;

  // Get the old and new bulb values and update the bulb (if needed)
  for (i = 0; i <=9; i++)
  {
    bulbOld = 0;
    bulbNew = 0;

    // Get old bulb value
    if (ov1 == i)
      bulbOld = (bulbOld | 0x1);
    if (ov2 == i)
      bulbOld = (bulbOld | 0x2);
    if (ov3 == i)
      bulbOld = (bulbOld | 0x4);

    // Get new bulb value
    if (nv1 == i)
      bulbNew = (bulbNew | 0x1);
    if (nv2 == i)
      bulbNew = (bulbNew | 0x2);
    if (nv3 == i)
      bulbNew = (bulbNew | 0x4);

    // Redraw bulb if needed
    if (bulbOld != bulbNew || (init == GLCD_TRUE && bulbNew != 0))
    {
      // Get draw color of bulb
      if (bulbDriver[bulbNew].colorCode == 0)
        color = mcFgColor;
      else
        color = mcBgColor;
      // Draw bulb
      glcdFillCircle2(PUZZLE_BULB_X_START + i * 12, y, PUZZLE_BULB_RADIUS,
        bulbDriver[bulbNew].fillType, color);
      glcdCircle2(PUZZLE_BULB_X_START + i * 12, y, PUZZLE_BULB_RADIUS, CIRCLE_FULL,
        mcFgColor);
    }
  }
}

//
// Function: puzzleHelp
//
// Provide help page for puzzle clock
//
void puzzleHelp(void)
{
  u08 i;
  u08 color;

  glcdClearScreen(mcBgColor);
  glcdPutStr2(1, 1, FONT_5X5P, "Puzzle - Help", mcFgColor);

  // Draw the bulbs
  for (i = 0; i < 4; i++)
  {
    if (i == 3)
    {
      color = mcBgColor;
    }
    else
    {
      if (bulbDriver[i].colorCode == 0)
        color = mcFgColor;
      else
        color = mcBgColor;
    }
    glcdFillCircle2(PUZZLE_HELP_LEFT_X, 14 + i * 14, PUZZLE_BULB_RADIUS,
      bulbDriver[i].fillType, color);
    glcdCircle2(PUZZLE_HELP_LEFT_X, 14 + i * 14, PUZZLE_BULB_RADIUS, CIRCLE_FULL,
      mcFgColor);

    if (i == 0)
    {
      color = mcFgColor;
    }
    else
    {
      if (bulbDriver[i + 4].colorCode == 0)
        color = mcFgColor;
      else
        color = mcBgColor;
    }
    glcdFillCircle2(PUZZLE_HELP_RIGHT_X, 14 + i * 14, PUZZLE_BULB_RADIUS,
      bulbDriver[i + 4].fillType, color);
    glcdCircle2(PUZZLE_HELP_RIGHT_X, 14 + i * 14, PUZZLE_BULB_RADIUS, CIRCLE_FULL,
      mcFgColor);
  }

  // Draw the help text
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 12, FONT_5X5P,
    "None", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 22, FONT_5X5P,
    animSec, mcFgColor);
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 29, FONT_5X5P,
    animDay, mcFgColor);
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 36, FONT_5X5P,
    animMin, mcFgColor);
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 43, FONT_5X5P,
    animMonth, mcFgColor);
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 50, FONT_5X5P,
    animHour, mcFgColor);
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 57, FONT_5X5P,
    animYear, mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET,  8, FONT_5X5P,
    "Sec / Min", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 15, FONT_5X5P,
    "Day / Mon", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 22, FONT_5X5P,
    "Sec / Hour", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 29, FONT_5X5P,
    "Day / Year", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 36, FONT_5X5P,
    "Min / Hour", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 43, FONT_5X5P,
    "Mon / Year", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 50, FONT_5X5P,
    "All Time", mcFgColor);
  glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 57, FONT_5X5P,
    "All Date", mcFgColor);
}

