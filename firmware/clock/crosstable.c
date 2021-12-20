//*****************************************************************************
// Filename : 'crosstable.c'
// Title    : Animation code for MONOCHRON cross table clock
//*****************************************************************************

#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "spotfire.h"
#include "crosstable.h"

// Monochron environment variables
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockInit;

// Common text labels
extern char animHour[];
extern char animMin[];
extern char animSec[];

// Local function prototypes
static void spotCrossTextBox(u08 x, u08 y, u08 direction, char *data);
static void spotCrossValDraw(u08 x, u08 oldVal, u08 newVal);

//
// Function: spotCrossTableCycle
//
// Update the Spotfire cross table and filter panel
//
void spotCrossTableCycle(void)
{
  // Update common Spotfire clock elements and check if clock requires update
  if (spotCommonUpdate() == MC_FALSE)
    return;

  DEBUGP("Update CrossTable");

  // Verify changes in time
  spotCrossValDraw(71, mcClockOldTS, mcClockNewTS);
  spotCrossValDraw(49, mcClockOldTM, mcClockNewTM);
  spotCrossValDraw(27, mcClockOldTH, mcClockNewTH);
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
  spotCrossTextBox(40, 20, ORI_HORIZONTAL, "columns");
  spotCrossTextBox(11, 46, ORI_VERTICAL_BU, "none");
  spotCrossTextBox(73, 54, ORI_HORIZONTAL, animSec);
  spotCrossTextBox(51, 54, ORI_HORIZONTAL, animMin);
  spotCrossTextBox(26, 54, ORI_HORIZONTAL, animHour);
  // 2 - Crosstable x-axis column names
  glcdPutStr2(71, 31, FONT_5X5P, animSec);
  glcdPutStr2(48, 31, FONT_5X5P, animMin);
  glcdPutStr2(25, 31, FONT_5X5P, animHour);
  // 3 - The crosstable layout itself
  glcdRectangle(21, 29, 67, 21);
  glcdFillRectangle(22, 37, 65, 1);
  glcdFillRectangle(43, 30, 1, 19);
  glcdFillRectangle(65, 30, 1, 19);
}

//
// Function: spotCrossTextBox
//
// Draw a fancy textbox for a crosstable label
//
static void spotCrossTextBox(u08 x, u08 y, u08 direction, char *data)
{
  u08 bx, by, dx, dy;
  u08 w, h, a, b;
  u08 textLen;

  // Depending on the text direction the textbox is drawn differently
  if (direction == ORI_HORIZONTAL)
  {
    // Set the draw parameters for the smooth textbox corners
    dx = 3; dy = 2;

    // Get the width+height and top-left x+y position for the textbox
    w = 7 + glcdPutStr2(x, y, FONT_5X5P, data);
    h = 9;
    bx = x - 4;
    by = y - 2;
  }
  else // ORI_VERTICAL_BU
  {
    // Set the draw parameters for the smooth textbox corners
    dx = 2; dy = 3;

    // Get the width+height and top-left x+y position for the textbox
    textLen = glcdPutStr3v(x, y, FONT_5X5P, direction, data, 1, 1);
    w = 9;
    h = 7 + textLen;
    bx = x - 2;
    by = y - 2 - textLen;
  }

  // Draw the textbox for the label
  a = bx + w - dx;
  b = by + h - dy;
  glcdRectangle(bx, by, w, h);
  glcdFillRectangle2(bx, by, dx, dy, ALIGN_AUTO, FILL_INVERSE);
  glcdFillRectangle2(a, by, dx, dy, ALIGN_AUTO, FILL_INVERSE);
  glcdFillRectangle2(bx, b, dx, dy, ALIGN_AUTO, FILL_INVERSE);
  glcdFillRectangle2(a, b, dx, dy, ALIGN_AUTO, FILL_INVERSE);
}

//
// Function: spotCrossValDraw
//
// Draw a crosstable value
//
static void spotCrossValDraw(u08 x, u08 oldVal, u08 newVal)
{
  char strVal[3];

  // See if we need to update the time element
  if (oldVal == newVal && mcClockInit == MC_FALSE)
    return;

  animValToStr(newVal, strVal);
  glcdPutStr2(x, 40, FONT_5X7M, strVal);
}
