//*****************************************************************************
// Filename : 'qr.c'
// Title    : Animation code for MONOCHRON QR clock
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
#include "qr.h"
#include "qrencode.h"

// Specifics for QR clock
#define QR_ALARM_X_START	2
#define QR_ALARM_Y_START	57
#define QR_X_START		39
#define QR_Y_START		7
#define QR_BORDER		4
#define QR_PIX_FACTOR		2

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

// Day of the week and month text strings
extern unsigned char *months[12];
extern unsigned char *days[7];

// Data interface to the QR encode module
extern unsigned char strinbuf[];
extern unsigned char qrframe[];

// Let's first do some basic math and apply common sense.
// This clock displays a QR redundancy 1 (L), level 2 (25x25) QR, allowing to
// encode a string up to 32 characters in its text.
// An initial estimate of calculating and drawing a QR from scratch shows this
// will take about 0.3 seconds of Atmel CPU power.
// If this were to be done in a single clock cycle, that is scheduled to last up
// to 75 msec, the button user interface and blinking elements, such as the alarm
// time, would freeze in that period. From a UI perspective this is not
// acceptable.
// To overcome this behavior we will split up the QR generation process in chunks
// where each chunk is executed in a single clock cycle, limited by its 75 msec
// duration. So, the QR generation process must be put into a state that the
// clock code will use to execute a manageable amount of work to be done for
// generating a QR. Splitting up the CPU workload over multiple clock cycles
// means that we need to wait more time before the actual QR is drawn on the LCD
// display, but we won't have any UI lag, and that's what matters most. We must
// make sure though that each chunk of work fits in a single clock cycle of 75
// msec.
// There is another benefit of splitting up the CPU load over clock cycles.
// The number of clock cycles needed to generate the QR is always the same and
// therefor always a constant x times 75 msec cycles. In adition to that, the
// last step, being the QR Draw, requires an almost constant amount of CPU
// regardless the encoded string, making the QR always appear at the same moment
// between consecutive seconds. This is good UI.
//
// For a single QR 8 different masks are tried (evaluated), and the best mask
// will be used for displaying the QR. A mask is a method of dispersing the data
// over the QR area. The quality of a mask is determined by looking at how good
// or bad the black and white pixels are spread over the QR. The most time
// consuming element in trying a mask is to determine that goodness/badness of a
// mask.
// For our QR generation process the following split-up is implemented using a
// process state variable. Each single process state is processed in a single
// clock cycle:
// 0    - Idle (no QR generation active).
// 1    - Init QR generation process and try mask 0.
// 2..4 - Try mask 1..6 (6 in total). Each state will try 2 masks.
// 5    - Try mask 7, apply best mask and complete QR.
// 6    - Draw QR.
//
// Using an initial debug version of the firmware we can find out how much CPU
// time each mask try will take to complete. Note that this time also includes
// interrupt handler time (1-msec handler, RTC handler, button handler). However,
// it also includes time to send the debug strings over the FTDI port and it is
// therefore believed that the actual numbers per cycle are slightly lower than
// shown here, so consider them worst-case scenario values.
//
// CPU time to complete a single mask (+/- 1 msec), using avr-gcc 4.3.5.
// Mask 0: (not relevant; can easily be combined with other tasks in state 1)
// Mask 1: 30 msec
// Mask 2: 29 msec
// Mask 3: 30 msec
// Mask 4: 30 msec
// Mask 5: 33 msec
// Mask 6: 33 msec
// Mask 7: (not relevant; can easily be combined with other tasks in state 5)
//
// We can see from this that mask 5 and 6 take the longest to complete. Therefor
// combining these two masks in the same calculation state should be avoided.
// It is chosen that in state 2, 3 and 4 we will combine resp. mask 1+4, 2+5 and
// 3+6, spreading the relatively long CPU time of mask 5 and 6 over separate
// states.
// Combining two mask calculations in a single clock cycle of 75 msec, where
// state 4 (combining mask 3+6) will consume the most CPU, leaves us about 15 to
// 12 msec spare CPU time for other tasks.
// In practice there is only one task remaining, which is inverting the alarm
// time when alarming/snoozing. It turns out this takes about 5 msec to complete
// and may appear in one of the cycles as additional time cost. Even including
// this additional 5 msec in a cycle there is still a small time buffer left to
// complete the cycle within 75 msec. A debug version shows that the minimum time
// left for state 4 during alarming/snoozing state was never lower than 6 msec.
// This is not much but it is well within the given timeframe we have available.
//
// So, how long will it take to calculate and display a QR from scratch?
// We need in total 6 clock cycles. Cycles 1..5 will take 75 msec each. A debug
// version shows that displaying a QR, in cycle 6, takes about 13 msec.
// This means that a total of 5 x 0.075 + 0.013 = 0.388 seconds is needed.
// You will notice this timelag upon initializing a QR clock.

// mcU8Util2 holds the state (=active chunk) of the QR generation process as
// described above
extern volatile uint8_t mcU8Util2;

// mcU8Util3 contains the clockId of the active clock
// CHRON_QR_HM  - Draw QR every minute
// CHRON_QR_HMS - Draw QR every second
extern volatile uint8_t mcU8Util3;

// Local function prototypes
void qrAlarmAreaUpdate(void);
void qrDraw(void);
void qrMarkerDraw(u08 x, u08 y);

// On april 1st, instead of the date, encode the message below. If you don't
// like it make the textstring empty (""), and the clock will ignore it.
// Note: The length of the message below will be truncated after 23 chars
// when in HMS mode and after 26 chars when in HM mode.
char *msgAprilFools = "The cake is a lie.";

//
// Function: qrCycle
//
// Update the LCD display of a QR clock
//
void qrCycle(void)
{
  // Update alarm info in clock
  qrAlarmAreaUpdate();

  // Only if a time event, init or QR cycle is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE &&
      mcU8Util2 == 0)
    return;

  if (mcU8Util2 == 0)
  {
    DEBUGP("Update QR");
  }

  // Verify changes in date+time
  if (mcClockTimeEvent == GLCD_TRUE || mcClockInit == GLCD_TRUE)
  {
    if (mcU8Util3 == CHRON_QR_HMS || mcClockInit == GLCD_TRUE ||
        mcClockNewTH != mcClockOldTH || mcClockNewTM != mcClockOldTM ||
        mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
        mcClockNewDY != mcClockOldDY)
    {
      // Something has changed in date+time forcing us to update the QR
      char *dow;
      char *mon;
      u08 offset = 0;
      u08 i = 0;

      // Set the text to encode
      // On first line add "HH:MM" or "HH:MM:SS"
      animValToStr(mcClockNewTH, (char *)strinbuf);
      strinbuf[2] = ':';
      animValToStr(mcClockNewTM, (char *)&(strinbuf[3]));
      if (mcU8Util3 == CHRON_QR_HMS)
      {
        // HMS clock so add seconds
        strinbuf[5] = ':';
        animValToStr(mcClockNewTS, (char *)&(strinbuf[6]));
        offset = 3;
      }
      strinbuf[5 + offset] = '\n';

      // Add date or special message on april 1st
      if (mcClockNewDD == 1 && mcClockNewDM == 4 && msgAprilFools[0] != '\0')
      {
        // Add special message on april 1st
        for (i = 0; msgAprilFools[i] != '\0'; i++)
        {
          strinbuf[i + offset + 6] = msgAprilFools[i];
        }
        strinbuf[i + offset + 6] = '\0';
      }
      else
      {
        // Add date "DDD MMM dd, 20YY"
        // Put the three chars of day of the week and month in QR string
        dow = (char *)days[dotw(mcClockNewDM, mcClockNewDD, mcClockNewDY)];
        mon = (char *)months[mcClockNewDM - 1];
        for (i = 0; i < 3; i++)
        {
          strinbuf[i + offset + 6] = dow[i];
          strinbuf[i + offset + 10] = mon[i];
        }

        // Put day in QR string
        animValToStr(mcClockNewDD, (char *)&(strinbuf[14 + offset]));

        // Put year in QR string
        animValToStr(20, (char *)&(strinbuf[18 + offset]));
        animValToStr(mcClockNewDY, (char *)&(strinbuf[20 + offset]));

        // Fill up with spaces and comma
        strinbuf[9 + offset]  = ' ';
        strinbuf[13 + offset] = ' ';
        strinbuf[16 + offset] = ',';
        strinbuf[17 + offset] = ' ';
      }

      // Start first cycle in generation of QR
      mcU8Util2 = 1;
    }
  }

  // Check the state of the QR generation process and take appropriate action
  if (mcU8Util2 == 1)
  {
    // Init QR generation and try the first mask (= mask 0)
    qrGenInit();
    qrMaskTry(0);
    // Set state for next QR generation cycle
    mcU8Util2++;
  }
  else if (mcU8Util2 >= 2 && mcU8Util2 <= 4)
  {
    // Try two of 6 QR masks (1..6)
    // Mask combination for a state: 1+4, 2+5, 3+6
    qrMaskTry(mcU8Util2 - 1);
    qrMaskTry(mcU8Util2 + 2);
    // Set state for next QR generation cycle
    mcU8Util2++;
  }
  else if (mcU8Util2 == 5)
  {
    // Try mask 7 and apply the best QR mask found
    qrMaskTry(7);
    qrMaskApply();
    // Set state for next QR generation cycle
    mcU8Util2++;
  }
  else if (mcU8Util2 == 6)
  {
    // Draw the QR
    qrDraw();
    // We're all done for this QR so next state is QR idle
    mcU8Util2 = 0;
  }
}

//
// Function: qrInit
//
// Initialize the LCD display of a QR clock
//
void qrInit(u08 mode)
{
  DEBUGP("Init QR");

  // Get the clockId
  mcU8Util3 = mcClockPool[mcMchronClock].clockId;

  // Start from scratch
  if (mode == DRAW_INIT_FULL)
  {
    glcdClearScreen(mcBgColor);

    if (mcBgColor == ON)
    {
      // Draw a black border around the QR clock
      glcdRectangle(QR_X_START - QR_BORDER - 1, QR_Y_START - QR_BORDER - 1,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER + 2,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER + 2,
        OFF);
    }
    else
    {
      // Draw a white border for the QR clock
      glcdFillRectangle(QR_X_START - QR_BORDER, QR_Y_START - QR_BORDER,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER,
        QR_PIX_FACTOR * WD + 2 * QR_BORDER,
        ON);
    }

    // Draw elements of QR that need to be drawn only once
    qrMarkerDraw(QR_X_START, QR_Y_START);
    qrMarkerDraw(QR_X_START, QR_Y_START + 18 * QR_PIX_FACTOR);
    qrMarkerDraw(QR_X_START + 18 * QR_PIX_FACTOR, QR_Y_START);
    glcdRectangle(QR_X_START + 16 * QR_PIX_FACTOR, QR_Y_START + 16 * QR_PIX_FACTOR,
      10, 10, OFF);
    glcdRectangle(QR_X_START + 16 * QR_PIX_FACTOR + 1, QR_Y_START + 16 * QR_PIX_FACTOR + 1,
      8, 8, OFF);
    glcdRectangle(QR_X_START + 18 * QR_PIX_FACTOR, QR_Y_START + 18 * QR_PIX_FACTOR,
      2, 2, OFF);
  }
  else
  {
    // Clear the QR area except the markers
    glcdFillRectangle(QR_X_START + 8 * QR_PIX_FACTOR, QR_Y_START,
      9 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR + 1, ON);
    glcdFillRectangle(QR_X_START, QR_Y_START + 8 * QR_PIX_FACTOR,
      16 * QR_PIX_FACTOR, 10 * QR_PIX_FACTOR, ON);
    glcdFillRectangle(QR_X_START + 16 * QR_PIX_FACTOR, QR_Y_START + 8 * QR_PIX_FACTOR,
      9 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR, ON);
    glcdFillRectangle(QR_X_START + 8 * QR_PIX_FACTOR, QR_Y_START + 18 * QR_PIX_FACTOR,
      8 * QR_PIX_FACTOR, 8 * QR_PIX_FACTOR, ON);
    glcdFillRectangle(QR_X_START + 21 * QR_PIX_FACTOR, QR_Y_START + 16 * QR_PIX_FACTOR,
      4 * QR_PIX_FACTOR, 9 * QR_PIX_FACTOR, ON);
    glcdFillRectangle(QR_X_START + 16 * QR_PIX_FACTOR, QR_Y_START + 21 * QR_PIX_FACTOR,
      5 * QR_PIX_FACTOR, 5 * QR_PIX_FACTOR, ON);
  }

  // Force the alarm info area to init itself
  mcAlarmSwitch = ALARM_SWITCH_NONE;
  mcU8Util1 = GLCD_FALSE;

  // Set initial QR generation state to idle
  mcU8Util2 = 0;
}

//
// Function: qrAlarmAreaUpdate
//
// Draw update in QR clock alarm area
//
void qrAlarmAreaUpdate(void)
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
      glcdPutStr2(QR_ALARM_X_START, QR_ALARM_Y_START, FONT_5X5P, msg, mcFgColor);
    }
    else
    {
      // Clear area (remove alarm time)
      glcdFillRectangle(QR_ALARM_X_START - 1, QR_ALARM_Y_START - 1, 19, 7,
        mcBgColor);
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
    glcdFillRectangle2(QR_ALARM_X_START - 1, QR_ALARM_Y_START - 1, 19, 7,
      ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

//
// Function: qrDraw
//
// Draw the complete QR on the LCD. Each QR dot is 2x2 pixels.
// The simple way to do this is to use glcdFillRectangle() for each QR dot.
// However, drawing 625 QR dots is inefficient and will take more that 0.6 sec
// to complete. Not good. Instead, we'll use dedicated code that will not require
// to read from the LCD and will only write full LCD bytes filled with multiple
// QR dots. The code also applies hardcoded shortcuts preventing unnecessary
// write actions to the LCD.
// The code uses similar techniques implemented in the glcd.c [firmware] library.
// It turns out it draws the QR in about 13 msec. Compared to using the simple
// glcdFillRectangle() solution that's pretty fast.
//
// WARNING: For reasons of efficiency, the code makes assumptions on the y start
// location, the size factor of the QR and the QR border in both normal and
// inverse display mode. If you change the value of the definitions for
// QR_Y_START, QR_PIX_FACTOR and QR_BORDER you must modify this function as
// well. Changing QR_X_START should be ok (but why would you want to do that?)
//
void qrDraw(void)
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
      if (mcBgColor == OFF)
        template = 0x78;
      else
        template = 0x7b;
    }
    else if (yByte == 7)
    {
      // Lcd bottom row pixels
      if (mcBgColor == OFF)
        template = 0x1e;
      else
        template = 0xde;
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
void qrMarkerDraw(u08 x, u08 y)
{
  glcdRectangle(x, y, 14, 14, OFF);
  glcdRectangle(x + 1, y + 1, 12, 12, OFF);
  glcdFillRectangle(x + 4, y + 4, 6, 6, OFF);
}
