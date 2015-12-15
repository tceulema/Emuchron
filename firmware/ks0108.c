//*****************************************************************************
// Filename : 'ks0108.c'
// Title    : Graphic LCD driver for HD61202/KS0108 displays
//*****************************************************************************

#ifndef EMULIN
// AVR specific includes
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#include "emulator/mchronutil.h"
#include "emulator/lcd.h"
#endif

#include "global.h"
#include "ks0108.h"

// Global variables
GrLcdStateType GrLcdState;

// Local function prototypes
static void glcdBusyWait(u08 controller);
//static u08 glcdControlRead(u08 controller);
static void glcdControlSelect(u08 controller);
static void glcdControlWrite(u08 controller, u08 data);
static void glcdHome(void);
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
  u08 i;

  // Initialize hardware
  glcdInitHW();

  // Bring lcd out of reset
  glcdReset(FALSE);

  // Turn on lcd
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdControlWrite(i, GLCD_ON_CTRL | GLCD_ON_DISPLAY);

  // Clear lcd
  glcdClearScreen(color);

  // Initialize lcd cursor position
  glcdHome();
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

  // Initialize LCD control lines levels
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
  // Initialize LCD control port to output
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
  // Initialize LCD data
  GLCD_DATAH_PORT &= ~(0xF0);
  GLCD_DATAL_PORT &= ~(0x0F);
  //outb(GLCD_DATA_PORT, 0x00);
  // Initialize LCD data port to output
  GLCD_DATAH_DDR |= 0xF0;
  GLCD_DATAL_DDR |= 0x0F;
  //outb(GLCD_DATA_DDR, 0xFF);
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
  //outb(GLCD_DATA_PORT, 0xFF);
  GLCD_DATAH_PORT |= 0xF0;
  GLCD_DATAL_PORT |= 0x0F;

  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  //outb(GLCD_DATA_DDR, 0x00);
  GLCD_DATAH_DDR &= ~(0xF0);
  GLCD_DATAL_DDR &= ~(0x0F);
  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  //while (inb(GLCD_DATA_PIN) & GLCD_STATUS_BUSY)
  while (((GLCD_DATAH_PIN & 0xF0) | (GLCD_DATAL_PIN & 0x0F)) & GLCD_STATUS_BUSY)
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
  //outb(GLCD_DATA_DDR, 0xFF);
  GLCD_DATAH_DDR |= 0xF0;
  GLCD_DATAL_DDR |= 0x0F;
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
// Write to the lcd controller
//
static void glcdControlWrite(u08 controller, u08 data)
{
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  //outb(GLCD_DATA_DDR, 0xFF);
  GLCD_DATAH_DDR |= 0xF0;
  GLCD_DATAL_DDR |= 0x0F;
  //outb(GLCD_DATA_PORT, data);
  // Clear and set top nibble
  GLCD_DATAH_PORT &= ~0xF0;
  GLCD_DATAH_PORT |= data & 0xF0;
  // Clear and set bottom nibble
  GLCD_DATAL_PORT &= ~0x0F;
  GLCD_DATAL_PORT |= data & 0x0F;
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
/*static u08 glcdControlRead(u08 controller)
{
  register u08 data;
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  cbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  //outb(GLCD_DATA_DDR, 0x00);
  GLCD_DATAH_DDR &= ~(0xF0);
  GLCD_DATAL_DDR &= ~(0x0F);
  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  //data = inb(GLCD_DATA_PIN);
  data = (GLCD_DATAH_PIN & 0xF0) | (GLCD_DATAL_PIN & 0x0F);
  cbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  //outb(GLCD_DATA_DDR, 0xFF);
  GLCD_DATAH_DDR |= 0xF0;
  GLCD_DATAL_DDR |= 0x0F;
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
#ifdef EMULIN
  // Write to lcd emulator device
  lcdWriteStub(data);
#else
#ifdef GLCD_PORT_INTERFACE
  register u08 controller = (GrLcdState.lcdXAddr/GLCD_CONTROLLER_XPIXELS);

  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  sbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  cbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  //outb(GLCD_DATA_DDR, 0xFF);
  GLCD_DATAH_DDR |= 0xF0;
  GLCD_DATAL_DDR |= 0x0F;

  //outb(GLCD_DATA_PORT, data);
  // Clear and set top nibble
  GLCD_DATAH_PORT &= ~0xF0;
  GLCD_DATAH_PORT |= data & 0xF0;
  // Clear and set bottom nibble
  GLCD_DATAL_PORT &= ~0x0F;
  GLCD_DATAL_PORT |= data & 0x0F;

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
  register u08 controller = (GrLcdState.lcdXAddr / GLCD_CONTROLLER_XPIXELS);
#ifdef EMULIN
  // Read from LCD emulator device
  data = lcdReadStub(controller);
#else
#ifdef GLCD_PORT_INTERFACE
  cli();
  // Wait until lcd not busy
  glcdBusyWait(controller);
  sbi(GLCD_CTRL_RS_PORT, GLCD_CTRL_RS);
  //outb(GLCD_DATA_DDR, 0x00);
  GLCD_DATAH_DDR &= ~(0xF0);
  GLCD_DATAL_DDR &= ~(0x0F);

  sbi(GLCD_CTRL_RW_PORT, GLCD_CTRL_RW);
  sbi(GLCD_CTRL_E_PORT, GLCD_CTRL_E);
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  asm volatile ("nop"); asm volatile ("nop");
  //data = inb(GLCD_DATA_PIN);
  data = (GLCD_DATAH_PIN & 0xF0) | (GLCD_DATAL_PIN & 0x0F);

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
}

//
// Function: glcdHome
//
// Set the lcd cursor to top left position
//
static void glcdHome(void)
{
  u08 i;

  // Initialize addresses/positions
  glcdStartLine(0);
  glcdSetAddress(0,0);

  // Initialize local data structures
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    GrLcdState.ctrlr[i].xAddr = 0;
    GrLcdState.ctrlr[i].yAddr = 0;
  }
}

//
// Function: glcdClearScreen
//
// Clear the lcd contents using the specified color
//
void glcdClearScreen(u08 color)
{
  u08 pageAddr;
  u08 xAddr;
  u08 data;

  if (color == ON)
    data = 0xff;
  else
    data = 0x00;

  // Clear lcd.
  // Loop through all pages.
  for (pageAddr = 0; pageAddr < (GLCD_YPIXELS >> 3); pageAddr++)
  {
    // Set page address
    glcdSetAddress(0, pageAddr);

    // Clear all lines of this page of display memory
    for (xAddr = 0; xAddr < GLCD_XPIXELS; xAddr++)
      glcdDataWrite(data);
  }
}

//
// Function: glcdStartLine
//
// Set the lcd controller cursor to the specified line
//
void glcdStartLine(u08 start)
{
  glcdControlWrite(0, GLCD_START_LINE | start);
  glcdControlWrite(1, GLCD_START_LINE | start);
}

//
// Function: glcdSetAddress
//
// Set the lcd cursor position
//
void glcdSetAddress(u08 x, u08 yLine)
{
  // Set addresses
#ifdef EMULIN
  extern long long lcdLcdSetAddress;

  lcdLcdSetAddress++;
  if (x >= GLCD_XPIXELS || yLine >= GLCD_YPIXELS / 8)
  {
    // We should never get here
    emuCoreDump(__func__, 0, x, yLine, 0);
  }
#endif
  glcdSetYAddress(yLine);
  glcdSetXAddress(x);
}

//
// Function: glcdSetXAddress
//
// Set lcd cursor x position
//
static void glcdSetXAddress(u08 xAddr)
{
  u08 i;

  // Record address change locally
  GrLcdState.lcdXAddr = xAddr;

  // Clear y (col) address on all controllers
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    glcdControlWrite(i, GLCD_SET_Y_ADDR | 0x00);
    GrLcdState.ctrlr[i].xAddr = 0;
  }

  // Set y (col) address on destination controller
  glcdControlWrite((GrLcdState.lcdXAddr / GLCD_CONTROLLER_XPIXELS),
    GLCD_SET_Y_ADDR | (GrLcdState.lcdXAddr & 0x3F));
}

//
// Function: glcdSetYAddress
//
// Set lcd cursor y position
//
static void glcdSetYAddress(u08 yAddr)
{
  u08 i;

  // Record address change locally
  GrLcdState.lcdYAddr = yAddr;

  // Set page address for all controllers
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    glcdControlWrite(i, GLCD_SET_PAGE | yAddr);
}

//
// Function: glcdNextAddress
//
// Increment lcd cursor position
//
void glcdNextAddress(void)
{
  register u08 controller = (GrLcdState.lcdXAddr/GLCD_CONTROLLER_XPIXELS);

  // Increment our local address counter
  GrLcdState.ctrlr[controller].xAddr++;
  GrLcdState.lcdXAddr++;
  if (GrLcdState.lcdXAddr >= GLCD_XPIXELS)
  {
    GrLcdState.lcdYAddr++;
    glcdSetYAddress(GrLcdState.lcdYAddr);
    glcdSetXAddress(0);
  }
}
