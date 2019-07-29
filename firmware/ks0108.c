//*****************************************************************************
// Filename : 'ks0108.c'
// Title    : Graphic lcd driver for HD61202/KS0108 displays
//*****************************************************************************

#ifndef EMULIN
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#else
#include "emulator/stubrefs.h"
#include "emulator/mchronutil.h"
#include "emulator/controller.h"
#endif
#include "ks0108.h"

// Definition of a structure that holds the functional lcd cursor and the
// active y line cursor in both controllers (x=0..127, y=0..7)
typedef struct _glcdLcdCursor_t
{
  unsigned char lcdXAddr;
  unsigned char lcdYAddr;
  unsigned char ctrlYAddr[GLCD_NUM_CONTROLLERS];
} glcdLcdCursor_t;

// The functional lcd cursor
static glcdLcdCursor_t glcdLcdCursor;

// Local function prototypes
static void glcdBusyWait(u08 controller);
static void glcdControlSelect(u08 controller);
static void glcdInitHW(void);
static void glcdReset(u08 resetState);
static void glcdSetXAddress(u08 xAddr);
static void glcdSetYAddress(u08 yAddr);

//
// Function: glcdInit
//
// Initialize the lcd
//
void glcdInit(u08 color)
{
  // Initialize hardware and bring lcd out of reset
  glcdInitHW();
  glcdReset(GLCD_FALSE);

  // Initialize each controller in the lcd by clearing the screen
  glcdClearScreen(color);
}

//
// Function: glcdInitHW
//
// Initialize the lcd hardware
//
static void glcdInitHW(void)
{
  // Initialize I/O ports
  // If I/O interface is in use
#ifdef GLCD_PORT_INTERFACE

  //TODO: make setup of chip select lines contingent on how
  // many controllers are actually in the display

  // Initialize lcd control lines levels
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_CS0_PORT, GLCD_CTRL_CS0);
#ifdef GLCD_CTRL_CS1
  cbi(GLCD_CTRL_CS1_PORT, GLCD_CTRL_CS1);
#endif
#ifdef GLCD_CTRL_CS2
  cbi(GLCD_CTRL_CS2_PORT, GLCD_CTRL_CS2);
#endif
#ifdef GLCD_CTRL_CS3
  cbi(GLCD_CTRL_CS3_PORT, GLCD_CTRL_CS3);
#endif
#ifdef GLCD_CTRL_RESET
  cbi(GLCD_CTRL_RESET_PORT, GLCD_CTRL_RESET);
#endif
  // Initialize lcd control port to output
  sbi(GLCD_CTRL_RS_DDR, GLCD_CTRL_RS);
  sbi(GLCD_CTRL_RW_DDR, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_DDR, GLCD_CTRL_E);
  sbi(GLCD_CTRL_CS0_DDR, GLCD_CTRL_CS0);
#ifdef GLCD_CTRL_CS1
  sbi(GLCD_CTRL_CS1_DDR, GLCD_CTRL_CS1);
#endif
#ifdef GLCD_CTRL_CS2
  sbi(GLCD_CTRL_CS2_DDR, GLCD_CTRL_CS2);
#endif
#ifdef GLCD_CTRL_CS3
  sbi(GLCD_CTRL_CS3_DDR, GLCD_CTRL_CS3);
#endif
#ifdef GLCD_CTRL_RESET
  sbi(GLCD_CTRL_RESET_DDR, GLCD_CTRL_RESET);
#endif
  // Initialize lcd data
  GLCD_DATAH_PORT &= ~(0xf0);
  GLCD_DATAL_PORT &= ~(0x0f);
  //outb(GLCD_DATA_PORT, 0x00);
  // Initialize lcd data port to output
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;
  //outb(GLCD_DATA_DDR, 0xff);
#endif
}

//
// Function: glcdBusyWait
//
// Wait until the lcd is no longer busy
//
static void glcdBusyWait(u08 controller)
{
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd busy bit goes to zero
  // Select the controller chip
  glcdControlSelect(controller);
  // Do a read from control register
  //outb(GLCD_DATA_PORT, 0xff);
  GLCD_DATAH_PORT |= 0xf0;
  GLCD_DATAL_PORT |= 0x0f;

  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  //outb(GLCD_DATA_DDR, 0x00);
  GLCD_DATAH_DDR &= ~(0xf0);
  GLCD_DATAL_DDR &= ~(0x0f);
  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  //while (inb(GLCD_DATA_PIN) & GLCD_STATUS_BUSY)
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
  //outb(GLCD_DATA_DDR, 0xff);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;
  sei();
#else
  // Enable RAM waitstate
  //sbi(MCUCR, SRW);
  // Wait until lcd busy bit goes to zero
  while (*(volatile unsigned char *)
    (GLCD_CONTROLLER0_CTRL_ADDR + GLCD_CONTROLLER_ADDR_OFFSET * controller) &
      GLCD_STATUS_BUSY);
  // Disable RAM waitstate
  //cbi(MCUCR, SRW);
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

  // Execute the action in the controller
  ctrlExecute(CTRL_METHOD_COMMAND, controller, data);
#endif
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  //outb(GLCD_DATA_DDR, 0xff);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;
  //outb(GLCD_DATA_PORT, data);
  // Clear and set top nibble
  GLCD_DATAH_PORT &= ~0xf0;
  GLCD_DATAH_PORT |= data & 0xf0;
  // Clear and set bottom nibble
  GLCD_DATAL_PORT &= ~0x0f;
  GLCD_DATAL_PORT |= data & 0x0f;
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  sei();
#else
  // Enable RAM waitstate
  //sbi(MCUCR, SRW);
  // Wait until lcd not busy
  glcdBusyWait(controller);
  *(volatile unsigned char *) (GLCD_CONTROLLER0_CTRL_ADDR +
    GLCD_CONTROLLER_ADDR_OFFSET * controller) = data;
  // Disable RAM waitstate
  //cbi(MCUCR, SRW);
#endif
}

//
// Function: glcdControlRead
//
// Read from the lcd controller
//
/*u08 glcdControlRead(u08 controller)
{
  register u08 data;
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  //outb(GLCD_DATA_DDR, 0x00);
  GLCD_DATAH_DDR &= ~(0xf0);
  GLCD_DATAL_DDR &= ~(0x0f);
  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  //data = inb(GLCD_DATA_PIN);
  data = (GLCD_DATAH_PIN & 0xf0) | (GLCD_DATAL_PIN & 0x0f);
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  //outb(GLCD_DATA_DDR, 0xff);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;
  sei();
#else
  // Enable RAM waitstate
  //sbi(MCUCR, SRW);
  // Wait until lcd not busy
  glcdBusyWait(controller);
  data = *(volatile unsigned char *) (GLCD_CONTROLLER0_CTRL_ADDR +
    GLCD_CONTROLLER_ADDR_OFFSET * controller);
  // Disable RAM waitstate
  //cbi(MCUCR, SRW);
#endif
  return data;
}*/

//
// Function: glcdControlSelect
//
// Select lcd controller
//
static void glcdControlSelect(u08 controller)
{
#ifdef GLCD_PORT_INTERFACE
  //TODO: make control of chip select lines contingent on how
  // many controllers are actually in the display

  // Unselect all controllers
  cbi(GLCD_CTRL_CS0_PORT, GLCD_CTRL_CS0);
#ifdef GLCD_CTRL_CS1
  cbi(GLCD_CTRL_CS1_PORT, GLCD_CTRL_CS1);
#endif
#ifdef GLCD_CTRL_CS2
  cbi(GLCD_CTRL_CS2_PORT, GLCD_CTRL_CS2);
#endif
#ifdef GLCD_CTRL_CS3
  cbi(GLCD_CTRL_CS3_PORT, GLCD_CTRL_CS3);
#endif

  // Select requested controller
  switch (controller)
  {
  case 0:
    sbi(GLCD_CTRL_CS0_PORT, GLCD_CTRL_CS0);
    break;
#ifdef GLCD_CTRL_CS1
  case 1:
    sbi(GLCD_CTRL_CS1_PORT, GLCD_CTRL_CS1);
    break;
#endif
#ifdef GLCD_CTRL_CS2
  case 2:
    sbi(GLCD_CTRL_CS2_PORT, GLCD_CTRL_CS2);
    break;
#endif
#ifdef GLCD_CTRL_CS3
  case 3:
    sbi(GLCD_CTRL_CS3_PORT, GLCD_CTRL_CS3);
    break;
#endif
  default:
    break;
  }
#endif
}

//
// Function: glcdDataWrite
//
// Write an 8-pixel byte to the lcd
//
void glcdDataWrite(u08 data)
{
  register u08 controller =
    (glcdLcdCursor.lcdXAddr >> GLCD_CONTROLLER_XPIXBITS);
#ifdef EMULIN
  // Check if administrative cursor is out of bounds (should never happen)
  if (controller >= GLCD_NUM_CONTROLLERS ||
      glcdLcdCursor.lcdXAddr >= GLCD_XPIXELS ||
      glcdLcdCursor.lcdYAddr >= GLCD_CONTROLLER_YPAGES)
    emuCoreDump(CD_GLCD, __func__, controller, glcdLcdCursor.lcdXAddr,
      glcdLcdCursor.lcdYAddr, data);

  // Write data to controller lcd buffer
  ctrlExecute(CTRL_METHOD_WRITE, controller, data);
#else
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  sbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  //outb(GLCD_DATA_DDR, 0xff);
  GLCD_DATAH_DDR |= 0xf0;
  GLCD_DATAL_DDR |= 0x0f;

  //outb(GLCD_DATA_PORT, data);
  // Clear and set top nibble
  GLCD_DATAH_PORT &= ~0xf0;
  GLCD_DATAH_PORT |= data & 0xf0;
  // Clear and set bottom nibble
  GLCD_DATAL_PORT &= ~0x0f;
  GLCD_DATAL_PORT |= data & 0x0f;

  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  sei();
#else
  // Enable RAM waitstate
  //sbi(MCUCR, SRW);
  // Wait until lcd not busy
  glcdBusyWait(controller);
  *(volatile unsigned char *) (GLCD_CONTROLLER0_CTRL_ADDR +
    GLCD_CONTROLLER_ADDR_OFFSET * controller) = data;
  // Disable RAM waitstate
  //cbi(MCUCR, SRW);
#endif
#endif

  // Increment our local address counter
  glcdNextAddress();
}

//
// Function: glcdDataRead
//
// Read an 8-pixel byte from the lcd
//
u08 glcdDataRead(void)
{
  register u08 data;
  register u08 controller =
    (glcdLcdCursor.lcdXAddr >> GLCD_CONTROLLER_XPIXBITS);
#ifdef EMULIN
  // Check if administrative cursor is out of bounds (should never happen)
  if (controller >= GLCD_NUM_CONTROLLERS ||
      glcdLcdCursor.lcdXAddr >= GLCD_XPIXELS ||
      glcdLcdCursor.lcdYAddr >= GLCD_CONTROLLER_YPAGES)
    emuCoreDump(CD_GLCD, __func__, controller, glcdLcdCursor.lcdXAddr,
      glcdLcdCursor.lcdYAddr, 0);

  // Read data from controller lcd buffer
  data = ctrlExecute(CTRL_METHOD_READ, controller, 0);
#else
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  sbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  //outb(GLCD_DATA_DDR, 0x00);
  GLCD_DATAH_DDR &= ~(0xf0);
  GLCD_DATAL_DDR &= ~(0x0f);

  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  //data = inb(GLCD_DATA_PIN);
  data = (GLCD_DATAH_PIN & 0xf0) | (GLCD_DATAL_PIN & 0x0f);

  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sei();
#else
  // Enable RAM waitstate
  //sbi(MCUCR, SRW);
  // Wait until lcd not busy
  glcdBusyWait(controller);
  data = *(volatile unsigned char *) (GLCD_CONTROLLER0_CTRL_ADDR +
    GLCD_CONTROLLER_ADDR_OFFSET * controller);
  // Disable RAM waitstate
  //cbi(MCUCR, SRW);
#endif
#endif
  return data;
}

//
// Function: glcdReset
//
// Reset the lcd
//
static void glcdReset(u08 resetState)
{
  u08 i;
  // Reset lcd if argument is true.
  // Run lcd if argument is false.
#ifdef GLCD_PORT_INTERFACE
#ifdef GLCD_CTRL_RESET
  if (resetState)
    cbi(GLCD_CTRL_RESET_PORT, GLCD_CTRL_RESET);
  else
    sbi(GLCD_CTRL_RESET_PORT, GLCD_CTRL_RESET);
#endif
#endif

  // Init admin of controller y page so it will sync at the first cursor request
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdLcdCursor.ctrlYAddr[i] = 255;
}

//
// Function: glcdClearScreen
//
// Clear the lcd contents using the specified color, and reset the controller
// display and startline settings that may have been modified by functional
// clock code
//
void glcdClearScreen(u08 color)
{
  u08 i;
  u08 xAddr;
  u08 data;

  if (color == GLCD_ON)
    data = 0xff;
  else
    data = 0x00;

  // Clear lcd by looping through all pages
  for (i = 0; i < GLCD_CONTROLLER_YPAGES; i++)
  {
    // Set page address
    glcdSetAddress(0, i);

    // Clear all lines of this page of display memory
    for (xAddr = 0; xAddr < GLCD_XPIXELS; xAddr++)
      glcdDataWrite(data);
  }

  // Enable all controller displays and reset startline to 0
  glcdResetScreen();
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
// Function: glcdStartLine
//
// Display the controller lcd data with a vertical pixel offset value
//
/*void glcdStartLine(u08 startLine)
{
  u08 i;

  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdControlWrite(i, GLCD_START_LINE | startLine);
}*/

//
// Function: glcdSetAddress
//
// Set the lcd cursor position in one of the lcd controllers
//
void glcdSetAddress(u08 xAddr, u08 yAddr)
{
#ifdef EMULIN
  extern long long ctrlLcdSetAddress;

  // Check if requested cursor is out of bounds (should never happen)
  ctrlLcdSetAddress++;
  if (xAddr >= GLCD_XPIXELS || (yAddr >> GLCD_CONTROLLER_YPAGEBITS) > 0)
    emuCoreDump(CD_GLCD, __func__, 0, xAddr, yAddr, 0);
#endif
  // Set cursor x and y address.
  // The set address functions are setup such that we must set the x position
  // first to get the destination controller and only then set the y position.
  glcdSetXAddress(xAddr);
  glcdSetYAddress(yAddr);
}

//
// Function: glcdSetXAddress
//
// Set lcd cursor x position
//
static void glcdSetXAddress(u08 xAddr)
{
  // Record address change locally
  glcdLcdCursor.lcdXAddr = xAddr;

  // Set x address on destination controller (confusingly named GLCD_SET_Y_ADDR)
  glcdControlWrite(glcdLcdCursor.lcdXAddr >> GLCD_CONTROLLER_XPIXBITS,
    GLCD_SET_Y_ADDR | (glcdLcdCursor.lcdXAddr & GLCD_CONTROLLER_XPIXMASK));
}

//
// Function: glcdSetYAddress
//
// Set lcd cursor y position
//
static void glcdSetYAddress(u08 yAddr)
{
  u08 controller = glcdLcdCursor.lcdXAddr >> GLCD_CONTROLLER_XPIXBITS;

  // Record address change locally
  glcdLcdCursor.lcdYAddr = yAddr & GLCD_CONTROLLER_YPAGEMASK;

  // Set page address on destination controller only when changed
  if (glcdLcdCursor.lcdYAddr != glcdLcdCursor.ctrlYAddr[controller])
  {
    glcdLcdCursor.ctrlYAddr[controller] = glcdLcdCursor.lcdYAddr;
    glcdControlWrite(controller, GLCD_SET_PAGE | glcdLcdCursor.lcdYAddr);
  }
}

//
// Function: glcdNextAddress
//
// Increment lcd cursor position
//
void glcdNextAddress(void)
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
  //   To do: Set cursor in next controller.
  // - At end of display line, move to x=0 in controller 0 on current y line.
  //   To do: Set cursor in controller 0.

  if (glcdLcdCursor.lcdXAddr >= GLCD_XPIXELS - 1)
  {
    // We're at the end of an lcd line; init cursor on controller 0 on
    // current line
    glcdSetXAddress(0);
  }
  else
  {
    // Next x
    glcdLcdCursor.lcdXAddr++;
    if ((glcdLcdCursor.lcdXAddr & GLCD_CONTROLLER_XPIXMASK) == 0)
    {
      // We move to next controller; init cursor on that controller
      glcdSetXAddress(glcdLcdCursor.lcdXAddr);
      glcdSetYAddress(glcdLcdCursor.lcdYAddr);
    }
  }
}
