//*****************************************************************************
// Filename : 'mchronutil.h'
// Title    : Definitions for mchron utility routines
//*****************************************************************************

#ifndef MCHRONUTIL_H
#define MCHRONUTIL_H

#include "lcd.h"
#include "scanutil.h"

// Definition of a structure to hold the main() arguments
typedef struct _argcArgv_t
{
  int argDebug;				// Index in argv for logfile argument
  int argGlutGeometry;			// Index in argv for geometry argument
  int argGlutPosition;			// Index in argv for window pos argument
  int argTty;				// Index in argv for ncurses tty argument
  int argLcdType;			// Index in argv for lcd device argument
  lcdDeviceParam_t lcdDeviceParam;	// Processed args for lcd stub interface
} argcArgv_t;

// mchron utility function prototypes
int emuArgcArgv(int argc, char *argv[], argcArgv_t *argcArgv);
void emuClockRelease(int echoCmd);
void emuClockUpdate(void);
int emuColorGet(char colorId, int *color);
int emuListExecute(cmdLine_t *cmdLineRoot, char *source, int echoCmd,
  int (*cmdHandler)(cmdInput_t *, int));
void emuLogfileClose(void);
void emuLogfileOpen(char debugFile[]);
void emuSigSetup(void);
int emuStartModeGet(char startId, int *start);
void emuTimePrint(void);
void emuWinClose(void);
#endif
