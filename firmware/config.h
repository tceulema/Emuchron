//*****************************************************************************
// Filename : 'config.h'
// Title    : Definitions for MONOCHRON configuration menu
//*****************************************************************************

#ifndef CONFIG_H
#define CONFIG_H

// Button keypress delay (msec)
#define KEYPRESS_DLY_1	150

// How many seconds to wait before exiting the config menu due to inactivity
#define CFG_TICK_ACTIVITY_SEC	10

// Main entry for the Monochron configuration menu
void cfgMenuMain(void);
#endif
