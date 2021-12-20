//*****************************************************************************
// Filename : 'ks0108.h'
// Title    : Low-level graphics lcd api for hd61202/ks0108 displays
//*****************************************************************************

#ifndef KS0108_H
#define KS0108_H

#include "avrlibtypes.h"

// The hd61202/ks0108 command set for use in glcdControlWrite():
// Note that GLCD_SET_PAGE and GLCD_SET_Y_ADDR def names are utterly confusing.
// GLCD_SET_Y_ADDR - This is actually the horizontal x address (0..63)
// GLCD_SET_PAGE   - This is actually the vertical y-byte address (0..7)
#define GLCD_ON_CTRL		0x3e	// 0011111X: set ctrl display on/off
#define GLCD_SET_Y_ADDR		0x40	// 01XXXXXX: set ctrl X address
#define GLCD_SET_PAGE		0xb8	// 10111YYY: set ctrl Y-byte address
#define GLCD_START_LINE		0xc0	// 11YYYYYY: set ctrl Y start line

#define GLCD_OFF_DISPLAY	0x00	// DB0: turn display off
#define GLCD_ON_DISPLAY		0x01	// DB0: turn display on

#define GLCD_STATUS_BUSY	0x80	// (1) -> lcd is busy
#define GLCD_STATUS_ONOFF	0x20	// (0) -> lcd is on
#define GLCD_STATUS_RESET	0x10	// (1) -> lcd is reset

// Hardware oriented functions
void glcdControlWrite(u08 controller, u08 data);
u08  glcdDataRead(void);
void glcdDataWrite(u08 data);
void glcdInit(void);

// Functional oriented functions
void glcdSetAddress(u08 xAddr, u08 yAddr);
#endif
