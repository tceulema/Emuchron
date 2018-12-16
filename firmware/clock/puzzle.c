//*****************************************************************************
// Filename : 'puzzle.c'
// Title    : Animation code for MONOCHRON puzzle clock
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

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
// Display timer for help page
extern volatile uint8_t mcU8Util1;
// Display mode for clock
extern volatile uint8_t mcU8Util2;

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];
extern char animDay[];
extern char animMonth[];
extern char animYear[];

// Arrays with help page left/right panel text strings
static char *puzzleHelpMsgsLeft[] =
{
  animSec,
  animDay,
  animMin,
  animMonth,
  animHour,
  animYear
};
static char *puzzleHelpMsgsRight[] =
{
  "Sec / Min",
  "Day / Mon",
  "Sec / Hour",
  "Day / Year",
  "Min / Hour",
  "Mon / Year",
  "All Time",
  "All Date"
};

// For each of the eight permutations of a bulb value specify the circle fill
// type (4 foreground + 4 background)
static const unsigned char __attribute__ ((progmem)) bulbFillType[] =
{
  FILL_BLANK,
  FILL_THIRDUP,
  FILL_THIRDDOWN,
  FILL_HALF,
  FILL_HALF,
  FILL_THIRDDOWN,
  FILL_THIRDUP,
  FILL_BLANK
};

// Local function prototypes
static void puzzleBulbRowSet(u08 y, u08 oldVal1, u08 oldVal2, u08 oldVal3,
  u08 newVal1, u08 newVal2, u08 newVal3, u08 high);
static void puzzleHelp(void);

//
// Function: puzzleButton
//
// Process pressed button for puzzle clock
//
void puzzleButton(u08 pressedButton)
{
  // Provide help page or switch back to clock
  if (mcU8Util2 == PUZZLE_MODE_CLOCK)
  {
    // Provide help page
    DEBUGP("Clock -> Help");
    mcU8Util1 = PUZZLE_HELP_TIMEOUT;
    mcU8Util2 = PUZZLE_MODE_HELP;
    puzzleHelp();
  }
  else
  {
    // The mode change is processed in puzzleCycle()
    DEBUGP("Help -> Clock");
    mcU8Util2 = PUZZLE_MODE_CLOCK;
  }
}

//
// Function: puzzleCycle
//
// Update the lcd display of a puzzle clock
//
void puzzleCycle(void)
{
  char counter[3];

  if ((mcU8Util1 == 1 && mcClockTimeEvent == GLCD_TRUE) ||
      (mcU8Util1 > 0 && mcU8Util2 == PUZZLE_MODE_CLOCK))
  {
    // Switch back from help page to clock
    animClockDraw(DRAW_INIT_FULL);
    animClockDraw(DRAW_CYCLE);
    return;
  }
  else if (mcU8Util2 == PUZZLE_MODE_HELP)
  {
    // We're in help mode so no screen updates, but decrease helppage timeout
    // counter when appropriate
    if (mcClockTimeEvent == GLCD_TRUE)
    {
      mcU8Util1--;
      animValToStr(mcU8Util1, counter);
      glcdPutStr2(120, 1, FONT_5X5P, counter, mcFgColor);
    }
    return;
  }

  // Update alarm info in clock
  animADAreaUpdate(PUZZLE_ALARM_X_START, PUZZLE_ALARM_Y_START,
    AD_AREA_ALM_ONLY);

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Puzzle");

  // Time high part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START, mcClockOldTS, mcClockOldTM,
    mcClockOldTH, mcClockNewTS, mcClockNewTM, mcClockNewTH, GLCD_TRUE);
  // Time low part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START + 12, mcClockOldTS, mcClockOldTM,
    mcClockOldTH, mcClockNewTS, mcClockNewTM, mcClockNewTH, GLCD_FALSE);
  // Date high part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START + 24, mcClockOldDD, mcClockOldDM,
    mcClockOldDY, mcClockNewDD, mcClockNewDM, mcClockNewDY, GLCD_TRUE);
  // Date low part
  puzzleBulbRowSet(PUZZLE_BULB_Y_START + 36, mcClockOldDD, mcClockOldDM,
    mcClockOldDY, mcClockNewDD, mcClockNewDM, mcClockNewDY, GLCD_FALSE);
}

//
// Function: puzzleInit
//
// Initialize the lcd display of puzzle clock
//
void puzzleInit(u08 mode)
{
  u08 x, y;
  char val[2];

  DEBUGP("Init Puzzle");

  // Draw the top row numbers.
  val[0] = '0';
  val[1] = '\0';
  for (x = 0; x <= 9 * 12; x = x + 12)
  {
    glcdPutStr2(PUZZLE_NUMBER_X_START + x, PUZZLE_NUMBER_Y_START, FONT_5X5P,
      val, mcFgColor);
    val[0] = val[0] + 1;
  }

  // Draw the text labels
  glcdPutStr3v(PUZZLE_LBL_X_START, PUZZLE_LBL_TIME_Y_START, FONT_5X5P,
    ORI_VERTICAL_BU, "Time", 1, 1, mcFgColor);
  glcdPutStr3v(PUZZLE_LBL_X_START, PUZZLE_LBL_DATE_Y_START, FONT_5X5P,
    ORI_VERTICAL_BU, "Date", 1, 1, mcFgColor);

  // Draw the bulbs
  for (x = 0; x <= 9 * 12; x = x + 12)
  {
    for (y = 0; y <= 3 * 12; y = y + 12)
    {
      if (y > 0 || x <= 5 * 12)
        glcdCircle2(PUZZLE_BULB_X_START + x, PUZZLE_BULB_Y_START + y,
          PUZZLE_BULB_RADIUS, CIRCLE_FULL, mcFgColor);
    }
  }

  // Reset the parameters for the clock/help page
  mcU8Util1 = 0;
  mcU8Util2 = PUZZLE_MODE_CLOCK;
}

//
// Function: puzzleBulbRowSet
//
// Update a single bulb row of a puzzle clock
//
static void puzzleBulbRowSet(u08 y, u08 oldVal1, u08 oldVal2, u08 oldVal3,
  u08 newVal1, u08 newVal2, u08 newVal3, u08 high)
{
  u08 i;
  u08 bulbOld;
  u08 bulbNew;
  u08 fillType;
  u08 color;
  u08 ov1, ov2, ov3, nv1, nv2, nv3;

  // Get the right bulb values based on whether we're dealing with the factor
  // 10 values (most significant digit) or the modulo 10 value (least
  // significant digit)
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
  if (ov1 == nv1 && ov2 == nv2 && ov3 == nv3 && mcClockInit == GLCD_FALSE)
    return;

  // Get the old and new bulb values and update the bulb (if needed)
  for (i = 0; i <= 9; i++)
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
    if (bulbOld != bulbNew || (mcClockInit == GLCD_TRUE && bulbNew != 0))
    {
      // Get filltype and draw color of bulb and draw it
      fillType = pgm_read_byte(bulbFillType + bulbNew);
      if (bulbNew < 4)
        color = mcFgColor;
      else
        color = mcBgColor;
      glcdFillCircle2(PUZZLE_BULB_X_START + i * 12, y, PUZZLE_BULB_RADIUS,
        fillType, color);
      glcdCircle2(PUZZLE_BULB_X_START + i * 12, y, PUZZLE_BULB_RADIUS,
        CIRCLE_FULL, mcFgColor);
    }
  }
}

//
// Function: puzzleHelp
//
// Provide help page for puzzle clock
//
static void puzzleHelp(void)
{
  u08 i;
  u08 color;
  u08 fillType;

  glcdClearScreen(mcBgColor);
  glcdPutStr2(1, 1, FONT_5X5P, "Puzzle - Help", mcFgColor);

  // Draw the bulbs
  for (i = 0; i < 4; i++)
  {
    // Left side
    if (i == 3)
      color = mcBgColor;
    else
      color = mcFgColor;
    fillType = pgm_read_byte(bulbFillType + i);
    glcdFillCircle2(PUZZLE_HELP_LEFT_X, 14 + i * 14, PUZZLE_BULB_RADIUS,
      fillType, color);
    glcdCircle2(PUZZLE_HELP_LEFT_X, 14 + i * 14, PUZZLE_BULB_RADIUS,
      CIRCLE_FULL, mcFgColor);

    // Right side
    if (i == 0)
      color = mcFgColor;
    else
      color = mcBgColor;
    fillType = pgm_read_byte(bulbFillType + i + 4);
    glcdFillCircle2(PUZZLE_HELP_RIGHT_X, 14 + i * 14, PUZZLE_BULB_RADIUS,
      fillType, color);
    glcdCircle2(PUZZLE_HELP_RIGHT_X, 14 + i * 14, PUZZLE_BULB_RADIUS,
      CIRCLE_FULL, mcFgColor);
  }

  // Draw the help text for the top left None bulb
  glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 12, FONT_5X5P,
    "None", mcFgColor);

  // Draw the help texts for the other bulbs
  // Left side
  for (i = 0; i < 6; i++)
    glcdPutStr2(PUZZLE_HELP_LEFT_X + PUZZLE_HELP_TEXT_OFFSET, 22 + i * 7,
      FONT_5X5P, puzzleHelpMsgsLeft[i], mcFgColor);
  // Right side
  for (i = 0; i < 8; i++)
    glcdPutStr2(PUZZLE_HELP_RIGHT_X + PUZZLE_HELP_TEXT_OFFSET, 8 + i * 7,
      FONT_5X5P, puzzleHelpMsgsRight[i], mcFgColor);
}
