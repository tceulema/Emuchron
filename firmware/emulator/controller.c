//*****************************************************************************
// Filename : 'controller.c'
// Title    : Lcd ks0108 controller stub functionality for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <string.h>

// Monochron and emuchron defines
#include "../global.h"
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "mchronutil.h"
#include "controller.h"

//
// Our Monochron 128x64 pixel lcd display image is implemented as follows:
// Two ks0108 controllers, each controlling 64x64 lcd pixels (= 512 byte).
//    Ctrl 0     Ctrl 1
//  <- 64 px -><- 64 px ->
//  ^          ^
//  |  64 px   |  64 px
//  v          v
//
// An lcd byte represents 8 px and is implemented vertically.
// So, when lcd byte bit 0 starts at px[x,y] then bit 7 ends at px[x,y+7].
//
//       Controller 0                        Controller 1
//       64 x 64 px = 512 byte               64 x 64 px = 512 byte
//
//  px     0    1    2         63             64   65   66         127
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//   0  |    |    |    |     |    |         |    |    |    |     |    |
//   1  |  b |  b |  b |     |  b |         |  b |  b |  b |     |  b |
//   2  |  y |  y |  y |     |  y |         |  y |  y |  y |     |  y |
//   3  |  t |  t |  t |     |  t |         |  t |  t |  t |     |  t |
//   4  |  e |  e |  e |     |  e |         |  e |  e |  e |     |  e |
//   5  |    |    |    |     |    |         |    |    |    |     |    |
//   6  | 0,0| 1,0| 2,0|     |63,0|         | 0,0| 1,0| 2,0|     |63,0|
//   7  |    |    |    |     |    |         |    |    |    |     |    |
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//       :
//       : repeat 6 bytes for additional 48 y px
//       :
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//  56  |    |    |    |     |    |         |    |    |    |     |    |
//  57  |  b |  b |  b |     |  b |         |  b |  b |  b |     |  b |
//  58  |  y |  y |  y |     |  y |         |  y |  y |  y |     |  y |
//  59  |  t |  t |  t |     |  t |         |  t |  t |  t |     |  t |
//  60  |  e |  e |  e |     |  e |         |  e |  e |  e |     |  e |
//  61  |    |    |    |     |    |         |    |    |    |     |    |
//  62  | 0,7| 1,7| 2,7|     |63,7|         | 0,7| 1,7| 2,7|     |63,7|
//  63  |    |    |    |     |    |         |    |    |    |     |    |
//      +----+----+----+ ... +----+         +----+----+----+ ... +----+
//
// Mapping a px(x,y) into data first requires a setoff in a controller after
// which it requires a mapping into the proper (x,y) byte followed by a mapping
// into the proper bit within that byte.
//
// The two lcd controllers are emulated using a finite state machine. For this
// we require a controller state, stubbed hardware registers, the controller
// command set and controller lcd data read/write operations. This also means
// that the controllers operate completely independent from one another.
//
// The controller control write commands are:
// - Switch controller display on/off (note: this is NOT backlight)
// - Set controller cursor x
// - Set controller cursor y
// - Set controller lcd display start line
//
// The controller lcd operations are:
// - Read byte from controller lcd using cursor
// - Write data to controller lcd using cursor
//
// This module only takes indirectly care of displaying the lcd data. The main
// function is to implement an emulated lcd controller. The result of a
// controller machine event however is forwarded via this module to each of the
// active lcd devices, each holding a private copy of the lcd image data.
//
// This module is setup such that it supports a set of two lcd controllers,
// which is an exact representation of Monochron hardware. With some coding
// efforts this module can be changed to support any combination of
// controllers, all driven by the same state-event machine.
//
// So how do we implement an lcd controller as a finite state machine?
// Any operation that interacts with the controller is mapped into a controller
// event.
// For example, a controller cursor points to a certain x/y location upon which
// a read or write operation can be performed. These operations are mapped into
// separate events, and depending on the current controller state the internal
// controller cursor may be impacted and lcd data will be copied to or from
// the internal lcd image buffer. After handling the event the controller moves
// to a new state that may be identical to the current one. Controller register
// command events will not impact the lcd image data but may impact how the lcd
// image will be displayed.
// The result of a controller event is a flag indicating whether the lcd image
// requires a redraw in an lcd stub device.
//
// The following state-event diagram is implemented where the action specifies
// a short description of the operation to perform, as well as the next
// controller state. As can be seen below, the controller is not complicated.
// When you look at the diagram you'll notice that the state/events for state
// CURSOR and WRITE are identical. This means that they could be merged into a
// single state. For reasons of clarity I prefer to keep both as the states
// identify whether the last action made on the controller is either setting
// a controller cursor or performing a write lcd action.
//
//                state
//                   CURSOR            READ           WRITE
//      event   +---------------+---------------+---------------+
//              | set cursor x  | set cursor x  | set cursor x  |
//  set cursor  |               |               |               |
//      x       | next = CURSOR | next = CURSOR | next = CURSOR |
//              +---------------+---------------+---------------+
//              | set cursor y  | set cursor y  | set cursor y  |
//  set cursor  |               |               |               |
//      y       | next = CURSOR | next = CURSOR | next = CURSOR |
//              +---------------+---------------+---------------+
//              | set display   | set display   | set display   |
//  set display |               |               |               |
//    on/off    | next = CURSOR | next = READ   | next = WRITE  |
//              +---------------+---------------+---------------+
//              | set startline | set startline | set startline |
//   set start  |               |               |               |
//     line     | next = CURSOR | next = READ   | next = WRITE  |
//              +---------------+---------------+---------------+
//              | dummy read    | read lcd      | dummy read    |
//   data read  |               | cursor++      |               |
//              | next = READ   | next = READ   | next = READ   |
//              +---------------+---------------+---------------+
//              | write lcd     | write lcd     | write lcd     |
//  data write  | cursor++      | cursor++      | cursor++      |
//              | next = WRITE  | next = WRITE  | next = WRITE  |
//              +---------------+---------------+---------------+
//
// Regarding cursor++:
// The following describes how the actual hardware behaves.
// - The y page is never auto-incremented.
// - Only the x column cursor is auto-incremented.
// - At the end of a controller line at position 63, the x cursor resets to
//   position 0. In other words: resets to the beginning of the same y page.
//
// The stubbed Atmel ports and pins interface for this module is limited to the
// following elements:
// GLCD_DATAH_PORT    - High nibble data byte port for write/control input
// GLCD_DATAL_PORT    - Low nibble data byte port for write/control input
// GLCD_DATAH_PIN     - High nibble data byte pin for read/busy output
// GLCD_DATAL_PIN     - Low nibble data byte pin for read/busy output
// GLCD_CTRL_CS0_PORT - Control port for selecting controller 0
// GLCD_CTRL_CS1_PORT - Control port for selecting controller 1
// Other elements like E/RS/RW ports are not supported.
//

// The controller commands (in addition to lcd read and write commands)
#define CTRL_CMD_DISPLAY	0	// Set display on/off
#define CTRL_CMD_COLUMN		1	// Set cursor x column
#define CTRL_CMD_PAGE		2	// Set cursor y page
#define CTRL_CMD_START_LINE	3	// Set display start line

// The controller states
#define CTRL_STATE_CURSOR	0	// Cursor has been set (initial state)
#define CTRL_STATE_READ		1	// Sequential read
#define CTRL_STATE_WRITE	2	// Sequential write
#define CTRL_STATE_MAX		3	// Used for data initialization

// The controller events
#define CTRL_EVENT_CURSOR_X	0	// Command: set cursor x
#define CTRL_EVENT_CURSOR_Y	1	// Command: set cursor y
#define CTRL_EVENT_DISPLAY	2	// Command: switch lcd on/off
#define CTRL_EVENT_STARTLINE	3	// Command: set display start line
#define CTRL_EVENT_READ		4	// Operation: lcd data read
#define CTRL_EVENT_WRITE	5	// Operation: lcd data write
#define CTRL_EVENT_MAX		6	// Used for data initialization

// Definition of a structure holding the lcd image data for a controller
typedef u08 ctrlImage_t[GLCD_CONTROLLER_XPIXELS][GLCD_CONTROLLER_YPAGES];

// Definition of a structure holding the glcd interface statistics counters
typedef struct _ctrlGlcdStats_t
{
  long long dataRead;			// Bytes read from lcd
  long long dataWrite;			// Bytes written to lcd
  long long addressSet;			// Cursor address set in lcd
  long long ctrlSet;			// Set lcd controller
} ctrlGlcdStats_t;

// Definition of a structure holding the controller statistics counters
typedef struct _ctrlStats_t
{
  long long displayReq;			// Display commands received
  long long displayCnf;			// Display cmds leading to lcd update
  long long startLineReq;		// Startline commands received
  long long startLineCnf;		// Startline cmds leading to lcd update
  long long xReq;			// Cursor x commands received
  long long xCnf;			// Cursor x cmds leading to x update
  long long yReq;			// Cursor y commands received
  long long yCnf;			// Cursor y cmds leading to y update
  long long readReq;			// Lcd read requests received
  long long readCnf;			// Lcd read reqs leading to actual read
  long long writeReq;			// Lcd write requests received
  long long writeCnf;			// Lcd write reqs leading to lcd update
} ctrlStats_t;

// Definition of a structure holding the stubbed controller hardware registers
typedef struct _ctrlRegister_t
{
  u08 display;				// Display on/off switch (0=off, 1=on)
  u08 x;				// Cursor x pos (0..63)
  u08 y;				// Cursor y page (0..7)
  u08 startLine;			// Vertical display start line (0..63)
  u08 dataRead;				// Last data read from the lcd
  u08 dataWrite;			// Last data written to the lcd
} ctrlRegister_t;

// A controller is represented in the data it holds. It is a combination of its
// current software machine state, the input/output registers, its lcd image
// data, and for software emulation purposes a set of statistics counters.
// Definition of a structure holding the data for a controller.
typedef struct _ctrlController_t
{
  u08 state;				// Controller state
  ctrlRegister_t ctrlRegister;		// Registers
  ctrlImage_t ctrlImage;		// Lcd image data
  ctrlStats_t ctrlStats;		// Aggregated statistics
  ctrlStats_t ctrlStatsCopy;		// Copy for single cycle stats
} ctrlController_t;

// Definition of a structure holding the state-event event handler and next
// state for the controller finite state machine
typedef struct _ctrlStateEvent_t
{
  u08 (*handler)(ctrlController_t *, u08); // The event handler
  u08 stateNext;			// The next machine state
} ctrlStateEvent_t;

// Definition of a structure holding the controller state-event diagram
typedef ctrlStateEvent_t ctrlSEDiagram_t[CTRL_EVENT_MAX][CTRL_STATE_MAX];

// The glut glcd pixel double-click event data
extern lcdGlutGlcdPix_t lcdGlutGlcdPix;

// Local finite state machine state-event function prototypes
static u08 ctrlCursorX(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlCursorY(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlDisplay(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlRead(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlReadDummy(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlStartline(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlWrite(ctrlController_t *ctrlController, u08 payload);

// Define the controller state-event diagram
static const ctrlSEDiagram_t ctrlSEDiagram =
{
  [CTRL_EVENT_CURSOR_X] =
  {
    // CTRL_EVENT_CURSOR_X event handler and next state
    { ctrlCursorX,   CTRL_STATE_CURSOR },	// CTRL_STATE_CURSOR
    { ctrlCursorX,   CTRL_STATE_CURSOR },	// CTRL_STATE_READ
    { ctrlCursorX,   CTRL_STATE_CURSOR }	// CTRL_STATE_WRITE
  },
  [CTRL_EVENT_CURSOR_Y] =
  {
    // CTRL_EVENT_CURSOR_Y event handler and next state
    { ctrlCursorY,   CTRL_STATE_CURSOR },	// CTRL_STATE_CURSOR
    { ctrlCursorY,   CTRL_STATE_CURSOR },	// CTRL_STATE_READ
    { ctrlCursorY,   CTRL_STATE_CURSOR }	// CTRL_STATE_WRITE
  },
  [CTRL_EVENT_DISPLAY] =
  {
    // CTRL_EVENT_DISPLAY event handler and next state
    { ctrlDisplay,   CTRL_STATE_CURSOR },	// CTRL_STATE_CURSOR
    { ctrlDisplay,   CTRL_STATE_READ },		// CTRL_STATE_READ
    { ctrlDisplay,   CTRL_STATE_WRITE }		// CTRL_STATE_WRITE
  },
  [CTRL_EVENT_STARTLINE] =
  {
    // CTRL_EVENT_STARTLINE event handler and next state
    { ctrlStartline, CTRL_STATE_CURSOR },	// CTRL_STATE_CURSOR
    { ctrlStartline, CTRL_STATE_READ },		// CTRL_STATE_READ
    { ctrlStartline, CTRL_STATE_WRITE }		// CTRL_STATE_WRITE
  },
  [CTRL_EVENT_READ] =
  {
    // CTRL_EVENT_READ event handler and next state
    { ctrlReadDummy, CTRL_STATE_READ },		// CTRL_STATE_CURSOR
    { ctrlRead,      CTRL_STATE_READ },		// CTRL_STATE_READ
    { ctrlReadDummy, CTRL_STATE_READ }		// CTRL_STATE_WRITE
  },
  [CTRL_EVENT_WRITE] =
  {
    // CTRL_EVENT_WRITE event handler and next state
    { ctrlWrite,     CTRL_STATE_WRITE },	// CTRL_STATE_CURSOR
    { ctrlWrite,     CTRL_STATE_WRITE },	// CTRL_STATE_READ
    { ctrlWrite,     CTRL_STATE_WRITE }		// CTRL_STATE_WRITE
  }
};

// The active controller (0 or 1)
static u08 controller = 0;

// The controller data containing the state, registers, lcd image and stats
static ctrlController_t ctrlControllers[GLCD_NUM_CONTROLLERS];

// Statistics counters on lcd glcd interface
static ctrlGlcdStats_t ctrlGlcdStats;		// Aggregated statistics
static ctrlGlcdStats_t ctrlGlcdStatsCopy;	// Copy for single cycle stats

// Identifiers to indicate what lcd stub devices are used
static u08 useGlut = MC_FALSE;
static u08 useNcurses = MC_FALSE;
static u08 useDevice = CTRL_DEVICE_NULL;

// Local function prototypes
static void ctrlAddressNext(ctrlRegister_t *ctrlRegister);
static u08 ctrlEventGet(u08 data, u08 *command, u08 *payload);

//
// Function: ctrlAddressNext
//
// Generate next controller address
//
static void ctrlAddressNext(ctrlRegister_t *ctrlRegister)
{
  // This is how the actual controller behaves:
  // At end of x reset to beginning of x, else just increment
  if (ctrlRegister->x >= GLCD_CONTROLLER_XPIXELS - 1)
    ctrlRegister->x = 0;
  else
    ctrlRegister->x++;
}

//
// Function: ctrlCursorX
//
// Event handler for setting x cursor in controller
//
static u08 ctrlCursorX(ctrlController_t *ctrlController, u08 payload)
{
  // Check if register will be changed
  ctrlGlcdStats.addressSet++;
  ctrlController->ctrlStats.xReq++;
  if (ctrlController->ctrlRegister.x != payload)
  {
    // Set x cursor in controller register
    ctrlController->ctrlStats.xCnf++;
    ctrlController->ctrlRegister.x = payload;
  }

  return MC_FALSE;
}

//
// Function: ctrlCursorY
//
// Event handler for setting y page cursor in controller
//
static u08 ctrlCursorY(ctrlController_t *ctrlController, u08 payload)
{
  // Check if register will be changed
  ctrlController->ctrlStats.yReq++;
  if (ctrlController->ctrlRegister.y != payload)
  {
    // Set y cursor in controller register
    ctrlController->ctrlStats.yCnf++;
    ctrlController->ctrlRegister.y = payload;
  }

  return MC_FALSE;
}

//
// Function: ctrlDisplay
//
// Event handler for setting controller display on/off
//
static u08 ctrlDisplay(ctrlController_t *ctrlController, u08 payload)
{
  u08 retVal = MC_FALSE;

  // Check if register will be changed
  ctrlController->ctrlStats.displayReq++;
  if (ctrlController->ctrlRegister.display != payload)
  {
    // Set display on/off in controller register and signal redraw
    ctrlController->ctrlStats.displayCnf++;
    ctrlController->ctrlRegister.display = payload;
    retVal = MC_TRUE;
  }

  return retVal;
}

//
// Function: ctrlExecute
//
// Execute an action in the active controller finite state machine
//
void ctrlExecute(u08 method)
{
  u08 x;
  u08 y;
  u08 event;
  u08 data;
  u08 command = 0;
  u08 payload = 0;
  u08 lcdUpdate = MC_FALSE;
  ctrlController_t *ctrlController = &ctrlControllers[controller];
  u08 state = ctrlController->state;

  // Create a controller finite state machine event using the action data
  if (method == CTRL_METHOD_READ)
  {
    // Read from the lcd controller. The result will end up in controller
    // register dataRead.
    ctrlGlcdStats.dataRead++;
    data = 0;
    event = CTRL_EVENT_READ;
  }
  else if (method == CTRL_METHOD_WRITE)
  {
    // Write to the lcd controller
    ctrlGlcdStats.dataWrite++;
    x = ctrlController->ctrlRegister.x + controller * GLCD_CONTROLLER_XPIXELS;
    y = ctrlController->ctrlRegister.y;
    data = (GLCD_DATAH_PORT & 0xf0) | (GLCD_DATAL_PORT & 0x0f);
    payload = data;
    event = CTRL_EVENT_WRITE;
  }
  else if (method == CTRL_METHOD_CTRL_W)
  {
    // Send command to the lcd controller.
    // Decode data to controller in event, command and payload.
    data = (GLCD_DATAH_PORT & 0xf0) | (GLCD_DATAL_PORT & 0x0f);
    event = ctrlEventGet(data, &command, &payload);
  }
  else
  {
    // Invalid action method
    emuCoreDump(CD_CTRL, __func__, method, 0, 0, 0);
  }

  // Dump controller request
  //printf("event: origin=%d ctrl=%d data=0x%02x cmd=%d payload=%d\n",
  //  method, controller, data, command, payload);

  // Execute the state-event handler and assign new machine state to controller
  lcdUpdate = ctrlSEDiagram[event][state].handler(ctrlController, payload);
  ctrlController->state = ctrlSEDiagram[event][state].stateNext;

  // Update the lcd devices if needed
  if (lcdUpdate == MC_TRUE)
  {
    if (event == CTRL_EVENT_DISPLAY)
    {
      if (useGlut == MC_TRUE)
        lcdGlutDisplaySet(controller, payload);
      if (useNcurses == MC_TRUE)
        lcdNcurDisplaySet(controller, payload);
    }
    else if (event == CTRL_EVENT_STARTLINE)
    {
      if (useGlut == MC_TRUE)
        lcdGlutStartLineSet(controller, payload);
      if (useNcurses == MC_TRUE)
        lcdNcurStartLineSet(controller, payload);
    }
    else if (event == CTRL_EVENT_WRITE)
    {
      if (useGlut == MC_TRUE)
        lcdGlutDataWrite(x, y, payload);
      if (useNcurses == MC_TRUE)
        lcdNcurDataWrite(x, y, payload);
    }
  }

  // In case of a read request return the data from the controller register in
  // the returning H/L data pins
  if (event == CTRL_EVENT_READ)
  {
    GLCD_DATAH_PIN &= 0x0f;
    GLCD_DATAH_PIN |= (ctrlController->ctrlRegister.dataRead & 0xf0);
    GLCD_DATAL_PIN &= 0xf0;
    GLCD_DATAL_PIN |= (ctrlController->ctrlRegister.dataRead & 0x0f);
  }
}

//
// Function: ctrlRead
//
// Event handler for reading lcd data
//
static u08 ctrlRead(ctrlController_t *ctrlController, u08 payload)
{
  u08 x = ctrlController->ctrlRegister.x;
  u08 y = ctrlController->ctrlRegister.y;
  u08 dataRead = ctrlController->ctrlImage[x][y];

  // Copy lcd data in controller state register
  ctrlController->ctrlStats.readReq++;
  ctrlController->ctrlStats.readCnf++;
  ctrlController->ctrlRegister.dataRead = dataRead;

  // Move to next controller address
  ctrlAddressNext(&ctrlController->ctrlRegister);

  return MC_FALSE;
}

//
// Function: ctrlReadDummy
//
// Event handler for dummy read of lcd data
//
static u08 ctrlReadDummy(ctrlController_t *ctrlController, u08 payload)
{
  ctrlController->ctrlStats.readReq++;

  return MC_FALSE;
}

//
// Function: ctrlStartline
//
// Event handler for setting controller display offset
//
static u08 ctrlStartline(ctrlController_t *ctrlController, u08 payload)
{
  u08 retVal = MC_FALSE;

  // Check if register will be changed
  ctrlController->ctrlStats.startLineReq++;
  if (ctrlController->ctrlRegister.startLine != payload)
  {
    // Set startline in controller register and signal redraw
    ctrlController->ctrlStats.startLineCnf++;
    ctrlController->ctrlRegister.startLine = payload;
    retVal = MC_TRUE;
  }

  return retVal;
}

//
// Function: ctrlWrite
//
// Event handler for writing lcd data
//
static u08 ctrlWrite(ctrlController_t *ctrlController, u08 payload)
{
  u08 x = ctrlController->ctrlRegister.x;
  u08 y = ctrlController->ctrlRegister.y;
  u08 retVal = MC_FALSE;

  // Set data to write in controller state register
  ctrlController->ctrlRegister.dataWrite = payload;

  // Check if controller lcd image data will be changed
  ctrlController->ctrlStats.writeReq++;
  if (ctrlController->ctrlImage[x][y] != payload)
  {
    // Set lcd display data in controller
    ctrlController->ctrlStats.writeCnf++;
    ctrlController->ctrlImage[x][y] = payload;
    retVal = MC_TRUE;
  }

  // Move to next controller address
  ctrlAddressNext(&ctrlController->ctrlRegister);

  return retVal;
}

//
// Function: ctrlBusyState
//
// Report the active controller to be never busy
//
void ctrlBusyState(void)
{
  GLCD_DATAH_PIN = GLCD_DATAH_PIN & ~GLCD_STATUS_BUSY;
}

//
// Function: ctrlControlSelect
//
// Select controller in ports and set it as active controller.
// Note that this function eerily resembles glcdControlSelect() in ks0108.c
// [firmware]. So, why not use that function instead?
// The reason for this is that glcdControlSelect() is a static function (for
// good reason) and we do not want to make it public in Monochron code just for
// wanting to use it in our mchron Emuchron code base.
// So, not really nice, but we'll cope with it.
//
void ctrlControlSelect(u08 controller)
{
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
  ctrlControlSet();
}

//
// Function: ctrlControlSet
//
// Set active controller. Request for both or no controller is erroneous.
//
void ctrlControlSet()
{
  ctrlGlcdStats.ctrlSet++;
  if ((GLCD_CTRL_CS0_PORT & (0x1 << GLCD_CTRL_CS0)) != 0 &&
      (GLCD_CTRL_CS1_PORT & (0x1 << GLCD_CTRL_CS1)) != 0)
    emuCoreDump(CD_CTRL, __func__, 1, 0, 0, 0);
  else if ((GLCD_CTRL_CS0_PORT & (0x1 << GLCD_CTRL_CS0)) != 0)
    controller = 0;
  else if ((GLCD_CTRL_CS1_PORT & (0x1 << GLCD_CTRL_CS1)) != 0)
    controller = 1;
  else
    emuCoreDump(CD_CTRL, __func__, 0, 0, 0, 0);
}

//
// Function: ctrlEventGet
//
// Decode a controller control byte into a controller event and associated
// command and payload.
// Coredump in case an invalid controller command is provided.
//
static u08 ctrlEventGet(u08 data, u08 *command, u08 *payload)
{
  // Get controller command and command argument
  if ((data & 0xc0) == GLCD_SET_Y_ADDR)
  {
    // Set x position
    *command = CTRL_CMD_COLUMN;
    *payload = data & 0x3f;
    return CTRL_EVENT_CURSOR_X;
  }
  else if ((data & 0xf8) == GLCD_SET_PAGE)
  {
    // Set y page
    *command = CTRL_CMD_PAGE;
    *payload = data & 0x07;
    return CTRL_EVENT_CURSOR_Y;
  }
  else if ((data & 0xfe) == GLCD_ON_CTRL)
  {
    // Display on/off command
    *command = CTRL_CMD_DISPLAY;
    *payload = data & 0x01;
    return CTRL_EVENT_DISPLAY;
  }
  else if ((data & 0xc0) == GLCD_START_LINE)
  {
    // Set display data start line
    *command = CTRL_CMD_START_LINE;
    *payload = data & 0x3f;
    return CTRL_EVENT_STARTLINE;
  }
  else
  {
    // Invalid command
    emuCoreDump(CD_CTRL, __func__, data, 0, 0, 0);
    // We will not get here (dummy return)
    return 0;
  }
}

//
// Function: ctrlGlcdPixConfirm
//
// Confirm a glut double-click event to allow next one
//
void ctrlGlcdPixConfirm(void)
{
  if (lcdGlutGlcdPix.active == MC_TRUE && lcdGlutGlcdPix.pixelLock == MC_TRUE)
    lcdGlutGlcdPix.pixelLock = MC_FALSE;
}

//
// Function: ctrlGlcdPixDisable
//
// Disable functionality to double-click a glut pixel
//
void ctrlGlcdPixDisable(void)
{
  lcdGlutGlcdPix.active = MC_FALSE;
  lcdGlutGlcdPix.pixelLock = MC_FALSE;
  lcdGlutGlcdPix.glcdX = 0;
  lcdGlutGlcdPix.glcdY = 0;
}

//
// Function: ctrlGlcdPixEnable
//
// Enable functionality to double-click a glut pixel
//
void ctrlGlcdPixEnable(void)
{
  lcdGlutGlcdPix.glcdX = 0;
  lcdGlutGlcdPix.glcdY = 0;
  lcdGlutGlcdPix.pixelLock = MC_FALSE;
  lcdGlutGlcdPix.active = MC_TRUE;
}

//
// Function: ctrlGlcdPixGet
//
// Check for and return glut double-click event data
//
u08 ctrlGlcdPixGet(u08 *x, u08 *y)
{
  if (lcdGlutGlcdPix.active == MC_TRUE && lcdGlutGlcdPix.pixelLock == MC_TRUE)
  {
    *x = lcdGlutGlcdPix.glcdX;
    *y = lcdGlutGlcdPix.glcdY;
    return MC_TRUE;
  }

  return MC_FALSE;
}

//
// Function: ctrlInit
//
// Initialize the data, registers and state of all controllers and the lcd stub
// device(s).
// Return: MC_TRUE (success) or MC_FALSE (failure).
//
u08 ctrlInit(ctrlDeviceArgs_t *ctrlDeviceArgs)
{
  u08 i;
  unsigned char initOk = MC_TRUE;

  // Administer which lcd stub devices are used
  useGlut = ctrlDeviceArgs->useGlut;
  useNcurses = ctrlDeviceArgs->useNcurses;
  useDevice = CTRL_DEVICE_NULL;
  if (useNcurses == MC_TRUE)
    useDevice = useDevice | CTRL_DEVICE_NCURSES;
  if (useGlut == MC_TRUE)
    useDevice = useDevice | CTRL_DEVICE_GLUT;

  // Initialize the controller data, registers and state
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    // Clear all controller data to 0 and initialize the controller state
    memset(&ctrlControllers[i], 0, sizeof(ctrlController_t));
    ctrlControllers[i].state = CTRL_STATE_CURSOR;
  }

  // Init the glut pixel double-click event data structure
  ctrlGlcdPixDisable();

  // Clear glcd and controller statistics before use. The stats for each lcd
  // device are cleared in their respective init method.
  ctrlStatsReset(CTRL_STATS_GLCD | CTRL_STATS_CTRL);

  // Init the ncurses device when requested
  if (useNcurses == MC_TRUE)
    initOk = lcdNcurInit(&ctrlDeviceArgs->lcdNcurInitArgs);

  // Init the OpenGL2/GLUT device when requested
  if (useGlut == MC_TRUE && initOk == MC_TRUE)
    initOk = lcdGlutInit(&ctrlDeviceArgs->lcdGlutInitArgs);

  // Cleanup in case there was a failure
  if (initOk == MC_FALSE)
    ctrlCleanup();

  return initOk;
}

//
// Function: ctrlCleanup
//
// Shut down the lcd display in stub device(s)
//
void ctrlCleanup(void)
{
  if (useNcurses == MC_TRUE)
    lcdNcurCleanup();
  if (useGlut == MC_TRUE)
    lcdGlutCleanup();
  useNcurses = MC_FALSE;
  useGlut = MC_FALSE;
  useDevice = CTRL_DEVICE_NULL;
}

//
// Function: ctrlDeviceActive
//
// Returns whether lcd device(s) is/are active
//
u08 ctrlDeviceActive(u08 device)
{
  if ((useDevice & device) != device)
    return MC_FALSE;

  return MC_TRUE;
}

//
// Function: ctrlLcdBacklightSet
//
// Set backlight brightness of lcd display in stub device
//
void ctrlLcdBacklightSet(u08 brightness)
{
  OCR2B = (uint16_t)brightness << OCR2B_BITSHIFT;
  if (useGlut == MC_TRUE)
    lcdGlutBacklightSet((unsigned char)brightness);
  if (useNcurses == MC_TRUE)
    lcdNcurBacklightSet((unsigned char)brightness);
}

//
// Function: ctrlLcdFlush
//
// Flush the lcd display in stub device
//
void ctrlLcdFlush(void)
{
  if (useGlut == MC_TRUE)
    lcdGlutFlush();
  if (useNcurses == MC_TRUE)
    lcdNcurFlush();
}

//
// Function: ctrlLcdGlutGrSet
//
// Enable/disable glut graphics options
//
void ctrlLcdGlutGrSet(u08 bezel, u08 grid)
{
  if (useGlut == MC_TRUE)
    lcdGlutGraphicsSet((unsigned char)bezel, (unsigned char)grid);
}

//
// Function: ctrlLcdGlutHlSet
//
// Set/reset glut glcd pixel highlight
//
void ctrlLcdGlutHlSet(u08 highlight, u08 x, u08 y)
{
  if (useGlut == MC_TRUE)
    lcdGlutHighlightSet((unsigned char)highlight, (unsigned char)x,
      (unsigned char)y);
}

//
// Function: ctrlLcdGlutSizeSet
//
// Set glut window size
//
void ctrlLcdGlutSizeSet(char axis, u16 size)
{
  if (useGlut == MC_TRUE)
    lcdGlutSizeSet((unsigned char)axis, (unsigned int)size);
}

//
// Function: ctrlLcdNcurGrSet
//
// Enable/disable ncurses graphics options
//
void ctrlLcdNcurGrSet(u08 backlight)
{
  if (useNcurses == MC_TRUE)
    lcdNcurGraphicsSet((unsigned char)backlight);
}

//
// Function: ctrlPortDataSet
//
// Set controller data high and low port with byte data
//
void ctrlPortDataSet(u08 data)
{
  GLCD_DATAH_PORT &= ~0xf0;
  GLCD_DATAH_PORT |= data & 0xf0;
  GLCD_DATAL_PORT &= ~0x0f;
  GLCD_DATAL_PORT |= data & 0x0f;
}

//
// Function: ctrlRegPrint
//
// Print controllers state and registers
//
void ctrlRegPrint(void)
{
  u08 i;
  char c;
  ctrlRegister_t *ctrlRegister;
  char *state;

  printf("controllers:\n");
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    // Indicator for selected controller
    if (i == controller)
      c = '*';
    else
      c = ' ';

    // Get controller state and its registers
    if (ctrlControllers[i].state == CTRL_STATE_CURSOR)
      state = "cursor";
    else if (ctrlControllers[i].state == CTRL_STATE_READ)
      state = "read";
    else if (ctrlControllers[i].state == CTRL_STATE_WRITE)
      state = "write";
    else // Should not occur
      state = "<unknown>";
    ctrlRegister = &ctrlControllers[i].ctrlRegister;

    // Print them
    printf("ctrl-%01d%c: state=%s, display=%d, startline=%d\n",
      i, c, state, ctrlRegister->display, ctrlRegister->startLine);
    printf("       : x=%d, y=%d, write=%d (0x%02x), read=%d (0x%02x)\n",
      ctrlRegister->x, ctrlRegister->y, ctrlRegister->dataWrite,
      ctrlRegister->dataWrite, ctrlRegister->dataRead, ctrlRegister->dataRead);
  }
}

//
// Function: ctrlStatsPrint
//
// Print statistics of the high level glcd interface and (optional) lcd
// controllers and display devices
//
void ctrlStatsPrint(u08 type)
{
  u08 i;
  ctrlStats_t *ctrlStats;
  ctrlStats_t *ctrlStatsCopy;

  // Report the glcd interface statistics
  if ((type & CTRL_STATS_GLCD) != CTRL_STATS_NULL)
  {
    // Aggregated statistics
    printf("glcd   : dataWrite=%llu, dataRead=%llu, addressSet=%llu\n",
      ctrlGlcdStats.dataWrite, ctrlGlcdStats.dataRead,
      ctrlGlcdStats.addressSet);
    printf("       : ctrlSet=%llu\n",
      ctrlGlcdStats.ctrlSet);
  }
  if ((type & CTRL_STATS_GLCD_CYCLE) != CTRL_STATS_NULL)
  {
    // Single cycle statistics
    printf("glcd   : dataWrite=%llu, dataRead=%llu, addressSet=%llu\n",
      ctrlGlcdStats.dataWrite - ctrlGlcdStatsCopy.dataWrite,
      ctrlGlcdStats.dataRead - ctrlGlcdStatsCopy.dataRead,
      ctrlGlcdStats.addressSet - ctrlGlcdStatsCopy.addressSet);
    printf("       : ctrlSet=%llu\n",
      ctrlGlcdStats.ctrlSet - ctrlGlcdStatsCopy.ctrlSet);
  }

  // Report controller statistics
  if ((type & CTRL_STATS_CTRL) != CTRL_STATS_NULL)
  {
    // Aggregated statistics
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    {
      ctrlStats = &ctrlControllers[i].ctrlStats;
      printf("ctrl-%d : ", i);
      if (ctrlStats->writeReq == 0)
        printf("write=%llu (-%%), ", ctrlStats->writeReq);
      else
        printf("write=%llu (%.0f%%), ", ctrlStats->writeReq,
          ctrlStats->writeCnf * 100 / (double)ctrlStats->writeReq);
      if (ctrlStats->readReq == 0)
        printf("read=%llu (-%%), ", ctrlStats->readReq);
      else
        printf("read=%llu (%.0f%%), ", ctrlStats->readReq,
          ctrlStats->readCnf * 100 / (double)ctrlStats->readReq);
      if (ctrlStats->displayReq == 0)
        printf("display=%llu (-%%)\n", ctrlStats->displayReq);
      else
        printf("display=%llu (%.0f%%)\n", ctrlStats->displayReq,
          ctrlStats->displayCnf * 100 / (double)ctrlStats->displayReq);
      if (ctrlStats->xReq == 0)
        printf("       : x=%llu (-%%), ", ctrlStats->xReq);
      else
        printf("       : x=%llu (%.0f%%), ", ctrlStats->xReq,
          ctrlStats->xCnf * 100 / (double)ctrlStats->xReq);
      if (ctrlStats->yReq == 0)
        printf("y=%llu (-%%), ", ctrlStats->yReq);
      else
        printf("y=%llu (%.0f%%), ", ctrlStats->yReq,
          ctrlStats->yCnf * 100 / (double)ctrlStats->yReq);
      if (ctrlStats->startLineReq == 0)
        printf("startline=%llu (-%%)\n", ctrlStats->startLineReq);
      else
        printf("startline=%llu (%.0f%%)\n", ctrlStats->startLineReq,
          ctrlStats->startLineCnf * 100 / (double)ctrlStats->startLineReq);
    }
  }
  if ((type & CTRL_STATS_CTRL_CYCLE) != CTRL_STATS_NULL)
  {
    // Single cycle statistics
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    {
      ctrlStats = &ctrlControllers[i].ctrlStats;
      ctrlStatsCopy = &ctrlControllers[i].ctrlStatsCopy;
      printf("ctrl-%d : ", i);
      if (ctrlStats->writeReq - ctrlStatsCopy->writeReq == 0)
        printf("write=%llu (-%%), ",
          ctrlStats->writeReq - ctrlStatsCopy->writeReq);
      else
        printf("write=%llu (%.0f%%), ",
          ctrlStats->writeReq - ctrlStatsCopy->writeReq,
          (ctrlStats->writeCnf - ctrlStatsCopy->writeCnf) * 100 /
            (double)(ctrlStats->writeReq - ctrlStatsCopy->writeReq));
      if (ctrlStats->readReq - ctrlStatsCopy->readReq == 0)
        printf("read=%llu (-%%), ",
          ctrlStats->readReq - ctrlStatsCopy->readReq);
      else
        printf("read=%llu (%.0f%%), ",
          ctrlStats->readReq - ctrlStatsCopy->readReq,
          (ctrlStats->readCnf - ctrlStatsCopy->readCnf) * 100 /
            (double)(ctrlStats->readReq - ctrlStatsCopy->readReq));
      if (ctrlStats->displayReq - ctrlStatsCopy->displayReq == 0)
        printf("display=%llu (-%%)\n",
          ctrlStats->displayReq - ctrlStatsCopy->displayReq);
      else
        printf("display=%llu (%.0f%%)\n",
          ctrlStats->displayReq - ctrlStatsCopy->displayReq,
          (ctrlStats->displayCnf - ctrlStatsCopy->displayCnf) * 100 /
            (double)(ctrlStats->displayReq - ctrlStatsCopy->displayReq));
      if (ctrlStats->xReq - ctrlStatsCopy->xReq == 0)
        printf("       : x=%llu (-%%), ",
          ctrlStats->xReq - ctrlStatsCopy->xReq);
      else
        printf("       : x=%llu (%.0f%%), ",
          ctrlStats->xReq - ctrlStatsCopy->xReq,
          (ctrlStats->xCnf - ctrlStatsCopy->xCnf) * 100 /
            (double)(ctrlStats->xReq - ctrlStatsCopy->xReq));
      if (ctrlStats->yReq - ctrlStatsCopy->yReq == 0)
        printf("y=%llu (-%%), ", ctrlStats->yReq - ctrlStatsCopy->yReq);
      else
        printf("y=%llu (%.0f%%), ", ctrlStats->yReq - ctrlStatsCopy->yReq,
          (ctrlStats->yCnf - ctrlStatsCopy->yCnf) * 100 /
            (double)(ctrlStats->yReq - ctrlStatsCopy->yReq));
      if (ctrlStats->startLineReq - ctrlStatsCopy->startLineReq == 0)
        printf("startline=%llu (-%%)\n",
          ctrlStats->startLineReq - ctrlStatsCopy->startLineReq);
      else
        printf("startline=%llu (%.0f%%)\n",
          ctrlStats->startLineReq - ctrlStatsCopy->startLineReq,
          (ctrlStats->startLineCnf - ctrlStatsCopy->startLineCnf) * 100 /
            (double)(ctrlStats->startLineReq - ctrlStatsCopy->startLineReq));
    }
  }

  // Report lcd stub device statistics
  if ((type & CTRL_STATS_LCD) != CTRL_STATS_NULL)
  {
    if (useGlut == MC_TRUE)
      lcdGlutStatsPrint();
    if (useNcurses == MC_TRUE)
      lcdNcurStatsPrint();
  }
}

//
// Function: ctrlStatsReset
//
// Reset the statistics for the glcd interface, controllers and lcd devices
//
void ctrlStatsReset(u08 type)
{
  u08 i;

  // Lcd glcd statistics. Note that the resetting the cycle statistics is
  // copying the aggregated statistics so we can calculate the delta between
  // the up-to-date aggregated stats and its earlier made copy.
  if ((type & CTRL_STATS_GLCD) != CTRL_STATS_NULL)
    memset(&ctrlGlcdStats, 0, sizeof(ctrlGlcdStats_t));
  if ((type & CTRL_STATS_GLCD_CYCLE) != CTRL_STATS_NULL)
    ctrlGlcdStatsCopy = ctrlGlcdStats;

  // Lcd controller statistics. Note that the resetting the cycle statistics is
  // copying the aggregated statistics so we can calculate the delta between
  // the up-to-date aggregated stats and its earlier made copy.
  if ((type & CTRL_STATS_CTRL) != CTRL_STATS_NULL)
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
      memset(&ctrlControllers[i].ctrlStats, 0, sizeof(ctrlStats_t));
  if ((type & CTRL_STATS_CTRL_CYCLE) != CTRL_STATS_NULL)
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
      ctrlControllers[i].ctrlStatsCopy = ctrlControllers[i].ctrlStats;

  // Glut and/or ncurses statistics
  if ((type & CTRL_STATS_LCD) != CTRL_STATS_NULL)
  {
    if (useGlut == MC_TRUE)
      lcdGlutStatsReset();
    if (useNcurses == MC_TRUE)
      lcdNcurStatsReset();
  }
}
