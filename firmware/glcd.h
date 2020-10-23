//*****************************************************************************
// Filename : 'glcd.h'
// Title    : Graphic lcd API functions
//*****************************************************************************

#ifndef GLCD_H
#define GLCD_H

#include "global.h"

// Fill types
#define FILL_FULL	0
#define FILL_HALF	1
#define FILL_THIRDUP	2
#define FILL_THIRDDOWN	3
#define FILL_INVERSE	4
#define FILL_BLANK	5

// Fill align types
#define ALIGN_TOP	0
#define ALIGN_BOTTOM	1
#define ALIGN_AUTO	2

// Circle types
#define CIRCLE_FULL	0
#define CIRCLE_HALF_E	1
#define CIRCLE_HALF_U	2
#define CIRCLE_THIRD	3

// Text orientation types
#define ORI_HORIZONTAL	0
#define ORI_VERTICAL_BU	1
#define ORI_VERTICAL_TD	2

// Text fonts
#define FONT_5X5P	0
#define FONT_5X7M	1

// Basic math
#define SIGN(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

// API-level interface commands

// Draw dot
void glcdDot(u08 x, u08 y, u08 color);

// Draw line
void glcdLine(u08 x1, u08 y1, u08 x2, u08 y2, u08 color);

// Draw rectangle
void glcdRectangle(u08 x, u08 y, u08 a, u08 b, u08 color);

// Draw and fill rectangle
void glcdFillRectangle(u08 x, u08 y, u08 a, u08 b, u08 color);
void glcdFillRectangle2(u08 x, u08 y, u08 a, u08 b, u08 align, u08 fillType,
  u08 color);

// Draw full/dotted/filled circle of <radius> at <xCenter,yCenter>
void glcdCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 lineType,
  u08 color);
void glcdFillCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 fillType,
  u08 color);

// Write a standard ascii character (values 20-127) to the display at current
// cursor location
void glcdWriteChar(unsigned char c, u08 color);
void glcdWriteCharFg(unsigned char c);

// Write a string at current cursor location
void glcdPutStr(char *data, u08 color);
void glcdPutStrFg(char *data);

// Write a number in two digits at current cursor location
void glcdPrintNumber(u08 n, u08 color);
void glcdPrintNumberBg(u08 n);
void glcdPrintNumberFg(u08 n);

// Write a horizontal string at pixel position [x,y]
u08 glcdPutStr2(u08 x, u08 y, u08 font, char *data, u08 color);
u08 glcdPutStr3(u08 x, u08 y, u08 font, char *data, u08 xScale, u08 yScale,
  u08 color);

// Write a vertical string at pixel position [x,y]
u08 glcdPutStr3v(u08 x, u08 y, u08 font, u08 orientation, char *data,
  u08 xScale, u08 yScale, u08 color);

// Get the pixel width of a string
u08 glcdGetWidthStr(u08 font, char *data);
#endif
