//*****************************************************************************
// Filename : 'mchronutil.h'
// Title    : Definitions for mchron utility routines
//*****************************************************************************

#ifndef MCHRONUTIL_H
#define MCHRONUTIL_H

#include <time.h>
#include <sys/time.h>
#include "controller.h"

// The coredump origin types
#define CD_GLCD		0
#define CD_CTRL		1
#define CD_EEPROM	2
#define CD_VAR		3

// The active alarm type
#define ALM_MONOCHRON	0
#define ALM_EMUCHRON	1

// Definition of a structure to hold the main() arguments
typedef struct _emuArgcArgv_t
{
  int argDebug;				// argv index for logfile arg
  int argGlutGeometry;			// argv index for glut geometry arg
  int argGlutPosition;			// argv index for glut window pos arg
  int argLcdType;			// argv index for lcd device arg
  int argTty;				// argv index for ncurses tty arg
  ctrlDeviceArgs_t ctrlDeviceArgs;	// Processed args for lcd stub interface
} emuArgcArgv_t;

// mchron command argument translation functions
u08 emuColorGet(char colorId);
u08 emuFontGet(char *fontName);
u08 emuOrientationGet(char orientationId);
u08 emuStartModeGet(char startId);

// mchron environment functions
u08 emuArgcArgvGet(int argc, char *argv[], emuArgcArgv_t *emuArgcArgv);
void emuCoreDump(u08 origin, const char *location, int arg1, int arg2,
  int arg3, int arg4);
void emuSigSetup(void);
void emuShutdown(void);

// mchron interpreter support functions
void emuClockRelease(u08 echoCmd);
void emuClockUpdate(void);
void emuTimePrint(u08 type);
void emuTimeSync(void);

// mchron system interval timer functions
void emuSysTimerStart(timer_t *timer, int interval, void(*handler)(void));
void emuSysTimerStop(timer_t *timer);

// mchron delay, sleep and timer functions
char waitDelay(int delay);
char waitKeypress(u08 allowQuit);
void waitSleep(int sleep);
char waitTimerExpiry(struct timeval *tvTimer, int expiry, u08 allowQuit,
  suseconds_t *remaining);
void waitTimerStart(struct timeval *tvTimer);
#endif
