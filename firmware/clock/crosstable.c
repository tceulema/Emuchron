//*****************************************************************************
// Filename : 'crosstable.c'
// Title    : Animation code for MONOCHRON cross table clock
//*****************************************************************************

#ifdef EMULIN
#include "../emulator/stub.h"
#else
#include "../util.h"
#endif
#include "../ks0108.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"
#include "crosstable.h"

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcFgColor;

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];

// Local function prototypes
static void spotTextBox(u08 x, u08 y, u08 direction, char *data);

//
// Function: spotCrossTableCycle
//
// Update the Spotfire cross table and filter panel
//
void spotCrossTableCycle(void)
{
  char newVal[3];

  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == GLCD_FALSE)
    return;

  DEBUGP("Update CrossTable");

  // Verify changes in time
  if (mcClockNewTS != mcClockOldTS || mcClockInit == GLCD_TRUE)
  {
    animValToStr(mcClockNewTS, newVal);
    glcdPutStr2(71, 40, FONT_5X7N, newVal, mcFgColor);
  }
  if (mcClockNewTM != mcClockOldTM || mcClockInit == GLCD_TRUE)
  {
    animValToStr(mcClockNewTM, newVal);
    glcdPutStr2(49, 40, FONT_5X7N, newVal, mcFgColor);
  }
  if (mcClockNewTH != mcClockOldTH || mcClockInit == GLCD_TRUE)
  {
    animValToStr(mcClockNewTH, newVal);
    glcdPutStr2(27, 40, FONT_5X7N, newVal, mcFgColor);
  }
}

//
// Function: spotCrossTableInit
//
// Initialize the lcd display of a Spotfire cross table
//
void spotCrossTableInit(u08 mode)
{
  DEBUGP("Init CrossTable");

  // Draw Spotfire form layout
  spotCommonInit("cross table", mode);

  // Draw static part of cross table
  // 1 - The crosstable labels in a textbox
  spotTextBox(40, 20, ORI_HORIZONTAL, "columns");
  spotTextBox(11, 46, ORI_VERTICAL_BU, "none");
  spotTextBox(73, 54, ORI_HORIZONTAL, animSec);
  spotTextBox(51, 54, ORI_HORIZONTAL, animMin);
  spotTextBox(26, 54, ORI_HORIZONTAL, animHour);
  // 2 - Crosstable x-axis column names
  glcdPutStr2(71, 31, FONT_5X5P, animSec, mcFgColor);
  glcdPutStr2(48, 31, FONT_5X5P, animMin, mcFgColor);
  glcdPutStr2(25, 31, FONT_5X5P, animHour, mcFgColor);
  // 3 - The crosstable layout itself
  glcdRectangle(21, 29, 67, 21, mcFgColor);
  glcdFillRectangle(22, 37, 65, 1, mcFgColor);
  glcdFillRectangle(43, 30, 1, 19, mcFgColor);
  glcdFillRectangle(65, 30, 1, 19, mcFgColor);
}

//
// Function: spotTextBox
//
// Draw a fancy textbox for a crosstable label
//
static void spotTextBox(u08 x, u08 y, u08 direction, char *data)
{
  u08 bx, by, dx, dy;
  u08 w, h;
  u08 textLen;

  // Depending on the text direction the textbox is drawn differently
  if (direction == ORI_HORIZONTAL)
  {
    // Set the draw parameters for the smooth textbox corners
    dx = 3; dy = 2;

    // Get the width+height and top-left x+y position for the textbox
    w = 7 + glcdPutStr2(x, y, FONT_5X5P, data, mcFgColor);
    h = 9;
    bx = x - 4;
    by = y - 2;
  }
  else // ORI_VERTICAL_BU
  {
    // Set the draw parameters for the smooth textbox corners
    dx = 2; dy = 3;

    // Get the width+height and top-left x+y position for the textbox
    textLen = glcdPutStr3v(x, y, FONT_5X5P, direction, data, 1, 1,
      mcFgColor);
    w = 9;
    h = 7 + textLen;
    bx = x - 2;
    by = y - 2 - textLen;
  }

  // Draw the textbox for the label
  glcdRectangle(bx, by, w, h, mcFgColor);
  glcdFillRectangle2(bx, by, dx, dy, ALIGN_AUTO, FILL_INVERSE, mcFgColor);
  glcdFillRectangle2(bx + w - dx, by, dx, dy, ALIGN_AUTO, FILL_INVERSE,
    mcFgColor);
  glcdFillRectangle2(bx, by + h - dy, dx, dy, ALIGN_AUTO, FILL_INVERSE,
    mcFgColor);
  glcdFillRectangle2(bx + w - dx, by + h - dy, dx, dy, ALIGN_AUTO,
    FILL_INVERSE, mcFgColor);
}
