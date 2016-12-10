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
#include "../monomain.h"
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
#define MOS_AD_Y_START		(MOS_AD_BAR_Y_START + 3)
#define MOS_DATE_X_START	2
#define MOS_ALARM_X_START	109
#define MOS_DATE_X_SIZE		23

// Structure defining the admin data for a time element indicator
typedef struct
{
  u08 startDelay;	// Start move delay in clock cycles
  u08 posX;		// Actual x position of element on display
  u08 posY;		// Actual y position of element on display
  u08 width;		// Width of text of time element (hour/min/sec)
  float mathPosX;	// Mathematical x position of element
  float mathPosY;	// Mathematical y position of element
  float dx;		// The x delta per move step
  float dy;		// The y delta per move step
  s08 textOffset;	// The relative x starting point of element text
  char *text;		// The element text (hour/min/sec)
} timeElement_t;

extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
extern char *animMonths[12];

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];

// Random value for determining the direction of the elements
static u16 mosRandBase = M_PI * M_PI * 1000;
static const float mosRandSeed = 3.9147258617;
static u16 mosRandVal = 0xA5C3;

// Init data for the hr/min/sec mosquite time elements
static timeElement_t elementSecInit = 
{
  MOS_SEC_START_DELAY, MOS_SEC_X_START, MOS_TIME_Y_START, MOS_SEC_X_WIDTH,
  (float)MOS_SEC_X_START, (float)MOS_TIME_Y_START, 0, 0, MOS_SEC_TXT_X_OFFSET,
  animSec
};
static timeElement_t elementMinInit = 
{
  MOS_MIN_START_DELAY, MOS_MIN_X_START, MOS_TIME_Y_START, MOS_MIN_X_WIDTH,
  (float)MOS_MIN_X_START, (float)MOS_TIME_Y_START, 0, 0, MOS_MIN_TXT_X_OFFSET,
  animMin
};
static timeElement_t elementHourInit =
{
  MOS_HOUR_START_DELAY, MOS_HOUR_X_START, MOS_TIME_Y_START, MOS_HOUR_X_WIDTH,
  (float)MOS_HOUR_X_START, (float)MOS_TIME_Y_START, 0, 0, MOS_HOUR_TXT_X_OFFSET,
  animHour
};

// Runtime environment for the mosquito elements
static timeElement_t elementSec;
static timeElement_t elementMin;
static timeElement_t elementHour;

// Local function prototypes
static void mosquitoDirectionSet(void);
static void mosquitoElementDirectionSet(timeElement_t *element);
static void mosquitoElementDraw(timeElement_t *element, u08 value);
static void mosquitoElementMovePrep(timeElement_t *element);

//
// Function: mosquitoCycle
//
// Update the LCD display of a mosquito clock
//
void mosquitoCycle(void)
{
  // Update alarm/date info in clock
  animAlarmAreaUpdate(MOS_ALARM_X_START, MOS_AD_Y_START, ALARM_AREA_ALM_ONLY);

  // Verify changes in date
  if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
      mcClockInit == GLCD_TRUE)
  {
    u08 pxDone;
    char msg[4];

    // Show new date and clean up potential remnants
    pxDone = glcdPutStr2(MOS_DATE_X_START, MOS_AD_Y_START, FONT_5X5P,
      (char *)animMonths[mcClockNewDM - 1], mcFgColor) + MOS_DATE_X_START;
    msg[0] = ' ';
    animValToStr(mcClockNewDD, &msg[1]);
    pxDone = pxDone +
      glcdPutStr2(pxDone, MOS_AD_Y_START, FONT_5X5P, msg, mcFgColor) -
      MOS_DATE_X_START;
    if (pxDone <= MOS_DATE_X_SIZE)
      glcdFillRectangle(MOS_DATE_X_START + pxDone, MOS_AD_Y_START,
        MOS_DATE_X_SIZE - pxDone + 1, FILL_BLANK, mcBgColor);
  }

  // Each minute change the direction of the elements
  if (mcClockTimeEvent == GLCD_TRUE &&
      (mcClockNewTM != mcClockOldTM || mcClockNewTH != mcClockOldTH))
    mosquitoDirectionSet();

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
  glcdFillRectangle(0, MOS_AD_BAR_Y_START, GLCD_XPIXELS, 1, mcFgColor);

  // Init the several time graphic elements
  elementSec = elementSecInit;
  elementMin = elementMinInit;
  elementHour = elementHourInit;

  // Force the alarm info area to init itself
  mcAlarmSwitch = ALARM_SWITCH_NONE;

  // Init the initial direction of each element
  mosquitoDirectionSet();
}

//
// Function: mosquitoDirectionSet
//
// Set the direction of the elements
//
static void mosquitoDirectionSet(void)
{
  mosquitoElementDirectionSet(&elementSec);
  mosquitoElementDirectionSet(&elementMin);
  mosquitoElementDirectionSet(&elementHour);
}

//
// Function: mosquitoElementDirectionSet
//
// Set the direction of a single time element
//
static void mosquitoElementDirectionSet(timeElement_t *element)
{
  float elementRad;
  u16 angle;

  // Generate a random number of most likely abysmal quality
  mosRandBase = (int)(mosRandSeed * (mosRandVal + mcClockNewTM) * 213);
  mosRandVal = mcCycleCounter * mosRandSeed + mosRandBase;

  // Get an angle while preventing too shallow/steep values
  angle = mosRandVal % (90 - MOS_DIRECTION_ANGLE_MIN * 2) + 
    MOS_DIRECTION_ANGLE_MIN;

  // New direction for the time element by putting angle in a quadrant
  elementRad = (float)(angle + 90 * (((mosRandVal >> 3) + angle) % 4)) /
    180L * M_PI;
  element->dx = sin(elementRad) * MOS_ELEMENT_SPEED;
  element->dy = -cos(elementRad) * MOS_ELEMENT_SPEED;
}

//
// Function: mosquitoElementDraw
//
// Draw time element in mosquito clock
//
static void mosquitoElementDraw(timeElement_t *element, u08 value)
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
static void mosquitoElementMovePrep(timeElement_t *element)
{
  float mathPosXNew = element->mathPosX + element->dx;
  float mathPosYNew = element->mathPosY + element->dy;
  s08 dx, dy;
  u08 textPosX;

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
    mathPosYNew = MOS_AD_BAR_Y_START -
      (mathPosYNew + 13.01 - MOS_AD_BAR_Y_START) - 13.01;
    element->dy = -element->dy;
  }

  // Clear parts that are to be removed upon redraw
  dx = (s08)mathPosXNew - (s08)element->posX;
  dy = (s08)mathPosYNew - (s08)element->posY;

  // Shortcut for calcs below
  textPosX = element->posX + element->textOffset;

  if (dx > 1)
  {
    // Clear left side of element value and text
    glcdFillRectangle(element->posX, element->posY, (u08)(dx - 1), 7,
      mcBgColor);
    glcdFillRectangle(textPosX, element->posY + 8, (u08)(dx - 1), 5,
      mcBgColor);
  }
  else if (dx < -1)
  {
    // Clear right side of element value and text
    glcdFillRectangle((u08)(element->posX + 12 + dx), element->posY,
      (u08)(-dx - 1), 7, mcBgColor);
    glcdFillRectangle((u08)(textPosX + element->width + dx + 1),
      element->posY + 8, (u08)(-dx - 1), 5, mcBgColor);
  }
  if (dy > 1)
  {
    // Clear top side of element value and text
    glcdFillRectangle(element->posX, element->posY, 11, (u08)(dy - 1),
      mcBgColor);
    glcdFillRectangle(textPosX, element->posY + 8, element->width,
      (u08)(dy - 1), mcBgColor);
  }
  else if (dy < -1)
  {
    // Clear bottom side of element value and text
    glcdFillRectangle(element->posX, element->posY + 8 + dy, 11,
      (u08)(-dy - 1), mcBgColor);
    glcdFillRectangle(textPosX, (u08)(element->posY + 13 - (-dy - 1)),
      element->width, (u08)(-dy - 1), mcBgColor);
  }

  // Sync new position of element
  element->posX = (u08)mathPosXNew;
  element->posY = (u08)mathPosYNew;
  element->mathPosX = mathPosXNew;
  element->mathPosY = mathPosYNew;
}

