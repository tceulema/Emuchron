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
#define NERD_CLOCK_BINARY	0
#define NERD_CLOCK_OCTAL	1
#define NERD_CLOCK_HEX		2
#define NERD_ITEM_LEN		14

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcFgColor;

// Structure defining the lcd element locations for a single clock
typedef struct _nerdLocation_t
{
  uint8_t maskLen;	// Number mask length (representing the number base)
  uint8_t locYHms;	// Start y location of h:m:s
  uint8_t locXTH;	// Start x location of time h
  uint8_t digitsTH;	// Number of digits of time h
  uint8_t locXTM;	// Start x location of time m
  uint8_t digitsTM;	// Number of digits of time m
  uint8_t locXTS;	// Start x location of time s
  uint8_t digitsTS;	// Number of digits of time s
  uint8_t locYDmy;	// Start y location of d/m/y
  uint8_t locXDD;	// Start x location of date d
  uint8_t digitsDD;	// Number of digits of date d
  uint8_t locXDM;	// Start x location of date m
  uint8_t digitsDM;	// Number of digits of date m
  uint8_t locXDY;	// Start x location of date y
  uint8_t digitsDY;	// Number of digits of date y
} nerdLocation_t;

// Location definitions for the binary, octal and hex clock elements
static nerdLocation_t nerdLocation[] =
{
  {
    1, // Binary clock
    17, 28, 5, 28 + 5 * 4 + 2, 6, 28 + 11 * 4 + 2 * 2,  6,
    24, 18, 5, 18 + 6 * 4    , 4, 18 + 11 * 4        , 12
  },
  {
    3, // Octal clock
    33, 48, 2, 48 + 3 * 4 + 2, 2, 48 +  6 * 4 + 2 * 2,  2,
    40, 42, 2, 42 + 4 * 4    , 2, 42 +  8 * 4        ,  4
  },
  {
    4, // Hex clock
    49, 46, 2, 46 + 4 * 4 + 2, 2, 46 +  8 * 4 + 2 * 2,  2,
    56, 44, 2, 44 + 5 * 4    , 1, 44 +  9 * 4        ,  3
  }
};

// The clock digit characters
static const unsigned char __attribute__ ((progmem)) nerdDigit[] =
{
  'o','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'
};

// Local function prototypes
static void nerdBaseClockUpdate(uint8_t clock);
static void nerdPrintNumber(uint8_t maskLen, uint8_t digits, uint16_t oldVal,
  uint16_t newVal, uint8_t x, uint8_t y);

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
static void nerdBaseClockUpdate(uint8_t clock)
{
  nerdLocation_t thisClock = nerdLocation[clock];

  // Verify changes in hour + min + sec
  nerdPrintNumber(thisClock.maskLen, thisClock.digitsTH, mcClockOldTH,
    mcClockNewTH, thisClock.locXTH, thisClock.locYHms);
  nerdPrintNumber(thisClock.maskLen, thisClock.digitsTM, mcClockOldTM,
    mcClockNewTM, thisClock.locXTM, thisClock.locYHms);
  nerdPrintNumber(thisClock.maskLen, thisClock.digitsTS, mcClockOldTS,
    mcClockNewTS, thisClock.locXTS, thisClock.locYHms);

  // Verify changes in day + mon + year
  nerdPrintNumber(thisClock.maskLen, thisClock.digitsDD, mcClockOldDD,
    mcClockNewDD, thisClock.locXDD, thisClock.locYDmy);
  nerdPrintNumber(thisClock.maskLen, thisClock.digitsDM, mcClockOldDM,
    mcClockNewDM, thisClock.locXDM, thisClock.locYDmy);
  nerdPrintNumber(thisClock.maskLen, thisClock.digitsDY, mcClockOldDY + 2000,
    mcClockNewDY + 2000, thisClock.locXDY, thisClock.locYDmy);
}

//
// Function: nerdBaseClockUpdate
//
// Construct and print text string for a clock element in the requested number
// base (derived from the provided number mask length) with the requested
// string length (that can lead to prefix '0' chars and a postfix space).
//
static void nerdPrintNumber(uint8_t maskLen, uint8_t digits, uint16_t oldVal,
  uint16_t newVal, uint8_t x, uint8_t y)
{
  uint8_t i;
  uint16_t mask;
  uint8_t digit;
  uint8_t thinDigits = 0;
  char numberStr[NERD_ITEM_LEN];	// The string to print filled backwards
  uint8_t idx = NERD_ITEM_LEN - 2;	// Index in numberStr

  // First check if we need to do anything
  if (oldVal == newVal && mcClockInit == GLCD_FALSE)
    return;

  // Generate the requested length of characters starting at the last digit
  // up to the first
  mask = ~(0xffff << maskLen);
  for (i = 0; i < digits; i++)
  {
    // Get digit and put it in string
    idx--;
    digit = (newVal & mask);
    numberStr[idx] = pgm_read_byte(&nerdDigit[digit]);

    // Count occurrences of e and f characters that are only two pixels wide
    if (digit > 13)
      thinDigits++;

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
