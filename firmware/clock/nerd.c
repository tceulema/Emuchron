//*****************************************************************************
// Filename : 'nerd.c'
// Title    : Animation code for MONOCHRON nerd clock
//*****************************************************************************

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
#include "nerd.h"

// Specifics for nerd clock
#define NERD_ALARM_X_START	2
#define NERD_ALARM_Y_START	57
#define NERD_CLOCK_BINARY	0
#define NERD_CLOCK_OCTAL	1
#define NERD_CLOCK_HEX		2

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcFgColor;

// Structure defining the LCD element locations for a single clock
typedef struct _nerdLocation_t
{
  uint8_t base;		// Base representation of numbers
  uint8_t locYHms;	// Start y location of h:m:s
  uint8_t locXTH;	// Start x location of time h
  uint8_t digitsTH;	// Number of digits of time h
  uint8_t locXTM;	// Start x location of time m
  uint8_t digitsTM;	// Number of digits of time h
  uint8_t locXTS;	// Start x location of time s
  uint8_t digitsTS;	// Number of digits of time h
  uint8_t locYDmy;	// Start y location of d/m/y
  uint8_t locXDD;	// Start x location of date d
  uint8_t digitsDD;	// Number of digits of time h
  uint8_t locXDM;	// Start x location of date m
  uint8_t digitsDM;	// Number of digits of time h
  uint8_t locXDY;	// Start x location of date y
  uint8_t digitsDY;	// Number of digits of time h
} nerdLocation_t;

// Location definitions for the binary, octal and hex clock elements
static nerdLocation_t nerdLocation[] =
{
  // Binary clock
  {
    2,
    17, 28, 5, 28 + 5 * 4 + 2, 6, 28 + 11 * 4 + 2 * 2,  6,
    24, 18, 5, 18 + 6 * 4    , 4, 18 + 11 * 4        , 12
  },
  // Octal clock
  {
    8,
    33, 48, 2, 48 + 3 * 4 + 2, 2, 48 +  6 * 4 + 2 * 2,  2,
    40, 42, 2, 42 + 4 * 4    , 2, 42 +  8 * 4        ,  4
  },
  // Hex clock
  {
    16,
    49, 46, 2, 46 + 4 * 4 + 2, 2, 46 +  8 * 4 + 2 * 2,  2,
    56, 44, 2, 44 + 5 * 4    , 1, 44 +  9 * 4        ,  3
  }
};

// Local function prototypes
static void nerdBaseClockUpdate(uint8_t clock);
static uint8_t nerdPrintNumber(uint8_t base, uint8_t digits, uint16_t newVal,
  uint16_t oldVal, char *numberStr);

//
// Function: nerdCycle
//
// Update the LCD display of a nerd clock
//
void nerdCycle(void)
{
  // Update alarm info in clock
  animAlarmAreaUpdate(NERD_ALARM_X_START, NERD_ALARM_Y_START,
    ALARM_AREA_ALM_ONLY);

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Nerd");

  // Update each of the nerd clocks
  nerdBaseClockUpdate(NERD_CLOCK_BINARY);
  nerdBaseClockUpdate(NERD_CLOCK_OCTAL);
  nerdBaseClockUpdate(NERD_CLOCK_HEX);
}

//
// Function: nerdInit
//
// Initialize the LCD display of a very nerdy clock
//
void nerdInit(u08 mode)
{
  DEBUGP("Init Nerd");

  // Draw clock header and fixed elements for the individual nerd clocks.
  glcdPutStr2(9,   1, FONT_5X5P, "*** binary/octal/hex clock ***", mcFgColor);
  glcdPutStr2(37,  8, FONT_5X5P, "(h:m:s - d/m/y)", mcFgColor);
  glcdPutStr2(48, 17, FONT_5X5P, ":            :", mcFgColor);
  glcdPutStr2(38, 24, FONT_5X5P, "/        /", mcFgColor);
  glcdPutStr2(44, 33, FONT_5X5P, "o    :o    :o", mcFgColor);
  glcdPutStr2(38, 40, FONT_5X5P, "o    /o    /o", mcFgColor);
  glcdPutStr2(38, 49, FONT_5X5P, "o\\    :o\\    :o\\", mcFgColor);
  glcdPutStr2(36, 56, FONT_5X5P, "o\\    /o\\  /o\\", mcFgColor);
}

//
// Function: nerdBaseClockUpdate
//
// Draw update for a single clock
//
static void nerdBaseClockUpdate(uint8_t clock)
{
  char numberStr[13];
  uint8_t digitOffset = 0;
  nerdLocation_t thisClock = nerdLocation[clock];

  // Hour
  if (mcClockOldTH != mcClockNewTH || mcClockInit == GLCD_TRUE)
  {
    digitOffset = nerdPrintNumber(thisClock.base,
      thisClock.digitsTH, mcClockNewTH, mcClockOldTH, numberStr);
    if (digitOffset != 255)
      glcdPutStr2(thisClock.locXTH + digitOffset * 4,
        thisClock.locYHms, FONT_5X5P, numberStr + digitOffset, mcFgColor);
  }

  // Minute
  if (mcClockOldTM != mcClockNewTM || mcClockInit == GLCD_TRUE)
  {
    digitOffset = nerdPrintNumber(thisClock.base,
      thisClock.digitsTM, mcClockNewTM, mcClockOldTM, numberStr);
    if (digitOffset != 255)
      glcdPutStr2(thisClock.locXTM + digitOffset * 4,
        thisClock.locYHms, FONT_5X5P, numberStr + digitOffset, mcFgColor);
  }

  // Seconds
  if (mcClockOldTS != mcClockNewTS || mcClockInit == GLCD_TRUE)
  {
    digitOffset = nerdPrintNumber(thisClock.base,
      thisClock.digitsTS, mcClockNewTS, mcClockOldTS, numberStr);
    if (digitOffset != 255)
      glcdPutStr2(thisClock.locXTS + digitOffset * 4,
        thisClock.locYHms, FONT_5X5P, numberStr + digitOffset, mcFgColor);
  }

  // Day
  if (mcClockOldDD != mcClockNewDD || mcClockInit == GLCD_TRUE)
  {
    digitOffset = nerdPrintNumber(thisClock.base,
      thisClock.digitsDD, mcClockNewDD, mcClockOldDD, numberStr);
    if (digitOffset != 255)
      glcdPutStr2(thisClock.locXDD + digitOffset * 4,
        thisClock.locYDmy, FONT_5X5P, numberStr + digitOffset, mcFgColor);
  }

  // Month
  if (mcClockOldDM != mcClockNewDM || mcClockInit == GLCD_TRUE)
  {
    digitOffset = nerdPrintNumber(thisClock.base,
      thisClock.digitsDM, mcClockNewDM, mcClockOldDM, numberStr);
    if (digitOffset != 255)
      glcdPutStr2(thisClock.locXDM + digitOffset * 4,
        thisClock.locYDmy, FONT_5X5P, numberStr + digitOffset, mcFgColor);
  }

  // Year
  if (mcClockOldDY != mcClockNewDY || mcClockInit == GLCD_TRUE)
  {
    digitOffset = nerdPrintNumber(thisClock.base,
      thisClock.digitsDY, mcClockNewDY + 2000, mcClockOldDY + 2000,
      numberStr);
    if (digitOffset != 255)
      glcdPutStr2(thisClock.locXDY + digitOffset * 4,
        thisClock.locYDmy, FONT_5X5P, numberStr + digitOffset, mcFgColor);
  }
}

//
// Function: nerdBaseClockUpdate
//
// Construct text string for a clock element in the requested number
// base with the requested string length (that can lead to prefix '0'
// characters).
// - The function returns an index in the new value text string for the
//   first character that deviates from the previous sting value, so we
//   don't have to print the entire string to the LCD.
// - In case the clock has to init itelf, the function returns value 0.
// - In case the new string value is identical to the old value the
//   function returns value 255.
//
static uint8_t nerdPrintNumber(uint8_t base, uint8_t digits, uint16_t newVal,
  uint16_t oldVal, char *numberStr)
{
  uint8_t i = 0;
  uint16_t mask = 0;
  uint8_t maskLen = 0;
  uint8_t maskPos = 0;
  uint8_t digitNew = 0;
  uint8_t digitOld = 0;
  uint8_t compareLoc = 255;

  // We're mostly going to use shift and and/or operations to calculate
  // our stuff so we need a bitmask. Depending on the number base we'll
  // have different mask lengths.
  if (base == 2)
    maskLen = 1;
  else if (base == 8)
    maskLen = 3;
  else
    maskLen = 4;

  // Set the initial mask position in the number value
  maskPos = maskLen * digits;
  mask = (base - 1) << (maskLen * (digits - 1));

  // Generate the requested length of characters
  for (i = 0; i < digits; i++)
  {
    // Get the digit value for both the old and new value
    digitNew = (newVal & mask) >> (maskPos - maskLen);
    digitOld = (oldVal & mask) >> (maskPos - maskLen);

    // Do we have our first mismatch value between the old and new value
    if (digitNew != digitOld && compareLoc > i)
      compareLoc = i;

    // Create the character for the digit value
    if (digitNew == 0)
      numberStr[i] = 'o';
    else if (digitNew < 10)
      numberStr[i] = (char)(digitNew + '0');
    else if (digitNew < 16)
      numberStr[i] = (char)(digitNew - 10 + 'a');

    // Get remainder value for the old and new value
    newVal = (newVal & ~mask);
    oldVal = (oldVal & ~mask);

    // And move to next digit
    mask = (mask >> maskLen);
    maskPos = maskPos - maskLen;
  }

  // Close the generated string
  numberStr[digits] = '\0';

  // If we need to init our clock overide the mismatch value and indicate
  // that the entire string is to be reported
  if (mcClockInit == GLCD_TRUE)
    compareLoc = 0;

  return compareLoc;
}

