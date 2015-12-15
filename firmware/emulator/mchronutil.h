//*****************************************************************************
// Filename : 'mchronutil.h'
// Title    : Definitions for mchron utility routines
//*****************************************************************************

#ifndef MCHRONUTIL_H
#define MCHRONUTIL_H

#include "lcd.h"
#include "interpreter.h"

// Definition of a structure to hold the main() arguments
typedef struct _emuArgcArgv_t
{
  int argDebug;				// Index in argv for logfile argument
  int argGlutGeometry;			// Index in argv for geometry argument
  int argGlutPosition;			// Index in argv for window pos argument
  int argTty;				// Index in argv for ncurses tty argument
  int argLcdType;			// Index in argv for lcd device argument
  lcdDeviceParam_t lcdDeviceParam;	// Processed args for lcd stub interface
} emuArgcArgv_t;

// mchron command argument translation functions
u08 emuColorGet(char colorId);
u08 emuFontGet(char *fontName);
u08 emuOrientationGet(char orientationId);
int emuStartModeGet(char startId);

// mchron environment functions
int emuArgcArgvGet(int argc, char *argv[]);
void emuCoreDump(const char *location, u08 controller, u08 x, u08 y, u08 data);
void emuSigSetup(void);
void emuWinClose(void);

// mchron interpreter command line/list functions
int emuLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput);
int emuListExecute(cmdLine_t *cmdLineRoot, char *source);

// mchron interpreter support functions
void emuClockRelease(int echoCmd);
void emuClockUpdate(void);
void emuTimePrint(void);
void emuTimeSync(void);

// mchron logfile functions
void emuLogfileClose(void);
void emuLogfileOpen(char debugFile[]);
#endif
