//*****************************************************************************
// Filename : 'mchronutil.h'
// Title    : Definitions for mchron utility routines
//*****************************************************************************

#ifndef MCHRONUTIL_H
#define MCHRONUTIL_H

#include <time.h>
#include <sys/time.h>
#include "../avrlibtypes.h"
#include "../anim.h"
#include "interpreter.h"
#include "controller.h"

// The coredump origin types
#define CD_GLCD		0
#define CD_CTRL		1
#define CD_EEPROM	2
#define CD_VAR		3

// The active alarm type
#define ALM_MONOCHRON	0
#define ALM_EMUCHRON	1

// The graphics data buffer usage type
#define GRAPH_NULL	0	// Not initialized
#define GRAPH_RAW	1	// Free format use of graphics data
#define GRAPH_IMAGE	2	// Single image with size (x,y) over z frames
#define GRAPH_SPRITE	3	// Sprite image with size (x,y) with z frames

// Get time diff between two timestamps in usec
#define TIMEDIFF_USEC(a,b)	\
  (((a).tv_sec - (b).tv_sec) * 1E6 + (a).tv_usec - (b).tv_usec)

// Definition of a structure to hold the main() arguments
typedef struct _emuArgcArgv_t
{
  int argDebug;			// argv index for logfile arg
  int argGlutGeometry;		// argv index for glut geometry arg
  int argGlutPosition;		// argv index for glut window pos arg
  int argLcdType;		// argv index for lcd device arg
  int argTty;			// argv index for ncurses tty arg
  ctrlDeviceArgs_t ctrlDeviceArgs; // Processed args for lcd stub interface
} emuArgcArgv_t;

// Definition of a structure holding a graphics buffer and its metadata
typedef struct _emuGrBuf_t
{
  u08 bufType;			// Graphics data type: null, raw, image, sprite
  char *bufOrigin;		// Origin of buffer data (malloc-ed)
  void *bufData;		// Graphics data elements buffer (malloc-ed)
  u16 bufElmCount;		// Number of elements in data buffer
  u08 bufElmFormat;		// Data element format
  u08 bufElmByteSize;		// Data element size in bytes (1/2/4)
  u08 bufElmBitSize;		// Data element size in bits (8/16/32-bit)
  u08 bufImgWidth;		// Image: full image width
  u08 bufImgHeight;		// Image: full image height
  u08 bufImgFrames;		// Image: image frames
  u08 bufSprWidth;		// Sprite: sprite width
  u08 bufSprHeight;		// Sprite: sprite height
  u08 bufSprFrames;		// Sprite: sprite frames
} emuGrBuf_t;

// mchron command argument translation functions
u08 emuEchoReqGet(char echo);
u08 emuFontGet(char *fontName);
u08 emuFormatGet(char formatId, u08 *formatBytes, u08 *formatBits);
u08 emuOrientationGet(char orientationId);
u08 emuSearchTypeGet(char searchType);
u08 emuStartModeGet(char startId);

// mchron environment functions
u08 emuArgcArgvGet(int argc, char *argv[], emuArgcArgv_t *emuArgcArgv);
void emuCoreDump(u08 origin, const char *location, int arg1, int arg2,
  int arg3, int arg4);
void emuSigSetup(void);
void emuShutdown(void);

// mchron interpreter support functions
clockDriver_t *emuClockPoolInit(int *count);
void emuClockPoolReset(clockDriver_t *clockDriver);
void emuClockPrint(void);
void emuClockRelease(u08 echoCmd);
void emuClockUpdate(void);
void emuEepromPrint(void);
void emuTimePrint(u08 type);
void emuTimeSync(void);

// mchron system interval timer functions
void emuSysTimerStart(timer_t *timer, int interval, void (*handler)(void));
void emuSysTimerStop(timer_t *timer);

// mchron delay, sleep and timer functions
char waitDelay(int delay);
char waitKeypress(u08 allowQuit);
void waitSleep(int sleep);
char waitTimerExpiry(struct timeval *tvTimer, int expiry, u08 allowQuit,
  suseconds_t *remaining);
void waitTimerStart(struct timeval *tvTimer);

// mchron graphics buffer data functions
u08 grBufCopy(emuGrBuf_t *emuGrBufFrom, emuGrBuf_t *emuGrBufTo);
void grBufInfoPrint(emuGrBuf_t *emuGrBuf);
void grBufInit(emuGrBuf_t *emuGrBuf, u08 reset);
void grBufLoadCtrl(u08 x, u08 y, u08 width, u08 height, char formatName,
  emuGrBuf_t *emuGrBuf);
u08 grBufLoadFile(char *argName, char formatName, u16 maxElements,
  char *fileName, emuGrBuf_t *emuGrBuf);
u08 grBufSaveFile(char *argName, u08 lineElements, char *fileName,
  emuGrBuf_t *emuGrBuf);
#endif
