//*****************************************************************************
// Filename : 'controller.c'
// Title    : Lcd controller stub functionality for emuchron emulator
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include "../ks0108.h"
#include "../config.h"
#include "mchronutil.h"
#include "controller.h"

// The lcd backlight register
extern uint16_t OCR2B;

//
// Our implementation of the 128x64 pixel lcd display image:
// - Two controllers, each containing 512 byte.
//
// Per controller:
//  <- 64 px -><- 64 px->
//  ^          ^
//  |  64 px   |  64 px
//  v          V
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
//       : repeat 6 byte for additional 48 y px
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
// The controller commands are:
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
// function is to implement an emulated lcd controller. The result of a controller
// machine event however is forwarded via this module to each of the active lcd
// devices, each holding a private copy of the lcd image data.
//
// This module is setup such that a single state-event machine is supported
// that emulates a set of two lcd controllers.
// The main reasons for this are ease of use and the fact that a Monochron
// actually only has a single set of controllers, so why support more.
// With some coding efforts however, this module can be changed to support
// multiple controller sets each having its own state, registers and lcd image,
// but this is considered out-of-scope for the Emuchron project.
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
//      x       | CURSOR        | CURSOR        | CURSOR        |
//              +---------------+---------------+---------------+
//              | set cursor y  | set cursor y  | set cursor y  |
//  set cursor  |               |               |               |
//    y page    | CURSOR        | CURSOR        | CURSOR        |
//              +---------------+---------------+---------------+
//              | set display   | set display   | set display   |
//    display   |               |               |               |
//    on/off    | CURSOR        | READ          | WRITE         |
//              +---------------+---------------+---------------+
//              | set startline | set startline | set startline |
//   set start  |               |               |               |
//     line     | CURSOR        | READ          | WRITE         |
//              +---------------+---------------+---------------+
//              | dummy read    | read lcd      | dummy read    |
//  data read   |               | cursor++      |               |
//              | READ          | READ          | READ          |
//              +---------------+---------------+---------------+
//              | write lcd     | write lcd     | write lcd     |
//  data write  | cursor++      | cursor++      | cursor++      |
//              | WRITE         | WRITE         | WRITE         |
//              +---------------+---------------+---------------+
//
// Regarding cursor++:
// The following describes how the actual hardware behaves.
// - The y page is never auto-incremented.
// - Only the x column cursor is auto-incremented.
// - At the end of a controller line at position 63, the x cursor resets to
//   position 0. In other words: resets to the beginning of the same y page.
//

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

// A controller is represented in the data it holds. It is a combination of
// its current software machine state, the input/output registers, its lcd
// image data, and for software emulation purposes a set of statistics
// counters.
// Definition of a structure holding the data for a controller.
typedef struct _ctrlController_t
{
  u08 state;				// Controller state
  ctrlRegister_t ctrlRegister;		// Registers
  ctrlImage_t ctrlImage;		// Lcd image data
  ctrlStats_t ctrlStats;		// Statistics
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

// Local finite state machine state-event function prototypes
static u08 ctrlCursorX(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlCursorY(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlDisplay(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlRead(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlReadDummy(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlStartline(ctrlController_t *ctrlController, u08 payload);
static u08 ctrlWrite(ctrlController_t *ctrlController, u08 payload);

// Define the controller state-event diagram
static ctrlSEDiagram_t ctrlSEDiagram =
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

// The controller data containing the state, registers, lcd image and stats
static ctrlController_t ctrlControllers[GLCD_NUM_CONTROLLERS];

// Identifiers to indicate what lcd stub devices are used
static u08 useGlut = GLCD_FALSE;
static u08 useNcurses = GLCD_FALSE;

// Statistics counters on glcd interface. The ncurses and glut devices
// have their own dedicated statistics counters that are administered
// independent from these.
static long long ctrlLcdByteRead = 0;	// Bytes read from lcd
static long long ctrlLcdByteWrite = 0;	// Bytes written to lcd
long long ctrlLcdSetAddress = 0;	// Cursor address set in lcd

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
  // This is how the actual contrioller behaves:
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
  ctrlController->ctrlStats.xReq++;
  if (ctrlController->ctrlRegister.x != payload)
  {
    // Set y-page in controller register
    ctrlController->ctrlStats.xCnf++;
    ctrlController->ctrlRegister.x = payload;
  }

  return GLCD_FALSE;
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
    // Set y-page in controller register
    ctrlController->ctrlStats.yCnf++;
    ctrlController->ctrlRegister.y = payload;
  }

  return GLCD_FALSE;
}

//
// Function: ctrlDisplay
//
// Event handler for setting controller display on/off
//
static u08 ctrlDisplay(ctrlController_t *ctrlController, u08 payload)
{
  u08 retVal = GLCD_FALSE;

  // Check if register will be changed
  ctrlController->ctrlStats.displayReq++;
  if (ctrlController->ctrlRegister.display != payload)
  {
    // Set display on/off in controller register and signal redraw
    ctrlController->ctrlStats.displayCnf++;
    ctrlController->ctrlRegister.display = payload;
    retVal = GLCD_TRUE;
  }

  return retVal;
}

//
// Function: ctrlExecute
//
// Execute an action in the controller finite state machine
//
u08 ctrlExecute(u08 method, u08 controller, u08 data)
{
  u08 x;
  u08 y;
  u08 event;
  u08 command = 0;
  u08 payload = 0;
  u08 lcdUpdate = GLCD_FALSE;
  ctrlController_t *ctrlController = &ctrlControllers[controller];
  u08 state = ctrlController->state;

  // Create a controller finite state machine event using the action data
  if (method == CTRL_METHOD_READ)
  {
    // Read from the lcd controller. The result will end up in controller
    // register dataRead.
    ctrlLcdByteRead++;
    event = CTRL_EVENT_READ;
  }
  else if (method == CTRL_METHOD_WRITE)
  {
    // Write to the lcd controller
    ctrlLcdByteWrite++;
    x = ctrlController->ctrlRegister.x + controller * GLCD_CONTROLLER_XPIXELS;
    y = ctrlController->ctrlRegister.y;
    event = CTRL_EVENT_WRITE;
    payload = data;
  }
  else if (method == CTRL_METHOD_COMMAND)
  {
    // Send command to the lcd controller.
    // Decode data to controller in event, command and payload.
    event = ctrlEventGet(data, &command, &payload);
  }
  else
  {
    // Invalid action method
    emuCoreDump(__func__, 0, 0, 0, method);
  }

  // Dump controller request
  //printf("event: origin=%d ctrl=%d data=0x%02x cmd=%d payload=%d\n",
  //  method, controller, data, command, payload);

  // Execute the state-event handler and assign new machine state to controller
  lcdUpdate = (*ctrlSEDiagram[event][state].handler)(ctrlController, payload);
  ctrlController->state = ctrlSEDiagram[event][state].stateNext;

  // Update the lcd devices if needed
  if (lcdUpdate == GLCD_TRUE)
  {
    if (event == CTRL_EVENT_DISPLAY)
    {
      if (useGlut == GLCD_TRUE)
        lcdGlutDisplaySet(controller, payload);
      if (useNcurses == GLCD_TRUE)
        lcdNcurDisplaySet(controller, payload);
    }
    else if (event == CTRL_EVENT_STARTLINE)
    {
      if (useGlut == GLCD_TRUE)
        lcdGlutStartLineSet(controller, payload);
      if (useNcurses == GLCD_TRUE)
        lcdNcurStartLineSet(controller, payload);
    }
    else if (event == CTRL_EVENT_WRITE)
    {
      if (useGlut == GLCD_TRUE)
        lcdGlutDataWrite(x, y, payload);
      if (useNcurses == GLCD_TRUE)
        lcdNcurDataWrite(x, y, payload);
    }
  }

  // In case of a read request return the data from the controller register
  if (event == CTRL_EVENT_READ)
    return ctrlController->ctrlRegister.dataRead;
  else
    return 0;
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

  return GLCD_FALSE;
}

//
// Function: ctrlReadDummy
//
// Event handler for dummy read of lcd data
//
static u08 ctrlReadDummy(ctrlController_t *ctrlController, u08 payload)
{
  ctrlController->ctrlStats.readReq++;

  return GLCD_FALSE;
}

//
// Function: ctrlStartline
//
// Event handler for setting controller display offset
//
static u08 ctrlStartline(ctrlController_t *ctrlController, u08 payload)
{
  u08 retVal = GLCD_FALSE;

  // Check if register will be changed
  ctrlController->ctrlStats.startLineReq++;
  if (ctrlController->ctrlRegister.startLine != payload)
  {
    // Set startline in controller register and signal redraw
    ctrlController->ctrlStats.startLineCnf++;
    ctrlController->ctrlRegister.startLine = payload;
    retVal = GLCD_TRUE;
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
  u08 retVal = GLCD_FALSE;

  // Set data to write in controller state register
  ctrlController->ctrlRegister.dataWrite = payload;

  // Check if controller lcd image data will be changed
  ctrlController->ctrlStats.writeReq++;
  if (ctrlController->ctrlImage[x][y] != payload)
  {
    // Set lcd display data in controller
    ctrlController->ctrlStats.writeCnf++;
    ctrlController->ctrlImage[x][y] = payload;
    retVal = GLCD_TRUE;
  }

  // Move to next controller address
  ctrlAddressNext(&ctrlController->ctrlRegister);

  return retVal;
}

//
// Function: ctrlEventGet
//
// Decode a controller command byte into a controller event and associated
// command and payload.
// Coredump in case an invalid controller command is provided.
//
static u08 ctrlEventGet(u08 data, u08 *command, u08 *payload)
{
  // Get controller command and command argument
  if ((data & 0xC0) == GLCD_SET_Y_ADDR)
  {
    // Set x position
    *command = CTRL_CMD_COLUMN;
    *payload = data & 0x3F;
    return CTRL_EVENT_CURSOR_X;
  }
  else if ((data & 0xF8) == GLCD_SET_PAGE)
  {
    // Set y page
    *command = CTRL_CMD_PAGE;
    *payload = data & 0x07;
    return CTRL_EVENT_CURSOR_Y;
  }
  else if ((data & 0xFE) == GLCD_ON_CTRL)
  {
    // Display on/off command
    *command = CTRL_CMD_DISPLAY;
    *payload = data & 0x01;
    return CTRL_EVENT_DISPLAY;
  }
  else if ((data & 0xC0) == GLCD_START_LINE)
  {
    // Set display data start line
    *command = CTRL_CMD_START_LINE;
    *payload = data & 0x3F;
    return CTRL_EVENT_STARTLINE;
  }
  else
  {
    // Invalid command
    emuCoreDump(__func__, 0, 0, 0, data);
    // We will not get here (dummy return)
    return 0;
  }
}

//
// Function: ctrlInit
//
// Initialize the data, registers and state of all controllers and
// the lcd stub device(s)
//
u08 ctrlInit(ctrlDeviceArgs_t ctrlDeviceArgs)
{
  u08 i;
  int initFail = GLCD_FALSE;
  int retVal;

  // Administer which lcd stub devices are used
  useGlut = ctrlDeviceArgs.useGlut;
  useNcurses = ctrlDeviceArgs.useNcurses;

  // Initialize the controller data, registers and state
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    // Clear all controller data to 0 and initialize the controller state
    memset(&ctrlControllers[i], 0, sizeof(ctrlController_t));
    ctrlControllers[i].state = CTRL_STATE_CURSOR;
  }

  // Initialize the lcd stub device(s)
  if (useNcurses == GLCD_TRUE)
  {
    // Init the ncurses device
    retVal = lcdNcurInit(&ctrlDeviceArgs.lcdNcurInitArgs);
    if (retVal != 0)
      initFail = GLCD_TRUE;
  }
  if (useGlut == GLCD_TRUE && initFail == GLCD_FALSE)
  {
    // Init the OpenGL2/GLUT device
    retVal = lcdGlutInit(&ctrlDeviceArgs.lcdGlutInitArgs);
    if (retVal != 0)
      initFail = GLCD_TRUE;
  }

  // Cleanup in case there was a failure
  if (initFail == GLCD_TRUE)
  {
    ctrlCleanup();
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: ctrlRegisterGet
//
// Get a copy of the controller registers
//
void ctrlRegisterGet(u08 controller, ctrlRegister_t *ctrlRegisterCopy)
{
  ctrlController_t *ctrlController = &ctrlControllers[controller];

  memcpy(ctrlRegisterCopy, &ctrlController->ctrlRegister,
    sizeof(ctrlRegister_t));
}

//
// Function: ctrlCleanup
//
// Shut down the lcd display in stub device
//
void ctrlCleanup(void)
{
  if (useGlut == GLCD_TRUE)
    lcdGlutCleanup();
  if (useNcurses == GLCD_TRUE)
    lcdNcurCleanup();
}

//
// Function: ctrlLcdBacklightSet
//
// Set backlight brightness of lcd display in stub device
//
void ctrlLcdBacklightSet(u08 brightness)
{
  OCR2B = (uint16_t)(brightness << OCR2B_BITSHIFT);
  if (useGlut == GLCD_TRUE)
    lcdGlutBacklightSet((unsigned char)brightness);
  if (useNcurses == GLCD_TRUE)
    lcdNcurBacklightSet((unsigned char)brightness);
}

//
// Function: ctrlLcdFlush
//
// Flush the lcd display in stub device
//
void ctrlLcdFlush(void)
{
  if (useGlut == GLCD_TRUE)
    lcdGlutFlush();
  if (useNcurses == GLCD_TRUE)
    lcdNcurFlush();
  return;
}

//
// Function: ctrlRegPrint
//
// Print controllers state and registers.
//
void ctrlRegPrint(void)
{
  ctrlRegister_t *ctrlRegister;
  u08 i;
  char *state;

  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
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
    printf("ctrl-%01d : state=%s, display=%d, startline=%d\n",
      i, state, ctrlRegister->display, ctrlRegister->startLine);
    printf("       : x=%d, y=%d, write=0x%02x, read=0x%02x\n", 
      ctrlRegister->x, ctrlRegister->y, ctrlRegister->dataWrite,
      ctrlRegister->dataRead);
  }
}

//
// Function: ctrlStatsPrint
//
// Print statistics of the high level glcd interface and (optional)
// lcd controllers and display devices
//
void ctrlStatsPrint(int type)
{
  int i;
  ctrlStats_t *ctrlStats;

  // Report the lcd interface statistics
  if ((type & CTRL_STATS_GLCD) != 0)
    printf("glcd   : dataWrite=%llu, dataRead=%llu, setAddress=%llu\n",
      ctrlLcdByteWrite, ctrlLcdByteRead, ctrlLcdSetAddress);

  // Report controller statistics
  if ((type & CTRL_STATS_CTRL) != 0)
  {
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    {
      ctrlStats = &ctrlControllers[i].ctrlStats;
      printf("ctrl-%d : ", i);
      if (ctrlStats->writeReq == 0)
        printf("write=%llu (-%%), ", ctrlStats->writeReq);
      else
        printf("write=%llu (%d%%), ", ctrlStats->writeReq,
          (int)(ctrlStats->writeCnf * 100 / ctrlStats->writeReq));
      if (ctrlStats->readReq == 0)
        printf("read=%llu (-%%), ", ctrlStats->readReq);
      else
        printf("read=%llu (%d%%), ", ctrlStats->readReq,
          (int)(ctrlStats->readCnf * 100 / ctrlStats->readReq));
      if (ctrlStats->displayReq == 0)
        printf("display=%llu (-%%)\n", ctrlStats->displayReq);
      else
        printf("display=%llu (%d%%)\n", ctrlStats->displayReq,
          (int)(ctrlStats->displayCnf * 100 / ctrlStats->displayReq));
      if (ctrlStats->xReq == 0)
        printf("       : x=%llu (-%%), ", ctrlStats->xReq);
      else
        printf("       : x=%llu (%d%%), ", ctrlStats->xReq,
          (int)(ctrlStats->xCnf * 100 / ctrlStats->xReq));
      if (ctrlStats->yReq == 0)
        printf("y=%llu (-%%), ", ctrlStats->yReq);
      else
        printf("y=%llu (%d%%), ", ctrlStats->yReq,
          (int)(ctrlStats->yCnf * 100 / ctrlStats->yReq));
      if (ctrlStats->startLineReq == 0)
        printf("startline=%llu (-%%)\n", ctrlStats->startLineReq);
      else
        printf("startline=%llu (%d%%)\n", ctrlStats->startLineReq,
          (int)(ctrlStats->startLineCnf * 100 / ctrlStats->startLineReq));
    }
  }
  
  // Report lcd stub device statistics
  if ((type & CTRL_STATS_LCD) != 0)
  {
    if (useGlut == GLCD_TRUE)
      lcdGlutStatsPrint();
    if (useNcurses == GLCD_TRUE)
      lcdNcurStatsPrint();
  }
  return;
}

//
// Function: ctrlStatsReset
//
// Reset the statistics for the glcd interface, controllers and lcd devices 
//
void ctrlStatsReset(int type)
{
  int i;

  // glcd statistics
  if ((type & CTRL_STATS_GLCD) != 0)
  {
    ctrlLcdByteRead = 0;
    ctrlLcdByteWrite = 0;
    ctrlLcdSetAddress = 0;
  }

  // Lcd controller statistics
  if ((type & CTRL_STATS_CTRL) != 0)
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
      memset(&ctrlControllers[i].ctrlStats, 0, sizeof(ctrlStats_t));

  // Glut and/or ncurses statistics
  if ((type & CTRL_STATS_LCD) != 0)
  {
    if (useGlut == GLCD_TRUE)
      lcdGlutStatsReset();
    if (useNcurses == GLCD_TRUE)
      lcdNcurStatsReset();
  }

  return;
}

