//*****************************************************************************
// Filename : 'nerd.c'
// Title    : Animation code for MONOCHRON nerd clock
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
#include "nerd.h"

// Specifics for nerd clock
#define NERD_ALARM_X_START	2
#define NERD_ALARM_Y_START	57
#define NERD_ITEM_LEN		14

// Size and index of element format info for a single clock in nerdFormat[]
#define NERD_DATA_LEN	15 // Number of data elements for a single clock
#define NERD_MASK_LEN	0  // Bit mask length (representing the number base)
#define NERD_LOC_YHMS	1  // Start y location of h:m:s
#define NERD_LOC_XTH	2  // Start x location of time h
#define NERD_DIG_TH	3  // Number of digits of time h
#define NERD_LOC_XTM	4  // Start x location of time m
#define NERD_DIG_TM	5  // Number of digits of time m
#define NERD_LOC_XTS	6  // Start x location of time s
#define NERD_DIG_TS	7  // Number of digits of time s
#define NERD_LOC_YDMY	8  // Start y location of d/m/y
#define NERD_LOC_XDD	9  // Start x location of date d
#define NERD_DIG_DD	10 // Number of digits of date d
#define NERD_LOC_XDM	11 // Start x location of date m
#define NERD_DIG_DM	12 // Number of digits of date m
#define NERD_LOC_XDY	13 // Start x location of date y
#define NERD_DIG_DY	14 // Number of digits of date y

// Clock element start index in nerdFormat[]
#define NERD_CLOCK_BINARY	(0 * NERD_DATA_LEN)
#define NERD_CLOCK_OCTAL	(1 * NERD_DATA_LEN)
#define NERD_CLOCK_HEX		(2 * NERD_DATA_LEN)

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcFgColor;

// Display definitions for the binary, octal and hex clock elements
static const unsigned char __attribute__ ((progmem)) nerdFormat[] =
{
  1, // Binary clock
  17, 28, 5, 28 + 5 * 4 + 2, 6, 28 + 11 * 4 + 2 * 2,  6,
  24, 18, 5, 18 + 6 * 4    , 4, 18 + 11 * 4        , 12,
  3, // Octal clock
  33, 48, 2, 48 + 3 * 4 + 2, 2, 48 +  6 * 4 + 2 * 2,  2,
  40, 42, 2, 42 + 4 * 4    , 2, 42 +  8 * 4        ,  4,
  4, // Hex clock
  49, 46, 2, 46 + 4 * 4 + 2, 2, 46 +  8 * 4 + 2 * 2,  2,
  56, 44, 2, 44 + 5 * 4    , 1, 44 +  9 * 4        ,  3
};

// The clock digit characters
static const unsigned char __attribute__ ((progmem)) nerdDigit[] =
{
  'o','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

// Local function prototypes
static void nerdBaseClockUpdate(u08 clock);
static void nerdPrintNumber(u08 maskLen, u08 digits, u16 oldVal, u16 newVal,
  u08 x, u08 y);

//
// Function: nerdCycle
//
// Update the lcd display of a nerd clock
//
void nerdCycle(void)
{
  // Update alarm info in clock
  animADAreaUpdate(NERD_ALARM_X_START, NERD_ALARM_Y_START, AD_AREA_ALM_ONLY);

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
// Initialize the lcd display of a very nerdy clock
//
void nerdInit(u08 mode)
{
  DEBUGP("Init Nerd");

  // Draw clock header and fixed elements for the individual nerd clocks
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
static void nerdBaseClockUpdate(u08 clock)
{
  // Verify changes in hour + min + sec
  nerdPrintNumber(clock + NERD_MASK_LEN, clock + NERD_DIG_TH, mcClockOldTH,
    mcClockNewTH, clock + NERD_LOC_XTH, clock + NERD_LOC_YHMS);
  nerdPrintNumber(clock + NERD_MASK_LEN, clock + NERD_DIG_TM, mcClockOldTM,
    mcClockNewTM, clock + NERD_LOC_XTM, clock + NERD_LOC_YHMS);
  nerdPrintNumber(clock + NERD_MASK_LEN, clock + NERD_DIG_TS, mcClockOldTS,
    mcClockNewTS, clock + NERD_LOC_XTS, clock + NERD_LOC_YHMS);

  // Verify changes in day + mon + year
  nerdPrintNumber(clock + NERD_MASK_LEN, clock + NERD_DIG_DD, mcClockOldDD,
    mcClockNewDD, clock + NERD_LOC_XDD, clock + NERD_LOC_YDMY);
  nerdPrintNumber(clock + NERD_MASK_LEN, clock + NERD_DIG_DM, mcClockOldDM,
    mcClockNewDM, clock + NERD_LOC_XDM, clock + NERD_LOC_YDMY);
  nerdPrintNumber(clock + NERD_MASK_LEN, clock + NERD_DIG_DY,
    mcClockOldDY + 2000, mcClockNewDY + 2000, clock + NERD_LOC_XDY,
    clock + NERD_LOC_YDMY);
}

//
// Function: nerdPrintNumber
//
// Construct and print text string for a clock element in the requested number
// base (derived from the provided number mask length) with the requested
// string length (that can lead to prefix '0' chars and a postfix space).
// Note that parameters maskLen, digits, x and y are indices in nerdFormat[]
// where to find the actual value.
//
static void nerdPrintNumber(u08 maskLen, u08 digits, u16 oldVal, u16 newVal,
  u08 x, u08 y)
{
  u08 i;
  u16 mask;
  u08 digit;
  u08 thinDigits = 0;
  char numberStr[NERD_ITEM_LEN];	// The string to print filled backwards
  u08 idx = NERD_ITEM_LEN - 2;		// Index in numberStr

  // First check if we need to do anything
  if (oldVal == newVal && mcClockInit == GLCD_FALSE)
    return;

  // Get the clock display metadata
  maskLen = pgm_read_byte(&nerdFormat[maskLen]);
  digits = pgm_read_byte(&nerdFormat[digits]);
  x = pgm_read_byte(&nerdFormat[x]);
  y = pgm_read_byte(&nerdFormat[y]);

  // Generate the requested length of characters starting at the last digit up
  // to the first
  mask = ~(0xffff << maskLen);
  for (i = 0; i < digits; i++)
  {
    // Get digit and put it in string
    idx = idx - 1;
    digit = (newVal & mask);
    numberStr[idx] = pgm_read_byte(&nerdDigit[digit]);

    // Count occurrences of e and f characters that are only two pixels wide
    if (digit > 13)
      thinDigits = thinDigits + 1;

    // Move to preceding digit
    newVal = newVal >> maskLen;
  }

  // Close the generated string. In case of a hex number we may need to add a
  // space to compensate for a too thin string due to e and f chars.
  if (thinDigits == 2)
  {
    numberStr[NERD_ITEM_LEN - 2] = ' ';
    numberStr[NERD_ITEM_LEN - 1] = '\0';
  }
  else
  {
    numberStr[NERD_ITEM_LEN - 2] = '\0';
  }

  // Print the generated string
  glcdPutStr2(x, y, FONT_5X5P, &numberStr[idx], mcFgColor);
}
