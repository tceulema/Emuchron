//*****************************************************************************
// Filename : 'ks0108.h'
// Title    : Graphic LCD driver for HD61202/KS0108 displays
//*****************************************************************************

#ifndef KS0108_H
#define KS0108_H

#include "global.h"
#include "ks0108conf.h"

// Our own definition of the glcd truth.
// Why do we do this? It turns out that gcc sometimes get utterly confused
// using the TRUE and FALSE defines/macros resulting in bad object code.
// This may be due to the mixing of stdlib, avr, ncurses and glut code.
// Even worse, similar issues also pop up in actual monochron firmware built
// in this environment by showing unexplained erratic behavior.
// In any case, I don not trust TRUE and FALSE in combination with this
// stdlib, avr, ncurses and glut software environment.
#define GLCD_FALSE	0
#define GLCD_TRUE	1

// LCD color values
#define OFF	0
#define ON	1

// HD61202/KS0108 command set
#define GLCD_ON_CTRL		0x3E	// 0011111X: lcd on/off control
#define GLCD_ON_DISPLAY		0x01	// DB0: turn display on

#define GLCD_START_LINE		0xC0	// 11XXXXXX: set lcd start line

#define GLCD_SET_PAGE		0xB8	// 10111XXX: set lcd page (X) address
#define GLCD_SET_Y_ADDR		0x40	// 01YYYYYY: set lcd Y address

#define GLCD_STATUS_BUSY	0x80	// (1)->LCD IS BUSY
#define GLCD_STATUS_ONOFF	0x20	// (0)->LCD IS ON
#define GLCD_STATUS_RESET	0x10	// (1)->LCD IS RESET

// Determine the number of controllers
// (make sure we round up for partial use of more than one controller)
#define GLCD_NUM_CONTROLLERS	((GLCD_XPIXELS+GLCD_CONTROLLER_XPIXELS-1)/GLCD_CONTROLLER_XPIXELS)

// typedefs/structures
typedef struct struct_GrLcdCtrlrStateType
{
  unsigned char xAddr;
  unsigned char yAddr;
} GrLcdCtrlrStateType;

typedef struct struct_GrLcdStateType
{
  unsigned char lcdXAddr;
  unsigned char lcdYAddr;
  GrLcdCtrlrStateType ctrlr[GLCD_NUM_CONTROLLERS];
} GrLcdStateType;

// Hardware oriented functons
void glcdInit(u08 color);
void glcdInitHW(void);
void glcdBusyWait(u08 controller);
void glcdControlWrite(u08 controller, u08 data);
void glcdDataWrite(u08 data);
u08  glcdDataRead(void);

// Functional oriented functions
//! Clear the display
void glcdClearScreen(u08 color);
//! Reset the display
void glcdReset(u08 resetState);
//! Set display memory access point back to upper,left corner
void glcdHome(void);
//! Set display memory access point to [x] horizontal pixel and [y] vertical line
void glcdSetAddress(u08 x, u08 yLine);
//! Set display memory access point to row [line] and column [col] assuming 5x7 font
void glcdStartLine(u08 start);
//! Generic delay routine for timed glcd access
#endif
