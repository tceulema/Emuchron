//*****************************************************************************
// Filename : 'glcd.h'
// Title    : Graphic lcd API functions
//*****************************************************************************

#ifndef GLCD_H
#define GLCD_H

#include "avrlibtypes.h"

// Fill types
#define FILL_FULL	0	// Full fill area
#define FILL_HALF	1	// Half fill area
#define FILL_THIRDUP	2	// Third fill area with upward issulion
#define FILL_THIRDDOWN	3	// Third fill area with downward illusion
#define FILL_INVERSE	4	// Invert area
#define FILL_BLANK	5	// Clear area

// Fill align types
#define ALIGN_TOP	0	// Align on top-left pixel
#define ALIGN_BOTTOM	1	// Align on bottom-left pixel
#define ALIGN_AUTO	2	// Align on (0,0) pixel (overlap)

// Circle types
#define CIRCLE_FULL	0	// Full circle
#define CIRCLE_HALF_E	1	// Half circle on even bits
#define CIRCLE_HALF_U	2	// Half circle on uneven bits
#define CIRCLE_THIRD	3	// Third circle

// Text orientation types
#define ORI_HORIZONTAL	0	// Horizontal
#define ORI_VERTICAL_BU	1	// Vertical bottom-up
#define ORI_VERTICAL_TD	2	// Vertical top-down

// Text fonts
#define FONT_5X5P	0	// 5x5 proportional font
#define FONT_5X7M	1	// 5x7 monospace font

// Graphics element data
#define ELM_NULL	0	// Element not initialized
#define ELM_BYTE	1	// Element is byte data (8 bits)
#define ELM_WORD	2	// Element is word data (16 bits)
#define ELM_DWORD	3	// Element is dword data (32 bits)
#define DATA_PMEM	0	// Bitmap data is stored in progmen
#define DATA_RAM	1	// Bitmap data is stored in ram

// Basic math
#define SIGN(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

// API-level interface commands

// Draw dot
void glcdDot(u08 x, u08 y, u08 color);

// Draw line
void glcdLine(u08 x1, u08 y1, u08 x2, u08 y2, u08 color);

// Draw rectangle
void glcdRectangle(u08 x, u08 y, u08 w, u08 h, u08 color);

// Draw and fill rectangle
void glcdFillRectangle(u08 x, u08 y, u08 w, u08 h, u08 color);
void glcdFillRectangle2(u08 x, u08 y, u08 w, u08 h, u08 align, u08 fillType,
  u08 color);

// Draw full/dotted/filled circle at [xCenter,yCenter] with [radius]
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

// Draw a bitmap at pixel position [x,y] with size [w,h]
void glcdBitmap(u08 x, u08 y, u16 xo, u08 yo, u08 w, u08 h, u08 elmType,
  u08 origin, void *bitmap, u08 color);
void glcdBitmap8PmFg(u08 x, u08 y, u08 w, u08 h, const uint8_t *bitmap);
void glcdBitmap8RaFg(u08 x, u08 y, u08 w, u08 h, uint8_t *bitmap);
void glcdBitmap16PmFg(u08 x, u08 y, u08 w, u08 h, const uint16_t *bitmap);
void glcdBitmap16RaFg(u08 x, u08 y, u08 w, u08 h, uint16_t *bitmap);
void glcdBitmap32PmFg(u08 x, u08 y, u08 w, u08 h, const uint32_t *bitmap);
void glcdBitmap32RaFg(u08 x, u08 y, u08 w, u08 h, uint32_t *bitmap);

// Get the pixel width of a string
u08 glcdGetWidthStr(u08 font, char *data);
#endif
