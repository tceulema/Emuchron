//*****************************************************************************
// Filename : 'ks0108.h'
// Title    : Graphic lcd driver for HD61202/KS0108 displays
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
// In any case, I do not trust TRUE and FALSE in combination with this
// stdlib, avr, ncurses and glut software environment.
#define GLCD_FALSE	0
#define GLCD_TRUE	1

// Lcd color values
#define GLCD_OFF	0
#define GLCD_ON		1

// HD61202/KS0108 command set:
// Note that GLCD_SET_PAGE and GLCD_SET_Y_ADDR are utterly confusing
// when used in Monochron.
// GLCD_SET_PAGE   - This is actually the vertical y-byte address (0..7)
// GLCD_SET_Y_ADDR - This is actually the horizontal x address (0..63)
#define GLCD_ON_CTRL		0x3E	// 0011111X: lcd on/off control
#define GLCD_START_LINE		0xC0	// 11XXXXXX: set lcd start line
#define GLCD_SET_PAGE		0xB8	// 10111XXX: set lcd page (X) address
#define GLCD_SET_Y_ADDR		0x40	// 01YYYYYY: set lcd Y address

#define GLCD_ON_DISPLAY		0x01	// DB0: turn display on

#define GLCD_STATUS_BUSY	0x80	// (1) -> Lcd is busy
#define GLCD_STATUS_ONOFF	0x20	// (0) -> Lcd is on
#define GLCD_STATUS_RESET	0x10	// (1) -> Lcd is reset

// Determine the number of controllers
// (make sure we round up for partial use of more than one controller)
#define GLCD_NUM_CONTROLLERS \
  ((GLCD_XPIXELS + GLCD_CONTROLLER_XPIXELS - 1) / GLCD_CONTROLLER_XPIXELS)

// Hardware oriented functions
//u08 glcdControlRead(u08 controller);
void glcdControlWrite(u08 controller, u08 data);
u08  glcdDataRead(void);
void glcdDataWrite(u08 data);
void glcdInit(u08 color);

// Functional oriented functions
void glcdClearScreen(u08 color);
void glcdResetScreen(void);
void glcdNextAddress(void);
void glcdSetAddress(u08 xAddr, u08 yAddr);
//void glcdStartLine(u08 startLine);
#endif
