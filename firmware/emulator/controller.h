//*****************************************************************************
// Filename : 'controller.h'
// Title    : Lcd controller stub definitions for emuchron emulator
//*****************************************************************************

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "lcdglut.h"
#include "lcdncurses.h"

// The controller interface source methods
#define CTRL_METHOD_COMMAND	0	// A glcdControlWrite() method
#define CTRL_METHOD_READ	1	// A glcdDataRead() method
#define CTRL_METHOD_WRITE	2	// A glcdDataWrite() method

// The controller commands used in combination with CTRL_METHOD_COMMAND
#define CTRL_CMD_DISPLAY	0	// Set display on/off
#define CTRL_CMD_COLUMN		1	// Set cursor x column
#define CTRL_CMD_PAGE		2	// Set cursor y page
#define CTRL_CMD_START_LINE	3	// Set display start line

// The mergeable graphics statistics report and reset types
#define CTRL_STATS_GLCD		0x1	// glcd stats
#define CTRL_STATS_CTRL		0x2	// Controller stats
#define CTRL_STATS_LCD		0x4	// Lcd (glut/ncurses) stats
#define CTRL_STATS_FULL		\
  (CTRL_STATS_GLCD + CTRL_STATS_CTRL + CTRL_STATS_LCD)
					// All stats

// Definition of a structure holding lcd device init related data
typedef struct _ctrlDeviceArgs_t
{
  u08 useNcurses;			// Will we use ncurses device
  u08 useGlut;				// Will we use glut device
  lcdNcurInitArgs_t lcdNcurInitArgs;	// Init args for ncurses lcd device
  lcdGlutInitArgs_t lcdGlutInitArgs;	// Init args for glut lcd device
} ctrlDeviceArgs_t;

// Controller device support methods
void ctrlCleanup(void);
u08 ctrlInit(ctrlDeviceArgs_t ctrlDeviceArgs);

// Controller status and device statistics methods
void ctrlRegPrint(void);
void ctrlStatsPrint(int type);
void ctrlStatsReset(int type);

// Controller device emulator method
u08 ctrlExecute(u08 method, u08 controller, u08 data);

// Lcd device methods
void ctrlLcdBacklightSet(u08 brightness);
void ctrlLcdFlush(void);
#endif
