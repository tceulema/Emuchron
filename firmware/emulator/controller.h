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
#define CTRL_STATS_NULL		0x00	// No stats
#define CTRL_STATS_GLCD		0x01	// Aggregated glcd stats
#define CTRL_STATS_GLCD_SNAP	0x02	// Snapshot glcd stats
#define CTRL_STATS_CTRL		0x04	// Aggregated controller stats
#define CTRL_STATS_CTRL_SNAP	0x08	// Snapshot controller stats
#define CTRL_STATS_LCD		0x10	// Lcd (glut/ncurses) stats
#define CTRL_STATS_ALL		\
  (CTRL_STATS_GLCD | CTRL_STATS_GLCD_SNAP | CTRL_STATS_CTRL | \
   CTRL_STATS_CTRL_SNAP | CTRL_STATS_LCD)
					// All stats (aggregate and snapshot)
#define CTRL_STATS_AGGREGATE	\
  (CTRL_STATS_GLCD | CTRL_STATS_CTRL | CTRL_STATS_LCD)
					// All aggregate stats
#define CTRL_STATS_SNAPSHOT	\
  (CTRL_STATS_GLCD_SNAP | CTRL_STATS_CTRL_SNAP)
					// All snapshot stats

// The mergeable lcd and monitor devices types
#define CTRL_DEVICE_NULL	0x0	// No device
#define CTRL_DEVICE_NCURLCD	0x1	// Ncurses lcd device
#define CTRL_DEVICE_GLUTLCD	0x2	// Glut lcd device
#define CTRL_DEVICE_NCURMON	0x4	// Ncurses monitor device
#define CTRL_DEVICE_LCD_ALL		\
  (CTRL_DEVICE_NCURLCD | CTRL_DEVICE_GLUTLCD)
					// All lcd devices
#define CTRL_DEVICE_NCUR_ALL		\
  (CTRL_DEVICE_NCURLCD | CTRL_DEVICE_NCURMON)
					// All ncurse devices

// Definition of a structure holding lcd device init related data
typedef struct _ctrlDeviceArgs_t
{
  u08 useNcurLcd;			// Will we use ncurses lcd device
  u08 useGlutLcd;			// Will we use glut lcd device
  u08 useNcurMon;			// Will we use ncurses monitor
  ncurInitArgs_t ncurInitArgs;		// Init args for ncurses lcd device
  lcdGlutInitArgs_t lcdGlutInitArgs;	// Init args for glut lcd device
} ctrlDeviceArgs_t;

// Definition of a structure holding the glcd interface statistics counters
typedef struct _ctrlGlcdStats_t
{
  long long dataRead;			// Bytes read from lcd
  long long dataWrite;			// Bytes written to lcd
  long long addressSet;			// Cursor address set in lcd
  long long ctrlSet;			// Set lcd controller
} ctrlGlcdStats_t;

// Definition of a structure holding the controller statistics counters
typedef struct _ctrlStats_t
{
  long long displayReq;			// Display commands received
  long long displayCnf;			// Display cmds leading to lcd update
  long long startLineReq;		// Startline commands received
  long long startLineCnf;		// Startline cmds leading to lcd update
  long long xReq;			// Cursor x commands received
  long long xCnf;			// Cursor x cmds leading to x update
  long long yReq;			// Cursor y commands received
  long long yCnf;			// Cursor y cmds leading to y update
  long long readReq;			// Lcd read requests received
  long long readCnf;			// Lcd read reqs leading to actual read
  long long writeReq;			// Lcd write requests received
  long long writeCnf;			// Lcd write reqs leading to lcd update
} ctrlStats_t;

// Definition of a structure holding the stubbed controller hardware registers
typedef struct _ctrlRegister_t
{
  u08 display;				// Display on/off switch (0=off, 1=on)
  u08 x;				// Cursor x pos (0..63)
  u08 y;				// Cursor y page (0..7)
  u08 startLine;			// Vertical display start line (0..63)
  u08 dataRead;				// Last data read from the lcd
  u08 dataWrite;			// Last data written to the lcd
} ctrlRegister_t;

// Controller device support methods
void ctrlCleanup(void);
u08 ctrlInit(ctrlDeviceArgs_t *ctrlDeviceArgs);

// Controller and device status and statistics methods
u08 ctrlDeviceActive(u08 device);
void ctrlRegPrint(void);
void ctrlStatsGet(u08 mode, u08 *ctrlActive, u08 *ctrlState0, u08 *ctrlState1,
  ctrlRegister_t *ctrlReg0, ctrlRegister_t *ctrlReg1, ctrlStats_t *ctrlStats0,
  ctrlStats_t *ctrlStats1, ctrlGlcdStats_t *ctrlGlcd);
void ctrlStatsPrint(u08 type);
void ctrlStatsReset(u08 type);
void ctrlStatsSnapMake(void);

// Controller data pin/port utility methods
void ctrlBusyState(void);
void ctrlControlSet(void);
void ctrlPortDataSet(u08 data);

// Controller device emulator methods
void ctrlControlSelect(u08 controller);
void ctrlExecute(u08 method);

// Glut glcd pixel double-click methods
void ctrlGlcdPixActivitySet(u08 enable);
void ctrlGlcdPixConfirm(void);
u08 ctrlGlcdPixGet(u08 *x, u08 *y);

// Lcd device methods
void ctrlLcdBacklightSet(u08 brightness);
void ctrlLcdFlush(void);
void ctrlLcdGlutGrSet(u08 bezel, u08 grid);
void ctrlLcdGlutHlSet(u08 highlight, u08 x, u08 y);
void ctrlLcdGlutSizeSet(char axis, u16 size);
void ctrlLcdNcurGrSet(u08 backlight);
#endif
