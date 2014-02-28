//*****************************************************************************
// Filename : 'lcd.c'
// Title    : LCD stub functionality for MONOCHRON Emulator
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "../ks0108.h"
#include "stub.h"
#include "lcd.h"
#include "lcdglut.h"
#include "lcdncurses.h"

extern GrLcdStateType GrLcdState;
extern uint16_t OCR2B;

//
// Our implementation of the 128*64px LCD Display:
// - Two controllers, each containing 512 byte.
//
// Per controller:
//  <- 64 px -><- 64 px->
//  ^          ^
//  |  64 px   |  64 px
//  v          V
//  
// An LCD byte represents 8 px and is implemented vertically.
// So, when LCD byte bit 0 starts at px[x,y] then bit 7 ends at px[x,y+7].
//
//       Controller 0                        Controller 1
//       64 x 64 px = 512 byte               64 x 64 px = 512 byte
//
//  px     0    1    2          63             64   65   66        127
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//   0  |    |    |    |     |    |         |    |    |    |     |    |
//   1  |  b |  b |  b |     |  b |         |  b |  b |  b |     |  b |
//   2  |  y |  y |  y |     |  y |         |  y |  y |  y |     |  y |
//   3  |  t |  t |  t |     |  t |         |  t |  t |  t |     |  t |
//   4  |  e |  e |  e |     |  e |         |  e |  e |  e |     |  e |
//   5  |    |    |    |     |    |         |    |    |    |     |    |
//   6  | 0,0| 1,0| 2,0|     |63,0|         | 0,0| 1,0| 2,0|     |63,0|
//   7  |    |    |    |     |    |         |    |    |    |     |    |
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//       :
//       : repeat 6 byte for additional 48 y px
//       :
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//  56  |    |    |    |     |    |         |    |    |    |     |    |
//  57  |  b |  b |  b |     |  b |         |  b |  b |  b |     |  b |
//  58  |  y |  y |  y |     |  y |         |  y |  y |  y |     |  y |
//  59  |  t |  t |  t |     |  t |         |  t |  t |  t |     |  t |
//  60  |  e |  e |  e |     |  e |         |  e |  e |  e |     |  e |
//  61  |    |    |    |     |    |         |    |    |    |     |    |
//  62  | 0,7| 1,7| 2,7|     |63,7|         | 0,7| 1,7| 2,7|     |63,7|
//  63  |    |    |    |     |    |         |    |    |    |     |    |
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//
// Mapping a px(x,y) into data first requires a setoff in a controller
// after which it requires a mapping into the proper (x,y) byte within
// the array and a mapping into the proper bit within that byte.
//
u08 lcdBuffer[GLCD_NUM_CONTROLLERS][GLCD_XPIXELS / GLCD_NUM_CONTROLLERS][GLCD_YPIXELS / 8];

// Identifiers to indicate what LCD stub devices are used
u08 useGlut = GLCD_FALSE;
u08 useNcurses = GLCD_FALSE;

//
// Function: lcdDeviceBacklightSet
//
// Set backlight brightness of LCD display in stubbed device
//
void lcdDeviceBacklightSet(u08 brightness)
{
  OCR2B = (uint16_t)brightness;
  if (useGlut == GLCD_TRUE)
    lcdGlutBacklightSet((unsigned char)brightness);
  if (useNcurses == GLCD_TRUE)
    lcdNcurBacklightSet((unsigned char)brightness);
}

//
// Function: lcdDeviceEnd
//
// Shut down the LCD display in stubbed device
//
void lcdDeviceEnd(void)
{
  if (useGlut == GLCD_TRUE)
    lcdGlutEnd();
  if (useNcurses == GLCD_TRUE)
    lcdNcurEnd();
}

//
// Function: lcdDeviceFlush
//
// Flush the LCD display in stubbed device
//
void lcdDeviceFlush(int force)
{
  if (useGlut == GLCD_TRUE)
    lcdGlutFlush(force);
  if (useNcurses == GLCD_TRUE)
    lcdNcurFlush(force);
  return;
}

//
// Function: lcdDeviceInit
//
// Initialize the LCD display stub device(s)
//
int lcdDeviceInit(lcdDeviceParam_t lcdDeviceParam)
{
  // Administer which LCD stub device is used
  useGlut = lcdDeviceParam.useGlut;
  useNcurses = lcdDeviceParam.useNcurses;

  if (useGlut == GLCD_TRUE)
    lcdGlutInit(lcdDeviceParam.lcdGlutPosX, lcdDeviceParam.lcdGlutPosY,
      lcdDeviceParam.lcdGlutSizeX, lcdDeviceParam.lcdGlutSizeY,
      lcdDeviceParam.winClose);
  if (useNcurses == GLCD_TRUE)
    lcdNcurInit(lcdDeviceParam.lcdNcurTty, lcdDeviceParam.winClose);
  return 0;
}

//
// Function: lcdDeviceRestore
//
// Restore layout of the LCD display in stubbed device
//
void lcdDeviceRestore(void)
{
  if (useGlut == GLCD_TRUE)
    lcdGlutRestore();
  if (useNcurses == GLCD_TRUE)
    lcdNcurRestore();
  return;
}

//
// Function: lcdStatsPrint
//
// Print the LCD device performance statistics 
//
void lcdStatsPrint(void)
{

  // Report glut statistics
  if (useGlut == GLCD_TRUE)
  {
    struct timeval tv;
    double diffDivider;
    lcdGlutStats_t lcdGlutStats;
    
    // Get and report LCD glut statistics
    lcdGlutStatsGet(&lcdGlutStats);
    printf("glut   : lcdByteRx=%llu, ", lcdGlutStats.lcdGlutByteReq);
    if (lcdGlutStats.lcdGlutByteReq == 0)
    {
      printf("byteEff=-%%, bitEff=-%%\n");
    }
    else
    {
      printf("byteEff=%d%%, bitEff=%d%%\n",
        (int)(lcdGlutStats.lcdGlutByteCnf * 100 / lcdGlutStats.lcdGlutByteReq),
        (int)(lcdGlutStats.lcdGlutBitCnf * 100 / lcdGlutStats.lcdGlutBitReq));
    }
    printf("         msgTx=%llu, msgRx=%llu, maxQLen=%d, ",
      lcdGlutStats.lcdGlutMsgSend, lcdGlutStats.lcdGlutMsgRcv,
      lcdGlutStats.lcdGlutQueueMax);
    if (lcdGlutStats.lcdGlutQueueEvents == 0)
    {
      printf("avgQLen=-\n");
    }
    else
    {
      printf("avgQLen=%llu\n",
        lcdGlutStats.lcdGlutMsgSend / lcdGlutStats.lcdGlutQueueEvents);
    }
    printf("         redraws=%d, cycles=%llu, updates=%llu, ",
      lcdGlutStats.lcdGlutRedraws, lcdGlutStats.lcdGlutTicks,
      lcdGlutStats.lcdGlutQueueEvents);
    if (lcdGlutStats.lcdGlutTicks == 0)
    {
      printf("fps=-\n");
    }
    else
    {
      // Get timediff with previous call
      (void) gettimeofday(&tv, NULL);
      diffDivider = (double)(((tv.tv_sec - lcdGlutStats.lcdGlutTimeStart.tv_sec) * 1E6 + 
        tv.tv_usec - lcdGlutStats.lcdGlutTimeStart.tv_usec) / 1E4);
      printf("fps=%3.1f\n", (double)lcdGlutStats.lcdGlutTicks / diffDivider * 100);
    }
  }

  // Report ncurses statistics
  if (useNcurses == GLCD_TRUE)
  {
    lcdNcurStats_t lcdNcurStats;
    
    // Get and report LCD ncurses statistics
    lcdNcurStatsGet(&lcdNcurStats);
    printf("ncurses: lcdByteRx=%llu, ", lcdNcurStats.lcdNcurByteReq);
    if (lcdNcurStats.lcdNcurByteReq == 0)
    {
      printf("byteEff=-%%, bitEff=-%%\n");
    }
    else
    {
      printf("byteEff=%d%%, bitEff=%d%%\n",
        (int)(lcdNcurStats.lcdNcurByteCnf * 100 / lcdNcurStats.lcdNcurByteReq),
        (int)(lcdNcurStats.lcdNcurBitCnf * 100 / lcdNcurStats.lcdNcurBitReq));
    }
  }
  return;
}

//
// Function: lcdStatsReset
//
// Reset the LCD device performance statistics 
//
void lcdStatsReset(void)
{
  if (useGlut == GLCD_TRUE)
    lcdGlutStatsReset();
  if (useNcurses == GLCD_TRUE)
    lcdNcurStatsReset();
  return;
}

//
// Function: lcdReadStub
//
// Read data from the LCD display
//
u08 lcdReadStub(u08 controller)
{
  u08 data;
  unsigned char x;
	
  // Get location in LCD emulator buffer
  x = GrLcdState.lcdXAddr;
  if (controller == GLCD_NUM_CONTROLLERS - 1)
  {
    x = x - GLCD_XPIXELS / GLCD_NUM_CONTROLLERS;
  }

  // Check if we're out of bounds on the LCD
  if (controller >= GLCD_NUM_CONTROLLERS ||
      x >= GLCD_XPIXELS / GLCD_NUM_CONTROLLERS ||
      GrLcdState.lcdYAddr >= GLCD_YPIXELS / 8)
  {
    // We should never get here
    coreDump(__func__, controller, GrLcdState.lcdXAddr, GrLcdState.lcdYAddr,
      0);
  }

  // Read from LCD emulator buffer
  data = lcdBuffer[controller][x][GrLcdState.lcdYAddr];

  return data;
}

//
// Function: lcdWriteStub
//
// Write data to the LCD display in stubbed device
//
void lcdWriteStub(u08 data)
{
  u08 controller = GrLcdState.lcdXAddr / GLCD_CONTROLLER_XPIXELS;
  unsigned char x;

  // Get location in LCD emulator buffer
  x = GrLcdState.lcdXAddr;
  if (controller == GLCD_NUM_CONTROLLERS - 1)
  {
    x = x - GLCD_XPIXELS / GLCD_NUM_CONTROLLERS;
  }

  // Check if we're out of bounds on the LCD
  if (controller >= GLCD_NUM_CONTROLLERS ||
      x >= GLCD_XPIXELS / GLCD_NUM_CONTROLLERS ||
      GrLcdState.lcdYAddr >= GLCD_YPIXELS / 8)
  {
    // We should never get here
    coreDump(__func__, controller, GrLcdState.lcdXAddr, GrLcdState.lcdYAddr,
      data);
  }

  // Write to LCD emulator buffer
  lcdBuffer[controller][x][GrLcdState.lcdYAddr] = data;

  // Write to LCD stubbed device
  if (useGlut == 1)
    lcdGlutDataWrite(GrLcdState.lcdXAddr, GrLcdState.lcdYAddr, data);
  if (useNcurses == 1)
    lcdNcurDataWrite(GrLcdState.lcdXAddr, GrLcdState.lcdYAddr, data);
}

