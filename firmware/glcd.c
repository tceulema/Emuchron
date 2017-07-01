//*****************************************************************************
// Filename : 'glcd.c'
// Title    : Graphic lcd API functions
//*****************************************************************************

#ifndef EMULIN
// AVR specific includes
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "util.h"
#else
#include <stdlib.h>
#include "emulator/stub.h"
#include "emulator/mchronutil.h"
#endif
#include "glcd.h"
#include "monomain.h"
#include "ks0108.h"

// Include 5x7 non-proportional and 5x5 proportional fonts
#include "font5x7.h"
#include "font5x5p.h"

// External data
extern volatile uint8_t mcBgColor, mcFgColor;

// To optimize lcd access, all relevant data from a single lcd line can be read
// in first, then processed and then written back to the lcd. The glcdBuffer[]
// array below is the buffer that will be used for this purpose.
// This method drastically reduces switching between the read and write modes
// of the lcd and significantly improves the speed of the lcd api: smoother
// graphics. And yes, it will cost us 128 byte on the 2K stack. There is
// another downside. Since multiple glcd functions use this buffer, the
// Monochron application using these functions may not implement concurrent or
// threaded calls to these functions. This is however not an issue since
// Monochron behaves like a monolithic application except for glcdPutStr() that
// never requires a buffer anyway.
// Note: Use glcdBufferRead() to read lcd data into the buffer.
static u08 glcdBuffer[GLCD_XPIXELS];

// The following arrays contain bitmap templates for fill options third up/down
static u08 pattern3Up[] =
  { 0x49, 0x24, 0x92 };
static u08 pattern3Down[] =
  { 0x49, 0x92, 0x24 };

// The following variables are used for obtaining font information and font
// bytes. They are used in the following functions:
// glcdPutStr3(), glcdPutStr3v(), glcdFontByteGet(), glcdFontIdxGet()
// To reduce large function interfaces to the latter two functions, the
// interface is implemented as static variables in this module serving code
// size and speed optimization purposes.
// The downside of using this method is that glcdPutStr3() and glcdPutStr3v()
// are forced to use these variables in their drawing code and that concurrent
// or threaded calls to either one of them is prohibited. Currently the
// glcdPutStr3() and glcdPutStr3v() functions show monolithic behavior in
// Monochron applications, so we should be safe.
static u08 fontId;
static u08 fontByteIdx;
static u08 fontWidth;
static u16 fontCharIdx;

// Local function prototypes
static void glcdBufferBitUpdate(u08 x, u08 y);
static void glcdBufferRead(u08 x, u08 yByte, u08 len);
static u08 glcdCharWidthGet(char c, u08 *idxOffset);
static u08 glcdFontByteGet(u08 color);
static u16 glcdFontIdxGet(unsigned char c);

//
// Function: glcdCircle2
//
// Draw a (dotted) circle centered at px[xCenter,yCenter] with radius in px.
//
void glcdCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 lineType, u08 color)
{
  u08 i;
  u08 j;
  s08 x = 0;
  s08 y = radius;
  s08 tswitch = 3 - 2 * (s08)radius;
  s08 half = 0;
  s08 third = 0;
  u08 yLine;
  u08 yLineEnd = ((yCenter + radius) >> 3);
  u08 xStart;
  u08 xEnd;

  // Set filter for HALF draw mode
  if (lineType == CIRCLE_HALF_U)
    half = 1;

  // Initialize buffer that stores the template of the circle section to draw
  for (j = 0; j <= radius; j++)
    glcdBuffer[GLCD_CONTROLLER_XPIXELS + j] = 0;

  // Split up the circle generation in circle sections per y-line byte
  for (yLine = ((yCenter - radius) >> 3); yLine <= yLineEnd; yLine++)
  {
    // Reset circle generation parameters
    x = 0;
    y = radius;
    third = 0;
    tswitch = 3 - 2 * (s08)radius;
    xStart = 255;
    xEnd = 0;

    // Generate template pixels using the right side of the circle y-line
    while (x <= y)
    {
      if (lineType == CIRCLE_FULL ||
          (lineType == CIRCLE_THIRD && third == 0) ||
          (lineType != CIRCLE_THIRD && (x & 0x1) == half))
      {
        j = GLCD_FALSE;
        if (((yCenter + y) >> 3) == yLine)
        {
          // Mark bottom-right pixel in template
          j = GLCD_TRUE;
          glcdBufferBitUpdate(GLCD_CONTROLLER_XPIXELS + x, yCenter + y);
        }
        if (((yCenter - y) >> 3) == yLine)
        {
          // Mark top-right pixel in template
          j = GLCD_TRUE;
          glcdBufferBitUpdate(GLCD_CONTROLLER_XPIXELS + x, yCenter - y);
        }
        if (j == GLCD_TRUE)
        {
          // Sync x range scope to process
          if (xCenter + x < xStart)
            xStart = xCenter + x;
          if (xCenter + x > xEnd)
            xEnd = xCenter + x;
          j = GLCD_FALSE;
        }
        if (((yCenter + x) >> 3) == yLine)
        {
          // Mark bottom-right pixel in template
          j = GLCD_TRUE;
          glcdBufferBitUpdate(GLCD_CONTROLLER_XPIXELS + y, yCenter + x);
        }
        if (((yCenter - x) >> 3) == yLine)
        {
          // Mark top-right pixel in template
          j = GLCD_TRUE;
          glcdBufferBitUpdate(GLCD_CONTROLLER_XPIXELS + y, yCenter - x);
        }
        if (j == GLCD_TRUE)
        {
          // Sync x range scope to process
          if (xCenter + y < xStart)
            xStart = xCenter + y;
          if (xCenter + y > xEnd)
            xEnd = xCenter + y;
        }
      }

      // Go to next set of circle dots
      if (tswitch < 0)
      {
        tswitch = tswitch + 4 * x + 6;
      }
      else
      {
        tswitch = tswitch + 4 * (x - y) + 10;
        y--;
      }
      x++;

      // Set next offset for THIRD draw type
      if (third == 2)
        third = 0;
      else
        third++;
    }

    // At this point the circle section template for the y-line is generated.
    // In case the template is empty, which is possible when using the two
    // HALF draw types, then quit this y-line
    if (xStart == 255)
      continue;

    // Load line section for right side of circle y-line
    glcdBufferRead(xStart, yLine, xEnd - xStart + 1);

    // Map section template onto the right side circle section and write
    // back to lcd
    glcdSetAddress(xStart, yLine);
    j = GLCD_CONTROLLER_XPIXELS + (xStart - xCenter);
    for (i = 0; i <= xEnd - xStart; i++)
    {
      if (color == GLCD_ON)
        glcdDataWrite(glcdBuffer[i] | glcdBuffer[j]);
      else
        glcdDataWrite(glcdBuffer[i] & ~glcdBuffer[j]);
      j++;
    }

    // Set adresses for line section for left side of circle y-line
    j = xStart;
    xStart = xCenter - (xEnd - xCenter);
    xEnd = xCenter - (j - xCenter);

    // The top/bottom center pixel has already been drawn
    j = xEnd;
    if (j == xCenter)
      j--;

    // Load lcd line section in buffer and prepare write back
    if (xStart <= j)
    {
      glcdBufferRead(xStart, yLine, j - xStart + 1);
      glcdSetAddress(xStart, yLine);
    }

    // Map mirrored section template on left circle section
    j = GLCD_CONTROLLER_XPIXELS + (xCenter - xStart);
    for (i = 0; i <= xEnd - xStart; i++)
    {
      if (j != GLCD_CONTROLLER_XPIXELS)
      {
        if (color == GLCD_ON)
          glcdDataWrite(glcdBuffer[i] | glcdBuffer[j]);
        else
          glcdDataWrite(glcdBuffer[i] & ~glcdBuffer[j]);
      }
      // Clear section template for next y-line
      glcdBuffer[j] = 0;
      j--;
    }
  }
}

//
// Function: glcdDot
//
// Paint a dot in a particular color
//
void glcdDot(u08 x, u08 y, u08 color)
{
  unsigned char oldByte;
  unsigned char newByte;
  u08 mask = (1 << (y & 0x7));

  // Get lcd byte containing the dot
  glcdSetAddress(x, y >> 3);
  glcdDataRead();		// Dummy read
  oldByte = glcdDataRead();	// Read back current value

  // Set/clear dot in new lcd byte
  if (color == GLCD_ON)
    newByte = (oldByte | mask);
  else
    newByte = (oldByte & ~mask);

  // Prevent unnecessary write back to lcd if nothing has
  // changed when compared to current byte
  if (oldByte != newByte)
  {
    glcdSetAddress(x, y >> 3);
    glcdDataWrite(newByte);
  }
}

//
// Function: glcdFillCircle2
//
// Draw a filled circle centered at px[xCenter,yCenter] with radius in px
// Note: fillType FILL_INVERSE is NOT supported
//
void glcdFillCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 fillType,
  u08 color)
{
  s08 x = 0;
  s08 y = radius;
  s08 tswitch = 3 - 2 * (u08)radius;
  u08 firstDraw = GLCD_TRUE;
  u08 drawSize = 0;

  // The code below still has the basic logic structure of the well known
  // method to fill a circle using tswitch. Optimizations applied in this
  // method are two-fold. First, an optimization avoids multiple vertical
  // line draw actions in the same area (so, draw the vertical line only
  // once). Consider this an optimization to the core of the tswitch method.
  // Second, an optimization merges multiple vertical line draw actions into
  // a single rectangle fill draw. This builds on optimizing the interface
  // to our lcd display and is therefor hardware oriented.
  while (x <= y)
  {
    if (x != y)
    {
      if (tswitch >= 0)
      {
        if (firstDraw == GLCD_TRUE)
          drawSize = 2 * drawSize;
        glcdFillRectangle2(xCenter - x, yCenter - y, drawSize + 1, y * 2,
          ALIGN_AUTO, fillType, color);
        if (x != 0)
          glcdFillRectangle2(xCenter + y, yCenter - x, 1, x * 2, ALIGN_AUTO,
            fillType, color);
      }
    }
    if (x != 0)
    {
      if (tswitch >= 0 && firstDraw == GLCD_FALSE)
        glcdFillRectangle2(xCenter + x - drawSize, yCenter - y, drawSize + 1,
          y * 2, ALIGN_AUTO, fillType, color);
      if (tswitch >= 0)
      {
        if (x != y)
          drawSize = 0;
        glcdFillRectangle2(xCenter - y, yCenter - x, drawSize + 1, x * 2,
          ALIGN_AUTO, fillType, color);
      }
    }

    if (tswitch < 0)
    {
      tswitch = tswitch + 4 * x + 6;
      drawSize++;
    }
    else
    {
      tswitch = tswitch + 4 * (x - y) + 10;
      firstDraw = GLCD_FALSE;
      drawSize = 0;
      y--;
    }

    x++;
  }
}

//
// Function: glcdFillRectangle
//
// Fill a rectangle
//
void glcdFillRectangle(u08 x, u08 y, u08 a, u08 b, u08 color)
{
  glcdFillRectangle2(x, y, a, b, ALIGN_AUTO, FILL_FULL, color);
}

//
// Function: glcdFillRectangle2
//
// Draw filled rectangle at px[x,y] with size px[a,b].
//
// align (note: used for filltypes HALF & THIRDUP/DOWN only):
// ALIGN_TOP     - Paint top left pixel of box
// ALIGN_BOTTOM  - Paint bottom left pixel of box
// ALIGN_AUTO    - Paint top left pixel of box relative to virtually painted px[0,0]
//
// fillType:
// FILL_FULL      - Fully filled
// FILL_HALF      - Half filled
// FILL_THIRDUP   - Third filled, creating an upward illusion
// FILL_THIRDDOWN - Third filled, creating a downward illusion
// FILL_INVERSE   - Inverse (ignore color)
// FILL_BLANK     - Clear
//
// Note: For optimization purposes we're using datatype s08, supporting a
// display width up to 128 pixels. Our Monochron display happens to be 128
// pixels wide.
//
void glcdFillRectangle2(u08 x, u08 y, u08 a, u08 b, u08 align, u08 fillType, u08 color)
{
  u08 h, i;
  s08 virX = 0;
  s08 virY = 0;
  u08 yByte = y / 8;
  u08 startBit = y % 8;
  u08 doBits = 0;
  u08 lcdByte = 0;
  u08 useBuffer;
  u08 mask;
  u08 template = 0;
  s08 distance = 0;

  // Set input for obtaining the first template for non-standard fill types
  // based on the requested pixel alignment. For this a virtual x and y
  // position is needed.
  if (align == ALIGN_TOP)
  {
    //virX = 0;
    if (fillType == FILL_THIRDUP)
      virY = -(startBit % 3);
    else if (fillType == FILL_THIRDDOWN)
      virY = startBit % 3;
    else if (fillType == FILL_HALF)
      virY = startBit & 0x1;
  }
  else if (align == ALIGN_BOTTOM)
  {
    //virX = 0;
    if (fillType == FILL_THIRDUP)
      virY = -(b + startBit) % 3 + 1;
    else if (fillType == FILL_THIRDDOWN)
      virY = (b + startBit - 1) % 3;
    else if (fillType == FILL_HALF)
      virY = (b + startBit + 1) & 0x1;
  }
  else if (align == ALIGN_AUTO)
  {
    virX = x;
    if (fillType == FILL_THIRDUP)
      virY = (y - startBit) % 3;
    else if (fillType == FILL_THIRDDOWN)
      virY = -(y - startBit) % 3;
    //else if (fillType == FILL_HALF)
    //  virY = 0;
  }

  // For third up/down align: we need a positive modulo result
  if (virY < 0)
    virY = virY + 3;

  // Loop through each affected y-pixel byte
  for (h = 0; h < b; h = h + doBits)
  {
    // In some cases we partly update an lcd byte or invert it
    if (startBit != 0 || b - h < 8 || fillType == FILL_INVERSE)
    {
      // Read all the required lcd bytes for this y-byte in the
      // line buffer and update them byte by byte
      useBuffer = GLCD_TRUE;
      glcdBufferRead(x, yByte, a);
    }
    else
    {
      // We're going to write full lcd bytes
      useBuffer = GLCD_FALSE;
    }

    // As of now on we're going to write consecutive lcd bytes
    glcdSetAddress(x, yByte);

    // Process max 8 y-pixel bits for the current y byte
    if (b - h < 8 - startBit)
      doBits = b - h;
    else
      doBits = 8 - startBit;

    // For this line of y-pixel bytes do prework for non-standard fills
    if (fillType == FILL_THIRDUP || fillType == FILL_THIRDDOWN)
    {
      // Determine relative distance to align pixel
      distance = (virX + virY) % 3;
    }
    else if (fillType == FILL_HALF)
    {
      // Set the template that we'll inverse for each x
      if ((virX & 0x1) == (virY & 0x1))
        template = 0xAA;
      else
        template = 0x55;
    }

    // Loop for each x for current y-pixel byte
    for (i = 0; i < a; i++)
    {
      // Get lcd source byte when needed
      if (useBuffer == GLCD_TRUE)
        lcdByte = glcdBuffer[i];

      // Set template that we have to apply to the lcd byte
      if (fillType == FILL_FULL)
        template = 0xFF;
      else if (fillType == FILL_BLANK)
        template = 0x00;
      else if (fillType == FILL_HALF)
      {
        if (color == GLCD_ON || i == 0)
          template = ~template;
      }
      else if (fillType == FILL_THIRDUP)
        template = pattern3Up[distance];
      else if (fillType == FILL_THIRDDOWN)
        template = pattern3Down[distance];
      else // fillType == FILL_INVERSE
        template = ~lcdByte;

      // Depending on the draw color invert the template
      if (color == GLCD_OFF && fillType != FILL_INVERSE)
        template = ~template;

      // Merge the lcd byte and the template we just made
      if (doBits == 8)
      {
        // Full byte replace so no merging needed
        lcdByte = template;
      }
      else
      {
        // Partial byte replace
        mask = ((0xff >> (8 - doBits)) << startBit);
        lcdByte = ((lcdByte & ~mask) | (template & mask));
      }

      // We've got the final full or masked lcd byte
      glcdDataWrite(lcdByte);

      // For next x get the 3up/3down relative distance to align pixel
      if (distance == 2)
        distance = 0;
      else
        distance++;
    }

    // Move on to next y-pixel byte where we'll start at the first bit
    yByte++;
    startBit = 0;

    // Set reference to first template for next y-pixel byte
    if (fillType == FILL_THIRDUP)
      virY = virY + 2;
    else if (fillType == FILL_THIRDDOWN)
      virY = virY + 1;
  }
}

//
// Function: glcdGetWidthStr
//
// Get the pixel width of a string, including the trailing white
// space pixel
//
u08 glcdGetWidthStr(u08 font, char *data)
{
  u08 width = 0;
  u08 idxOffset;

  while (*data)
  {
    if (font == FONT_5X5P)
      width = width + glcdCharWidthGet(*data, &idxOffset) + 1;
    else // FONT_5X7N
      width = width + 6;
    data++;
  }
  return width;
}

//
// Function: glcdInverseDisplay
//
// Optimized function to invert the contents of the lcd display.
// Can also be achieved via glcdFillRectangle2().
//
/*void glcdInverseDisplay(void)
{
  u08 x,y;

  // We have a number of y byte pixel rows
  for (y = 0; y < (GLCD_YPIXELS / 8); y++)
  {
    // Buffer all 128 (GLCD_XPIXELS) bytes for the y-byte pixel row
    glcdBufferRead(0, y, GLCD_XPIXELS);

    // Go back to the beginning of the row and write the
    // inverted pixel bytes
    glcdSetAddress(0, y);
    for (x = 0; x < GLCD_XPIXELS; x++)
      glcdDataWrite(~(glcdBuffer[x]));
  }
}*/

//
// Function: glcdLine
//
// Draw a line from px[x1,y1] to px[x2,y2]
//
void glcdLine(u08 x1, u08 y1, u08 x2, u08 y2, u08 color)
{
  u08 n = 0;
  u08 i;
  s08 deltaX = x2 - x1;
  s08 deltaY = y2 - y1;
  u08 deltaXAbs = (u08)ABS(deltaX);
  u08 deltaYAbs = (u08)ABS(deltaY);
  u08 sgnDeltaX = SIGN(deltaX);
  u08 sgnDeltaY = SIGN(deltaY);
  u08 modifierX = (deltaYAbs >> 1);
  u08 modifierY = (deltaXAbs >> 1);
  u08 drawX = x1;
  u08 drawY = y1;
  u08 yLine = y1 >> 3;
  u08 lineCount = ABS((y2 >> 3) - yLine) + 1;
  u08 line;
  u08 startX, endX;
  s08 firstWrite;
  u08 readByte;
  u08 finalByte;
  u08 mode = 1;
  u08 endValue = deltaYAbs;

  // Set selector for line pixel generation
  if (deltaXAbs >= deltaYAbs)
  {
    mode = 0;
    endValue = deltaXAbs;
  }

  // Initialize buffer that stores the template of the line section to draw
  for (i = (x1 <= x2 ? x1 : x2); i <= (x1 >= x2 ? x1 : x2); i++)
    glcdBuffer[i] = 0;

  // Split up the draw line in sections of lcd y-lines
  for (line = 0; line < lineCount; line++)
  {
    // Find the x range for the y line section
    startX = drawX;
    endX = drawX;
    firstWrite = -1;

    // Apply the first line section pixel in the line buffer
    glcdBufferBitUpdate(drawX, drawY);

    // Add points until we find the end of a line or line section
    while (n < endValue)
    {
      // Set x and y draw points for line section pixel
      if (mode == 0)
      {
        modifierY += deltaYAbs;
        if (modifierY >= deltaXAbs)
        {
          modifierY -= deltaXAbs;
          drawY += sgnDeltaY;
        }
        drawX += sgnDeltaX;
      }
      else
      {
        modifierX += deltaXAbs;
        if (modifierX >= deltaYAbs)
        {
          modifierX -= deltaYAbs;
          drawX += sgnDeltaX;
        }
        drawY += sgnDeltaY;
      }

      // Detect end of line section
      if (yLine != (drawY >> 3))
        break;

      // Update the line section x start and end point
      if (drawX < startX)
        startX = drawX;
      if (drawX > endX)
        endX = drawX;

      // Update the line section pixel in the line buffer
      glcdBufferBitUpdate(drawX, drawY);
      n++;
    }

    // At this point a linebuffer contains the pixel template for the line
    // section. Now read all affected lcd pixel bytes and apply template.
    glcdSetAddress(startX, yLine);
    for (i = startX; i <= endX; i++)
    {
      // Do dummy read when needed and apply template
      if (i == startX || (i & GLCD_CONTROLLER_XPIXMASK) == 0)
        glcdDataRead();
      readByte = glcdDataRead();
      if (color == GLCD_ON)
        finalByte = readByte | glcdBuffer[i];
      else
        finalByte = readByte & ~glcdBuffer[i];

      // Save final byte while keeping track of first byte changed
      if (firstWrite == -1 && readByte != finalByte)
        firstWrite = i;
      glcdBuffer[i] = finalByte;
      glcdNextAddress();
    }

    // At this point the linebuffer contains the bytes to write to the lcd.
    // Write back starting at the first byte that has changed (if any).
    if (firstWrite >= 0)
      glcdSetAddress(firstWrite, yLine);
    for (i = startX; i <= endX; i++)
    {
      if (firstWrite >= 0 && i >= firstWrite)
        glcdDataWrite(glcdBuffer[i]);
      glcdBuffer[i] = 0;
    }

    // Starting points for next iteration
    n = n + 1;
    yLine = yLine + sgnDeltaY;
  }
}

//
// Function: glcdPrintNumber
//
// Print a number in two digits at current cursor location
//
void glcdPrintNumber(u08 n, u08 color)
{
  glcdWriteChar(n / 10 + '0', color);
  glcdWriteChar(n % 10 + '0', color);
}

//
// Function: glcdPrintNumberBg
//
// Print a number in two digits at current cursor location
// in background color
//
void glcdPrintNumberBg(u08 n)
{
  glcdPrintNumber(n, mcBgColor);
}

//
// Function: glcdPrintNumberFg
//
// Print a number in two digits at current cursor location
// in foreground color
//
void glcdPrintNumberFg(u08 n)
{
  glcdPrintNumber(n, mcFgColor);
}

//
// Function: glcdPutStr
//
// Write a character string starting at current cursor location
//
void glcdPutStr(char *data, u08 color)
{
  while (*data)
  {
    glcdWriteChar(*data, color);
    data++;
  }
}

//
// Function: glcdPutStrFg
//
// Write a character string starting at current cursor location
// in foreground color
//
void glcdPutStrFg(char *data)
{
  glcdPutStr(data, mcFgColor);
}

//
// Function: glcdPutStr2
//
// Write a character string starting at the px[x,y] position
//
u08 glcdPutStr2(u08 x, u08 y, u08 font, char *data, u08 color)
{
  return glcdPutStr3(x, y, font, data, 1, 1, color);
}

//
// Function: glcdPutStr3
//
// Write a character string starting at px[x,y] position with font scaling
//
u08 glcdPutStr3(u08 x, u08 y, u08 font, char *data, u08 xScale, u08 yScale,
  u08 color)
{
  u08 h = 0;
  u08 i = 0;
  u08 strWidth;
  u08 strHeight;
  u08 fontByte = 0;
  u08 lcdByte = 0;
  u08 currXScale = 0;
  u08 currYScale = 0;
  u08 lastYScale = 0;
  u08 currFontPixel = 0;
  u08 lastFontPixel = 0;
  u08 fontBytePixel = 0;
  u08 yByte = y / 8;
  u08 startBit = y % 8;
  u08 lcdPixelsToDo;
  u08 lcdPixelsLeft;
  u08 mask = 0;
  u08 bitmask;
  u08 template = 0;
  char *c;

  // Get the width and height of the entire string
  fontId = font;
  strWidth = glcdGetWidthStr(fontId, data) * xScale;
  if (font == FONT_5X5P)
    strHeight = 5;
  else
    strHeight = 7;
  strHeight = strHeight * yScale;

  // Loop through each y-pixel byte
  while (h < strHeight)
  {
    // In most cases we partly update an lcd byte
    if (startBit != 0 || strHeight - h < 8)
    {
      // Read all the required lcd bytes for this y-byte in the
      // line buffer and update them byte by byte
      glcdBufferRead(x, yByte, strWidth);
      if (startBit + (strHeight - h) > 8)
        lcdPixelsToDo = 8 - startBit;
      else
        lcdPixelsToDo = strHeight - h;
    }
    else
    {
      // We're going to write full lcd bytes
      lcdPixelsToDo = 8;
    }

    // As of now on for this y-pixel byte range we're going to write
    // consecutive lcd bytes
    glcdSetAddress(x, yByte);

    // Loop for each of the character width elements
    c = data;
    currXScale = 0;

    for (i = 0; i < strWidth; i++)
    {
      // Do we need to get the next font byte
      if (fontByteIdx > fontWidth || i == 0)
      {
        // Get next string character and start at first font byte
        fontByteIdx = 0;

        // Get entry point in font array and width of the character
        fontCharIdx = glcdFontIdxGet(*c);

        // Prepare for next character
        c++;
      }

      // When x scale of current font byte is reached get next font byte
      if (currXScale == xScale || i == 0)
      {
        // Get next font byte
        fontByte = glcdFontByteGet(color);

        // Restart x scaling
        currXScale = 0;

        // Prepare for next font byte
        fontByteIdx++;
        fontCharIdx++;
      }

      // Get lcd byte in case not all 8 pixels are to be processed
      if (lcdPixelsToDo != 8)
        lcdByte = glcdBuffer[i];

      // Set the number of bits to process in lcd byte
      lcdPixelsLeft = lcdPixelsToDo;

      // In case of x scaling, the template for the final lcd byte merge
      // is already known
      if (currXScale == 0)
      {
        // Reposition on y scale and font pixel
        currYScale = lastYScale;
        currFontPixel = lastFontPixel;

        // Set mask for final build of lcd byte
        mask = (0xff >> (8 - lcdPixelsLeft)) << startBit;

        // We can optimize when the fontbyte contains the template we need
        if (yScale == 1)
        {
          // No y scaling so we only need to shift the fontbyte to obtain
          // the lcd byte template
          template = (fontByte >> currFontPixel) << startBit;
          currFontPixel = currFontPixel + lcdPixelsLeft;
        }
        else
        {
          // There is y scaling: build the lcd byte template bit by bit
          template = 0;
          fontBytePixel = (fontByte >> currFontPixel);
          bitmask = (0x1 << startBit);
          while (lcdPixelsLeft != 0)
          {
            // Map a single fontbit on the lcd byte
            if ((fontBytePixel & 0x1) == 0x1)
              template = (template | bitmask);

            // Increment y scaling
            currYScale++;
            if (currYScale == yScale)
            {
              // End of y scaling; continue with next font pixel
              currYScale = 0;
              fontBytePixel = (fontBytePixel >> 1);
              currFontPixel++;
            }

            // Proceed with next lcd bit
            bitmask = (bitmask << 1);
            lcdPixelsLeft--;
          }
        }
      }

      // Add the template to the final lcd byte and write it to the lcd
      lcdByte = ((lcdByte & ~mask) | (template & mask));
      glcdDataWrite(lcdByte);

      // Set x scaling offset for next lcd byte
      currXScale++;
    }

    // Go to next y position where we'll start at the first bit
    yByte++;
    startBit = 0;
    lastYScale = currYScale;
    lastFontPixel = currFontPixel;
    h = h + lcdPixelsToDo;
  }

  // Width + trailing blank px
  return strWidth;
}

//
// Function: glcdPutStr3v
//
// Write a character string vertically starting at the px[x,y] position
// in either bottom-up or top-down orientation with font scaling
//
u08 glcdPutStr3v(u08 x, u08 y, u08 font, u08 orientation, char *data,
  u08 xScale, u08 yScale, u08 color)
{
  u08 h = 0;
  u08 i;
  u08 strWidth;
  u08 strHeight;
  u08 fontByte;
  u08 lcdByte = 0;
  u08 currXScale = 0;
  u08 currYScale = 0;
  u08 lastYScale = 0;
  u08 fontBytePixel;
  u08 yByte = y / 8;
  u08 startBit = y % 8;
  u08 lastFontByteIdx = 0;
  u08 lcdPixelsToDo;
  u08 lcdPixelsLeft;
  u08 mask = 0;
  u08 bitmask;
  u08 template = 0;
  char *c = data;
  char *startChar = c;
  s08 byteDelta;
  u08 xStart;
  u08 fontPixelStart;
  u08 lcdPixelStart;

  // Get the width and height of the entire string
  fontId = font;
  strHeight = glcdGetWidthStr(fontId, data) * yScale;
  if (font == FONT_5X5P)
    strWidth = 5;
  else
    strWidth = 7;
  fontPixelStart = strWidth - 1;
  strWidth = strWidth * xScale;

  // Get x starting position to read buffer
  if (orientation == ORI_VERTICAL_TD)
  {
    xStart = x - strWidth + 1;
    byteDelta = 1;
    lcdPixelStart = 0;
  }
  else
  {
    xStart = x;
    byteDelta = -1;
    fontPixelStart = 0;
    lcdPixelStart = 7;
  }

  // Loop through each y-pixel byte bit by bit
  while (h < strHeight)
  {
    // In some cases we partly update an lcd byte
    if ((startBit != 0 && orientation == ORI_VERTICAL_TD) ||
        (startBit != 7 && orientation == ORI_VERTICAL_BU) ||
         strHeight - h < 8)
    {
      // Read all the required lcd bytes for this y-byte in the
      // line buffer and update them byte by byte
      glcdBufferRead(xStart, yByte, strWidth);
      if (orientation == ORI_VERTICAL_TD &&
          (startBit + (strHeight - h) > 8))
        lcdPixelsToDo = 8 - startBit;
      else if (orientation == ORI_VERTICAL_BU &&
          ((8 - startBit) + (strHeight - h) > 8))
        lcdPixelsToDo = startBit + 1;
      else
        lcdPixelsToDo = strHeight - h;
    }
    else
    {
      // We're going to write full lcd bytes
      lcdPixelsToDo = 8;
    }

    // As of now on for this y-pixel byte range we're going to write
    // consecutive lcd bytes
    glcdSetAddress(xStart, yByte);

    // Start at the current string character and its font byte

    // Set mask for final build of lcd byte
    mask = (0xff >> (8 - lcdPixelsToDo));
    if (orientation == ORI_VERTICAL_TD)
      mask = mask << startBit;
    else
      mask = mask << (startBit + 1 - lcdPixelsToDo);

    // Restart x scaling
    currXScale = 0;
    fontBytePixel = fontPixelStart;

    // Loop through all x positions
    for (i = 0; i < strWidth; i++)
    {
      // Get lcd byte in case not all 8 pixels are to be processed
      if (lcdPixelsToDo != 8)
        lcdByte = glcdBuffer[i];

      // Set the number of bits to process in lcd byte
      lcdPixelsLeft = lcdPixelsToDo;

      // In case of x scaling, the template for the final lcd byte merge
      // is already known. So, only build it when we start with x scaling.
      if (currXScale == 0)
      {
        // Reposition on character, y scale and font byte
        c = startChar;
        currYScale = lastYScale;
        fontByteIdx = lastFontByteIdx;

        // Get entry point in font array and width of the character
        fontCharIdx = glcdFontIdxGet(*c) + fontByteIdx;

        // Start at the proper font byte
        fontByte = glcdFontByteGet(color);

        // Build the lcd byte template bit by bit
        template = 0;
        bitmask = (0x1 << startBit);
        while (lcdPixelsLeft != 0)
        {
          // Map a single font bit on the lcd byte template
          if ((fontByte & (1 << fontBytePixel)) != 0)
            template = (template | bitmask);

          // Proceed with next lcd bit
          if (orientation == ORI_VERTICAL_TD)
            bitmask = (bitmask << 1);
          else
            bitmask = (bitmask >> 1);

          // Increment y scaling
          currYScale++;
          if (currYScale == yScale)
          {
            // Continue with next font byte pixel or move to next
            // character in string
            if (fontByteIdx != fontWidth)
            {
              // Move to next font byte
              fontByteIdx++;
              fontCharIdx++;
            }
            else
            {
              // Processed the last font byte so move to next character
              // in string to process
              c++;
              fontByteIdx = 0;
              if (*c != 0)
              {
                // Not yet at end of string
                currYScale = 0;
                fontByteIdx = 0;

                // Get entry point in font array and width of the character
                fontCharIdx = glcdFontIdxGet(*c);
              }
            }

            // Get the font byte
            fontByte = glcdFontByteGet(color);

            // Reset y scaling
            currYScale = 0;
          }

          // Reduce number of pixels to process for this lcd byte
          lcdPixelsLeft--;
        }
      }

      // Add the template to the final lcd byte and write it to the lcd
      lcdByte = ((lcdByte & ~mask) | (template & mask));
      glcdDataWrite(lcdByte);

      // Set x scaling offset for next lcd byte
      currXScale++;
      if (currXScale == xScale)
      {
        // Reset x scaling and move to next or previous pixel in font byte
        currXScale = 0;
        fontBytePixel = fontBytePixel - byteDelta;
      }
    }

    // Go to next y byte where we'll start at either the first or last bit
    yByte = yByte + byteDelta;
    h = h + lcdPixelsToDo;
    startBit = lcdPixelStart;

    // Define new starting points in string, y scale and font byte.
    // Upon starting the next loop we will sync with these settings.
    startChar = c;
    lastYScale = currYScale;
    lastFontByteIdx = fontByteIdx;
  }

  // Height + trailing blank px
  return strHeight;
}

//
// Function: glcdRectangle
//
// Draw a rectangle
//
void glcdRectangle(u08 x, u08 y, u08 w, u08 h, u08 color)
{
  // When there's nothing to paint we're done
  if (w == 0 || h == 0)
    return;

  if (w > 2)
  {
    glcdFillRectangle2(x + 1, y, w - 2, 1, ALIGN_AUTO, FILL_FULL, color);
    glcdFillRectangle2(x + 1, y + h - 1, w - 2, 1, ALIGN_AUTO, FILL_FULL,
      color);
  }
  glcdFillRectangle2(x, y, 1, h, ALIGN_AUTO, FILL_FULL, color);
  if (w > 1 || h > 1)
    glcdFillRectangle2(x + w - 1, y, 1, h, ALIGN_AUTO, FILL_FULL, color);
}

//
// Function: glcdWriteChar
//
// Write a character at the current cursor position
//
void glcdWriteChar(unsigned char c, u08 color)
{
  u08 i = 0;
  u08 fontByte = 0;

  // Write all five font bytes
  for (i = 0; i < 5; i++)
  {
    fontByte = pgm_read_byte(&Font5x7[(c - 0x20) * 5 + i]);
    if (color == GLCD_OFF)
      glcdDataWrite(~fontByte);
    else
      glcdDataWrite(fontByte);
  }

  // Write a spacer line
  if (color == GLCD_OFF)
    glcdDataWrite(0xff);
  else
    glcdDataWrite(0x00);
}

//
// Function: glcdWriteCharFg
//
// Write a character at the current cursor position
// in foreground color
//
void glcdWriteCharFg(unsigned char c)
{
  glcdWriteChar(c, mcFgColor);
}

//
// Function: glcdBufferRead
//
// Read lcd data from a y byte into buffer glcdBuffer[]
//
static void glcdBufferRead(u08 x, u08 yByte, u08 len)
{
  u08 i;

#ifdef EMULIN
  // Check for buffer read overflow request
  if (len > GLCD_XPIXELS)
    emuCoreDump(ORIGIN_GLCD, __func__, 0, x, yByte, len);
#endif

  // Set cursor on first byte to read
  glcdSetAddress(x, yByte);

  for (i = 0; i < len; i++)
  {
    // Do a dummy read on the first read and the first read after switching
    // between controllers. For this refer to the controller specs.
    if (i == 0 || ((x + i) & GLCD_CONTROLLER_XPIXMASK) == 0)
      glcdDataRead();

    // Read the lcd byte and move to next cursor address
    glcdBuffer[i] = glcdDataRead();
    glcdNextAddress();
  }
}

//
// Function: glcdBufferBitUpdate
//
// Update a bit in a line buffer byte
//
static void glcdBufferBitUpdate(u08 x, u08 y)
{
  glcdBuffer[x] = (glcdBuffer[x] | (1 << (y & 0x7)));
}

//
// Function: glcdCharWidthGet
//
// Get the pixel width of a single character in the FONT_5X5P font
// (excluding the trailing white space pixel) and its internal
// fontmap array offset
//
static u08 glcdCharWidthGet(char c, u08 *idxOffset)
{
  if (c >= 'a' && c <= 'z')
    *idxOffset = 0x20;
  else if (c > 'z')
    *idxOffset = 26;
  else
    *idxOffset = 0;
  return pgm_read_byte(&Font5x5p[(c - 0x20 - *idxOffset) * 5]) >> 5;
}

//
// Function: glcdFontByteGet
//
// Get a font byte
//
static u08 glcdFontByteGet(u08 color)
{
  u08 fontByte;

  if (fontByteIdx != fontWidth)
  {
    // Get the font byte
    if (fontId == FONT_5X5P)
      fontByte = (pgm_read_byte(&Font5x5p[fontCharIdx]) & 0x1f);
    else
      fontByte = pgm_read_byte(&Font5x7[fontCharIdx]);
  }
  else
  {
    // End of character space line
    fontByte = 0x00;
  }

  // In case of reverse color invert font byte
  if (color == GLCD_OFF)
    fontByte = ~fontByte;

  return fontByte;
}

//
// Function: glcdFontIdxGet
//
// Get the start index of a character in a font array and its font width
//
static u16 glcdFontIdxGet(unsigned char c)
{
  u08 idxOffset;

  if (fontId == FONT_5X5P)
  {
    fontWidth = glcdCharWidthGet(c, &idxOffset);
    return (c - 0x20 - idxOffset) * 5;
  }
  else // font == FONT_5X7NP
  {
    fontWidth = 5;
    return (c - 0x20) * 5;
  }
}

