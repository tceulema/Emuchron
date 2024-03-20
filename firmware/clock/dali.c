//*****************************************************************************
// Filename  : 'dali.c'
// Title     : Animation code for MONOCHRON dali clock
//*****************************************************************************

// History of original code:
// Initial work xdaliclock by Jamie Zawinski that is later integrated in
// Monochron MultiChron code.
// https://www.jwz.org/xdaliclock
// https://github.com/CaitSith2/monochron/tree/MultiChron/firmware

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "../monomain.h"
#include "dali.h"

// Load the dali font digit bitmaps
#include "../font28x64.h"

// Data to be represented in the digits
#define DIGIT_MODE_TIME		0
#define DIGIT_MODE_DATE		1
#define DIGIT_MODE_YEAR		2
#define DIGIT_MODE_ALARM	3

// Graphic element to be shown in the mid-digits separator area
#define SEPARATOR_NONE		0
#define SEPARATOR_DOTS		1
#define SEPARATOR_DASH		2

// Clock digits layout parameters
#define DALI_DIGITS		4
#define DALI_DIGIT_SPACING	3
#define DALI_DISP_H10_X		0
#define DALI_DISP_H1_X		DALI_DIGIT_WIDTH + DALI_DIGIT_SPACING
#define DALI_DISP_M10_X		GLCD_XPIXELS - 2 * DALI_DIGIT_WIDTH - \
  DALI_DIGIT_SPACING
#define DALI_DISP_M1_X		GLCD_XPIXELS - DALI_DIGIT_WIDTH
#define DALI_DISP_DIGIT_Y_LINE	0

// Digit separator layout parameters
#define DALI_SEPARATOR_X	59
#define DALI_SEPARATOR_RADIUS	4
#define DALI_DOT_TOP_Y		27
#define DALI_DOT_BOTTOM_Y	55
#define DALI_DASH_HEIGHT	3
#define DALI_DASH_Y		33

// Time in seconds (translated to app cycles) for info to remain static after
// completing a transition, including the cycles needed to do the transition
#define COUNTDOWN_INFO_SEC(a)	\
  (u08)(1000L * a / ANIM_TICK_CYCLE_MS + DALI_GEN_CYCLES)

// Monochron environment variables
extern volatile uint8_t mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarmEvent, mcAlarming;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t mcFgColor;
extern volatile uint8_t mcAlarmSwitchEvent, mcAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
// mcU8Util1 = the active/pending digit mode (time/date/year/alarm)
// mcU8Util2 = request to apply (pending) digit mode (MC_TRUE/MC_FALSE)
extern volatile uint8_t mcU8Util1, mcU8Util2;

// Local function prototypes
static void daliDigitsSet(u08 high, u08 low, u08 separator);
static void daliFontLineRead(u08 digit, u08 line, u08 *lineInfo, u08 *segment);
static void daliLineToSegment(u08 i, u08 *line, u08 *segment);
static void daliTrans(void);
static void daliTransDigit(u08 x, u08 oldVal, u08 newVal);
static void daliTransSeparator(void);

// Local data for the display digits and digit separator
static const u08 digitLocX[DALI_DIGITS] =
  { DALI_DISP_H10_X, DALI_DISP_H1_X, DALI_DISP_M10_X, DALI_DISP_M1_X };
static u08 genStep;			// Digit transition step
static u08 genStatic;			// Static digit display timeout counter
static u08 digitOld[DALI_DIGITS];	// Old display digits
static u08 digitNew[DALI_DIGITS];	// New display digits
static u08 sepOld, sepNew;		// Old and new digit separator

//
// Function: daliButton
//
// Process pressed button for dali clock
//
void daliButton(u08 pressedButton)
{
  // Start the info cycle: date -> year -> alarm (only if active) -> time
  mcU8Util1 = DIGIT_MODE_DATE;
  mcU8Util2 = MC_TRUE;
}

//
// Function: daliCycle
//
// Update the lcd display of dali clock
//
void daliCycle(void)
{
  // First check alarm related events that will override any (pending) state
  if (mcClockInit == MC_FALSE && mcAlarmSwitchEvent == MC_TRUE)
  {
    // The alarm switch is switched on or off
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
      mcU8Util1 = DIGIT_MODE_ALARM;
    else
      mcU8Util1 = DIGIT_MODE_TIME;
    mcU8Util2 = MC_TRUE;
    genStatic = 0;
  }
  else if (mcAlarmEvent == MC_TRUE)
  {
    // Alarm is triggered or is ended
    mcU8Util1 = DIGIT_MODE_TIME;
    mcU8Util2 = MC_TRUE;
    genStatic = 0;
  }

  // See if we need to change the digits
  if (mcU8Util1 == DIGIT_MODE_TIME)
  {
    // Transistion to and update time. However, do not transition a time update
    // while alarming since we're constantly toggling between time and alarm.
    if (((mcClockOldTM != mcClockNewTM || mcClockOldTH != mcClockNewTH) &&
        mcAlarming == MC_FALSE) || mcU8Util2 == MC_TRUE)
    {
      if (genStep == MAX_U08 && genStatic == 0)
      {
        // Set time display data and signal transition start
        daliDigitsSet(mcClockNewTH, mcClockNewTM, SEPARATOR_DOTS);
        if (mcAlarming == MC_FALSE)
          genStatic = 0;
        else
          genStatic = COUNTDOWN_INFO_SEC(3);
      }
      else
      {
        // Postpone time draw until current draw and wait are done
        mcU8Util2 = MC_TRUE;
      }
    }
    else if (genStatic == 0 && mcAlarming == MC_TRUE)
    {
      // We're alarming/snoozing so toggle between time and alarm time
      mcU8Util1 = DIGIT_MODE_ALARM;
      mcU8Util2 = MC_TRUE;
    }
  }
  else if (mcU8Util1 == DIGIT_MODE_DATE)
  {
    // Transistion to date
    if (mcU8Util2 == MC_TRUE)
    {
      if (genStep == MAX_U08)
      {
        // Set date display data and signal transition start
        daliDigitsSet(mcClockNewDD, mcClockNewDM, SEPARATOR_DASH);
        genStatic = COUNTDOWN_INFO_SEC(3);
      }
    }
    else if (genStatic == 0)
    {
      // Switch to year
      mcU8Util1 = DIGIT_MODE_YEAR;
      mcU8Util2 = MC_TRUE;
    }
  }
  else if (mcU8Util1 == DIGIT_MODE_YEAR)
  {
    // Transistion to year
    if (mcU8Util2 == MC_TRUE)
    {
      if (genStatic == 0)
      {
        // Set year display data and signal transition start
        daliDigitsSet(20, mcClockNewDY, SEPARATOR_NONE);
        genStatic = COUNTDOWN_INFO_SEC(3);
      }
    }
    else if (genStatic == 0)
    {
      // Switch to alarm or time depending on alarm switch position
      if (mcAlarmSwitch == ALARM_SWITCH_ON)
        mcU8Util1 = DIGIT_MODE_ALARM;
      else
        mcU8Util1 = DIGIT_MODE_TIME;
      mcU8Util2 = MC_TRUE;
    }
  }
  else if (mcU8Util1 == DIGIT_MODE_ALARM)
  {
    // Transistion to alarm time
    if (mcU8Util2 == MC_TRUE)
    {
      if (genStep == MAX_U08 && genStatic == 0)
      {
        // Set alarm display data and signal transition start
        daliDigitsSet(mcAlarmH, mcAlarmM, SEPARATOR_DOTS);
        genStatic = COUNTDOWN_INFO_SEC(3);
      }
    }
    else if (genStatic == 0 || mcAlarmSwitch == ALARM_SWITCH_OFF)
    {
      // Switch back to time due to end of info cycle, switching off the alarm
      // switch or toggle between time and alarm time during alarming/snoozing
      mcU8Util1 = DIGIT_MODE_TIME;
      mcU8Util2 = MC_TRUE;
      genStatic = 0;
    }
  }

  // Start or continue transitioning display
  if (genStep != MAX_U08)
    daliTrans();

  // Countdown of completed display transition before starting next one
  if (genStatic > 0)
    genStatic--;
}

//
// Function: daliDigitsSet
//
// Set new digit values and separator to be shown in the clock and initiate its
// transition
//
static void daliDigitsSet(u08 high, u08 low, u08 separator)
{
  digitNew[0] = high / 10;
  digitNew[1] = high % 10;
  digitNew[2] = low / 10;
  digitNew[3] = low % 10;
  sepNew = separator;
  mcU8Util2 = MC_FALSE;
  genStep = 0;
}

//
// Function: daliInit
//
// Initialize the lcd display of dali clock and initiate first transition to
// display the time (hh:mm).
//
void daliInit(u08 mode)
{
  DEBUGP("Init Dali");
  digitOld[0] = digitOld[1] = digitOld[2] = digitOld[3] = MAX_U08;
  mcU8Util1 = DIGIT_MODE_TIME;
  mcU8Util2 = MC_TRUE;
  genStep = MAX_U08;
  genStatic = 0;
  sepOld = SEPARATOR_NONE;
  sepNew = SEPARATOR_NONE;
}

//
// Function: daliFontLineRead
//
// Load dali font and segment info for a single dali digit font line. For more
// info on how dali font line data is encoded refer to font28x64.h [firmware].
//
static void daliFontLineRead(u08 digit, u08 line, u08 *lineInfo, u08 *segment)
{
  // Get line start location in dali digit font data
  uint8_t *fontLine = (uint8_t *)daliFont +
    (uint16_t)digit * ((DALI_DIGIT_HEIGHT / 2) * 5) + (line / 2) * 5;

  if (line & 1)
  {
    // Decode font data for phase 1
    u08 d2 = pgm_read_byte(fontLine + 2);
    u08 d3 = pgm_read_byte(fontLine + 3);
    u08 d4 = pgm_read_byte(fontLine + 4);
    lineInfo[0] = d2 & 0x1f;
    lineInfo[1] = d3 & 0x1f;
    lineInfo[2] = d4 & 0x1f;
    lineInfo[3] = (d4 >> 5) | ((d3 & 0x60) >> 2);
  }
  else
  {
    // Decode font data for phase 0
    u08 d0 = pgm_read_byte(fontLine + 0);
    u08 d1 = pgm_read_byte(fontLine + 1);
    u08 d2 = pgm_read_byte(fontLine + 2);
    u08 d3 = pgm_read_byte(fontLine + 3);
    lineInfo[0] = d0 & 0x1f;
    lineInfo[1] = d1 & 0x1f;
    lineInfo[2] = ((d0 >> 3) & 0x1c) | ((d1 >> 6) & 3);
    lineInfo[3] = ((d1 >> 1) & 0x10) | ((d2 >> 4) & 0x0e) | ((d3 >> 7) & 1);
  }

  // Determine the line segment
  *segment = 0;
  if (lineInfo[0] != DALI_SEG_TERM)
    *segment = *segment + 1;
  if (lineInfo[2] != DALI_SEG_TERM)
    *segment = *segment + 1;
}

//
// Function: daliLineToSegment
//
// Set a segment from a font line
//
static void daliLineToSegment(u08 i, u08 *line, u08 *segment)
{
  if (line[i * 2] != DALI_SEG_TERM)
  {
    segment[0] = line[i * 2];
    segment[1] = line[i * 2 + 1];
  }
  else
  {
    segment[0] = line[0];
    segment[1] = line[1];
  }
}

//
// Function: daliTrans
//
// Draw transition step for digits and digit separator
//
static void daliTrans(void)
{
  u08 i;
  u08 digitChange = MC_FALSE;

  // Execute next transition step for all impacted digits
  for (i = 0; i < DALI_DIGITS; i++)
  {
    if (digitOld[i] != digitNew[i])
    {
      digitChange = MC_TRUE;
      daliTransDigit(digitLocX[i], digitOld[i], digitNew[i]);
    }
  }

  // Check no-change or end of transition
  if (digitChange == MC_FALSE && sepOld == sepNew)
  {
    // No digit or separator changed so end transition as we're done
    genStep = MAX_U08;
    if (genStatic > 0)
      genStatic = genStatic - DALI_GEN_CYCLES;
    return;
  }
  else if (genStep == DALI_GEN_CYCLES)
  {
    // End of transition so sync digit and separator state
    genStep = MAX_U08;
    for (i = 0; i < DALI_DIGITS; i++)
      digitOld[i] = digitNew[i];
    sepOld = sepNew;
    return;
  }

  // Draw next transition of the digit separator
  if (sepOld != sepNew)
    daliTransSeparator();

  // Increment transition step
  genStep++;
}

//
// Function: daliTransDigit
//
// For a single clock digit generate and draw a single transition bitmap
//
static void daliTransDigit(u08 x, u08 oldVal, u08 newVal)
{
  u08 lineOld[DALI_DIGITS], lineNew[DALI_DIGITS], i, j, line;
  u08 segLineOld, segLineNew;
  u08 segMain;
  u08 segOld[2], segNew[2];
  u08 bitmap[DALI_DIGIT_HEIGHT / 8 * DALI_DIGIT_WIDTH] = {0};
  u08 y = DALI_DISP_DIGIT_Y_LINE;
  uint16_t segMerge[2];

  // For each vertical line determine a horizontal transition line
  for (line = 0; line < DALI_DIGIT_HEIGHT; line++)
  {
    // Get the font info for the old and new digit
    if (oldVal == MAX_U08)
    {
      lineOld[0] = lineOld[1] = lineOld[2] = lineOld[3] = 0;
      segLineOld = 2;
    }
    else
    {
      daliFontLineRead(oldVal, line, lineOld, &segLineOld);
    }
    daliFontLineRead(newVal, line, lineNew, &segLineNew);
    segMain = MAX(segLineOld, segLineNew);

    // Merge the segments from the old and new digit
    for (i = 0; i < segMain; i++)
    {
      // Merge the segments of a single old and new digit line
      daliLineToSegment(i, lineOld, segOld);
      daliLineToSegment(i, lineNew, segNew);
      for (j = 0; j < 2; j++)
      {
        segMerge[j] = (segNew[j] - segOld[j]) * genStep + DALI_GEN_CYCLES / 2;
        segMerge[j] = (segMerge[j] / DALI_GEN_CYCLES + segOld[j]) & 0xff;
      }

      // Save the merged segments in the final merge digit bitmap
      while (segMerge[0] < segMerge[1])
      {
        bitmap[segMerge[0] + (line / 8) * DALI_DIGIT_WIDTH] |= _BV(line % 8);
        segMerge[0]++;
      }
    }
  }

  // Draw the merged 28x64 font digit bitmap
  for (i = 0; i < DALI_DIGIT_HEIGHT / 8 * DALI_DIGIT_WIDTH; i++)
  {
    if (i % DALI_DIGIT_WIDTH == 0)
    {
      // Set cursor at start of new y-line
      glcdSetAddress(x, y);
      y++;
    }
    // Write bitmap byte
    j = bitmap[i];
    if (mcFgColor == GLCD_OFF)
      glcdDataWrite(~j);
    else
      glcdDataWrite(j);
  }
}

//
// Function: daliTransSeparator
//
// Draw a single digit separator transition step
//
static void daliTransSeparator(void)
{
  u08 widthOld, widthNew, fillType;

  // The old and new radius/width of the dots and dash separators
  widthOld = (genStep * DALI_SEPARATOR_RADIUS) / DALI_GEN_CYCLES;
  widthNew = ((genStep + 1) * DALI_SEPARATOR_RADIUS) / DALI_GEN_CYCLES;

  // Buildup/clear separators inside out and only when needed
  if (widthOld != widthNew || genStep == 0)
  {
    // Dot separators
    if (sepNew == SEPARATOR_DOTS)
      fillType = FILL_FULL;
    else
      fillType = FILL_BLANK;
    glcdFillCircle2(DALI_SEPARATOR_X + DALI_SEPARATOR_RADIUS, DALI_DOT_TOP_Y,
      widthNew, fillType);
    glcdFillCircle2(DALI_SEPARATOR_X + DALI_SEPARATOR_RADIUS,
      DALI_DOT_BOTTOM_Y, widthNew, fillType);

    // Dash separator
    if (sepNew == SEPARATOR_DASH)
      fillType = FILL_FULL;
    else
      fillType = FILL_BLANK;
    glcdFillRectangle2(DALI_SEPARATOR_X + DALI_SEPARATOR_RADIUS - widthNew,
      DALI_DASH_Y, 2 * widthNew + 1, DALI_DASH_HEIGHT, ALIGN_AUTO, fillType);
  }
}
