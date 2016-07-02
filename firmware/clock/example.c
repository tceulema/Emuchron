//*****************************************************************************
// Filename : 'example.c'
// Title    : Animation code for a very simple example clock
//*****************************************************************************

// We need the following includes to build for Linux Emuchron / Atmel Monochron
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
#include "example.h"

// Get a subset of the global variables representing the Monochron state
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;

//
// Function: exampleCycle
//
// Update the lcd display of a very simple clock.
// This function is called every clock cycle (75 msec).
//
void exampleCycle(void)
{
  char dtInfo[9];

  // Use the generic method to update the alarm info in the clock.
  // This includes showing/hiding the alarm time upon flipping the alarm
  // switch as well as flashing the alarm time while alarming/snoozing.
  animAlarmAreaUpdate(2, 57, ALARM_AREA_ALM_ONLY);

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == GLCD_FALSE && mcClockInit == GLCD_FALSE)
    return;

  DEBUGP("Update Example");

  // Put new hour, min, sec in a string and paint it on the lcd
  animValToStr(mcClockNewTH, dtInfo);
  dtInfo[2] = ':';
  animValToStr(mcClockNewTM, &dtInfo[3]);
  dtInfo[5] = ':';
  animValToStr(mcClockNewTS, &dtInfo[6]);
  glcdPutStr2(41, 20, FONT_5X7N, dtInfo, mcFgColor);

  // Only paint the date when it has changed or when initializing the clock
  if (mcClockNewDD != mcClockOldDD || mcClockNewDM != mcClockOldDM ||
      mcClockNewDY != mcClockOldDY || mcClockInit == GLCD_TRUE)
  {
    // Put new month, day, year in a string and paint it on the lcd
    animValToStr(mcClockNewDM, dtInfo);
    dtInfo[2] = '/';
    animValToStr(mcClockNewDD, &dtInfo[3]);
    dtInfo[5] = '/';
    animValToStr(mcClockNewDY, &dtInfo[6]);
    glcdPutStr2(41, 29, FONT_5X7N, dtInfo, mcFgColor);
  }
}

//
// Function: exampleInit
//
// Initialize the lcd display of a very simple clock.
// This function is called once upon clock initialization.
//
void exampleInit(u08 mode)
{
  DEBUGP("Init Example");

  // Start from scratch by clearing the lcd using the background color
  glcdClearScreen(mcBgColor);

  // Paint a text on the lcd with 2x horizontal scaling
  glcdPutStr3(11, 2, FONT_5X7N, "-Example-", 2, 1, mcFgColor);

  // Force the alarm info area to init itself in animAlarmAreaUpdate()
  // upon the first call to exampleCycle()
  mcAlarmSwitch = ALARM_SWITCH_NONE;
}

