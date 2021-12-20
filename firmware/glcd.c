//*****************************************************************************
// Filename : 'glcd.c'
// Title    : High-level graphics lcd api for hd61202/ks0108 displays
//*****************************************************************************

#include "global.h"
#include "ks0108.h"
#include "ks0108conf.h"
#ifdef EMULIN
#include "emulator/mchronutil.h"
#endif
#include "glcd.h"

// Include 5x7 monospace and 5x5 proportional fonts
#include "font5x7.h"
#include "font5x5p.h"

// External data
extern volatile uint8_t mcBgColor, mcFgColor;

// The draw color to be used in every graphics function below, except
// glcdClearScreen().
// Use glcdColorSet*() to set its value and glcdColorGet() to get its value.
static u08 glcdColor;

// To optimize lcd access, all relevant data from a single lcd line can be read
// in first, then processed and then written back to the lcd. The glcdBuffer[]
// array below is the buffer that will be used for this purpose.
// This method drastically reduces switching between the read and write modes
// of the lcd and significantly improves the speed of the lcd api: smoother
// graphics. And yes, it will cost us 128 byte on the 2K stack. There is
// another downside. Since multiple glcd functions use this buffer, the
// Monochron application using these functions may not implement concurrent or
// threaded calls to these functions. This is however not an issue since
// glcd functionality is not used in interrupt functions and thus behaves like
// a monolithic application.
// Note: Use glcdBufferRead() to read lcd data into the buffer.
static u08 glcdBuffer[GLCD_XPIXELS];

// The following arrays contain bitmap templates for fill options third up/down
static const uint8_t __attribute__ ((progmem)) pattern3Up[] =
  { 0x49, 0x24, 0x92 };
static const uint8_t __attribute__ ((progmem)) pattern3Down[] =
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
static void glcdBufferBitSet(u08 x, u08 y);
static void glcdBufferRead(u08 x, u08 yByte, u08 len);
static u08 glcdFontByteGet(void);
static u16 glcdFontIdxGet(unsigned char c);
static u08 glcdFontInfoGet(char c);

//
// Function: glcdBitmap
//
// Draw a bitmap of up to 128 pixels wide and up to 8/16/32 pixels high at any
// (x,y) pixel position using a bitmap data array.
// Argument elmType defines the max pixel height of the bitmap data being
// drawn.
// Argument origin defines whether the bitmap data points to progmem or ram
// (stack/heap) data.
// Arguments xo and yo allow to set an (x,y) offset in the bitmap data. When
// combining these with w and h we can access any 'rectangular' section of
// bitmap element data, thus providing support for many image/sprite use cases.
//
void glcdBitmap(u08 x, u08 y, u16 xo, u08 yo, u08 w, u08 h, u08 elmType,
  u08 origin, void *bitmap)
{
  u08 i, j;
  u08 yByte = y / 8;
  u08 startBit = y % 8;
  u08 doBits = 0;
  u08 lcdByte = 0;
  u08 mask;
  u08 merge;
  uint32_t template = 0;

  // Loop through each affected y-pixel byte
  for (i = 0; i < h; i = i + doBits)
  {
    // Determine bits to process for a byte and create a mask for it
    if (h - i < 8 - startBit)
      doBits = h - i;
    else
      doBits = 8 - startBit;
    mask = (0xff >> (8 - doBits)) << startBit;

    // In case we partly update an lcd byte get current lcd data
    if (doBits < 8)
      glcdBufferRead(x, yByte, w);

    // As of now on we're going to write consecutive lcd bytes
    glcdSetAddress(x, yByte);

    // Loop for each x for current y-pixel byte
    for (j = 0; j < w; j++)
    {
      // Set template from bitmap data that we have to apply to the lcd byte
      if (elmType == ELM_BYTE)
      {
        if (origin == DATA_PMEM)
          template = pgm_read_byte((const uint8_t *)bitmap + xo + j) >> i;
        else
          template = (uint32_t)(((uint8_t *)bitmap)[xo + j] >> i);
      }
      else if (elmType == ELM_WORD)
      {
        if (origin == DATA_PMEM)
          template = pgm_read_word((const uint16_t *)bitmap + xo + j) >> i;
        else
          template = (uint32_t)(((uint16_t *)bitmap)[xo + j] >> i);
      }
      else // ELM_DWORD
      {
        if (origin == DATA_PMEM)
          template = pgm_read_dword((const uint32_t *)bitmap + xo + j) >> i;
        else
          template = ((uint32_t *)bitmap)[xo + j] >> i;
      }
      merge = (u08)((template >> yo) << startBit);
      if (glcdColor == GLCD_OFF)
        merge = ~merge;

      // Merge the lcd byte with merge template and write it to lcd
      if (doBits == 8)
        lcdByte = merge;
      else
        lcdByte = (glcdBuffer[j] & ~mask) | (merge & mask);
      glcdDataWrite(lcdByte);
    }

    // Move on to next y-pixel byte where we'll start at the first bit
    yByte++;
    startBit = 0;
  }
}

//
// Function: glcdBitmap8Pm
//
// Draw a bitmap up to 8 pixels high using a bitmap data array. The bitmap
// data resides in program space.
//
void glcdBitmap8Pm(u08 x, u08 y, u08 w, u08 h, const uint8_t *bitmap)
{
  glcdBitmap(x, y, 0, 0, w, h, ELM_BYTE, DATA_PMEM, (void *)bitmap);
}

//
// Function: glcdBitmap8Ra
//
// Draw a bitmap up to 8 pixels high using a bitmap data array. The bitmap
// data resides in ram.
//
void glcdBitmap8Ra(u08 x, u08 y, u08 w, u08 h, uint8_t *bitmap)
{
  glcdBitmap(x, y, 0, 0, w, h, ELM_BYTE, DATA_RAM, (void *)bitmap);
}

//
// Function: glcdBitmap16Pm
//
// Draw a bitmap up to 16 pixels high using a bitmap data array. The bitmap
// data resides in program space.
//
void glcdBitmap16Pm(u08 x, u08 y, u08 w, u08 h, const uint16_t *bitmap)
{
  glcdBitmap(x, y, 0, 0, w, h, ELM_WORD, DATA_PMEM, (void *)bitmap);
}

//
// Function: glcdBitmap16Ra
//
// Draw a bitmap up to 16 pixels high using a bitmap data array. The bitmap
// data resides in ram.
//
void glcdBitmap16Ra(u08 x, u08 y, u08 w, u08 h, uint16_t *bitmap)
{
  glcdBitmap(x, y, 0, 0, w, h, ELM_WORD, DATA_RAM, (void *)bitmap);
}

//
// Function: glcdBitmap32Pm
//
// Draw a bitmap up to 32 pixels high using a bitmap data array. The bitmap
// data resides in program space.
//
void glcdBitmap32PmFg(u08 x, u08 y, u08 w, u08 h, const uint32_t *bitmap)
{
  glcdBitmap(x, y, 0, 0, w, h, ELM_DWORD, DATA_PMEM, (void *)bitmap);
}

//
// Function: glcdBitmap32Ra
//
// Draw a bitmap up to 32 pixels high using a bitmap data array. The bitmap
// data resides in ram.
//
void glcdBitmap32RaFg(u08 x, u08 y, u08 w, u08 h, uint32_t *bitmap)
{
  glcdBitmap(x, y, 0, 0, w, h, ELM_DWORD, DATA_RAM, (void *)bitmap);
}

//
// Function: glcdCircle2
//
// Draw a (dotted) circle centered at px[xCenter,yCenter] with radius in px.
//
void glcdCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 lineType)
{
  u08 i;
  u08 j = MC_FALSE;
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
  for (i = 0; i <= radius; i++)
    glcdBuffer[GLCD_CONTROLLER_XPIXELS + i] = 0;

  // Split up the circle generation in circle sections per y-line byte
  for (yLine = ((yCenter - radius) >> 3); yLine <= yLineEnd; yLine++)
  {
    // Reset circle generation parameters
    y = radius;
    third = 0;
    tswitch = 3 - 2 * (s08)radius;
    xStart = MAX_U08;
    xEnd = 0;

    // Generate template pixels using the right side of the circle y-line
    for (x = 0; x <= y; x++)
    {
      if (lineType == CIRCLE_FULL ||
          (lineType == CIRCLE_THIRD && third == 0) ||
          (lineType != CIRCLE_THIRD && (x & 0x1) == half))
      {
        j = MC_FALSE;
        i = yCenter + y;
        if ((i >> 3) == yLine)
        {
          // Mark bottom-right pixel in template
          j = MC_TRUE;
          glcdBufferBitSet(GLCD_CONTROLLER_XPIXELS + x, i);
        }
        i = yCenter - y;
        if ((i >> 3) == yLine)
        {
          // Mark top-right pixel in template
          j = MC_TRUE;
          glcdBufferBitSet(GLCD_CONTROLLER_XPIXELS + x, i);
        }
        if (j == MC_TRUE)
        {
          // Sync x range scope to process
          i = xCenter + x;
          if (i < xStart)
            xStart = i;
          if (i > xEnd)
            xEnd = i;
          j = MC_FALSE;
        }
        i = yCenter + x;
        if ((i >> 3) == yLine)
        {
          // Mark bottom-right pixel in template
          j = MC_TRUE;
          glcdBufferBitSet(GLCD_CONTROLLER_XPIXELS + y, i);
        }
        i = yCenter - x;
        if ((i >> 3) == yLine)
        {
          // Mark top-right pixel in template
          j = MC_TRUE;
          glcdBufferBitSet(GLCD_CONTROLLER_XPIXELS + y, i);
        }
        if (j == MC_TRUE)
        {
          // Sync x range scope to process
          i = xCenter + y;
          if (i < xStart)
            xStart = i;
          if (i > xEnd)
            xEnd = i;
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

      // Set next offset for THIRD draw type
      if (third == 2)
        third = 0;
      else
        third++;
    }

    // At this point the circle section template for the y-line is generated.
    // In case the template is empty, which is possible when using the two HALF
    // draw types, then quit this y-line
    if (xStart == MAX_U08)
      continue;

    // Load line section for right side of circle y-line
    glcdBufferRead(xStart, yLine, xEnd - xStart + 1);

    // Map section template onto the right side circle section and write back
    // to lcd
    glcdSetAddress(xStart, yLine);
    j = GLCD_CONTROLLER_XPIXELS + (xStart - xCenter);
    for (i = 0; i <= xEnd - xStart; i++)
    {
      if (glcdColor == GLCD_ON)
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
        if (glcdColor == GLCD_ON)
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
// Function: glcdClearScreen
//
// Fill the lcd contents with the background color, and reset the controller
// display and startline settings that may have been modified by functional
// clock code
//
void glcdClearScreen(void)
{
  u08 i;
  u08 j;
  u08 data;

  if (mcBgColor == GLCD_OFF)
    data = 0x00;
  else
    data = 0xff;

  // Clear lcd by looping through all pages
  for (i = 0; i < GLCD_CONTROLLER_YPAGES; i++)
  {
    // Set page address
    glcdSetAddress(0, i);

    // Clear all lines of this page of display memory
    for (j = 0; j < GLCD_XPIXELS; j++)
      glcdDataWrite(data);
  }

  // Enable all controller displays and reset startline to 0
  glcdResetScreen();
}

//
// Function: glcdColorGet
//
// Get the draw color
//
u08 glcdColorGet(void)
{
  return glcdColor;
}

//
// Function: glcdColorSet
//
// Set the draw color to GLCD_OFF or GLCD_ON
//
void glcdColorSet(u08 color)
{
  glcdColor = color;
}

//
// Function: glcdColorSetBg
//
// Set the draw color to the current background color
//
void glcdColorSetBg(void)
{
  glcdColor = mcBgColor;
}

//
// Function: glcdColorSetFg
//
// Set the draw color to the current foreground color
//
void glcdColorSetFg(void)
{
  glcdColor = mcFgColor;
}

//
// Function: glcdDot
//
// Paint a dot
//
void glcdDot(u08 x, u08 y)
{
  u08 oldByte;
  u08 newByte;
  u08 mask = (1 << (y & 0x7));

  // Get lcd byte containing the dot
  glcdSetAddress(x, y >> 3);
  glcdDataRead();		// Dummy read
  oldByte = glcdDataRead();	// Read back current value

  // Set/clear dot in new lcd byte
  if (glcdColor == GLCD_ON)
    newByte = (oldByte | mask);
  else
    newByte = (oldByte & ~mask);

  // Prevent unnecessary write back to lcd if nothing has changed when compared
  // to current byte
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
//
void glcdFillCircle2(u08 xCenter, u08 yCenter, u08 radius, u08 fillType)
{
  s08 x;
  s08 y = radius;
  s08 tswitch = 3 - 2 * (u08)radius;
  u08 firstDraw = MC_TRUE;
  u08 drawSize = 0;

  // The code below still has the basic logic structure of the well known
  // method to fill a circle using tswitch. Optimizations applied in this
  // method are two-fold. First, an optimization avoids multiple vertical line
  // draw actions in the same area (so, draw the vertical line only once).
  // Consider this an optimization to the core of the tswitch method.
  // Second, an optimization merges multiple vertical line draw actions into a
  // single rectangle fill draw. This builds on optimizing the interface to our
  // lcd display and is therefor hardware oriented.
  for (x = 0; x <= y; x++)
  {
    if (x != y)
    {
      if (tswitch >= 0)
      {
        if (firstDraw == MC_TRUE)
          drawSize = 2 * drawSize;
        glcdFillRectangle2(xCenter - x, yCenter - y, drawSize + 1, y * 2 + 1,
          ALIGN_AUTO, fillType);
        if (x != 0)
          glcdFillRectangle2(xCenter + y, yCenter - x, 1, x * 2 + 1,
            ALIGN_AUTO, fillType);
      }
    }
    if (x != 0)
    {
      if (tswitch >= 0 && firstDraw == MC_FALSE)
        glcdFillRectangle2(xCenter + x - drawSize, yCenter - y, drawSize + 1,
          y * 2 + 1, ALIGN_AUTO, fillType);
      if (tswitch >= 0)
      {
        if (x != y)
          drawSize = 0;
        glcdFillRectangle2(xCenter - y, yCenter - x, drawSize + 1, x * 2 + 1,
          ALIGN_AUTO, fillType);
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
      firstDraw = MC_FALSE;
      drawSize = 0;
      y--;
    }
  }
}

//
// Function: glcdFillRectangle
//
// Fill a rectangle
//
void glcdFillRectangle(u08 x, u08 y, u08 w, u08 h)
{
  glcdFillRectangle2(x, y, w, h, ALIGN_AUTO, FILL_FULL);
}

//
// Function: glcdFillRectangle2
//
// Draw filled rectangle at px[x,y] with size px[w,h].
//
// align (note: used for filltypes HALF & THIRDUP/DOWN only):
// ALIGN_TOP      - Paint top left pixel of box
// ALIGN_BOTTOM   - Paint bottom left pixel of box
// ALIGN_AUTO     - Paint top left pixel of box relative to virtually painted
//                  px[0,0]
//
// fillType:
// FILL_FULL      - Fully filled
// FILL_HALF      - Half filled
// FILL_THIRDUP   - Third filled, creating an upward illusion
// FILL_THIRDDOWN - Third filled, creating a downward illusion
// FILL_INVERSE   - Inverse
// FILL_BLANK     - Clear
//
// Note: For optimization purposes we're using datatype s08, supporting a
// display width max 128 pixels. Our Monochron display happens to be 128 pixels
// wide.
//
void glcdFillRectangle2(u08 x, u08 y, u08 w, u08 h, u08 align, u08 fillType)
{
  u08 i, j;
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
      virY = -(h + startBit) % 3 + 1;
    else if (fillType == FILL_THIRDDOWN)
      virY = (h + startBit - 1) % 3;
    else if (fillType == FILL_HALF)
      virY = (h + startBit + 1) & 0x1;
  }
  else //if (align == ALIGN_AUTO)
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
  for (i = 0; i < h; i = i + doBits)
  {
    // In some cases we partly update an lcd byte or invert it
    if (startBit != 0 || h - i < 8 || fillType == FILL_INVERSE)
    {
      // Read all the required lcd bytes for this y-byte in the line buffer and
      // update them byte by byte
      useBuffer = MC_TRUE;
      glcdBufferRead(x, yByte, w);
    }
    else
    {
      // We're going to write full lcd bytes
      useBuffer = MC_FALSE;
    }

    // As of now on we're going to write consecutive lcd bytes
    glcdSetAddress(x, yByte);

    // Process max 8 y-pixel bits for the current y byte
    if (h - i < 8 - startBit)
      doBits = h - i;
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
        template = 0xaa;
      else
        template = 0x55;
    }

    // Loop for each x for current y-pixel byte
    for (j = 0; j < w; j++)
    {
      // Get lcd source byte when needed
      if (useBuffer == MC_TRUE)
        lcdByte = glcdBuffer[j];

      // Set template that we have to apply to the lcd byte
      if (fillType == FILL_FULL)
        template = 0xff;
      else if (fillType == FILL_BLANK)
        template = 0x00;
      else if (fillType == FILL_HALF)
      {
        if (glcdColor == GLCD_ON || j == 0)
          template = ~template;
      }
      else if (fillType == FILL_THIRDUP)
        template = pgm_read_byte(pattern3Up + distance);
      else if (fillType == FILL_THIRDDOWN)
        template = pgm_read_byte(pattern3Down + distance);
      else // fillType == FILL_INVERSE
        template = ~lcdByte;

      // Depending on the draw color invert the template
      if (glcdColor == GLCD_OFF && fillType != FILL_INVERSE)
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
// Get the pixel width of a string, including the trailing white space pixels
//
u08 glcdGetWidthStr(u08 font, char *data)
{
  u08 width = 0;

  fontId = font;
  while (*data)
  {
    glcdFontIdxGet(*data);
    width = width + fontWidth + 1;
    data++;
  }
  return width;
}

//
// Function: glcdLine
//
// Draw a line from px[x1,y1] to px[x2,y2]
//
void glcdLine(u08 x1, u08 y1, u08 x2, u08 y2)
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
  s08 lineCount;
  u08 line;
  u08 startX, endX;
  s08 firstWrite;
  u08 readByte;
  u08 finalByte;
  u08 mode = 1;
  u08 endValue = deltaYAbs;

  // Determine number of lcd y-pixel bytes to draw
  lineCount = (y2 >> 3) - yLine;
  if (lineCount < 0)
    lineCount = -lineCount;
  lineCount++;

  // Set selector for line pixel generation
  if (deltaXAbs >= deltaYAbs)
  {
    mode = 0;
    endValue = deltaXAbs;
  }

  // Initialize buffer that stores the template of the line section to draw
  endX = (x1 >= x2 ? x1 : x2);
  for (i = (x1 <= x2 ? x1 : x2); i <= endX; i++)
    glcdBuffer[i] = 0;

  // Split up the draw line in sections of lcd y-lines
  for (line = 0; line < lineCount; line++)
  {
    // Find the x range for the y line section
    startX = drawX;
    endX = drawX;
    firstWrite = -1;

    // Apply the first line section pixel in the line buffer
    glcdBufferBitSet(drawX, drawY);

    // Add points until we find the end of a line or line section
    while (n < endValue)
    {
      n++;
      // Set x and y draw points for line section pixel
      if (mode == 0)
      {
        modifierY = modifierY + deltaYAbs;
        if (modifierY >= deltaXAbs)
        {
          modifierY = modifierY - deltaXAbs;
          drawY = drawY + sgnDeltaY;
        }
        drawX = drawX + sgnDeltaX;
      }
      else
      {
        modifierX = modifierX + deltaXAbs;
        if (modifierX >= deltaYAbs)
        {
          modifierX = modifierX - deltaYAbs;
          drawX = drawX + sgnDeltaX;
        }
        drawY = drawY + sgnDeltaY;
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
      glcdBufferBitSet(drawX, drawY);
    }

    // At this point a linebuffer contains the pixel template for the line
    // section. Now read all affected lcd pixel bytes and apply template.
    for (i = startX; i <= endX; i++)
    {
      // Set cursor and do a dummy read on the first read and the first read
      // upon switching between controllers
      if (i == startX || (i & GLCD_CONTROLLER_XPIXMASK) == 0)
      {
        glcdSetAddress(i, yLine);
        glcdDataRead();
      }
      readByte = glcdDataRead();
      if (glcdColor == GLCD_ON)
        finalByte = readByte | glcdBuffer[i];
      else
        finalByte = readByte & ~glcdBuffer[i];

      // Save final byte while keeping track of first byte changed
      if (firstWrite == -1 && readByte != finalByte)
        firstWrite = i;
      glcdBuffer[i] = finalByte;
    }

    // At this point the linebuffer contains the bytes to write to the lcd.
    // Write back starting at the first byte that has changed (if any).
    if (firstWrite >= 0)
      glcdSetAddress(firstWrite, yLine);
    for (i = startX; i <= endX; i++)
    {
      if (i >= (u08)firstWrite)
        glcdDataWrite(glcdBuffer[i]);
      glcdBuffer[i] = 0;
    }

    // Starting points for next iteration
    yLine = yLine + sgnDeltaY;
  }
}

//
// Function: glcdPrintNumber
//
// Print a number in two digits at current cursor location
//
void glcdPrintNumber(u08 n)
{
  glcdWriteChar(n / 10 + '0');
  glcdWriteChar(n % 10 + '0');
}

//
// Function: glcdPrintNumberBg
//
// Print a number in two digits at current cursor location in background color
// and (re)sets the draw color to foreground.
//
void glcdPrintNumberBg(u08 n)
{
  glcdColorSetBg();
  glcdPrintNumber(n);
  glcdColorSetFg();
}

//
// Function: glcdPutStr
//
// Write a character string starting at current cursor location
//
void glcdPutStr(char *data)
{
  while (*data)
  {
    glcdWriteChar(*data);
    data++;
  }
}

//
// Function: glcdPutStr2
//
// Write a character string starting at the px[x,y] position
//
u08 glcdPutStr2(u08 x, u08 y, u08 font, char *data)
{
  return glcdPutStr3(x, y, font, data, 1, 1);
}

//
// Function: glcdPutStr3
//
// Write a character string starting at px[x,y] position with font scaling
//
u08 glcdPutStr3(u08 x, u08 y, u08 font, char *data, u08 xScale, u08 yScale)
{
  u08 h;
  u08 i;
  u08 j;
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
  u08 mask = 0;
  u08 bitmask;
  u08 template = 0;
  char *c;

  // Get the width and height of the entire string
  strWidth = glcdGetWidthStr(font, data) * xScale;
  if (font == FONT_5X5P)
    strHeight = 5;
  else
    strHeight = 7;
  strHeight = strHeight * yScale;

  // Loop through each y-pixel byte
  for (h = 0; h < strHeight; h = h + lcdPixelsToDo)
  {
    // In most cases we partly update an lcd byte
    if (startBit != 0 || strHeight - h < 8)
    {
      // Read all the required lcd bytes for this y-byte in the line buffer and
      // update them byte by byte
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
      // Do we need to get the next string character
      if (fontByteIdx > fontWidth || i == 0)
      {
        // Get next string character and start at first font byte
        fontByteIdx = 0;
        fontCharIdx = glcdFontIdxGet(*c);

        // Prepare for next character
        c++;
      }

      // When x scale of current font byte is reached get next font byte
      if (currXScale == xScale || i == 0)
      {
        currXScale = 0;
        fontByte = glcdFontByteGet();

        // Prepare for next font byte
        fontByteIdx++;
        fontCharIdx++;
      }

      // Get lcd byte in case not all 8 pixels are to be processed
      if (lcdPixelsToDo != 8)
        lcdByte = glcdBuffer[i];

      // In case of x scaling, the template for the final lcd byte merge is
      // already known
      if (currXScale == 0)
      {
        // Reposition on y scale and font pixel
        currYScale = lastYScale;
        currFontPixel = lastFontPixel;

        // Set mask for final build of lcd byte
        mask = (0xff >> (8 - lcdPixelsToDo)) << startBit;

        // We can optimize when the fontbyte contains the template we need
        if (yScale == 1)
        {
          // No y scaling so we only need to shift the fontbyte to obtain the
          // lcd byte template
          template = (fontByte >> currFontPixel) << startBit;
          currFontPixel = currFontPixel + lcdPixelsToDo;
        }
        else
        {
          // There is y scaling: build the lcd byte template bit by bit
          template = 0;
          fontBytePixel = (fontByte >> currFontPixel);
          bitmask = (0x1 << startBit);
          for (j = lcdPixelsToDo; j > 0; j--)
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
  }

  // Width + trailing blank px
  return strWidth;
}

//
// Function: glcdPutStr3v
//
// Write a character string vertically starting at the px[x,y] position in
// either bottom-up or top-down orientation with font scaling
//
u08 glcdPutStr3v(u08 x, u08 y, u08 font, u08 orientation, char *data,
  u08 xScale, u08 yScale)
{
  u08 h;
  u08 i;
  u08 j;
  u08 strWidth;
  u08 strHeight;
  u08 fontByte;
  u08 lcdByte = 0;
  u08 currYScale = 0;
  u08 lastYScale = 0;
  u08 fontBytePixel;
  u08 yByte = y / 8;
  u08 startBit = y % 8;
  u08 lastFontByteIdx = 0;
  u08 lcdPixelsToDo;
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
  strHeight = glcdGetWidthStr(font, data) * yScale;
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
  for (h = 0; h < strHeight; h = h + lcdPixelsToDo)
  {
    // In some cases we partly update an lcd byte
    if ((orientation == ORI_VERTICAL_TD && startBit != 0) ||
        (orientation == ORI_VERTICAL_BU && startBit != 7) ||
         strHeight - h < 8)
    {
      // Read all the required lcd bytes for this y-byte in the line buffer and
      // update them byte by byte
      glcdBufferRead(xStart, yByte, strWidth);
      if (orientation == ORI_VERTICAL_TD && (startBit + (strHeight - h) > 8))
        lcdPixelsToDo = 8 - startBit;
      else if (orientation == ORI_VERTICAL_BU &&
          (8 - startBit) + (strHeight - h) > 8)
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

    // Set mask for final build of lcd byte
    mask = (0xff >> (8 - lcdPixelsToDo));
    if (orientation == ORI_VERTICAL_TD)
      mask = mask << startBit;
    else
      mask = mask << (startBit + 1 - lcdPixelsToDo);

    // Loop through all x positions
    fontBytePixel = fontPixelStart;
    i = 0;
    while (i < strWidth)
    {
      // Reposition on character, y scale and font byte
      c = startChar;
      currYScale = lastYScale;
      fontByteIdx = lastFontByteIdx;

      // Get entry point in font array and width of the character
      fontCharIdx = glcdFontIdxGet(*c) + fontByteIdx;

      // Start at the proper font byte
      fontByte = glcdFontByteGet();

      // Build the lcd byte template bit by bit
      template = 0;
      bitmask = (0x1 << startBit);
      for (j = lcdPixelsToDo; j > 0; j--)
      {
        // Map a single font bit on the lcd byte template
        if ((fontByte & (1 << fontBytePixel)) != 0)
          template = (template | bitmask);

        // Proceed with next lcd bit
        if (orientation == ORI_VERTICAL_TD)
          bitmask = (bitmask << 1);
        else
          bitmask = (bitmask >> 1);

        // For y scaling repeat current font pixel or move to the next
        currYScale++;
        if (currYScale == yScale)
        {
          // Continue with next font byte or move to next character in string
          currYScale = 0;
          if (fontByteIdx != fontWidth)
          {
            // Move to next font byte
            fontByteIdx++;
            fontCharIdx++;
          }
          else
          {
            // Processed the last font byte so move to next character in string
            // to process
            fontByteIdx = 0;
            if (*(c + 1) != 0)
            {
              // Get entry point in font array and width of the character
              c++;
              fontCharIdx = glcdFontIdxGet(*c);
            }
          }

          // Get the font byte
          fontByte = glcdFontByteGet();
        }
      }

      // Add the template to the final lcd byte and write it to the lcd
      for (j = 0; j < xScale; j++)
      {
        if (lcdPixelsToDo != 8)
          lcdByte = glcdBuffer[i];
        lcdByte = ((lcdByte & ~mask) | (template & mask));
        glcdDataWrite(lcdByte);
        i++;
      }

      // Move to next or previous pixel in font byte
      fontBytePixel = fontBytePixel - byteDelta;
    }

    // Go to next y byte where we'll start at either the first or last bit
    yByte = yByte + byteDelta;
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
void glcdRectangle(u08 x, u08 y, u08 w, u08 h)
{
  // When there's nothing to paint we're done
  if (w == 0 || h == 0)
    return;

  if (w > 2)
  {
    glcdFillRectangle2(x + 1, y, w - 2, 1, ALIGN_AUTO, FILL_FULL);
    if (h > 1)
      glcdFillRectangle2(x + 1, y + h - 1, w - 2, 1, ALIGN_AUTO, FILL_FULL);
  }
  glcdFillRectangle2(x, y, 1, h, ALIGN_AUTO, FILL_FULL);
  if (w > 1)
    glcdFillRectangle2(x + w - 1, y, 1, h, ALIGN_AUTO, FILL_FULL);
}

//
// Function: glcdResetScreen
//
// Reset the lcd display by enabling display and setting startline to 0
//
void glcdResetScreen(void)
{
  u08 i;

  // Switch on display and reset startline
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    glcdControlWrite(i, GLCD_START_LINE | 0);
    glcdControlWrite(i, GLCD_ON_CTRL | GLCD_ON_DISPLAY);
  }
}

//
// Function: glcdWriteChar
//
// Write a character at the current cursor position
//
void glcdWriteChar(unsigned char c)
{
  u08 i = 0;
  u08 fontByte = 0;

  // Write all five font bytes
  for (i = 0; i < 5; i++)
  {
    fontByte = pgm_read_byte(&Font5x7[(c - 0x20) * 5 + i]);
    if (glcdColor == GLCD_OFF)
      glcdDataWrite(~fontByte);
    else
      glcdDataWrite(fontByte);
  }

  // Write a spacer line
  if (glcdColor == GLCD_OFF)
    glcdDataWrite(0xff);
  else
    glcdDataWrite(0x00);
}

//
// Function: glcdBufferBitSet
//
// Set a bit in a line buffer byte
//
static void glcdBufferBitSet(u08 x, u08 y)
{
  glcdBuffer[x] = (glcdBuffer[x] | (1 << (y & 0x7)));
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
  // Check for buffer read overflow request as well as attempting to read
  // beyond the end of the horizontal display size
  if ((int)x + len > GLCD_XPIXELS)
    emuCoreDump(CD_GLCD, __func__, 0, x, yByte, len);
#endif

  for (i = 0; i < len; i++)
  {
    // Set cursor and do a dummy read on the first read and the first read
    // upon switching between controllers. For this refer to the controller
    // specs.
    if (i == 0 || ((i + x) & GLCD_CONTROLLER_XPIXMASK) == 0)
    {
      glcdSetAddress(i + x, yByte);
      glcdDataRead();
    }

    // Read the lcd byte
    glcdBuffer[i] = glcdDataRead();
  }
}

//
// Function: glcdFontByteGet
//
// Get a font byte
//
static u08 glcdFontByteGet(void)
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
  if (glcdColor == GLCD_OFF)
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
  if (fontId == FONT_5X5P)
  {
    return glcdFontInfoGet(c);
  }
  else // font == FONT_5X7M
  {
    fontWidth = 5;
    return (c - 0x20) * 5;
  }
}

//
// Function: glcdFontInfoGet
//
// Get the pixel width of a single character in the FONT_5X5P font (excluding
// the trailing white space pixel) and return its internal font array offset
//
static u08 glcdFontInfoGet(char c)
{
  u08 idx;

  if (c >= 'a' && c <= 'z')
    idx = 0x20;
  else if (c > 'z')
    idx = 26;
  else
    idx = 0;
  idx = pgm_read_byte(&Font5x5pIdx[c - 0x20 - idx]);
  fontWidth = pgm_read_byte(&Font5x5p[idx]) >> 5;
  return idx;
}
