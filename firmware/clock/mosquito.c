//*****************************************************************************
// Filename : 'mosquito.c'
// Title    : Animation code for MONOCHRON mosquito clock
//*****************************************************************************

#include <math.h>
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
#include "mosquito.h"

// Info on hr/min/sec elements of mosquito clock
#define MOS_SEC_START_DELAY	30
#define MOS_MIN_START_DELAY	60
#define MOS_HOUR_START_DELAY	90
#define MOS_SEC_X_START		99
#define MOS_MIN_X_START		58
#define MOS_HOUR_X_START	17
#define MOS_TIME_Y_START	20
#define MOS_SEC_TXT_X_OFFSET	0
#define MOS_MIN_TXT_X_OFFSET	-1
#define MOS_HOUR_TXT_X_OFFSET	-2
#define MOS_TXT_Y_OFFSET	8
#define MOS_SEC_X_WIDTH		10
#define MOS_MIN_X_WIDTH		12
#define MOS_HOUR_X_WIDTH	15
#define MOS_TIME_Y_WIDTH	15

// Element speed and angle
#define MOS_ELEMENT_SPEED	1.5
#define MOS_DIRECTION_ANGLE_MIN	10

// Specifics for alarm/date info area
#define MOS_AD_BAR_Y_START	54
#define MOS_AD_X_START		2
#define MOS_AD_Y_START		(MOS_AD_BAR_Y_START + 3)
#define MOS_AD_WIDTH		24

// Structure defining the admin data for a time element indicator
typedef struct
{
  u08 startDelay;
  u08 posX;
  u08 posY;
  u08 width;
  float mathPosX;
  float mathPosY;
  float dx;
  float dy;
  s08 textOffset;
  char *text;
} timeElement_t;

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

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];
extern unsigned char *months[12];

// Local function prototypes
void mosquitoAlarmAreaUpdate(void);
void mosquitoDirectionSet(u16 seed);
void mosquitoElementDirectionSet(u16 angle, u08 quadrant, timeElement_t *element);
void mosquitoElementDraw(timeElement_t *element, u08 value);
void mosquitoElementMovePrep(timeElement_t *element);
u16 mosquitoRandGet(u16 seed);

// Init data for the hr/min/sec mosquite time elements
timeElement_t elementSecInit = 
{
  MOS_SEC_START_DELAY, MOS_SEC_X_START, MOS_TIME_Y_START, MOS_SEC_X_WIDTH,
  (float)MOS_SEC_X_START, (float)MOS_TIME_Y_START, 0, 0, MOS_SEC_TXT_X_OFFSET,
  animSec
};
timeElement_t elementMinInit = 
{
  MOS_MIN_START_DELAY, MOS_MIN_X_START, MOS_TIME_Y_START, MOS_MIN_X_WIDTH,
  (float)MOS_MIN_X_START, (float)MOS_TIME_Y_START, 0, 0, MOS_MIN_TXT_X_OFFSET,
  animMin
};
timeElement_t elementHourInit =
{
  MOS_HOUR_START_DELAY, MOS_HOUR_X_START, MOS_TIME_Y_START, MOS_HOUR_X_WIDTH,
  (float)MOS_HOUR_X_START, (float)MOS_TIME_Y_START, 0, 0, MOS_HOUR_TXT_X_OFFSET,
  animHour
};

// Runtime environment for the mosquito elements
timeElement_t elementSec;
timeElement_t elementMin;
timeElement_t elementHour;

// Random value for determining the direction of the elements
u16 randVal = 0xA5C3;

//
// Function: mosquitoCycle
//
// Update the LCD display of a mosquito clock
//
void mosquitoCycle(void)
{
  // Update alarm info in clock
  mosquitoAlarmAreaUpdate();

  if (mcClockTimeEvent == GLCD_TRUE &&
      (mcClockNewTM != mcClockOldTM || mcClockNewTH != mcClockOldTH))
  {
    // Each minute change the direction of the elements
    randVal = mosquitoRandGet(randVal);
    mosquitoDirectionSet(randVal);
  }

  // Question: Why not move all elements in every clock cycle?
  // Answer: Well my friend, from a cpu point of view we're fast enough to draw
  // all elements each cycle. However, the lcd display response is so slow that
  // moving each element in every clock cycle makes the time barely readable
  // (especially when the display is inversed (=black on white)).
  // In other words: the lcd display has a very bad response time for its pixels.
  // So, it's a trade-off between eye candy + eye strain versus slowly moving
  // elements + actually being able to read the time. I've chosen for the latter.
  if ((mcCycleCounter & 1) == 1)
  {
    if (elementSec.startDelay > 0)
      elementSec.startDelay--;
    else
      mosquitoElementMovePrep(&elementSec);

    if (elementMin.startDelay > 0)
      elementMin.startDelay--;
    else
      mosquitoElementMovePrep(&elementMin);

    if (elementHour.startDelay > 0)
      elementHour.startDelay--;
    else
      mosquitoElementMovePrep(&elementHour);
  }

  // Redraw all time elements regardless whether changed or not to
  // countereffect distorted elements that are overlapped by others
  if ((mcCycleCounter & 1) == 1 || mcClockTimeEvent == GLCD_TRUE)
  {
    mosquitoElementDraw(&elementSec, mcClockNewTS);
    mosquitoElementDraw(&elementMin, mcClockNewTM);
    mosquitoElementDraw(&elementHour, mcClockNewTH);
  }
}

//
// Function: mosquitoInit
//
// Initialize the LCD display of a mosquito clock
//
void mosquitoInit(u08 mode)
{
  DEBUGP("Init Mosquito");

  // Draw static clock layout
  glcdClearScreen(mcBgColor);
  glcdFillRectangle(0, MOS_AD_BAR_Y_START, GLCD_XPIXELS, 1, mcFgColor);

  // Init the several time graphic elements
  elementSec = elementSecInit;
  elementMin = elementMinInit;
  elementHour = elementHourInit;

  // Force the alarm info area to init itself
  mcAlarmSwitch = ALARM_SWITCH_NONE;
  mcU8Util1 = GLCD_FALSE;

  // Init the initial direction of each element
  randVal = mosquitoRandGet(randVal);
  mosquitoDirectionSet(randVal);
}

//
// Function: mosquitoAlarmAreaUpdate
//
// Draw update in mosquito clock alarm area
//
void mosquitoAlarmAreaUpdate(void)
{
  u08 inverseAlarmArea = GLCD_FALSE;
  u08 newAlmDisplayState = GLCD_FALSE;
  u08 pxDone = 0;
  char msg[5];

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
      pxDone = glcdPutStr2(MOS_AD_X_START, MOS_AD_Y_START, FONT_5X5P, msg, mcFgColor);
    }
    else
    {
      // Prior to showing the date clear border of inverse alarm area (if needed)
      if (mcU8Util1 == GLCD_TRUE)
      {
        glcdRectangle(MOS_AD_X_START - 1, MOS_AD_Y_START - 1, 19, 7, mcBgColor);
        mcU8Util1 = GLCD_FALSE;
      }

      // Show date
      msg[0] = ' ';
      pxDone = glcdPutStr2(MOS_AD_X_START, MOS_AD_Y_START, FONT_5X5P,
        (char *)months[mcClockNewDM - 1], mcFgColor) + MOS_AD_X_START;
      animValToStr(mcClockNewDD, &(msg[1]));
      pxDone = pxDone + glcdPutStr2(pxDone, MOS_AD_Y_START, FONT_5X5P, msg, mcFgColor) -
        MOS_AD_X_START;
    }

    // Clean up any trailing remnants of previous text
    if (pxDone < MOS_AD_WIDTH)
      glcdFillRectangle(MOS_AD_X_START + pxDone, MOS_AD_Y_START,
        MOS_AD_WIDTH - pxDone, 5, mcBgColor);
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
    glcdFillRectangle2(MOS_AD_X_START - 1, MOS_AD_Y_START - 1, 19, 7,
      ALIGN_AUTO, FILL_INVERSE, mcBgColor);
}

//
// Function: mosquitoDirectionSet
//
// Set the direction of the elements
//
void mosquitoDirectionSet(u16 seed)
{
  u16 angle;

  // Direction for sec element
  angle = seed % (90 - MOS_DIRECTION_ANGLE_MIN * 2) +
    MOS_DIRECTION_ANGLE_MIN;
  mosquitoElementDirectionSet(angle, (seed >> 3) % 4, &elementSec);

  // Direction for min element
  angle = ((seed >> 5) + (seed << 12)) % (90 - MOS_DIRECTION_ANGLE_MIN * 2) +
    MOS_DIRECTION_ANGLE_MIN;
  mosquitoElementDirectionSet(angle, (seed >> 7) % 4, &elementMin);

  // Direction for hour element
  angle = ((seed >> 9) + (seed << 7)) % (90 - MOS_DIRECTION_ANGLE_MIN * 2) +
    MOS_DIRECTION_ANGLE_MIN;
  mosquitoElementDirectionSet(angle, (seed >> 13) % 4, &elementHour);
}

//
// Function: mosquitoElementDirectionSet
//
// Set the direction of a single time element
//
void mosquitoElementDirectionSet(u16 angle, u08 quadrant, timeElement_t *element)
{
  float elementRad;

  // New direction for the time element
  elementRad = (float)(angle + 90 * quadrant) / 180L * M_PI;
  element->dx = sin(elementRad) * MOS_ELEMENT_SPEED;
  element->dy = -cos(elementRad) * MOS_ELEMENT_SPEED;
}

//
// Function: mosquitoElementDraw
//
// Draw time element in mosquito clock
//
void mosquitoElementDraw(timeElement_t *element, u08 value)
{
  char msg[3];
  u08 pxDone;

  animValToStr(value, msg);

  // Draw element value
  glcdPutStr2(element->posX, element->posY, FONT_5X7N, msg, mcFgColor);
  // Draw border around element value 
  glcdRectangle(element->posX - 1, element->posY - 1, 13, 9, mcBgColor);

  // Draw element text
  pxDone = glcdPutStr2(element->posX + element->textOffset,
    element->posY + MOS_TXT_Y_OFFSET, FONT_5X5P, element->text, mcFgColor);
  // Draw border around element text 
  glcdRectangle(element->posX + element->textOffset - 1,
    element->posY + MOS_TXT_Y_OFFSET - 1, pxDone + 1, 7, mcBgColor);
}

//
// Function: mosquitoElementMovePrep
//
// Set new position of element in mosquito clock and remove stuff that
// won't be overwritten by the element redraw
//
void mosquitoElementMovePrep(timeElement_t *element)
{
  float mathPosXNew = element->mathPosX + element->dx;
  float mathPosYNew = element->mathPosY + element->dy;
  s08 dx, dy;

  // Check bouncing on left and right wall
  if (mathPosXNew + element->textOffset - 1.01 <= 0L)
  {
    mathPosXNew = -(mathPosXNew + 2 * element->textOffset - 2.02);
    element->dx = -element->dx;
  }
  else if (mathPosXNew + element->textOffset + element->width + 1.01 >= GLCD_XPIXELS)
  {
    mathPosXNew = mathPosXNew -
      2 * (mathPosXNew + element->textOffset + element->width + 1.01 - GLCD_XPIXELS);
    element->dx = -element->dx;
  }

  // Check bouncing on top and bottom wall
  if (mathPosYNew - 1.01 <= 1)
  {
    mathPosYNew = -(mathPosYNew - 2.02) + 1;
    element->dy = -element->dy;
  }
  else if (mathPosYNew + 13.01 >= MOS_AD_BAR_Y_START)
  {
    mathPosYNew = MOS_AD_BAR_Y_START - (mathPosYNew + 13.01 - MOS_AD_BAR_Y_START) - 13.01;
    element->dy = -element->dy;
  }

  // Clear parts that are to be removed upon redraw
  dx = (s08)mathPosXNew - (s08)element->posX;
  dy = (s08)mathPosYNew - (s08)element->posY;

  if (dx > 1)
  {
    // Clear left side of element value and text
    glcdFillRectangle(element->posX, element->posY, (u08)(dx - 1), 7, mcBgColor);
    glcdFillRectangle(element->posX + element->textOffset, element->posY + 8,
      (u08)(dx - 1), 5, mcBgColor);
  }
  else if (dx < -1)
  {
    // Clear right side of element value and text
    glcdFillRectangle(element->posX + 12 - (-dx), element->posY, (u08)(-dx - 1), 7, mcBgColor);
    glcdFillRectangle(element->posX + element->textOffset + element->width - (-dx) + 1,
      element->posY + 8, (u08)(-dx - 1), 5, mcBgColor);
  }
  if (dy > 1)
  {
    // Clear top side of element value and text
    glcdFillRectangle(element->posX, element->posY, 11, (u08)(dy - 1), mcBgColor);
    glcdFillRectangle(element->posX + element->textOffset, element->posY + 8,
      element->width, (u08)(dy - 1), mcBgColor);
  }
  else if (dy < -1)
  {
    // Clear bottom side of element value and text
    glcdFillRectangle(element->posX, element->posY + 8 - (-dy), 11, (u08)(-dy - 1), mcBgColor);
    glcdFillRectangle(element->posX + element->textOffset, element->posY + 13 - ((-dy) - 1),
      element->width, (u08)(-dy - 1), mcBgColor);
  }

  // Sync new position of element
  element->posX = (u08)mathPosXNew;
  element->posY = (u08)mathPosYNew;
  element->mathPosX = mathPosXNew;
  element->mathPosY = mathPosYNew;
}

//
// Function: mosquitoRandGet
//
// A simple random generator of (most likely) abysmal quality
//
u16 mosquitoRandGet(u16 seed)
{
  u16 retVal =
    (seed << 4) + (seed >> 6) +
    ((u16)mcClockOldTS << 8) + ~mcClockOldTS +
    ((u16)mcClockOldTM << 7) + ((u16)mcClockOldTM << 2) + ((u16)~mcClockOldTM >> 1) +
    ((u16)~mcClockOldTH << 6) + ~mcClockOldTH +
    ((u16)~mcClockNewTS << 5) + ((u16)mcClockNewTS << 4) + ((u16)~mcClockNewTS >> 2) +
    ((u16)mcClockNewTM << 4) + ~mcClockNewTM +
    ((u16)mcClockNewTH << 3) + ((u16)mcClockNewTH << 6) + ((u16)~mcClockNewTH >> 3) +
    ((u16)mcAlarmH << 8) + ~mcAlarmH +
    ((u16)~mcAlarmM << 8) + mcAlarmM +
    ((u16)mcCycleCounter << 8) + ~mcCycleCounter;
  return retVal;
}

