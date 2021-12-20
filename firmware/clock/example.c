//*****************************************************************************
// Filename : 'example.c'
// Title    : Animation code for a very simple example clock
//*****************************************************************************

// We need the following includes to build for Atmel Monochron / Linux Emuchron
#include "../global.h"		// Main Monochron/Emuchron defs
#include "../glcd.h"		// High-level graphics function defs
#include "../anim.h"		// Animation util function defs
#include "example.h"		// Our clock init/cycle function defs

// Get a subset of the global variables representing the Monochron state.
// For more info, refer to section 2.5 in Emuchron_Manual.pdf [support].
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcClockTimeEvent, mcClockDateEvent;

//
// Function: exampleCycle
//
// Update the lcd display of a very simple clock.
// This function is called every application clock cycle (75 msec).
// At this point the draw color is set to the foreground color.
//
void exampleCycle(void)
{
  char dtInfo[9];

  // Use the generic method to update the alarm info in the clock.
  // This includes showing/hiding the alarm time upon flipping the alarm
  // switch as well as flashing the alarm time while alarming/snoozing.
  animADAreaUpdate(2, 57, AD_AREA_ALM_ONLY);

  // Only if a time event or init is flagged we need to update the clock
  if (mcClockTimeEvent == MC_FALSE && mcClockInit == MC_FALSE)
    return;

  DEBUGP("Update Example");

  // Put new hour, min, sec in a string and paint it on the lcd
  animValToStr(mcClockNewTH, dtInfo);
  dtInfo[2] = ':';
  animValToStr(mcClockNewTM, &dtInfo[3]);
  dtInfo[5] = ':';
  animValToStr(mcClockNewTS, &dtInfo[6]);
  glcdPutStr2(41, 20, FONT_5X7M, dtInfo);

  // Only paint the date when it has changed or when initializing the clock
  if (mcClockDateEvent == MC_TRUE || mcClockInit == MC_TRUE)
  {
    // Put new month, day, year in a string and paint it on the lcd
    animValToStr(mcClockNewDM, dtInfo);
    dtInfo[2] = '/';
    animValToStr(mcClockNewDD, &dtInfo[3]);
    dtInfo[5] = '/';
    animValToStr(mcClockNewDY, &dtInfo[6]);
    glcdPutStr2(41, 29, FONT_5X7M, dtInfo);
  }
}

//
// Function: exampleInit
//
// Initialize the lcd display of a very simple clock.
// This function is called once upon clock initialization.
// At this point the display has already been cleared and the draw color is set
// to the foreground color.
//
void exampleInit(u08 mode)
{
  DEBUGP("Init Example");

  // Paint a text on the lcd with 2x horizontal scaling
  glcdPutStr3(11, 2, FONT_5X7M, "-Example-", 2, 1);
}
