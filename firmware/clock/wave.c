//*****************************************************************************
// Filename : 'wave.c'
// Title    : Animation code for MONOCHRON wave banner clock
//*****************************************************************************

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "wave.h"

// Layout of the wave banner
#define DIGIT_SIZE		6
#define START_HOUR_DIGIT_1	0
#define START_HOUR_DIGIT_2	(START_HOUR_DIGIT_1 + DIGIT_SIZE)
#define START_HOURMIN_SEP	(START_HOUR_DIGIT_2 + DIGIT_SIZE)
#define START_MIN_DIGIT_1	(START_HOURMIN_SEP + 2)
#define START_MIN_DIGIT_2	(START_MIN_DIGIT_1 + DIGIT_SIZE)
#define START_TIMEDATE_SEP	(START_MIN_DIGIT_2 + DIGIT_SIZE)
#define START_DAY_DIGIT_1	(START_TIMEDATE_SEP + DIGIT_SIZE)
#define START_DAY_DIGIT_2	(START_DAY_DIGIT_1 + DIGIT_SIZE)
#define START_DAYMON_SEP	(START_DAY_DIGIT_2 + DIGIT_SIZE)
#define START_MON_DIGIT_1	(START_DAYMON_SEP + 4)
#define START_MON_DIGIT_2	(START_MON_DIGIT_1 + DIGIT_SIZE)
#define BANNER_LENGTH		(START_MON_DIGIT_2 + DIGIT_SIZE - 1)
#define BANNER_START_X		4

// Wave sinus and sinus table constants
#define WAVE_DELTA_Y	14
#define WAVE_WAVE_LEN	((u08)sizeof(yDelta))
#define WAVE_STEP	2

// Pointers to the '/' and ':' charset characters
#define CHAR_SLASH	(DIGIT_SIZE * 10)
#define CHAR_COLON	(DIGIT_SIZE * 10 + 4)
#define CHAR_BLANK	255

// Monochron environment variables
extern volatile u08 mcClockNewTS;
extern volatile u08 mcClockNewTM, mcClockNewTH;
extern volatile u08 mcClockNewDD, mcClockNewDM;
extern volatile u08 mcFgColor;

// Digit 0..9 with '/' and ':' char images for the wave banner
static const uint32_t __attribute__ ((progmem)) charset[] =
{
  0x00fffff0,0x0f0f000f,0x0f00f00f,0x0f000f0f,0x00fffff0,0x00000000, // 0
  0x00000000,0x0f0000f0,0x0fffffff,0x0f000000,0x00000000,0x00000000, // 1
  0x0f0000f0,0x0ff0000f,0x0f0f000f,0x0f00f00f,0x0f000ff0,0x00000000, // 2
  0x00f0000f,0x0f00000f,0x0f000f0f,0x0f00f0ff,0x00ff000f,0x00000000, // 3
  0x000ff000,0x000f0f00,0x000f00f0,0x0fffffff,0x000f0000,0x00000000, // 4
  0x00f00fff,0x0f000f0f,0x0f000f0f,0x0f000f0f,0x00fff00f,0x00000000, // 5
  0x00ffff00,0x0f00f0f0,0x0f00f00f,0x0f00f00f,0x00ff0000,0x00000000, // 6
  0x0000000f,0x0fff000f,0x0000f00f,0x00000f0f,0x000000ff,0x00000000, // 7
  0x00ff0ff0,0x0f00f00f,0x0f00f00f,0x0f00f00f,0x00ff0ff0,0x00000000, // 8
  0x00000ff0,0x0f00f00f,0x0f00f00f,0x00f0f00f,0x000ffff0,0x00000000, // 9
  0x03fc0000,0x0003fc00,0x000003fc,0x00000000,			     // '/'
  0x007e07e0,0x00000000						     // :
};

// Generated sinus y variation movements for wave banner.
// So, why don't we calculate the sin() values in our cycle method code instead
// of hardcoding them here at the expense of flexibility?
// Calculating the sin() value in the code twice for every BANNER_LENGTH
// element takes a whopping ~45 msec. Keeping in mind we have a 75 msec clock
// cycle time it turns out we do not have enough time left to do the actual
// wave drawing. As such, we're forced to implement pre-calculated sin() values
// as progmem data.
// The values below were generated using utility wavesin.c [support].
static const uint8_t __attribute__ ((progmem)) yDelta[] =
{
  0x0e,0x0f,0x0f,0x10,0x11,0x11,0x12,0x13,0x13,0x14,0x15,0x15,0x16,0x16,0x17,
  0x17,0x18,0x18,0x19,0x19,0x19,0x1a,0x1a,0x1a,0x1a,0x1b,0x1b,0x1b,0x1b,0x1b,
  0x1b,0x1b,0x1b,0x1b,0x1b,0x1b,0x1a,0x1a,0x1a,0x1a,0x19,0x19,0x19,0x18,0x18,
  0x17,0x17,0x16,0x16,0x15,0x15,0x14,0x13,0x13,0x12,0x11,0x11,0x10,0x0f,0x0f,
  0x0e,0x0d,0x0d,0x0c,0x0b,0x0b,0x0a,0x09,0x09,0x08,0x08,0x07,0x06,0x06,0x05,
  0x05,0x04,0x04,0x03,0x03,0x03,0x02,0x02,0x02,0x02,0x01,0x01,0x01,0x01,0x01,
  0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x04,0x04,
  0x05,0x05,0x06,0x06,0x07,0x08,0x08,0x09,0x09,0x0a,0x0b,0x0b,0x0c,0x0d,0x0d
};

// Starting point in the sin() value data
static u08 yDeltaStart = WAVE_STEP;
static u08 yDeltaPrevStart = 0;

//
// Function: waveCycle
//
// Update the lcd display of a wave banner clock
//
void waveCycle(void)
{
  u08 i,j,k;
  u08 yDeltaIdx;
  u08 yDeltaPrevIdx;
  u08 y;
  u08 yPrev;
  u08 idx = 0;
  u08 lcdByte;
  u08 cursorOk;
  uint32_t element;

  // Update alarm area
  animADAreaUpdate(53, 58, AD_AREA_ALM_ONLY);

  // Draw exactly 7 full vertical y-pixel byte rows
  for (i = 0; i < GLCD_YPIXELS - 8; i = i + 8)
  {
    // Reset cursor status and base offset in sinus table
    cursorOk = MC_FALSE;
    yDeltaIdx = yDeltaStart;
    yDeltaPrevIdx = yDeltaPrevStart;

    // Determine contents for y-line byte
    for (j = 0; j < BANNER_LENGTH; j++)
    {
      // Based on the x position determine font offset in the charset image
      // source, or force a blank (space)
      if (j == START_HOUR_DIGIT_1)
        idx = DIGIT_SIZE * (mcClockNewTH / 10);
      else if (j == START_HOUR_DIGIT_2)
        idx = DIGIT_SIZE * (mcClockNewTH % 10);
      else if (j == START_HOURMIN_SEP)
      {
        if ((mcClockNewTS & 0x1) == 0)
          idx = CHAR_COLON;
        else
          idx = CHAR_BLANK;
      }
      else if (j == START_MIN_DIGIT_1)
        idx = DIGIT_SIZE * (mcClockNewTM / 10);
      else if (j == START_MIN_DIGIT_2)
        idx = DIGIT_SIZE * (mcClockNewTM % 10);
      else if (j == START_TIMEDATE_SEP - 1)
      {
        // Skip the entire separator as it is always blank
        cursorOk = MC_FALSE;
        j = START_DAY_DIGIT_1 - 1;
        continue;
      }
      else if (j == START_DAY_DIGIT_1)
        idx = DIGIT_SIZE * (mcClockNewDD / 10);
      else if (j == START_DAY_DIGIT_2)
        idx = DIGIT_SIZE * (mcClockNewDD % 10);
      else if (j == START_DAYMON_SEP)
        idx = CHAR_SLASH;
      else if (j == START_MON_DIGIT_1)
        idx = DIGIT_SIZE * (mcClockNewDM / 10);
      else if (j == START_MON_DIGIT_2)
        idx = DIGIT_SIZE * (mcClockNewDM % 10);

      // Get font element or a blank
      if (idx != CHAR_BLANK)
      {
        element = pgm_read_dword(charset + idx);
        idx++;
      }
      else
      {
        element = 0;
      }

      // Generate two lcd bytes based on the same font element, but each having
      // its own sinus table y offset
      for (k = 0; k < 2; k++)
      {
        // Get sinus y shift and move to preceding sinus table entry
        y = pgm_read_byte(yDelta + yDeltaIdx);
        yPrev = pgm_read_byte(yDelta + yDeltaPrevIdx);
        if (yDeltaIdx == 0)
          yDeltaIdx = WAVE_WAVE_LEN - 1;
        else
          yDeltaIdx--;
        if (yDeltaPrevIdx == 0)
          yDeltaPrevIdx = WAVE_WAVE_LEN - 1;
        else
          yDeltaPrevIdx--;

        // See if we have to write an lcd byte
        if (((y >> 3) > (i >> 3) && (yPrev >> 3) > (i >> 3)) ||
            (((y + 29) >> 3) < (i >> 3) || ((yPrev + 29) >> 3) < (i >> 3)))
        {
          // New and current lcd byte is not used so no need to write to lcd
          cursorOk = MC_FALSE;
        }
        else
        {
          // Set lcd cursor when we skipped a certain x area for the y-line
          if (cursorOk == MC_FALSE)
          {
            cursorOk = MC_TRUE;
            glcdSetAddress(BANNER_START_X + j * 2 + k, i >> 3);
          }

          // Extract lcd byte from sinus shifted font element and write to lcd
          if (i < 32)
            lcdByte = (element << y) >> i;
          else if (i - y >= 32)
            lcdByte = 0;
          else
            lcdByte = element >> (i - y);
          if (mcFgColor == GLCD_OFF)
            lcdByte = ~lcdByte;
          glcdDataWrite(lcdByte);
        }
      }
    }
  }

  // Set base offset in sinus table for next wave
  yDeltaPrevStart = yDeltaStart;
  if (yDeltaStart + WAVE_STEP > WAVE_WAVE_LEN - 1)
    yDeltaStart = yDeltaStart + WAVE_STEP - WAVE_WAVE_LEN;
  else
    yDeltaStart = yDeltaStart + WAVE_STEP;
}

//
// Function: waveInit
//
// Initialize the lcd display of a wave banner clock
//
void waveInit(u08 mode)
{
  DEBUGP("Init wave");
  glcdLine(0, 56, 127, 56);
}
