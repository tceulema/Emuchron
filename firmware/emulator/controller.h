//*****************************************************************************
// Filename : 'controller.h'
// Title    : Lcd controller stub definitions for emuchron emulator
//*****************************************************************************

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "../avrlibtypes.h"
#include "lcdglut.h"
#include "lcdncurses.h"

// The controller interface source methods
#define CTRL_METHOD_CTRL_W	0	// A glcdControlWrite() method
#define CTRL_METHOD_READ	1	// A glcdDataRead() method
#define CTRL_METHOD_WRITE	2	// A glcdDataWrite() method

// The mergeable graphics statistics report and reset types
#define CTRL_STATS_NULL		0x0	// No stats
#define CTRL_STATS_GLCD		0x1	// glcd stats
#define CTRL_STATS_CTRL		0x2	// Controller stats
#define CTRL_STATS_LCD		0x4	// Lcd (glut/ncurses) stats
#define CTRL_STATS_ALL		\
  (CTRL_STATS_GLCD | CTRL_STATS_CTRL | CTRL_STATS_LCD)
					// All stats

// The mergeable lcd devices types
#define CTRL_DEVICE_NULL	0x0	// No device
#define CTRL_DEVICE_NCURSES	0x1	// Ncurses device
#define CTRL_DEVICE_GLUT	0x2	// Glut device
#define CTRL_DEVICE_ALL		\
  (CTRL_DEVICE_NCURSES | CTRL_DEVICE_GLUT)
					// All devices

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
u08 ctrlInit(ctrlDeviceArgs_t *ctrlDeviceArgs);

// Controller and device status and statistics methods
u08 ctrlDeviceActive(u08 device);
void ctrlRegPrint(void);
void ctrlStatsPrint(u08 type);
void ctrlStatsReset(u08 type);

// Controller data pin/port utility methods
void ctrlBusyState(void);
void ctrlControlSet(void);
void ctrlPortDataSet(u08 data);

// Controller device emulator methods
void ctrlControlSelect(u08 controller);
void ctrlExecute(u08 method);

// Glut glcd pixel double-click methods
void ctrlGlcdPixConfirm(void);
void ctrlGlcdPixDisable(void);
void ctrlGlcdPixEnable(void);
u08 ctrlGlcdPixGet(u08 *x, u08 *y);

// Lcd device methods
void ctrlLcdBacklightSet(u08 brightness);
void ctrlLcdFlush(void);
void ctrlLcdGlutGrSet(u08 bezel, u08 grid);
void ctrlLcdHighlight(u08 highlight, u08 x, u08 y);
void ctrlLcdNcurGrSet(u08 backlight);
#endif
