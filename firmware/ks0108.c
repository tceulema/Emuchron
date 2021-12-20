//*****************************************************************************
// Filename : 'ks0108.c'
// Title    : Low-level graphics lcd api for hd61202/ks0108 displays
//*****************************************************************************

#ifndef EMULIN
#include <avr/interrupt.h>
#endif
#include "global.h"
#include "ks0108conf.h"
#ifdef EMULIN
#include "emulator/mchronutil.h"
#include "emulator/controller.h"
#endif
#include "ks0108.h"

// This module 'talks' to the lcd controllers that drive the lcd display.
// The Monochron 128x64 lcd uses two controllers; one for the left side and one
// for the right side. Each controller takes care of 64x64 pixels.
// Only one of them may be selected as active controller though. This module
// administers which controller is selected and switches between controllers
// only when needed. It further takes care of an administration of the
// functional lcd cursor and the hardware y cursor in each controller.
// The controller and cursor administration prevents unneccessary interaction
// with the controllers, thus improving the graphics performance of the glcd
// layer.

// Definition of a structure that holds the active controller, functional lcd
// cursor and the active y line cursor in each controller (y=0..7)
typedef struct _glcdLcdCursor_t
{
  u08 controller;
  u08 lcdXAddr;
  u08 lcdYAddr;
  u08 ctrlYAddr[GLCD_NUM_CONTROLLERS];
} glcdLcdCursor_t;

// The lcd controller and cursor administration
static glcdLcdCursor_t glcdLcdCursor;

// Local function prototypes
static void glcdBusyWait(void);
static void glcdControlSelect(u08 controller);
static void glcdNextAddress(void);
static void glcdSetXAddress(void);
static void glcdSetYAddress(u08 yAddr);

//
// Function: glcdBusyWait
//
// Wait until the active lcd controller is no longer busy
//
static void glcdBusyWait(void)
{
  // Do a read from control register
  cli();
  GLCD_DATAH_PORT |= 0xf0;
  GLCD_DATAL_PORT |= 0x0f;

  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  GLCD_DATAH_DDR &= ~(0xf0);
  GLCD_DATAL_DDR &= ~(0x0f);
  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
#ifdef EMULIN
  ctrlBusyState();
#endif
  while (((GLCD_DATAH_PIN & 0xf0) | (GLCD_DATAL_PIN & 0x0f)) & GLCD_STATUS_BUSY)
  {
    cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
    asm volatile ("nop"); asm volatile ("nop");
    asm volatile ("nop"); asm volatile ("nop");
    sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
    asm volatile ("nop"); asm volatile ("nop");
    asm volatile ("nop"); asm volatile ("nop");
  }

  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;
  sei();
}

//
// Function: glcdControlSelect
//
// Select lcd controller 0 or 1
//
static void glcdControlSelect(u08 controller)
{
#ifdef EMULIN
  // Check if controller is out of bounds (should never happen)
  if (controller >= GLCD_NUM_CONTROLLERS)
    emuCoreDump(CD_GLCD, __func__, controller, 0, 0, 0);
#endif

  // Unselect other controller and select requested controller
  if (controller == 0)
  {
    cbi(GLCD_CTRL_CS1_PORT, GLCD_CTRL_CS1);
    sbi(GLCD_CTRL_CS0_PORT, GLCD_CTRL_CS0);
  }
  else
  {
    cbi(GLCD_CTRL_CS0_PORT, GLCD_CTRL_CS0);
    sbi(GLCD_CTRL_CS1_PORT, GLCD_CTRL_CS1);
  }
#ifdef EMULIN
  ctrlControlSet();
#endif
}

//
// Function: glcdControlWrite
//
// Send command to the lcd controller
//
void glcdControlWrite(u08 controller, u08 data)
{
#ifdef EMULIN
  // Check if controller is out of bounds (should never happen)
  if (controller >= GLCD_NUM_CONTROLLERS)
    emuCoreDump(CD_GLCD, __func__, controller, 0, 0, data);
#endif

  cli();
  // Temporarily switch to requested controller when needed
  if (controller != glcdLcdCursor.controller)
    glcdControlSelect(controller);

  glcdBusyWait();
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;

  GLCD_DATAH_PORT &= ~0xf0;
  GLCD_DATAH_PORT |= data & 0xf0;
  GLCD_DATAL_PORT &= ~0x0f;
  GLCD_DATAL_PORT |= data & 0x0f;
#ifdef EMULIN
  ctrlExecute(CTRL_METHOD_CTRL_W);
#endif
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);

  // Switch back to administered controller when needed
  if (controller != glcdLcdCursor.controller)
    glcdControlSelect(glcdLcdCursor.controller);
  sei();
}

//
// Function: glcdDataRead
//
// Read an 8-pixel byte from the lcd using the controller cursor
//
u08 glcdDataRead(void)
{
#ifdef EMULIN
  // Check if administrative cursor is out of bounds (should never happen)
  if (glcdLcdCursor.lcdXAddr >= GLCD_XPIXELS ||
      glcdLcdCursor.lcdYAddr >= GLCD_CONTROLLER_YPAGES)
    emuCoreDump(CD_GLCD, __func__, glcdLcdCursor.controller,
      glcdLcdCursor.lcdXAddr, glcdLcdCursor.lcdYAddr, 0);
#endif

  register u08 data;

  cli();
  glcdBusyWait();
  sbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  GLCD_DATAH_DDR &= ~(0xf0);
  GLCD_DATAL_DDR &= ~(0x0f);

  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
#ifdef EMULIN
  ctrlExecute(CTRL_METHOD_READ);
#endif
  data = (GLCD_DATAH_PIN & 0xf0) | (GLCD_DATAL_PIN & 0x0f);

  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sei();

  return data;
}

//
// Function: glcdDataWrite
//
// Write an 8-pixel byte to the lcd using the controller cursor
//
void glcdDataWrite(u08 data)
{
#ifdef EMULIN
  // Check if administrative cursor is out of bounds (should never happen)
  if (glcdLcdCursor.lcdXAddr >= GLCD_XPIXELS ||
      glcdLcdCursor.lcdYAddr >= GLCD_CONTROLLER_YPAGES)
    emuCoreDump(CD_GLCD, __func__, glcdLcdCursor.controller,
      glcdLcdCursor.lcdXAddr, glcdLcdCursor.lcdYAddr, data);
#endif

  cli();
  glcdBusyWait();
  sbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;

  GLCD_DATAH_PORT &= ~0xf0;
  GLCD_DATAH_PORT |= data & 0xf0;
  GLCD_DATAL_PORT &= ~0x0f;
  GLCD_DATAL_PORT |= data & 0x0f;
#ifdef EMULIN
  ctrlExecute(CTRL_METHOD_WRITE);
#endif

  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  sei();

  // Increment our local address counter
  glcdNextAddress();
}

//
// Function: glcdInit
//
// Initialize the lcd hardware and setup controller/cursor administration
//
void glcdInit(void)
{
  u08 i;

  // Initialize lcd control lines levels
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_CS0_PORT, GLCD_CTRL_CS0);
  cbi(GLCD_CTRL_CS1_PORT, GLCD_CTRL_CS1);

  // Initialize lcd control port to output
  sbi(GLCD_CTRL_RS_DDR, GLCD_CTRL_RS);
  sbi(GLCD_CTRL_RW_DDR, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_DDR, GLCD_CTRL_E);
  sbi(GLCD_CTRL_CS0_DDR, GLCD_CTRL_CS0);
  sbi(GLCD_CTRL_CS1_DDR, GLCD_CTRL_CS1);

  // Initialize lcd data
  GLCD_DATAH_PORT &= ~(0xf0);
  GLCD_DATAL_PORT &= ~(0x0f);

  // Initialize lcd data port to output
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;

  // Hardware is now properly setup so now we can initialize the software
  // administration of the active controller and the lcd cursor

  // Select controller 0 as active controller
  glcdControlSelect(0);
  glcdLcdCursor.controller = 0;

  // Init admin of controller y page so it will sync at first cursor request
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdLcdCursor.ctrlYAddr[i] = MAX_U08;
}

//
// Function: glcdNextAddress
//
// Increment lcd cursor position
//
static void glcdNextAddress(void)
{
  // Moving to the next logical x address is more complicated than it seems
  // since we need to map a logical address into a controller address with a
  // potential controller address overflow situation. Also, a read/write action
  // performed on a controller almost always results in an automatic increment
  // of the cursor in the controller.
  // The following situations apply:
  // - Go to x+1 in current controller on current y line.
  //   This is done automatically in the controller after the 2nd sequential
  //   read or after every write operation.
  // - At end of controller, move to x=0 in next controller on current y line.
  //   To do: Set x and y cursor in next controller.
  // - At end of display line, the cheapest thing to do is to follow what
  //   happens in the last controller (controller 1): reset x to 0 in that
  //   controller.
  if (glcdLcdCursor.lcdXAddr >= GLCD_XPIXELS - 1)
  {
    // We're at the end of an lcd line; put x cursor on the start of
    // the last controller (controller 1) for the current y-line
    glcdLcdCursor.lcdXAddr =
      (GLCD_NUM_CONTROLLERS - 1) * GLCD_CONTROLLER_XPIXELS;
  }
  else
  {
    // Next x
    glcdLcdCursor.lcdXAddr++;
    if ((glcdLcdCursor.lcdXAddr & GLCD_CONTROLLER_XPIXMASK) == 0)
    {
      // Move to the next controller and init its cursor
      glcdSetXAddress();
      glcdSetYAddress(glcdLcdCursor.lcdYAddr);
    }
  }
}

//
// Function: glcdSetAddress
//
// Set the lcd cursor position in one of the lcd controllers
//
void glcdSetAddress(u08 xAddr, u08 yAddr)
{
#ifdef EMULIN
  // Check if requested cursor is out of bounds (should never happen)
  if (xAddr >= GLCD_XPIXELS || yAddr >= GLCD_CONTROLLER_YPAGES)
    emuCoreDump(CD_GLCD, __func__, 0, xAddr, yAddr, 0);
#endif
  // Set cursor x and y address.
  // The set address functions are setup such that we must set the x position
  // first to get the destination controller and only then set the y position.
  glcdLcdCursor.lcdXAddr = xAddr;
  glcdSetXAddress();
  glcdSetYAddress(yAddr);
}

//
// Function: glcdSetXAddress
//
// Set lcd cursor x position
//
static void glcdSetXAddress(void)
{
  u08 ctrlNew = glcdLcdCursor.lcdXAddr >> GLCD_CONTROLLER_XPIXBITS;

  // Change active controller when necessary
  if (glcdLcdCursor.controller != ctrlNew)
  {
    glcdControlSelect(ctrlNew);
    glcdLcdCursor.controller = ctrlNew;
  }

  // Set x address (confusingly named GLCD_SET_Y_ADDR) on destination
  // controller
  glcdControlWrite(glcdLcdCursor.controller,
    GLCD_SET_Y_ADDR | (glcdLcdCursor.lcdXAddr & GLCD_CONTROLLER_XPIXMASK));
}

//
// Function: glcdSetYAddress
//
// Set lcd cursor y position
//
static void glcdSetYAddress(u08 yAddr)
{
  // Update administrative cursor
  glcdLcdCursor.lcdYAddr = yAddr;

  // Set y address (confusingly named GLCD_SET_PAGE) on destination controller
  // only when changed
  if (yAddr != glcdLcdCursor.ctrlYAddr[glcdLcdCursor.controller])
  {
    glcdLcdCursor.ctrlYAddr[glcdLcdCursor.controller] = yAddr;
    glcdControlWrite(glcdLcdCursor.controller, GLCD_SET_PAGE | yAddr);
  }
}
