//*****************************************************************************
// Filename : 'analog.c'
// Title    : Animation code for MONOCHRON analog clock
//*****************************************************************************

#include <math.h>
#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../monomain.h"
#include "analog.h"

// Specifics for analog clock main dial
#define ANA_X_START			64
#define ANA_Y_START			31
#define ANA_RADIUS			30
#define ANA_DOT_RADIUS			(ANA_RADIUS - 1.9)
#define ANA_SEC_RADIUS_LINE		(ANA_RADIUS - 3.9)
#define ANA_SEC_RADIUS_ARROW		(ANA_RADIUS - 2.3)
#define ANA_MIN_RADIUS			(ANA_RADIUS - 2.9)
#define ANA_HOUR_RADIUS			(ANA_RADIUS - 9.9)
#define ANA_SEC_LEG_RADIUS		(ANA_RADIUS - 7.0)
#define ANA_MIN_LEG_RADIUS		8
#define ANA_HOUR_LEG_RADIUS		5
#define ANA_SEC_STEPS			60.0
#define ANA_MIN_STEPS			60.0
#define ANA_HOUR_STEPS			12.0
#define ANA_SEC_LEG_RADIAL_OFFSET	(0.1)
#define ANA_MIN_LEG_RADIAL_OFFSET	(2.0 * M_PI / 2.5)
#define ANA_HOUR_LEG_RADIAL_OFFSET	(2.0 * M_PI / 2.5)

// Specifics for alarm and date element areas
#define ANA_ALARM_X_START		118
#define ANA_ALARM_Y_START		54
#define ANA_ALARM_RADIUS		7
#define ANA_ALARM_MIN_RADIUS		(ANA_ALARM_RADIUS)
#define ANA_ALARM_HOUR_RADIUS		(ANA_ALARM_RADIUS - 2)
#define ANA_DATE_X_START		2
#define ANA_DATE_Y_START		57

// Determine the second indicator shape.
// 0 = Needle
// 1 = Floating arrow
#define ANA_SEC_TYPE		1

// Determine how the second indicator moves.
// 0 = Only at a full second stop
// 1 = Whenever the (x,y) position of a leg changes
#define ANA_SEC_MOVE		1

// Determine how the minute arrow moves.
// 0 = Only at a full minute stop
// 1 = Whenever the (x,y) position of the arrow tip changes
#define ANA_MIN_MOVE		1

// Monochron environment variables
extern volatile uint8_t mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcAlarmSwitchEvent;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockTimeEvent;

// Holds the current normal/inverted state of the alarm area
extern volatile uint8_t mcU8Util1;
// Clock cycle counter since last time event/init that is used for smooth
// motion behavior of second indicator
extern volatile uint8_t mcU8Util2;
// Indicator whether to show the second needle/arrow
extern volatile uint8_t mcU8Util3;

// Local function prototypes
static void analogAlarmAreaUpdate(void);
static u08 analogElementCalc(s08 position[], s08 positionNew[], float radial,
  float radialOffset, u08 arrowRadius, u08 legRadius, u08 legsCheck);
static void analogElementDraw(s08 position[]);
static void analogElementSync(s08 position[], s08 positionNew[]);
static void analogInit(u08 mode);

// Arrays holding the [x,y] positions of the three arrow points for the hour
// and minute arrows and the seconds needle
// arr[0+1] = x,y arrow endpoint
// arr[2+3] = x,y arrow leg endpoint 1
// arr[4+5] = x,y arrow leg endpoint 2
static s08 posSec[6];
static s08 posMin[6];
static s08 posHour[6];

//
// Function: analogCycle
//
// Update the lcd display of a very simple analog clock
//
void analogCycle(void)
{
  // Update alarm info in clock
  analogAlarmAreaUpdate();

  if (ANA_SEC_MOVE == 0)
  {
    // Only if a time event or init is flagged we need to update the clock
    if (mcClockTimeEvent == MC_FALSE && mcClockInit == MC_FALSE)
      return;
    DEBUGP("Update Analog");
  }
  else
  {
    // Smooth second indicator so we may update every clock cycle
    if (mcClockTimeEvent == MC_TRUE || mcClockInit == MC_TRUE)
    {
      DEBUGP("Update Analog");
      mcU8Util2 = 0;
    }
    else
    {
      mcU8Util2++;
    }
  }

  // Local data
  float radElement;
  s08 posSecNew[6], posMinNew[6], posHourNew[6];
  u08 secElementChanged = MC_FALSE;
  u08 minElementChanged = MC_FALSE;
  u08 hourElementChanged = MC_FALSE;

  // Verify changes in date
  animADAreaUpdate(ANA_DATE_X_START, ANA_DATE_Y_START, AD_AREA_DATE_ONLY);

  // Calculate (potential) changes in seconds needle
  if (mcU8Util3 == MC_TRUE)
  {
    if (ANA_SEC_MOVE == 0)
    {
      // Move at time event (once per second or init)
      radElement = (2.0 * M_PI / ANA_SEC_STEPS) * (float)mcClockNewTS;
    }
    else
    {
      // Move when leg position changes
      radElement = (2.0 * M_PI / ANA_SEC_STEPS) * (float)mcClockNewTS +
        (2.0 * M_PI / ANA_SEC_STEPS / (1000.0 / ANIM_TICK_CYCLE_MS + 0.5)) *
        mcU8Util2;
    }

    if (ANA_SEC_TYPE == 0)
    {
      // Needle indicator
      secElementChanged = analogElementCalc(posSec, posSecNew, radElement,
        0.0, ANA_SEC_RADIUS_LINE, 0, 2);
    }
    else
    {
      // Floating arrow indicator
      secElementChanged = analogElementCalc(posSec, posSecNew, radElement,
        ANA_SEC_LEG_RADIAL_OFFSET, ANA_SEC_RADIUS_ARROW, ANA_SEC_LEG_RADIUS, 6);
    }
  }

  if (mcClockTimeEvent == MC_TRUE || mcClockInit == MC_TRUE)
  {
    // Calculate (potential) changes in minute arrow
    if (ANA_MIN_MOVE == 0)
    {
      // Move once per minute
      radElement = (2.0 * M_PI / ANA_MIN_STEPS) * (float)mcClockNewTM;
    }
    else
    {
      // Move when tip position changes
      radElement = (2.0 * M_PI / ANA_MIN_STEPS) * (float)mcClockNewTM  +
        (2.0 * M_PI / ANA_SEC_STEPS / ANA_MIN_STEPS) * (float)mcClockNewTS;
    }
    minElementChanged = analogElementCalc(posMin, posMinNew, radElement,
      ANA_MIN_LEG_RADIAL_OFFSET, ANA_MIN_RADIUS, ANA_MIN_LEG_RADIUS, 2);

    // Calculate (potential) changes in hour arrow. In normal operation only
    // change the hour arrow if the minute arrow moves as well.
    // Note: Include progress in minutes during the hour.
    if (minElementChanged == MC_TRUE || mcClockOldTH != mcClockNewTH ||
        mcClockInit == MC_TRUE)
    {
      radElement = (2.0 * M_PI / ANA_HOUR_STEPS) * (float)(mcClockNewTH % 12) +
        (2.0 * M_PI / ANA_MIN_STEPS / ANA_HOUR_STEPS) * (float)mcClockNewTM;
      hourElementChanged = analogElementCalc(posHour, posHourNew, radElement,
        ANA_HOUR_LEG_RADIAL_OFFSET, ANA_HOUR_RADIUS, ANA_HOUR_LEG_RADIUS, 6);
    }
  }

  // Redraw seconds needle if needed
  if (mcU8Util3 == MC_TRUE &&
      (secElementChanged == MC_TRUE || mcClockInit == MC_TRUE))
  {
    // Remove the old seconds needle, sync with new position and redraw
    glcdColorSetBg();
    analogElementDraw(posSec);
    analogElementSync(posSec, posSecNew);
    glcdColorSetFg();
    analogElementDraw(posSec);
  }

  // Redraw minute arrow if needed
  if (minElementChanged == MC_TRUE || mcClockInit == MC_TRUE)
  {
    // Remove the old minute arrow, sync with new position and redraw
    glcdColorSetBg();
    analogElementDraw(posMin);
    analogElementSync(posMin, posMinNew);
    glcdColorSetFg();
    analogElementDraw(posMin);

    // Redraw the seconds needle as it got distorted by the minute arrow draw
    if (mcU8Util3 == MC_TRUE)
      analogElementDraw(posSec);
  }
  else if (secElementChanged == MC_TRUE)
  {
    // The minute arrow has not changed but the sec needle has.
    // Redraw the minute arrow as it got distorted by the seconds needle draw.
    analogElementDraw(posMin);
  }

  // Redraw hour arrow only if needed
  if (hourElementChanged == MC_TRUE || mcClockInit == MC_TRUE)
  {
    // Remove the old hour arrow, sync with new position and redraw
    glcdColorSetBg();
    analogElementDraw(posHour);
    analogElementSync(posHour, posHourNew);
    glcdColorSetFg();
    analogElementDraw(posHour);

    // Redraw the seconds needle and minute arrow as they get distorted by the
    // arrow redraw
    if (mcU8Util3 == MC_TRUE)
      analogElementDraw(posSec);
    analogElementDraw(posMin);
  }
  else if (secElementChanged == MC_TRUE || minElementChanged == MC_TRUE)
  {
    // The hour arrow has not changed but the seconds needle and/or minute
    // arrow has.
    // Redraw the hour arrow as it got distorted by the other draws.
    analogElementDraw(posHour);
  }
}

//
// Function: analogHmInit
//
// Initialize the lcd display of a very simple analog clock with Hour and
// Minutes arrow
//
void analogHmInit(u08 mode)
{
  mcU8Util3 = MC_FALSE;
  analogInit(mode);
}

//
// Function: analogHmsInit
//
// Initialize the lcd display of a very simple analog clock with Hour and
// Minutes arrow and Seconds needle
//
void analogHmsInit(u08 mode)
{
  mcU8Util3 = MC_TRUE;
  analogInit(mode);
}

//
// Function: analogAlarmAreaUpdate
//
// Draw update in analog clock alarm area
//
static void analogAlarmAreaUpdate(void)
{
  u08 newAlmDisplayState = MC_FALSE;

  // Detect change in displaying alarm
  if (mcAlarmSwitchEvent == MC_TRUE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Show alarm time in small clock
      s08 dxM, dyM, dxH, dyH;
      float radM, radH;

      // Prepare the analog alarm clock
      radM = (2.0 * M_PI / ANA_MIN_STEPS) * (float)mcAlarmM;
      dxM = (s08)(sin(radM) * ANA_ALARM_MIN_RADIUS);
      dyM = (s08)(-cos(radM) * ANA_ALARM_MIN_RADIUS);
      radH = (2.0 * M_PI / ANA_HOUR_STEPS) * (float)(mcAlarmH % 12) +
        (2.0 * M_PI / ANA_MIN_STEPS / ANA_HOUR_STEPS) * (float)mcAlarmM;
      dxH = (s08)(sin(radH) * ANA_ALARM_HOUR_RADIUS);
      dyH = (s08)(-cos(radH) * ANA_ALARM_HOUR_RADIUS);

      // Show the alarm time
      glcdCircle2(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_RADIUS,
        CIRCLE_FULL);
      glcdLine(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_X_START + dxM,
        ANA_ALARM_Y_START + dyM);
      glcdLine(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_X_START + dxH,
        ANA_ALARM_Y_START + dyH);
    }
    else
    {
      // Clear alarm area
      glcdFillCircle2(ANA_ALARM_X_START, ANA_ALARM_Y_START,
        ANA_ALARM_RADIUS + 1, FILL_BLANK);
      mcU8Util1 = MC_FALSE;
    }
  }

  // Set alarm blinking state in case we're alarming
  if (mcAlarming == MC_TRUE && (mcCycleCounter & 0x08) == 8)
    newAlmDisplayState = MC_TRUE;

  // Make alarm area blink during alarm or cleanup after end of alarm
  if (newAlmDisplayState != mcU8Util1)
  {
    // Inverse the alarm area
    mcU8Util1 = newAlmDisplayState;
    glcdFillCircle2(ANA_ALARM_X_START, ANA_ALARM_Y_START, ANA_ALARM_RADIUS + 1,
      FILL_INVERSE);
  }
}

//
// Function: analogElementCalc
//
// Calculate the position of a needle or three points of an analog clock arrow
//
static u08 analogElementCalc(s08 position[], s08 positionNew[], float radial,
  float radialOffset, u08 arrowRadius, u08 legRadius, u08 legsCheck)
{
  u08 i;

  // Calculate the new position of a needle or each of the three arrow points
  positionNew[0] = (s08)(sin(radial) * arrowRadius) + ANA_X_START;
  positionNew[1] = (s08)(-cos(radial) * arrowRadius) + ANA_Y_START;
  positionNew[2] = (s08)(sin(radial + radialOffset) * legRadius) + ANA_X_START;
  positionNew[3] = (s08)(-cos(radial + radialOffset) * legRadius) + ANA_Y_START;
  positionNew[4] = (s08)(sin(radial - radialOffset) * legRadius) + ANA_X_START;
  positionNew[5] = (s08)(-cos(radial - radialOffset) * legRadius) + ANA_Y_START;

  // Provide info if the needle or arrow has changed position
  for (i = 0; i < legsCheck; i++)
  {
    if (position[i] != positionNew[i])
      return MC_TRUE;
  }

  return MC_FALSE;
}

//
// Function: analogElementDraw
//
// Draw an arrow or needle in the analog clock. Depending on the active draw
// color it is drawn or removed.
//
static void analogElementDraw(s08 position[])
{
  // An arrow consists of three points, so draw lines between each of them. If
  // it turns out to be a needle only draw the first line.
  glcdLine(position[0], position[1], position[2], position[3]);
  if (position[2] != ANA_X_START || position[3] != ANA_Y_START)
  {
    // We're dealing with an arrow so draw the other two lines
    glcdLine(position[0], position[1], position[4], position[5]);
    glcdLine(position[2], position[3], position[4], position[5]);
  }
}

//
// Function: analogElementSync
//
// Sync the current needle or arrow position with the new one
//
static void analogElementSync(s08 position[], s08 positionNew[])
{
  u08 i;
  u08 posLimit;

  // For the seconds needle we don't want to copy leg info
  if (position[2] == ANA_X_START && position[3] == ANA_Y_START)
    posLimit = 2;
  else
    posLimit = 6;

  for (i = 0; i < posLimit; i++)
    position[i] = positionNew[i];
}

//
// Function: analogInit
//
// Initialize the lcd display of an analog clock
//
static void analogInit(u08 mode)
{
  s08 i, dxDot, dyDot;

  DEBUGP("Init Analog");

  if (mode == DRAW_INIT_FULL)
  {
    // Draw static clock layout
    glcdCircle2(ANA_X_START, ANA_Y_START, ANA_RADIUS, CIRCLE_FULL);
    glcdDot(ANA_X_START, ANA_Y_START);

    // Paint 5-minute and 15 minute markers in clock
    for (i = 0; i < 12; i++)
    {
      // The 5-minute markers
      dxDot = (s08)(sin(2.0 * M_PI / 12.0 * i) * ANA_DOT_RADIUS);
      dyDot = (s08)(-cos(2.0 * M_PI / 12.0 * i) * ANA_DOT_RADIUS);
      glcdDot(ANA_X_START + dxDot, ANA_Y_START + dyDot);

      // The additional 15-minute markers
      if (i % 3 == 0)
      {
        if (i == 0)
          dyDot--;
        else if (i == 3)
          dxDot++;
        else if (i == 6)
          dyDot++;
        else
          dxDot--;
        glcdDot(ANA_X_START + dxDot, ANA_Y_START + dyDot);
      }
    }

    // Init the arrow point position arrays with harmless values inside the
    // clock area
    for (i = 0; i < 6; i++)
    {
      posSec[i] = 40;
      posMin[i] = 40;
      posHour[i] = 40;
    }

    // The following inits force the seconds element to become a needle
    if (ANA_SEC_TYPE == 0)
    {
      posSec[2] = ANA_X_START;
      posSec[3] = ANA_Y_START;
    }

    // Init alarm blink state
    mcU8Util1 = MC_FALSE;
  }
  else if (mcU8Util3 == MC_FALSE)
  {
    // Assume this is a partial init from an analog HMS clock to an analog HM
    // clock. So, we should remove the seconds needle.
    glcdColorSetBg();
    analogElementDraw(posSec);
    glcdColorSetFg();

    // Restore dot at center of clock
    glcdDot(ANA_X_START, ANA_Y_START);
  }
}
