//*****************************************************************************
// Filename : 'anim.c'
// Title    : Main animation and drawing code driver for MONOCHRON clocks
//*****************************************************************************

#ifdef EMULIN
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#include "emulator/lcd.h"
#else
#include <util/delay.h>
#include "util.h"
#endif
#include "ks0108.h"
#include "ratt.h"
#include "glcd.h"
#include "anim.h"

// Include the supported clocks
#include "clock/analog.h"
#include "clock/bigdigit.h"
#include "clock/digital.h"
#include "clock/mosquito.h"
#include "clock/nerd.h"
#include "clock/pong.h"
#include "clock/puzzle.h"
#include "clock/qr.h"
#include "clock/slider.h"
#include "clock/cascade.h"
#include "clock/speeddial.h"
#include "clock/spiderplot.h"
#include "clock/trafficlight.h"

// The following ratt.c global variables are used to transfer the hardware
// state to a stable functional Monochron clock state.
// They are not to be used in individual Monochron clocks as their
// contents are considered unstable.
extern volatile uint8_t new_ts, new_tm, new_th;
extern volatile uint8_t new_dd, new_dm, new_dy;
extern volatile uint8_t alarmOn, alarming;

// The following mcXYZ global variables are for use in any Monochron clock.
// In a Monochron clock its contents are considered stable.

// Previous and new date/time (also ref mcClockTimeEvent)
volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;

// Indicates whether real time clock has changed since last check
volatile uint8_t mcClockTimeEvent = GLCD_FALSE;

// Indicates whether the alarm switch is On or Off
volatile uint8_t mcAlarmSwitch;

// Indicates whether the clock is alarming (including snoozing)
volatile uint8_t mcAlarming = GLCD_FALSE;

// The alarm time
volatile uint8_t mcAlarmH, mcAlarmM;

// The index in the Monochron clockdriver array
volatile uint8_t mcMchronClock = 0;

// Clock cycle ticker
volatile uint8_t mcCycleCounter = 0;

// Flag to force clocks to paint clock values
volatile uint8_t mcClockInit = GLCD_FALSE;

// The foreground and background colors of the BW LCD display.
// Upon changing the display mode their values are swapped.
// OFF = 0 = black color (=0x0 bit value in LCD memory)
// ON  = 1 = white color (=0x1 bit value in LCD memory)
volatile uint8_t mcFgColor;
volatile uint8_t mcBgColor;

// Runtime pointer to active clockdriver array
clockDriver_t *mcClockPool;

// Indicates whether the alarm switch has changed
volatile uint8_t mcUpdAlarmSwitch = GLCD_FALSE;

// Free for use variables for clocks
volatile uint8_t mcU8Util1;
volatile uint8_t mcU8Util2;
volatile uint8_t mcU8Util3;
volatile uint8_t mcU8Util4;
volatile uint16_t mcU16Util1;
volatile uint16_t mcU16Util2;
volatile uint16_t mcU16Util3;
volatile uint16_t mcU16Util4;

// Common labels for time/date elements
char animHour[] = "Hour";
char animMin[] = "Min";
char animSec[] = "Sec";
char animDay[] = "Day";
char animMonth[] = "Mon";
char animYear[] = "Year";

// The monochron array defines the clocks and their round-robin sequence
// as supported in the application.
// Based on the selection of clocks in monochron[], configure the source
// files in SRC in Makefile [firmware].
clockDriver_t monochron[] =
{
   {CHRON_CASCADE,     DRAW_INIT_FULL,    spotCascadeInit,    spotCascadeCycle,    0}
  ,{CHRON_SPEEDDIAL,   DRAW_INIT_PARTIAL, spotSpeedDialInit,  spotSpeedDialCycle,  0}
  ,{CHRON_SPIDERPLOT,  DRAW_INIT_PARTIAL, spotSpiderPlotInit, spotSpiderPlotCycle, 0}
  ,{CHRON_TRAFLIGHT,   DRAW_INIT_PARTIAL, spotTrafLightInit,  spotTrafLightCycle,  0}
  ,{CHRON_ANALOG_HMS,  DRAW_INIT_FULL,    analogHmsInit,      analogCycle,         0}
  ,{CHRON_ANALOG_HM,   DRAW_INIT_PARTIAL, analogHmInit,       analogCycle,         0}
  ,{CHRON_DIGITAL_HMS, DRAW_INIT_FULL,    digitalHmsInit,     digitalCycle,        0}
  ,{CHRON_DIGITAL_HM,  DRAW_INIT_PARTIAL, digitalHmInit,      digitalCycle,        0}
  //,{CHRON_MOSQUITO,    DRAW_INIT_FULL,    mosquitoInit,       mosquitoCycle,       0}
  //,{CHRON_NERD,        DRAW_INIT_FULL,    nerdInit,           nerdCycle,           0}
  //,{CHRON_PONG,        DRAW_INIT_FULL,    pongInit,           pongCycle,           pongButton}
  ,{CHRON_PUZZLE,      DRAW_INIT_FULL,    puzzleInit,         puzzleCycle,         puzzleButton}
  //,{CHRON_SLIDER,      DRAW_INIT_FULL,    sliderInit,         sliderCycle,         0}
  //,{CHRON_BIGDIG_TWO,  DRAW_INIT_FULL,    bigdigInit,         bigdigCycle,         bigdigButton}
  //,{CHRON_BIGDIG_ONE,  DRAW_INIT_PARTIAL, bigdigInit,         bigdigCycle,         bigdigButton}
  //,{CHRON_QR_HM,       DRAW_INIT_FULL,    qrInit,             qrCycle,             0}
  //,{CHRON_QR_HMS,      DRAW_INIT_PARTIAL, qrInit,             qrCycle,             0}
};

// Local function prototypes
void animAlarmSwitchCheck(void);
void animDateTimeCopy(void);

//
// Function: animAlarmSwitchCheck
//
// Check the position of the alarm switch versus the software state of
// the alarm info, resulting in a flag indicating whether the alarm info
// area must be updated.
//
void animAlarmSwitchCheck(void)
{
  // Reset pending functional alarm switch change
  mcUpdAlarmSwitch = GLCD_FALSE;

  if (alarmOn == GLCD_TRUE)
  {
    if (mcAlarmSwitch != ALARM_SWITCH_ON)
    {
      // Let the clock show the alarm time
      DEBUGP("Alarm info -> Alarm");
      mcAlarmSwitch = ALARM_SWITCH_ON;
      mcUpdAlarmSwitch = GLCD_TRUE;
    }
  }
  else
  {
    // Show the current date or a new date when we've passed midnight
    if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
         mcAlarmSwitch != ALARM_SWITCH_OFF)
    {
      // Let the clock show something else than the alarm time
      DEBUGP("Alarm info -> Other");
      mcAlarmSwitch = ALARM_SWITCH_OFF;
      mcUpdAlarmSwitch = GLCD_TRUE;
    }
  }
}

//
// Function: animClockButton
//
// Wrapper function for clocks to react to the Set and/or Plus button
// (when supported).
// Returns GLCD_TRUE when a button handler is configured for the clock. 
//
u08 animClockButton(u08 pressedButton)
{
  u08 retVal = GLCD_FALSE;
  clockDriver_t *clockDriver = &(mcClockPool[mcMchronClock]);

  if (clockDriver->button != 0)
  {
    // Sync alarming state for clock
    mcAlarming = alarming;
    
    // Execute the configured button function
    (*(clockDriver->button))(pressedButton);
    retVal = GLCD_TRUE;
  }
  return retVal;
}

//
// Function: animClockDraw
//
// Wrapper function for clocks to draw themselves
//
void animClockDraw(u08 mode)
{
  clockDriver_t *clockDriver = &(mcClockPool[mcMchronClock]);

  // Sync alarming state for clock
  mcAlarming = alarming;

  // If there's a time event, sync Monochron time with RTC
  if (mcClockTimeEvent == GLCD_TRUE)
  {
    DEBUGP("Update by time event");
    mcClockNewTS = new_ts;
    mcClockNewTM = new_tm;
    mcClockNewTH = new_th; 
    mcClockNewDD = new_dd;
    mcClockNewDM = new_dm;
    mcClockNewDY = new_dy;
  }

  // When a clock needs a full init, set the old date/time to something
  // harmless that can be used to undraw stuff: use current time
  if (mode == DRAW_INIT_FULL)
  {
    animDateTimeCopy();
  }

  // Have the clock initialize or update itself
  if (clockDriver->clockId != CHRON_NONE)
  {
    if (mode == DRAW_CYCLE)
    {
      // Update clock and sync old date/time to current for next comparison
      animAlarmSwitchCheck();
      (*(clockDriver->cycle))();
      if (mcClockTimeEvent == GLCD_TRUE)
      {
        animDateTimeCopy();
      }
      // Clear init flag (if set anyway) in case of first clock draw
      mcClockInit = GLCD_FALSE;
    }
    else // DRAW_INIT_FULL or DRAW_INIT_PARTIAL
    {
      // Do a (partial) clock initialization
      mcClockInit = GLCD_TRUE;
      (*(clockDriver->init))(mode);
    }
  }
  else
  {
    DEBUGP("Unknown clock in animClockDraw()");
  }
}

//
// Function: animClockNext
//
// Get next clock based on the current one and return its init type.
//
u08 animClockNext(void)
{
  // Select the next clock
  mcMchronClock++;
  if (mcMchronClock >= sizeof(monochron) / sizeof(clockDriver_t))
  {
    // End of clock pool reached: continue at beginning
    mcMchronClock = 0;
  }

  return mcClockPool[mcMchronClock].initType;
}

//
// Function: animDateTimeCopy
//
// Copy new date/time to old date/time
//
void animDateTimeCopy(void)
{
  mcClockOldTS = mcClockNewTS;
  mcClockOldTM = mcClockNewTM;
  mcClockOldTH = mcClockNewTH;
  mcClockOldDD = mcClockNewDD;
  mcClockOldDM = mcClockNewDM;
  mcClockOldDY = mcClockNewDY;
}

//
// Function: animValToStr
//
// Translate a h/m/s value into a two-digit character string.
//
void animValToStr(u08 value, char valString[])
{
  valString[0] = (char)(value / 10 + '0');
  valString[1] = (char)(value % 10 + '0');
  valString[2] = '\0';
}

//
// Function: animWelcome
//
// Give Monochron startup message
//
void animWelcome(void)
{
  // Give startup welcome message
  glcdPutStr2(33, 14, FONT_5X7N, "Welcome to", mcFgColor);
  glcdPutStr2(18, 30, FONT_5X7N, "-- T1 clocks --", mcFgColor);

#ifdef EMULIN
  lcdDeviceFlush(0);
  _delay_ms(1000);
#else
  _delay_ms(3000);
#endif
  beep(3750, 100);
  beep(4000, 100);
}

