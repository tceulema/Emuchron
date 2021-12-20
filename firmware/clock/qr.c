//*****************************************************************************
// Filename : 'qr.c'
// Title    : Animation code for MONOCHRON QR clock
//*****************************************************************************

#include <string.h>
#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../ks0108.h"
#include "qrencode.h"
#include "qr.h"

//
// Let's first do some basic math and apply common sense.
// This clock displays a QR redundancy 1 (L), level 2 (25x25) QR, allowing to
// encode a text string up to 32 characters.
// An initial estimate of calculating and drawing a QR from scratch shows this
// will take about 0.25 seconds of raw Atmel CPU power (avr-gcc 4.8.1 with
// Emuchron v3.0 code base).
// If this were to be done in a single clock cycle, that is scheduled to last
// up to 75 msec, the button user interface and blinking elements such as the
// alarm time, would freeze in that period. From a UI perspective this is not
// acceptable.
// To overcome this behavior we need to split up the QR generation process in
// chunks where each chunk is executed in a single clock cycle, limited by its
// 75 msec duration. So, the QR generation process must be put into a state
// that the clock code will use to execute a manageable chunck of work.
// Splitting up the CPU workload over multiple clock cycles means that we need
// to wait more time before the actual QR is drawn on the lcd display, but we
// won't have any UI lag, and that's what matters most. We must make sure
// though that each chunk of work fits within a single clock cycle of 75 msec.
// There is another benefit of splitting up the CPU load over clock cycles.
// The number of clock cycles needed to generate the QR will always be the same
// and therefor always a constant x times 75 msec cycles. In addition to that,
// the last step, being the QR Draw, requires an almost constant amount of CPU
// time regardless the encoded string, making the QR always appear at the same
// moment between consecutive seconds. This is good UI.
//
// For a single QR 8 different masks are tried (evaluated), and the best mask
// will be used for displaying the QR. A mask is a method of dispersing the
// data over the QR area. The quality of a mask is determined by looking at how
// good or bad the black and white pixels are spread evenly over the QR. The
// most CPU consuming element by far in trying a mask is to determine that
// good/badness.
//
// For our QR generation process the following split-up is implemented using a
// process state variable. Each single process state is processed in a single
// clock cycle of 75 msec:
// 0    - Idle (no QR generation active).
// 1    - Init QR generation process and try mask 0+1.
// 2    - Try mask 2+5.
// 3    - Try mask 3+6.
// 4    - Try mask 4+7, apply best mask and complete QR.
// 5    - Draw QR.
//
// Using a debug version of the firmware we can find out how much CPU time each
// state has left from its 75 msec cycle. Note that this time also includes
// interrupt handler time (1-msec handler, RTC handler, button handler) and
// blinking the alarm time during alarming. The latter appears to cost somewhat
// more than 1 msec. However, it also includes time to send the debug strings
// over the FTDI port and it is therefore believed that the actual numbers per
// cycle are slightly higher than shown here, so consider them worst-case
// scenario values.
// The following numbers are obtained using avr-gcc 4.8.1 (Debian 8) and the
// Emuchron v3.0 code base while the clock is alarming:
//
// State    Min time      Avg time
//         left (msec)   left (msec)
//   1         9            10.8
//   2        21            22.8
//   3        20            21.2
//   4        13            17.1
//   5        63            63.8
//
// Seeing the result for state 2 and 3, trying a single mask will take on
// average about 27 msec of raw CPU power. State 1 is the most CPU intensive,
// but having 9 msec left within its cycle as a worst-case is well within cycle
// 'safety limits'.
//
// So, how long will it take to generate and display a QR from scratch?
// We need in total 5 clock cycles. Cycles 1..4  will take 75 msec each.
// Drawing the QR in cycle 5 takes about 11 msec to complete.
// This means that a total of 4 x 0.075 + 0.011 = 0.311 seconds is required.
// You will notice this timelag upon initializing a QR clock.
//

// Specifics for QR clock
#define QR_ALARM_X_START	2
#define QR_ALARM_Y_START	57
#define QR_X_START		39
#define QR_Y_START		7
#define QR_BORDER		4
#define QR_PIX_FACTOR		2

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent, mcClockDateEvent;
extern volatile uint8_t mcFgColor;
extern volatile uint8_t mcMchronClock;
extern clockDriver_t *mcClockPool;

// Day of the week and month text strings
extern char *animMonths[12];
extern char *animDays[7];

// Data interface to the QR encode module
extern unsigned char strinbuf[];
extern unsigned char qrframe[];

// mcU8Util1 holds the state (=active chunk) of the QR generation process as
// described above
extern volatile uint8_t mcU8Util1;

// mcU8Util2 contains the clockId of the active clock
// CHRON_QR_HM  - Draw QR every minute
// CHRON_QR_HMS - Draw QR every second
extern volatile uint8_t mcU8Util2;

// On april 1st, instead of the date, encode the message below. If you don't
// like it make the textstring empty (""), and the clock will ignore it.
// Note: The length of the message below will be truncated after 23 chars
// when in HMS mode and after 26 chars when in HM mode.
static char *qrAprilFools = "The cake is a lie.";

// Local function prototypes
static void qrDraw(void);
static void qrMarkerDraw(u08 x, u08 y);

//
// Function: qrCycle
//
// Update the lcd display of a QR clock
//
void qrCycle(void)
{
  // Update alarm info in clock
  animADAreaUpdate(QR_ALARM_X_START, QR_ALARM_Y_START, AD_AREA_ALM_ONLY);

  // Only when a time event, init or QR cycle is flagged we need to update the
  // clock
  if (mcClockTimeEvent == MC_FALSE && mcClockInit == MC_FALSE &&
      mcU8Util1 == 0)
    return;

  if (mcU8Util1 == 0)
  {
    DEBUGP("Update QR");
  }

  // Verify changes in date+time
  if (mcClockTimeEvent == MC_TRUE || mcClockInit == MC_TRUE)
  {
    if (mcU8Util2 == CHRON_QR_HMS || mcClockInit == MC_TRUE ||
        mcClockNewTH != mcClockOldTH || mcClockNewTM != mcClockOldTM ||
        mcClockDateEvent == MC_TRUE)
    {
      // Something has changed in date+time forcing us to update the QR
      char *dow;
      char *mon;
      u08 offset = 0;
      u08 i = 0;

      // Set the text to encode.
      // On first line add "HH:MM" or "HH:MM:SS".
      animValToStr(mcClockNewTH, (char *)strinbuf);
      strinbuf[2] = ':';
      animValToStr(mcClockNewTM, (char *)&strinbuf[3]);
      if (mcU8Util2 == CHRON_QR_HMS)
      {
        // HMS clock so add seconds
        strinbuf[5] = ':';
        animValToStr(mcClockNewTS, (char *)&strinbuf[6]);
        offset = 3;
      }
      strinbuf[5 + offset] = '\n';

      // Add date or special message on april 1st
      if (mcClockNewDD == 1 && mcClockNewDM == 4 && qrAprilFools[0] != '\0')
      {
        // Add special message on april 1st
        memcpy(&strinbuf[offset + 6], qrAprilFools, strlen(qrAprilFools) + 1);
      }
      else
      {
        // Add date "DDD MMM dd, 20YY".
        // Put the three chars of day of the week and month in QR string.
        dow = (char *)animDays[calDotw(mcClockNewDM, mcClockNewDD,
          mcClockNewDY)];
        mon = (char *)animMonths[mcClockNewDM - 1];
        for (i = 0; i < 3; i++)
        {
          strinbuf[i + offset + 6] = dow[i];
          strinbuf[i + offset + 10] = mon[i];
        }

        // Fill up with spaces
        strinbuf[9 + offset]  = ' ';
        strinbuf[13 + offset] = ' ';

        // Put day in QR string while excluding prefix 0
        if (mcClockNewDD < 10)
        {
          strinbuf[14 + offset] = '0' + mcClockNewDD;
          offset--;
        }
        else
        {
          animValToStr(mcClockNewDD, (char *)&strinbuf[14 + offset]);
        }

        // Fill up with comma and space
        strinbuf[16 + offset] = ',';
        strinbuf[17 + offset] = ' ';

        // Put year in QR string
        animValToStr(20, (char *)&strinbuf[18 + offset]);
        animValToStr(mcClockNewDY, (char *)&strinbuf[20 + offset]);
      }

      // Start first cycle in generation of QR
      mcU8Util1 = 1;
    }
  }

  // Check the state of the QR generation process and take appropriate action
  if (mcU8Util1 == 1)
  {
    // Init QR generation and try mask 0+1
    qrGenInit();
    qrMaskTry(0);
    qrMaskTry(1);
    // Set state for next QR generation cycle
    mcU8Util1++;
  }
  else if (mcU8Util1 == 2 || mcU8Util1 == 3)
  {
    // Try two out of 4 QR masks.
    // Mask combination for a state: 2+5, 3+6.
    qrMaskTry(mcU8Util1);
    qrMaskTry(mcU8Util1 + 3);
    // Set state for next QR generation cycle
    mcU8Util1++;
  }
  else if (mcU8Util1 == 4)
  {
    // Try mask 4+7 and apply the best QR mask found
    qrMaskTry(4);
    qrMaskTry(7);
    qrMaskApply();
    // Set state for next QR generation cycle
    mcU8Util1++;
  }
  else if (mcU8Util1 == 5)
  {
    // Draw the QR
    qrDraw();
    // We're all done for this QR so next state is QR idle
    mcU8Util1 = 0;
  }
}

//
// Function: qrInit
//
// Initialize the lcd display of a QR clock
//
void qrInit(u08 mode)
{
  DEBUGP("Init QR");

  // Get the clockId
  mcU8Util2 = mcClockPool[mcMchronClock].clockId;

  // Start from scratch
  if (mode == DRAW_INIT_FULL)
  {
    if (mcFgColor == GLCD_OFF)
    {
      // Draw a black border around the QR clock
      glcdColorSet(GLCD_OFF);
      glcdRectangle(QR_X_START - QR_BORDER - 1, QR_Y_START - QR_BORDER - 1,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER + 2,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER + 2);
    }
    else
    {
      // Draw a white area for the QR clock
      glcdColorSet(GLCD_ON);
      glcdFillRectangle(QR_X_START - QR_BORDER, QR_Y_START - QR_BORDER,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER);
    }

    // Draw elements of QR that need to be drawn only once
    qrMarkerDraw(QR_X_START, QR_Y_START);
    qrMarkerDraw(QR_X_START, QR_Y_START + 18 * QR_PIX_FACTOR);
    qrMarkerDraw(QR_X_START + 18 * QR_PIX_FACTOR, QR_Y_START);
    glcdColorSet(GLCD_OFF);
    glcdRectangle(QR_X_START + 16 * QR_PIX_FACTOR,
      QR_Y_START + 16 * QR_PIX_FACTOR, 10, 10);
    glcdRectangle(QR_X_START + 16 * QR_PIX_FACTOR + 1,
      QR_Y_START + 16 * QR_PIX_FACTOR + 1, 8, 8);
    glcdRectangle(QR_X_START + 18 * QR_PIX_FACTOR,
      QR_Y_START + 18 * QR_PIX_FACTOR, 2, 2);
  }
  else
  {
    // Clear the QR area except the markers
    glcdColorSet(GLCD_ON);
    glcdFillRectangle(QR_X_START + 8 * QR_PIX_FACTOR, QR_Y_START,
      9 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR + 1);
    glcdFillRectangle(QR_X_START, QR_Y_START + 8 * QR_PIX_FACTOR,
      16 * QR_PIX_FACTOR, 10 * QR_PIX_FACTOR);
    glcdFillRectangle(QR_X_START + 16 * QR_PIX_FACTOR,
      QR_Y_START + 8 * QR_PIX_FACTOR, 9 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR);
    glcdFillRectangle(QR_X_START + 8 * QR_PIX_FACTOR,
      QR_Y_START + 18 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR);
    glcdFillRectangle(QR_X_START + 21 * QR_PIX_FACTOR,
      QR_Y_START + 16 * QR_PIX_FACTOR, 4 * QR_PIX_FACTOR, 9 * QR_PIX_FACTOR);
    glcdFillRectangle(QR_X_START + 16 * QR_PIX_FACTOR,
      QR_Y_START + 21 * QR_PIX_FACTOR, 5 * QR_PIX_FACTOR, 5 * QR_PIX_FACTOR);
  }
  glcdColorSetFg();

  // Set initial QR generation state to idle
  mcU8Util1 = 0;
}

//
// Function: qrDraw
//
// Draw the complete QR on the LCD. Each QR dot is 2x2 pixels.
// The simple way to do this is to use glcdFillRectangle() for each QR dot.
// However, drawing 625 QR dots is inefficient and will take more that 0.3 sec
// to complete. Not good. Instead, we'll use dedicated code that will not
// require to read from the lcd and will only write full lcd bytes filled with
// multiple QR dots. The code also applies hardcoded shortcuts preventing
// unnecessary write actions to the LCD.
// The code uses similar techniques implemented in the glcd.c [firmware]
// library. It turns out it draws the QR in about 11 msec. Compared to using
// the simple glcdFillRectangle() solution that's pretty fast.
//
// WARNING: For reasons of efficiency, the code makes assumptions on the y
// start location, the size factor of the QR and the QR border in both normal
// and inverse display mode. If you change the value of the definitions for
// QR_Y_START, QR_PIX_FACTOR and QR_BORDER you must modify this function as
// well. Changing QR_X_START should be ok (but why would you want to do that?)
//
static void qrDraw(void)
{
  // Dedicated code requires lots of things to administer
  u08 x = 0;
  u08 y = 0;
  u08 xStart = 0;
  u08 xEnd = 0;
  u08 yPos = 0;
  u08 yByte = 0;
  u08 lcdByte = 0;
  u08 template = 0;
  u08 bitPosStart = 0;
  u08 bitPosEnd = 0;
  u08 doBit = 0;

  // Process all lcd y byte rows
  while (y < WD * 2)
  {
    // Get the lcd y byte row and determine what bit pixels to fill
    yByte = (y + QR_Y_START) / 8;
    bitPosStart = (y + QR_Y_START) % 8;
    if (WD * QR_PIX_FACTOR - y < 8)
      bitPosEnd = WD * QR_PIX_FACTOR - y;
    else
      bitPosEnd = 7;

    // Set an lcd byte template to modify
    if (yByte == 0)
    {
      // Lcd top row pixels
      if (mcFgColor == GLCD_OFF)
        template = 0x7b;
      else
        template = 0x78;
    }
    else if (yByte == 7)
    {
      // Lcd bottom row pixels
      if (mcFgColor == GLCD_OFF)
        template = 0xde;
      else
        template = 0x1e;
    }
    else
    {
      template = 0;
    }

    // Avoid areas that do not need to be redrawn for the QR
    if (yByte < 2)
    {
      // Only draw in between the two top markers
      xStart = 8 * QR_PIX_FACTOR;
      xEnd = 17 * QR_PIX_FACTOR;
    }
    else if (yByte > 5)
    {
      // Only draw to the right of the bottom left marker
      xStart = 8 * QR_PIX_FACTOR;
      xEnd = WD * QR_PIX_FACTOR;
    }
    else
    {
      // We need to draw everything
      xStart = 0;
      xEnd = WD * QR_PIX_FACTOR;
    }

    // Write consecutive lcd bytes starting from this point
    glcdSetAddress(xStart + QR_X_START, yByte);

    // Process two (QR_PIX_FACTOR) lcd bytes at a time since they
    // have identical pixels to be drawn.
    x = xStart;
    while (x < xEnd)
    {
      // Get lcd byte template
      lcdByte = template;

      // Add bits to lcd byte
      yPos = y;
      for (doBit = bitPosStart; doBit <= bitPosEnd; doBit++)
      {
        // A QR defaults to black on white: QR value 0 means white
        // and value 1 means black. This is the inverse of Monochron
        // colors where 0=Off=black and 1=On=white.
        // The QR template bits are set to 0, so only when we see a
        // white QR dot we need to set the bit in the lcd byte.
        if (QRBIT((x >> 1),(yPos >> 1)) == 0)
        {
          // Set pixel bit to 1
          lcdByte |= _BV(doBit);
        }
        yPos++;
      }

      // Write the byte to lcd twice (QR_PIX_FACTOR)
      glcdDataWrite(lcdByte);
      glcdDataWrite(lcdByte);

      // Next lcd bytes
      x = x + QR_PIX_FACTOR;
    }

    // Next lcd byte row
    y = y + 8 - bitPosStart;
  }
}

//
// Function: qrMarkerDraw
//
// Draw a fixed QR marker element
//
static void qrMarkerDraw(u08 x, u08 y)
{
  glcdColorSet(GLCD_OFF);
  glcdRectangle(x, y, 14, 14);
  glcdRectangle(x + 1, y + 1, 12, 12);
  glcdFillRectangle(x + 4, y + 4, 6, 6);
  glcdColorSetFg();
}
