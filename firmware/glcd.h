//*****************************************************************************
// Filename : 'glcd.h'
// Title    : High-level graphics lcd api for hd61202/ks0108 displays
//*****************************************************************************

#ifndef GLCD_H
#define GLCD_H

#include "avrlibtypes.h"

// Lcd color values
#define GLCD_OFF	0	// Black pixel data
#define GLCD_ON		1	// White pixel data

// Fill types
#define FILL_FULL	0	// Full fill area
#define FILL_HALF	1	// Half fill area
#define FILL_THIRDUP	2	// Third fill area with upward illusion
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

// Bitmap data storage type
#define DATA_PMEM	0	// Bitmap data is stored in progmen
#define DATA_RAM	1	// Bitmap data is stored in ram

// API-level interface commands

// Clear and reset screen
void glcdClearScreen(void);
void glcdResetScreen(void);

// Draw color
u08 glcdColorGet(void);
void glcdColorSet(u08 color);
void glcdColorSetBg(void);
void glcdColorSetFg(void);

// Draw dot
void glcdDot(u08 x, u08 y);

// Draw line
void glcdLine(u08 x1, u08 y1, u08 x2, u08 y2);

// Draw rectangle
void glcdRectangle(u08 x, u08 y, u08 w, u08 h);

// Draw and fill rectangle
void glcdFillRectangle(u08 x, u08 y, u08 w, u08 h);
void glcdFillRectangle2(u08 x, u08 y, u08 w, u08 h, u08 align, u08 fillType);

// Draw full/dotted/filled circle at [xCenter,yCenter] with [radius]
void glcdCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 lineType);
void glcdFillCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 fillType);

// Write standard ascii character(s) (values 20-127) or number to the display
// at current cursor location
void glcdPutStr(char *data);
void glcdWriteChar(unsigned char c);
void glcdPrintNumber(u08 n);
void glcdPrintNumberBg(u08 n);

// Write a horizontal string at pixel position [x,y]
u08 glcdPutStr2(u08 x, u08 y, u08 font, char *data);
u08 glcdPutStr3(u08 x, u08 y, u08 font, char *data, u08 xScale, u08 yScale);

// Write a vertical string at pixel position [x,y]
u08 glcdPutStr3v(u08 x, u08 y, u08 font, u08 orientation, char *data,
  u08 xScale, u08 yScale);

// Draw a bitmap at pixel position [x,y] with size [w,h]
void glcdBitmap(u08 x, u08 y, u16 xo, u08 yo, u08 w, u08 h, u08 elmType,
  u08 origin, void *bitmap);
void glcdBitmap8Pm(u08 x, u08 y, u08 w, u08 h, const uint8_t *bitmap);
void glcdBitmap8Ra(u08 x, u08 y, u08 w, u08 h, uint8_t *bitmap);
void glcdBitmap16Pm(u08 x, u08 y, u08 w, u08 h, const uint16_t *bitmap);
void glcdBitmap16Ra(u08 x, u08 y, u08 w, u08 h, uint16_t *bitmap);
void glcdBitmap32Pm(u08 x, u08 y, u08 w, u08 h, const uint32_t *bitmap);
void glcdBitmap32Ra(u08 x, u08 y, u08 w, u08 h, uint32_t *bitmap);

// Get the pixel width of a string
u08 glcdGetWidthStr(u08 font, char *data);
#endif
