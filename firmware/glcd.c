//*****************************************************************************
// Filename : 'glcd.c'
// Title    : Graphic LCD API functions
//*****************************************************************************

#ifndef EMULIN
// AVR specific includes
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "util.h"
#else
#include <stdlib.h>
#include "emulator/stub.h"
#endif
#include "glcd.h"
#include "ratt.h"
#include "ks0108.h"
// include fonts
#include "font5x7.h"
#include "font5x5p.h"

// Local function prototypes
u08 glcdGetWidthChar(char c, u08 *idxOffset);
u08 glcdWriteChar2(u08 x, u08 y, u08 font, unsigned char c, u08 xScale,
  u08 yScale, u08 color);
u08 glcdWriteChar2v(u08 x, u08 y, u08 font, u08 orientation, unsigned char c,
  u08 xScale, u08 yScale, u08 color);

// To optimize LCD access, all relevant data from a single LCD line can be read
// in first, then processed and then written back to the LCD. The lcdLine[] array
// below is the buffer that will be used for this purpose.
// This method drastically reduces switching between the read and write modes of
// the LCD and significantly improves the speed of the LCD api: smoother graphics.
// Yes, it will cost us 128 byte on the stack, but those functions in the glcd
// library that will use this buffer will be optimized on speed (without a
// coding burden on every glcd function having to reserve its own LCD line
// buffer that will be taken from the stack anyway).
u08 lcdLine[GLCD_XPIXELS];

//
// Function: glcdCircle
//
// Draw a circle centered at px[x,y] with radius in px
//
/*void glcdCircle(u08 xcenter, u08 ycenter, u08 radius, u08 color)
{
  int tswitch, y, x = 0;
  unsigned char d;

  d = ycenter - xcenter;
  y = radius;
  tswitch = 3 - 2 * radius;
  while (x <= y)
  {
    glcdDot(xcenter + x, ycenter + y, color);
    glcdDot(xcenter + x, ycenter - y, color);
    glcdDot(xcenter - x, ycenter + y, color);
    glcdDot(xcenter - x, ycenter - y, color);
    glcdDot(ycenter + y - d, ycenter + x, color);
    glcdDot(ycenter + y - d, ycenter - x, color);
    glcdDot(ycenter - y - d, ycenter + x, color);
    glcdDot(ycenter - y - d, ycenter - x, color);
    if (tswitch < 0)
    {
      tswitch += (4 * x + 6);
    }
    else
    {
      tswitch += (4 * (x - y) + 10);
      y--;
    }
    x++;
  }
}*/

//
// Function: glcdCircle2
//
// Draw a (dotted) circle centered at px[x,y] with radius in px
//
void glcdCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 lineType, u08 color)
{
  s08 tswitch, y, x = 0;
  u08 filter = 0;
  s08 d;

  if (lineType == CIRCLE_HALF_U)
    filter = 1;
  d = yCenter - xCenter;
  y = radius;
  tswitch = 3 - 2 * radius;

  while (x <= y)
  {
    if (lineType == CIRCLE_FULL ||
        ((lineType == CIRCLE_HALF_U || lineType == CIRCLE_HALF_E) &&
         ((x & 0x1) == filter)) ||
        (lineType == CIRCLE_THIRD && x % 3 == 0))
    {
      glcdDot(xCenter + x, yCenter + y, color);
      glcdDot(xCenter + x, yCenter - y, color);
      glcdDot(xCenter - x, yCenter + y, color);
      glcdDot(xCenter - x, yCenter - y, color);
      glcdDot(yCenter + y - d, yCenter + x, color);
      glcdDot(yCenter + y - d, yCenter - x, color);
      glcdDot(yCenter - y - d, yCenter + x, color);
      glcdDot(yCenter - y - d, yCenter - x, color);
    }
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
  }
}

//
// Function: glcdDot
//
// Paint a dot in a particular color
//
void glcdDot(u08 x, u08 y, u08 color)
{
  unsigned char temp;

  glcdSetAddress(x, y / 8);
  temp = glcdDataRead();	// dummy read
  temp = glcdDataRead();	// read back current value
  glcdSetAddress(x, y / 8);
  if (color == ON)
    glcdDataWrite(temp | (1 << (y & 0x7)));
  else
    glcdDataWrite(temp & ~(1 << (y & 0x7)));
}

//
// Function: glcdFillCircle2
//
// Draw a filled circle centered at px[x,y] with radius in px
// Note: fillType FILL_INVERSE is NOT supported
//
void glcdFillCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 fillType, u08 color)
{
  s08 tswitch, y, x = 0;
  s08 d;

  d = yCenter - xCenter;
  y = radius;
  tswitch = 3 - 2 * radius;
  while (x <= y)
  {
    glcdFillRectangle2(xCenter + x, yCenter - y, 1, y * 2, ALIGN_AUTO,
      fillType, color);
    glcdFillRectangle2(xCenter - x, yCenter - y, 1, y * 2, ALIGN_AUTO,
      fillType, color);
    glcdFillRectangle2(yCenter + y - d, yCenter - x, 1, x * 2, ALIGN_AUTO,
      fillType, color);
    glcdFillRectangle2(yCenter - y - d, yCenter - x, 1, x * 2, ALIGN_AUTO, 
      fillType, color);
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
// FILL_INVERSE   - Inverse (ignore param color)
// FILL_BLANK     - Clear
//
void glcdFillRectangle2(u08 x, u08 y, u08 a, u08 b, u08 align, u08 fillType, u08 color)
{
  u08 h, i, j, virInitX, virInitY;
  u08 virX, virY;
  u08 yByte;
  u08 lcdByte = 0;
  u08 lcdBitPos;
  u08 startLcdBitPos;
  u08 bitColor = color;
  u08 revColor;
  u08 useBuffer;
  u08 processBits;

  // Define draw color
  if (color == ON)
    revColor = OFF;
  else
    revColor = ON;

  // So, why the virInitY values 6 and 252?
  // Because each of these values y will calculate to y%2=0 and y%3=0.
  // We're using the % function to determine the bit patterns for the HALF
  // (using %2) and THIRD (using %3) fill types.
  // We must make sure that the virY will never overflow from 255 to 0 or
  // underflow from 0 to 255.
  // And that's why we pick a very high value (=252) that can be decreased
  // (for FILL_THIRDUP) at will and pick a very low value that can be
  // increased (for all other filltypes) at will, but in both cases will
  // never over/underflow.
  // This mechanism works with displays where y is less than ~250 px. 

  // Define topleft or bottomleft or topleft auto color
  if (align == ALIGN_TOP)
  {
    virInitX = 0;
    if (fillType == FILL_THIRDUP)
      virInitY = 252;
    else
      virInitY = 6;
  }
  else if (align == ALIGN_BOTTOM)
  {
    virInitX = 0;
    if (fillType == FILL_THIRDUP)
      virInitY = 252 - 1 + (b % 3);
    else if (fillType == FILL_THIRDDOWN)
      virInitY = 6 + 1 - (b % 3);
    else if (fillType == FILL_HALF)
      virInitY = 6 - 1 + (b % 2);
    else
      virInitY = 6;
  }
  else // auto = align to (0,0)
  {
    virInitX = x;
    if (fillType == FILL_THIRDUP)
      virInitY = 252 - (y % 3);
    else if (fillType == FILL_THIRDDOWN)
      virInitY = 6 + (y % 3);
    else if (fillType == FILL_HALF)
      virInitY = 6 + (y % 2);
    else
      virInitY = 6;
  }

  // Loop through each y pixel byte
  h = 0;
  while (h < b)
  {
    yByte = (y + h) / 8;
    startLcdBitPos = (y + h) % 8;

    // In some cases we partly update an lcd byte or invert it
    if (startLcdBitPos != 0 || b - h < 8 || fillType == FILL_INVERSE)
    {
      // Read all the required LCD bytes for this 8-bit y position
      // in our line buffer and update them byte by byte
      useBuffer = GLCD_TRUE;
      for (i = 0; i < a; i++)
      {
        glcdSetAddress(x + i, yByte);
        lcdLine[i] = glcdDataRead();	// dummy read
        lcdLine[i] = glcdDataRead();	// read back current value
      }
    }
    else
    {
      // We're going to write full LCD bytes
      useBuffer = GLCD_FALSE;
    }

    // As of now on we're going to write consecutive LCD bytes
    glcdSetAddress(x, yByte);

    // Process max 8 y-pixel bits for the current y byte
    // Do this either by one bit at a time or by a whole byte at once
    if (b - h < 8 - startLcdBitPos)
      processBits = b - h;
    else
      processBits = 8 - startLcdBitPos;

    // Set start for virtual x
    virX = virInitX;

    // Loop for each x for current 8-bit y-pixels byte
    for (i = 0; i < a; i++)
    {
      // Get lcd template byte (if needed)
      if (useBuffer == GLCD_TRUE)
        lcdByte = lcdLine[i];

      // Start at the proper bit in the byte
      lcdBitPos = startLcdBitPos;

      // Set virtual y position
      if (fillType == FILL_THIRDUP)
         virY = virInitY - h;
      else
         virY = virInitY + h;

      // Process a set of y bits or a full y byte
      j = 0;
      while (j < processBits)
      {
        // Can we process an lcd byte at once or do we process bit by bit
        if (processBits == 8)
        {
          // Start of lcd byte and at least 8 bits to go: full lcd byte
          // Define a template for each of the fill types
          if (fillType == FILL_FULL)
          {
            lcdByte = 0xFF;
          }
          else if (fillType == FILL_HALF)
          {
            if ((virX % 2) == (virY % 2))
              lcdByte = 0x55;
            else
              lcdByte = 0xAA;
          }
          else if (fillType == FILL_THIRDDOWN)
          {
            if ((virX % 3) == (virY % 3))
              lcdByte = 0x49;
            else if (((virX + 1) % 3) == (virY % 3))
              lcdByte = 0x24;
            else
              lcdByte = 0x92;
          }
          else if (fillType == FILL_THIRDUP)
          {
            if ((virX % 3) == (virY % 3))
              lcdByte = 0x49;
            else if (((virX + 1) % 3) == (virY % 3))
              lcdByte = 0x92;
            else
              lcdByte = 0x24;
          }
          else if (fillType == FILL_INVERSE)
          {
            if (color == ON)
              lcdByte = ~lcdByte;
          }
          else // fillType == FILL_BLANK
          {
            lcdByte = 0x00;
          }

          // Depending on the draw color invert the template
          if (color != ON)
            lcdByte = ~lcdByte;

          // Processed all 8 bits of a full lcd byte
          j = j + 8;
        }
        else
        {
          // Not at start of lcd byte or less than 8 y-bits to go: bit by bit

          // Define for each of the fill types how the color should be
          if (fillType == FILL_FULL)
          {
            bitColor = color;
          }
          else if (fillType == FILL_HALF)
          {
            if ((virX % 2) == (virY % 2))
              bitColor = color;
            else
              bitColor = revColor;
          }
          else if (fillType == FILL_THIRDDOWN || fillType == FILL_THIRDUP)
          {
            if ((virX % 3) == (virY % 3))
              bitColor = color;
            else
              bitColor = revColor;
          }
          else if (fillType == FILL_INVERSE)
          {
            if (((lcdByte >> lcdBitPos) & 0x1) == 0)
              bitColor = ON;
            else
              bitColor = OFF;
          }
          else // fillType == FILL_BLANK
          {
            bitColor = revColor;
          }

          // Set or clear the bit
          if (bitColor == ON)
            lcdByte |= _BV(lcdBitPos);
          else
            lcdByte &= ~_BV(lcdBitPos);

          // Shift virtual position for next lcd bit
          if (fillType == FILL_THIRDUP)
            virY--;
          else
            virY++;

          // Processed a single bit in lcd byte
          j++;
          lcdBitPos++;
        }
      }

      // We're done with a single LCD byte: write it back
      glcdDataWrite(lcdByte);

      // Move on to next x
      virX++;
    }

    // Move on to next y-pixel byte
    h = h + processBits;
  }
}

//
// Function: glcdGetWidthChar
//
// Get the pixel width of a single character in the FONT_5X5P font
// (excluding the trailing white space pixel) and its internal
// fontmap array offset
//
u08 glcdGetWidthChar(char c, u08 *idxOffset)
{
  if (c >= 'a' && c <= 'z')
    *idxOffset = 0x20;
  else if (c > 'z')
    *idxOffset = 26;
  else
    *idxOffset = 0;
  return pgm_read_byte(&Font5x5p[(c - 0x20 - *idxOffset) * 6]);
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
      width = width + glcdGetWidthChar(*data, &idxOffset) + 1;
    else // FONT_5X7N
      width = width + 6;
    data++;
  }
  return width;
}

//
// Function: glcdInverseDisplay
//
// Optimized function to invert the contents of the LCD display.
// Can also be achieved via glcdFillRectangle2().
//
/*void glcdInverseDisplay(void)
{
  u08 x,y;

  // We have a number of y byte pixel rows
  for (y = 0; y < (GLCD_YPIXELS / 8); y++)
  {
    // Invert and store all 128 (GLCD_XPIXELS) bytes in the y
    // byte pixel row
    for (x = 0; x < GLCD_XPIXELS; x++)
    {
      glcdSetAddress(x, y);
      lcdLine[x] = glcdDataRead(); // dummy read
      lcdLine[x] = ~glcdDataRead(); // read back current value
    }

    // Go back to the beginning of the row and write the
    // inverted bytes back
    glcdSetAddress(0, y);
    for (x = 0; x < GLCD_XPIXELS; x++)
    {
      glcdDataWrite(lcdLine[x]);
    }
  }
}*/

//
// Function: glcdLine
//
// Draw a line from px[x1,y1] to px[x2,y2]
//
void glcdLine(u08 x1, u08 y1, u08 x2, u08 y2, u08 color)
{
  s16 n, deltaX, deltaY, sgnDeltaX, sgnDeltaY, deltaXAbs, deltaYAbs, x, y, drawX, drawY;

  deltaX = x2 - x1;
  deltaY = y2 - y1;
  deltaXAbs = ABS(deltaX);
  deltaYAbs = ABS(deltaY);
  sgnDeltaX = SIGN(deltaX);
  sgnDeltaY = SIGN(deltaY);
  x = deltaYAbs >> 1;
  y = deltaXAbs >> 1;
  drawX = x1;
  drawY = y1;

  glcdDot(drawX, drawY, color);

  if(deltaXAbs >= deltaYAbs)
  {
    for (n = 0; n < deltaXAbs; n++)
    {
      y += deltaYAbs;
      if(y >= deltaXAbs)
      {
        y -= deltaXAbs;
        drawY += sgnDeltaY;
      }
      drawX += sgnDeltaX;
      glcdDot(drawX, drawY, color);
    }
  }
  else
  {
    for (n = 0; n < deltaYAbs; n++)
    {
      x += deltaXAbs;
      if(x >= deltaYAbs)
      {
        x -= deltaYAbs;
        drawX += sgnDeltaX;
      }
      drawY += sgnDeltaY;
      glcdDot(drawX, drawY, color);
    }
  }
};

//
// Function: glcdPrintNumber
//
// Print a number in two digits at current cursor location
//
void glcdPrintNumber(uint8_t n, u08 color)
{
  glcdWriteChar(n / 10 + '0', color);
  glcdWriteChar(n % 10 + '0', color);
}

//
// Function: glcdPutStr
//
// Write a character string starting at the px[x,y] position
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
u08 glcdPutStr3(u08 x, u08 y, u08 font, char *data, u08 xScale,
  u08 yScale, u08 color)
{
  u08 h,i;
  u08 fontByte;
  u08 lcdByte;
  u08 strWidth, width;
  u08 strHeight, height;

  u08 currYScale, lastYScale;
  u08 currXScale;
  u08 currFontPixel, lastFontPixel;
  u08 fontBytePixel;
  u08 lcdPixelStart, fontByteIdx;
  u08 lcdPixelsToDo, lcdPixelsLeft;
  u08 maskByte;
  u08 idxOffset;
  u08 yByte;

  char *c;
  u08 currChar;

  // Get the height of the font character
  if (font == FONT_5X5P)
    height = 5;
  else
    height = 7;

  // Get the width and height of the entire string
  strWidth = glcdGetWidthStr(font, data) * xScale;
  strHeight = height * yScale;

  // Prepare our main loop
  lcdByte = 0;
  fontByte = 0;
  fontByteIdx = 0;
  currFontPixel = 0;
  currYScale = 0;
  lastYScale = 0;
  lastFontPixel = 0;
  fontBytePixel = 0;
  currChar = '\0';
  h = 0;

  // Loop through each y pixel byte
  while (h < strHeight)
  {
    // For this y pixel byte range get the number of lcd pixels to process
    // and the pixel start position in each lcd byte 
    yByte = (y + h) / 8;
    lcdPixelStart = (y + h) % 8;

    // In most cases we partly update an lcd byte
    if (lcdPixelStart != 0 || strHeight - h < 8)
    {
      // Read all the required LCD bytes for this 8-bit y position
      // in our line buffer and update them byte by byte
      if (lcdPixelStart + (strHeight - h) > 8)
        lcdPixelsToDo = 8 - lcdPixelStart;
      else
        lcdPixelsToDo = strHeight - h;
      for (i = 0; i < strWidth; i++)
      {
        glcdSetAddress(x + i, yByte);
        lcdLine[i] = glcdDataRead();	// dummy read
        lcdLine[i] = glcdDataRead();	// read back current value
      }
    }
    else
    {
      // We're going to write full LCD bytes
      lcdPixelsToDo = 8;
    }

    // As of now on for this y pixel byte range we're going to write
    // consecutive lcd bytes
    glcdSetAddress(x, yByte);

    // Loop for each of the character width elements
    c = data;
    currXScale = 0;
    width = 0;

    for (i = 0; i < strWidth; i++)
    {
      // Do we need to get the next font byte
      if (fontByteIdx > width || i == 0)
      {
        // Get next string character
        currChar = *c;

        // Prepare for next character
        c++;

        // Get font byte width of character
        if (font == FONT_5X5P)
          width = glcdGetWidthChar(currChar, &idxOffset);
        else
          width = 5;

        // Start at first font byte
        fontByteIdx = 0;
      }

      // When x scale of current font byte is reached get next font byte
      if (currXScale == xScale || i == 0)
      {
        if (fontByteIdx != width)
        {
          // Get the next font byte
          if (font == FONT_5X5P)
            fontByte = pgm_read_byte(&Font5x5p[(currChar - 0x20 - idxOffset) * 6 +
              fontByteIdx + 1]);
          else
            fontByte = pgm_read_byte(&Font5x7[(currChar - 0x20) * 5 + fontByteIdx]);
        }
        else
        {
          // End of character space line
          fontByte = 0x00;
        }

        // Prepare for next font byte
        fontByteIdx++;

        // In case of reverse color invert font byte
        if (color == OFF)
          fontByte = ~fontByte;

        // Restart x scaling
        currXScale = 0;
      }

      // Counter for number of bits to process
      lcdPixelsLeft = lcdPixelsToDo;

      // If our lcd byte is a full byte and is not the first scaling byte,
      // then the last processed lcd byte is identical to the one we created
      // in the previous loop.
      // As such, skip compiling the next lcd byte since we're already done. 
      if (lcdPixelsToDo != 8 || currXScale == 0)
      {
        // Reposition on y scale and font pixel
        currYScale = lastYScale;
        currFontPixel = lastFontPixel;

        // Get LCD byte to process
        if (lcdPixelsToDo != 8)
        {
          lcdByte = lcdLine[i];
          maskByte = (0x1 << lcdPixelStart);
        }
        else
        {
          lcdByte = (color == OFF ? 0x00 : 0xff);
          maskByte = 0x1;
        }

        // Add the font bits
        fontBytePixel = (fontByte >> currFontPixel);
        while (lcdPixelsLeft != 0)
        {
          // Map the fontbit on the lcd byte
          if ((fontBytePixel & 0x1) == 0x0)
          {
            // Clear bit
            lcdByte = (lcdByte & ~maskByte);
          }
          else
          {
            // Set bit
            lcdByte = (lcdByte | maskByte);
          }

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
          maskByte = (maskByte << 1);
          lcdPixelsLeft--;
        }
      }

      // Write the processed lcd byte
      glcdDataWrite(lcdByte);

      // Set x scaling offset for next lcd byte
      currXScale++;
    }

    // Go to next y position
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
  u08 heightChar = 0;
  u08 height = 0;
  u08 newY = y;

  while (*data)
  {
    heightChar = glcdWriteChar2v(x, newY, font, orientation, *data, xScale,
      yScale, color);
    if (orientation == ORI_VERTICAL_BU)
      newY = newY - heightChar;
    else
      newY = newY + heightChar;
    height = height + heightChar;
    data++;
  }
  return height;
}

//
// Function: glcdRectangle
//
// Draw a rectangle
//
void glcdRectangle(u08 x, u08 y, u08 w, u08 h, u08 color)
{
  if (w > 2)
  {
    glcdFillRectangle2(x + 1, y, w - 2, 1, ALIGN_AUTO, FILL_FULL, color);
    glcdFillRectangle2(x + 1, y + h - 1, w - 2, 1, ALIGN_AUTO, FILL_FULL, color);
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
    fontByte = pgm_read_byte(&Font5x7[((c - 0x20) * 5) + i]);
    if (color == OFF)
      glcdDataWrite(~fontByte);
    else
      glcdDataWrite(fontByte);
  }

  // Write a spacer line
  if (color == OFF) 
    glcdDataWrite(0xff);
  else 
    glcdDataWrite(0x00);

  glcdStartLine(0);
}

//
// Function: glcdWriteChar2v
//
// Write a vertical character starting at px[x,y] position with font scaling
//
u08 glcdWriteChar2v(u08 x, u08 y, u08 font, u08 orientation, unsigned char c,
  u08 xScale, u08 yScale, u08 color)
{
  u08 i,j,k,l;
  u08 lcdByte;
  s08 lcdBitPos;
  s08 ovflPos;
  s08 nextPos;
  u08 fontByte, fontBit;
  u08 maskByte;
  u08 posX, posY;
  u08 lastY;
  u08 idxOffset = 0;
  u08 width;
  u08 length;

  // Write in LCD bytes. There may be overflow to next LCD bytes
  // as vertically it may not fit completely.

  // Get the width and height of the character
  if (font == FONT_5X5P)
  {
    length = glcdGetWidthChar(c, &idxOffset);
    width = 5;
  }
  else
  {
    length = 5;
    width = 7;
  }

  // Define lcd byte cutoffs and processing direction
  if (orientation == ORI_VERTICAL_BU)
  {
    ovflPos = -1;
    nextPos = -1;
  }
  else
  {
    ovflPos = 8;
    nextPos = 1;
  }

  // Define offset for x
  posX = x;

  // The x is represented by the width
  for (i = 0; i < width; i++)
  {
    // Loop for width scaling
    for (j = 0; j < xScale; j++)
    {
      // Read initial byte to change
      posY = y;
      lastY = posY;
      lcdBitPos = posY % 8;
      glcdSetAddress(posX, lastY / 8);
      lcdByte = glcdDataRead(); // dummy read
      lcdByte = glcdDataRead(); // read back current value

      // The y is represented by the length
      for (k = 0; k <= length; k++)
      {
        // Get the fontbyte we need
        if (k != length)
        {
          if (font == FONT_5X5P)
            fontByte = pgm_read_byte(&Font5x5p[(c - 0x20 - idxOffset) * 6 + k + 1]);
          else
            fontByte = pgm_read_byte(&Font5x7[(c - 0x20) * 5 + k]);
        }
        else
        {
            fontByte = 0;
        }
        if (color == OFF)
          fontByte = ~fontByte;
        fontBit = ((fontByte >> i) & 0x01);

        // Loop for height scaling
        for (l = 0; l < yScale; l++)
        {
          // If we have overflow move to previous lcd byte
          if (lcdBitPos == ovflPos)
          {
            // Write the last processed lcd byte
            glcdSetAddress(posX, lastY / 8);
            glcdDataWrite(lcdByte);

            // And read the next one
            lastY = posY;
            glcdSetAddress(posX, lastY / 8);
            lcdByte = glcdDataRead(); // dummy read
            lcdByte = glcdDataRead(); // read back current value

           // Define start position in next lcd byte
           if (orientation == ORI_VERTICAL_BU)
             lcdBitPos = 7;
           else
             lcdBitPos = 0;
          }

          // Map the fontbit on the lcd byte
          maskByte = (0x1 << lcdBitPos);
          if (fontBit == 0x0)
          {
            // Clear bit
            lcdByte = (lcdByte & ~maskByte);
          }
          else
          {
            // Set bit
            lcdByte = (lcdByte | maskByte);
          }

          // Go to next lcd bit and y position
          lcdBitPos = lcdBitPos + nextPos;
          posY = posY + nextPos;
        }
      }

      // Write the current working LCD byte
      glcdSetAddress(posX, lastY / 8);
      glcdDataWrite(lcdByte);
      posX = posX - nextPos;
    }
  }

  // Length + trailing blank px
  return (length + 1) * yScale;
}

