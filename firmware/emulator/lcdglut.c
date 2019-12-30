//*****************************************************************************
// Filename : 'lcdglut.c'
// Title    : Lcd glut stub functionality for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <GL/freeglut.h>
#include "lcdglut.h"

// This is ugly:
// Why can't we include the GLCD_* defs below from ks0108conf.h and ks0108.h?
// The reason for this is that headers in glut and avr define identical
// typedefs like uint32_t, making it impossible to compile glut code in an avr
// environment.
// So, we have to build glut completely independent from avr, and because of
// this we have to duplicate common stuff defines like lcd panel pixel sizes
// and colors in here.
// This also means that we can 'communicate' with the outside world using only
// common data types such as int, char, etc and whatever we define locally and
// expose via our header.
#define GLCD_XPIXELS		128
#define GLCD_YPIXELS		64
#define GLCD_CONTROLLER_XPIXELS	64
#define GLCD_CONTROLLER_YPIXELS	64
#define GLCD_NUM_CONTROLLERS \
  ((GLCD_XPIXELS + GLCD_CONTROLLER_XPIXELS - 1) / GLCD_CONTROLLER_XPIXELS)
#define GLCD_CONTROLLER_XPIXBITS 6
#define GLCD_CONTROLLER_YPIXMASK 0x3f
#define GLCD_FALSE		0
#define GLCD_TRUE		1
#define GLCD_OFF		0
#define GLCD_ON			1

// The actual Monochron lcd display has thin pixel bezels: in-between pixels
// you'll see a thin black line (quality display, huh?). We can mimick these
// pixel bezel lines in glut, but we need a minimum Monochron pixel window
// width or else the actual pixels get blurry. Showing the bezels however can
// also be a useful thing as it allows investigating the display layout pixel
// by pixel. Use mchron command 'lgg' to enable/disable.
#define GLUT_PIXBEZEL_WIDTH_PX	895.999

// How long window and Monochron pixel size info is shown after a redraw (msec)
#define GLUT_SHOW_PIXSIZE_MS	3000

// We use a frame around our lcd display, being 1 Monochron pixel wide on each
// side. So, the number of glut Monochron pixels in our display is a bit higher
// than the number of glcd Monochron pixels.
#define GLUT_XPIXELS		(GLCD_XPIXELS + 2)
#define GLUT_YPIXELS		(GLCD_YPIXELS + 2)

// The size of a glut Monochron pixel.
// Since the x and y range is from -1 to 1 we divide this
// range (=2) with the number of pixels we need.
#define GLUT_PIX_X_SIZE		((float)2 / GLUT_XPIXELS)
#define GLUT_PIX_Y_SIZE		((float)2 / GLUT_YPIXELS)

// The hor/vert aspect ratio of the glut lcd display (almost 2:1)
#define GLUT_ASPECTRATIO	((float)GLUT_XPIXELS / GLUT_YPIXELS)

// The lcd frame and gridline brightness
#define GLUT_FRAME_BRIGHTNESS	0.5
#define GLUT_GRID_BRIGHTNESS	0.3

// Get glut brightness greyscale based on display backlight level 0..16
#define GLUT_BRIGHTNESS(level)	((float)1 / 22 * (6 + (level)))

// The glut thread main loop sleep duration (msec)
#define GLUT_THREAD_SLEEP_MS	33

// The number of glut messages per message buffer malloc (= 2^bits)
#define GLUT_MSGS_BUFFER_BITS	6
#define GLUT_MSGS_PER_BUFFER	(0x1 << (GLUT_MSGS_BUFFER_BITS - 1))
#define GLUT_MSGS_MASK		(GLUT_MSGS_PER_BUFFER - 1)

// The lcd message queue commands
#define GLUT_CMD_EXIT		0
#define GLUT_CMD_BYTEDRAW	1
#define GLUT_CMD_BACKLIGHT	2
#define GLUT_CMD_DISPLAY	3
#define GLUT_CMD_STARTLINE	4
#define GLUT_CMD_OPTIONS	5
#define GLUT_CMD_HIGHLIGHT	6

// Definition of an lcd message to process for our glut window.
// Structure is populated depending on the message command:
// cmd = GLUT_CMD_EXIT		- <no arguments used>
// cmd = GLUT_CMD_BYTEDRAW	- arg1 = pixel byte, arg2 = x, arg3 = yline
// cmd = GLUT_CMD_BACKLIGHT	- arg1 = backlight value
// cmd = GLUT_CMD_DISPLAY	- arg1 = controller, arg2 = display value
// cmd = GLUT_CMD_STARTLINE	- arg1 = controller, arg2 = startline value
// cmd = GLUT_CMD_OPTIONS	- arg1 = pixel bezel, arg2 = gridlines
// cmd = GLUT_CMD_HIGHLIGHT	- arg1 = highlight, arg = x, arg3 = y
typedef struct _lcdGlutMsg_t
{
  unsigned char cmd;		// Message command (draw, backlight (etc))
  unsigned char arg1;		// First command argument
  unsigned char arg2;		// Second command argument
  unsigned char arg3;		// Third acommand argument
} lcdGlutMsg_t;

// Definition of a glut lcd message buffer
typedef struct _lcdGlutMsgBuf_t
{
  lcdGlutMsg_t lcdGlutMsg[GLUT_MSGS_PER_BUFFER];	// The glut lcd messages
  struct _lcdGlutMsgBuf_t *next;			// Ptr to next msg buffer
} lcdGlutMsgBuf_t;

// Definition of a structure holding the glut lcd device statistics
typedef struct _lcdGlutStats_t
{
  long long msgSend;		// Msgs sent
  long long msgRcv;		// Msgs received
  long long bitCnf;		// Lcd bits leading to glut update
  long long byteReq;		// Lcd bytes processed
  long long redraws;		// Glut window redraws
  long long queueMax;		// Max length of lcd message queue
  long long queueEvents;	// Clear non-zero length lcd message queue
  long long ticks;		// Glut thread cycles
  struct timeval timeStart;	// Timestamp start of glut interface
} lcdGlutStats_t;

// Definition of a structure to hold controller related data
typedef struct _lcdGlutCtrl_t
{
  unsigned char display;	// Indicates if controller display is active
  unsigned char startLine;	// Indicates the data display line offset
  int winPixMajority;		// Black vs. white pixels in controller image
  unsigned char drawInit;	// Whether we need to draw white background
} lcdGlutCtrl_t;

// The glut controller windows data
static lcdGlutCtrl_t lcdGlutCtrl[GLCD_NUM_CONTROLLERS];

// A private copy of the window image from which we draw our glut window
static unsigned char lcdGlutImage[GLCD_XPIXELS][GLCD_YPIXELS / 8];

// The glut window thread we create that opens and manages the OpenGL/Glut
// window and processes messages in the lcd message queue
static pthread_t threadGlut;
static unsigned char deviceActive = GLCD_FALSE;
static int winGlutWin;

// Start and end of lcd message queue and length
static lcdGlutMsgBuf_t *queueStart = NULL;
static lcdGlutMsgBuf_t *queueEnd = NULL;
static int queueLen = 0;

// Mutex to access the lcd message queue and statistics counters
// WARNING:
// To prevent deadlock *never* lock mutexQueue within a mutexStats lock
static pthread_mutex_t mutexQueue = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutexStats = PTHREAD_MUTEX_INITIALIZER;

// Identifiers to signal certain glut tasks
static unsigned char winExit = GLCD_FALSE;
static unsigned char winRedraw = GLCD_TRUE;

// Identifiers controlling temporary display of window pixel size
// info after a window resize
static unsigned char winRedrawFirst = GLCD_FALSE;
static unsigned char winResize = GLCD_FALSE;
static unsigned char winShowWinSize = GLCD_FALSE;
static struct timeval tvWinReshapeLast;

// Identifiers right-button down event and pixel highlight location
static unsigned char winRButtonEvent = GLCD_FALSE;
static int winRButtonX = 0;
static int winRButtonY = 0;
static unsigned char winPixHighlight = GLCD_FALSE;
static int winPixGlcdX = 0;
static int winPixGlcdY = 0;

// The init parameters
static lcdGlutInitArgs_t lcdGlutInitArgs;

// The (dummy) message we use upon creating the glut thread
static char *createMsg = "Monochron (glut)";

// The brightness of the pixels we draw
static float winBrightness = 1.0L;

// Statistics counters on glut and the lcd message queue
static lcdGlutStats_t lcdGlutStats;

// Window keyboard hit timestamp and key counter
static struct timeval tvWinKbLastHit;
static unsigned char winKbKeyCount;

// Indicator to draw window pixel bezels and gridlines
static unsigned char winGridLines = GLCD_FALSE;
static unsigned char winPixelBezel = GLCD_FALSE;

// Local function prototypes
static void lcdGlutClose(void);
static void lcdGlutKeyboard(unsigned char key, int x, int y);
static void *lcdGlutMain(void *ptr);
static void lcdGlutMouse(int button, int state, int x, int y);
static void lcdGlutMsgQueueAdd(unsigned char cmd, unsigned char arg1,
  unsigned char arg2, unsigned char arg3);
static void lcdGlutMsgQueueProcess(void);
static void lcdGlutRender(void);
static void lcdGlutRenderBezel(float arX, int winWidth);
static void lcdGlutRenderGrid(void);
static void lcdGlutRenderHighlight(float arX, float arY, int winWidth,
  int winHeight);
static void lcdGlutRenderInit(void);
static void lcdGlutRenderPixels(void);
static void lcdGlutRenderSchedule(void);
static void lcdGlutRenderSize(float arX, float arY, int winWidth,
  int winHeight);
static void lcdGlutReshape(int x, int y);
static void lcdGlutReshapeProcess(void);
static void lcdGlutSleep(int sleep);

//
// Function: lcdGlutBacklightSet
//
// Set backlight in lcd display in glut window
//
void lcdGlutBacklightSet(unsigned char backlight)
{
  // Add msg to queue to set backlight brightness
  lcdGlutMsgQueueAdd(GLUT_CMD_BACKLIGHT, backlight, 0, 0);
}

//
// Function: lcdGlutCleanup
//
// Shut down the lcd display in glut window
//
void lcdGlutCleanup(void)
{
  // Nothing to do if the glut environment is not initialized
  if (deviceActive == GLCD_FALSE)
    return;

  // Add msg to queue to exit glut thread
  lcdGlutMsgQueueAdd(GLUT_CMD_EXIT, 0, 0, 0);

  // Wait for glut thread to exit
  pthread_join(threadGlut, NULL);
  deviceActive = GLCD_FALSE;
}

//
// Function: lcdGlutClose
//
// Callback for glut window close event
//
static void lcdGlutClose(void)
{
  // Attempt to shutdown gracefully
  glutCloseFunc(NULL);
  glutDestroyWindow(winGlutWin);
  deviceActive = GLCD_FALSE;
  lcdGlutInitArgs.winClose();
}

//
// Function: lcdGlutDataWrite
//
// Draw pixels in lcd display in glut window
//
void lcdGlutDataWrite(unsigned char x, unsigned char y, unsigned char data)
{
  // Add msg to queue to draw a pixel byte (8 vertical pixels)
  lcdGlutMsgQueueAdd(GLUT_CMD_BYTEDRAW, data, x, y);
}

//
// Function: lcdGlutDisplaySet
//
// Switch controller display off or on
//
void lcdGlutDisplaySet(unsigned char controller, unsigned char display)
{
  // Add msg to queue to switch a controller display off or on
  lcdGlutMsgQueueAdd(GLUT_CMD_DISPLAY, controller, display, 0);
}

//
// Function: lcdGlutFlush
//
// Flush the lcd display in glut window (dummy)
//
void lcdGlutFlush(void)
{
  return;
}

//
// Function: lcdGlutGraphicsSet
//
// Enable/disable pixel bezel and gridline support
//
void lcdGlutGraphicsSet(unsigned char bezel, unsigned char grid)
{
  // Add msg to queue to enable/disable pixel bezel and gridline support
  lcdGlutMsgQueueAdd(GLUT_CMD_OPTIONS, bezel, grid, 0);
}

//
// Function: lcdGlutHighlightSet
//
// Enable/disable pixel highlight
//
void lcdGlutHighlightSet(unsigned char highlight, unsigned char x,
  unsigned char y)
{
  // Add msg to queue to set/reset glcd pixel highlight
  lcdGlutMsgQueueAdd(GLUT_CMD_HIGHLIGHT, highlight, x, y);
}

//
// Function: lcdGlutInit
//
// Initialize the lcd display in glut window
//
unsigned char lcdGlutInit(lcdGlutInitArgs_t *lcdGlutInitArgsSet)
{
  // Nothing to do if the glut environment is already initialized
  if (deviceActive == GLCD_TRUE)
    return GLCD_TRUE;

  // Reset the glut statistics
  lcdGlutStatsReset();

  // Copy initial glut window geometry and position
  lcdGlutInitArgs = *lcdGlutInitArgsSet;

  // Create the glut thread with lcdGlutMain() as main event loop
  (void)pthread_create(&threadGlut, NULL, lcdGlutMain, (void *)createMsg);
  deviceActive = GLCD_TRUE;

  return GLCD_TRUE;
}

//
// Function: lcdGlutKeyboard
//
// Event handler for glut keyboard event.
// Since a keyboard stroke has no function in our glut window briefly 'blink'
// the screen to gently indicate that focus should be put on the mchron command
// line terminal window.
//
static void lcdGlutKeyboard(unsigned char key, int x, int y)
{
  unsigned char controller;
  unsigned char k;
  unsigned char l;
  struct timeval tvNow;
  suseconds_t timeDiff;

  // Do not blink at every keyboard hit. When someone press-holds a key the
  // blinking will prevent regular updates from being drawn. Not good.
  // Only blink when a certain time has elapsed since the last blink or when
  // a consecutive number of previous keypresses were ignored for blinking.
  gettimeofday(&tvNow, NULL);
  timeDiff = (tvNow.tv_sec - tvWinKbLastHit.tv_sec) * 1E6 +
    tvNow.tv_usec - tvWinKbLastHit.tv_usec;
  if (timeDiff / 1000 <= GLUT_THREAD_SLEEP_MS + 3)
  {
    // Not enough time has elapsed to warrant a blink
    gettimeofday(&tvWinKbLastHit, NULL);
    winKbKeyCount++;

    // Keep ignoring up to a number of consecutive ignored keypresses
    if (winKbKeyCount < 15)
      return;
  }

  // Either enough time between blinks has elapsed or enough consecutive
  // keypresses were ignored. Hence, we'll blink.
  winKbKeyCount = 0;

  // Invert display in local image and render it
  for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
    lcdGlutCtrl[controller].winPixMajority =
      -lcdGlutCtrl[controller].winPixMajority;
  for (k = 0; k < GLCD_XPIXELS; k++)
    for (l = 0; l < GLCD_YPIXELS / 8; l++)
      lcdGlutImage[k][l] = ~(lcdGlutImage[k][l]);
  lcdGlutRender();

  // Wait 0.1 sec (this will lower the fps statistic)
  lcdGlutSleep(100);

  // And invert back to original
  for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
    lcdGlutCtrl[controller].winPixMajority =
      -lcdGlutCtrl[controller].winPixMajority;
  for (k = 0; k < GLCD_XPIXELS; k++)
    for (l = 0; l < GLCD_YPIXELS / 8; l++)
      lcdGlutImage[k][l] = ~(lcdGlutImage[k][l]);
  lcdGlutRender();

  // Set time offset for next blink
  gettimeofday(&tvWinKbLastHit, NULL);
}

//
// Function: lcdGlutMain
//
// Main function for glut thread
//
static void *lcdGlutMain(void *ptr)
{
  unsigned char controller;
  char *myArgv[1];
  int myArgc = 1;

  // Init our image display and controller related data
  memset(lcdGlutImage, 0, sizeof(lcdGlutImage));
  for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
  {
    lcdGlutCtrl[controller].display = GLCD_FALSE;
    lcdGlutCtrl[controller].startLine = 0;
    lcdGlutCtrl[controller].winPixMajority =
      -GLCD_CONTROLLER_XPIXELS * GLCD_CONTROLLER_YPIXELS / 2;
    lcdGlutCtrl[controller].drawInit = GLCD_FALSE;
  }

  // Init our glut environment
  myArgv[0] = (char *)ptr;
  glutInit(&myArgc, myArgv);
  glutInitDisplayMode(GLUT_DOUBLE);
  glutInitWindowSize(lcdGlutInitArgs.sizeX, lcdGlutInitArgs.sizeY);
  glutInitWindowPosition(lcdGlutInitArgs.posX, lcdGlutInitArgs.posY);
  winGlutWin = glutCreateWindow((char *)ptr);
  glutDisplayFunc(lcdGlutRenderSchedule);
  glutKeyboardFunc(lcdGlutKeyboard);
  glutMouseFunc(lcdGlutMouse);
  glutReshapeFunc(lcdGlutReshape);
  glutCloseFunc(lcdGlutClose);

  // Statistics
  pthread_mutex_lock(&mutexStats);
  gettimeofday(&lcdGlutStats.timeStart, NULL);
  pthread_mutex_unlock(&mutexStats);
  gettimeofday(&tvWinKbLastHit, NULL);
  gettimeofday(&tvWinReshapeLast, NULL);
  winKbKeyCount = 0;

  // Main glut process loop until we signal shutdown
  while (winExit == GLCD_FALSE)
  {
    // Statistics
    pthread_mutex_lock(&mutexStats);
    lcdGlutStats.ticks++;
    pthread_mutex_unlock(&mutexStats);

    // Process glut system events such as window resize, overlapping window
    // movements and window mouse/keypresses. They may invoke a window redraw.
    glutMainLoopEvent();

    // Process reshape logic and application message queue and redraw when
    // needed
    lcdGlutReshapeProcess();
    lcdGlutMsgQueueProcess();
    if (winRedraw == GLCD_TRUE)
    {
      lcdGlutRender();
      winRedraw = GLCD_FALSE;
      winRButtonEvent = GLCD_FALSE;
    }

    // Go to sleep to achieve low cpu usage combined with a glut refresh rate
    // at max ~30 fps
    lcdGlutSleep(GLUT_THREAD_SLEEP_MS);
  }

  // We are about to exit the glut thread. Disable the close callback as it
  // may get triggered upon exit. Why disable? In combination with an ncurses
  // device and the readline library it may cause a race condition in readline
  // library cleanup, potentially leading to an mchron coredump.
  glutCloseFunc(NULL);
  glutDestroyWindow(winGlutWin);

  return NULL;
}

//
// Function: lcdGlutMsgQueueAdd
//
// Add message to lcd message queue. The message queue consists of linked
// message buffers. When starting a new queue or when the current buffer is
// full create a new message buffer and add it at the end of the linked list.
//
static void lcdGlutMsgQueueAdd(unsigned char cmd, unsigned char arg1,
  unsigned char arg2, unsigned char arg3)
{
  lcdGlutMsg_t *lcdGlutMsg;
  lcdGlutMsgBuf_t *lcdGlutMsgBuf;
  int buffIndex;

  // Get exclusive access to the message queue
  pthread_mutex_lock(&mutexQueue);

  // Create new message buffer every x msgs and add it to the end of the queue
  buffIndex = (queueLen & GLUT_MSGS_MASK);
  if (buffIndex == 0)
  {
    lcdGlutMsgBuf = malloc(sizeof(lcdGlutMsgBuf_t));
    lcdGlutMsgBuf->next = NULL;
    if (queueStart == NULL)
    {
      // This is the first buffer in a new buffer queue
      queueStart = lcdGlutMsgBuf;
      queueEnd = queueStart;
    }
    else
    {
      // The queue is at least one buffer long. Link the last full buffer to
      // the next buffer.
      queueEnd->next = lcdGlutMsgBuf;
      queueEnd = queueEnd->next;
    }
  }

  // Fill in functional content of message
  lcdGlutMsg = queueEnd->lcdGlutMsg + buffIndex;
  lcdGlutMsg->cmd = cmd;
  lcdGlutMsg->arg1 = arg1;
  lcdGlutMsg->arg2 = arg2;
  lcdGlutMsg->arg3 = arg3;

  // Statistics
  queueLen++;
  pthread_mutex_lock(&mutexStats);
  lcdGlutStats.msgSend++;
  pthread_mutex_unlock(&mutexStats);

  // Release exclusive access to the message queue
  pthread_mutex_unlock(&mutexQueue);
}

//
// Function: lcdGlutMsgQueueProcess
//
// Process all messages in the lcd message queue
//
static void lcdGlutMsgQueueProcess(void)
{
  lcdGlutMsg_t *lcdGlutMsg;
  lcdGlutMsgBuf_t *freeBuf;
  lcdGlutMsgBuf_t *lcdGlutMsgBuf;
  int queueDone = 0;
  unsigned char lcdByte;
  unsigned char msgByte;
  unsigned char controller;
  unsigned char pixel = 0;

  // Get exclusive access to the message queue and statistics counters
  pthread_mutex_lock(&mutexQueue);
  pthread_mutex_lock(&mutexStats);

  // Statistics
  lcdGlutMsgBuf = queueStart;
  if (lcdGlutMsgBuf != NULL)
  {
    lcdGlutStats.queueEvents++;
    lcdGlutMsg = lcdGlutMsgBuf->lcdGlutMsg;
  }

  // Eat entire queue message by message from the message buffers
  freeBuf = queueStart;
  while (queueDone != queueLen)
  {
    // Statistics
    queueDone++;
    lcdGlutStats.msgRcv++;

    // Process the glut command
    if (lcdGlutMsg->cmd == GLUT_CMD_BYTEDRAW)
    {
      // Draw monochron pixels in window. The controller has decided that the
      // new data differs from the current lcd data.
      controller = lcdGlutMsg->arg2 >> GLCD_CONTROLLER_XPIXBITS;
      winRedraw = GLCD_TRUE;
      msgByte = lcdGlutMsg->arg1;
      lcdByte = lcdGlutImage[lcdGlutMsg->arg2][lcdGlutMsg->arg3];

      // Sync internal window image
      lcdGlutImage[lcdGlutMsg->arg2][lcdGlutMsg->arg3] = msgByte;

      // Statistics
      lcdGlutStats.byteReq++;
      for (pixel = 0; pixel < 8; pixel++)
      {
        if ((lcdByte & 0x1) != (msgByte & 0x1))
        {
          lcdGlutStats.bitCnf++;
          if ((msgByte & 0x1) == GLCD_ON)
            lcdGlutCtrl[controller].winPixMajority++;
          else
            lcdGlutCtrl[controller].winPixMajority--;
        }
        lcdByte = lcdByte >> 1;
        msgByte = msgByte >> 1;
      }
    }
    else if (lcdGlutMsg->cmd == GLUT_CMD_BACKLIGHT)
    {
      // Set background brightness and force redraw
      if (winBrightness != GLUT_BRIGHTNESS(lcdGlutMsg->arg1))
      {
        winBrightness = GLUT_BRIGHTNESS(lcdGlutMsg->arg1);
        winRedraw = GLCD_TRUE;
      }
    }
    else if (lcdGlutMsg->cmd == GLUT_CMD_DISPLAY)
    {
      // Set controller display and force redraw
      if (lcdGlutCtrl[lcdGlutMsg->arg1].display != lcdGlutMsg->arg2)
      {
        lcdGlutCtrl[lcdGlutMsg->arg1].display = lcdGlutMsg->arg2;
        winRedraw = GLCD_TRUE;
      }
    }
    else if (lcdGlutMsg->cmd == GLUT_CMD_STARTLINE)
    {
      // Set controller display line offset and force redraw
      if (lcdGlutCtrl[lcdGlutMsg->arg1].startLine != lcdGlutMsg->arg2)
      {
        lcdGlutCtrl[lcdGlutMsg->arg1].startLine = lcdGlutMsg->arg2;
        winRedraw = GLCD_TRUE;
      }
    }
    else if (lcdGlutMsg->cmd == GLUT_CMD_OPTIONS)
    {
      // Enable/disable pixel bezels and force redraw
      if (winPixelBezel != lcdGlutMsg->arg1)
      {
        winPixelBezel = lcdGlutMsg->arg1;
        winRedraw = GLCD_TRUE;
      }
      // Enable/disable gridlines and force redraw
      if (winGridLines != lcdGlutMsg->arg2)
      {
        winGridLines = lcdGlutMsg->arg2;
        winRedraw = GLCD_TRUE;
      }
    }
    else if (lcdGlutMsg->cmd == GLUT_CMD_HIGHLIGHT)
    {
      // Set/reset glcd pixel highlight
      if (winPixHighlight != lcdGlutMsg->arg1 ||
          winPixGlcdX != lcdGlutMsg->arg2 || winPixGlcdY != lcdGlutMsg->arg3)
      {
        winPixHighlight = lcdGlutMsg->arg1;
        winPixGlcdX = lcdGlutMsg->arg2;
        winPixGlcdY = lcdGlutMsg->arg3;
        winRedraw = GLCD_TRUE;
      }
    }
    else if (lcdGlutMsg->cmd == GLUT_CMD_EXIT)
    {
      // Signal to exit glut thread (when queue is processed)
      winExit = GLCD_TRUE;
    }

    // If at end of buffer go to next buffer, or get next message in buffer
    if ((queueDone & GLUT_MSGS_MASK) == 0)
    {
      lcdGlutMsgBuf = freeBuf->next;
      free(freeBuf);
      freeBuf = lcdGlutMsgBuf;
      lcdGlutMsg = lcdGlutMsgBuf->lcdGlutMsg;
    }
    else
    {
      lcdGlutMsg++;
    }
  }

  // Message queue is processed so let's cleanup
  if (freeBuf != NULL)
    free(freeBuf);
  queueStart = NULL;
  queueEnd = NULL;
  queueLen = 0;

  // Statistics
  if ((long long)queueDone > lcdGlutStats.queueMax)
    lcdGlutStats.queueMax = (long long)queueDone;

  // Release exclusive access to the statistics counters and message queue
  pthread_mutex_unlock(&mutexStats);
  pthread_mutex_unlock(&mutexQueue);
}

//
// Function: lcdGlutMouse
//
// Get right-click mouse down event to toggle glcd pixel highlight
//
static void lcdGlutMouse(int button, int state, int x, int y)
{
  if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
  {
    winRedraw = GLCD_TRUE;
    winRButtonEvent = GLCD_TRUE;
    winRButtonX = x;
    winRButtonY = y;
  }
}

//
// Function: lcdGlutRender
//
// Render a full redraw of glut window in alternating buffer
//
static void lcdGlutRender(void)
{
  int winWidth = glutGet(GLUT_WINDOW_WIDTH);
  int winHeight = glutGet(GLUT_WINDOW_HEIGHT);
  float arView = (float)winWidth / winHeight;
  float arX, arY;

  // Statistics
  pthread_mutex_lock(&mutexStats);
  lcdGlutStats.redraws++;
  pthread_mutex_unlock(&mutexStats);

  // Clear window buffer
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // We need to set the projection of our display to maintain the glut lcd
  // display aspect ratio of (almost) 2:1
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Set the projection orthogonal
  if (arView < GLUT_ASPECTRATIO)
  {
    // Shrink drawing area on the y-axis
    glOrtho(-1, 1, -GLUT_ASPECTRATIO / arView, GLUT_ASPECTRATIO / arView, -1,
      1);
    arX = 1;
    arY = GLUT_ASPECTRATIO / arView;
  }
  else
  {
    // Shrink drawing area on the x-axis
    glOrtho(-arView / GLUT_ASPECTRATIO, arView / GLUT_ASPECTRATIO, -1, 1, -1,
      1);
    arX = arView / GLUT_ASPECTRATIO;
    arY = 1;
  }

  // Draw our glut window that is fully cleared as a black background.
  // Init the Monochron display layout and then add pixels, pixel bezels,
  // gridlines, redraw size info and highlight pixel with glcd position.
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  lcdGlutRenderInit();
  lcdGlutRenderPixels();
  lcdGlutRenderBezel(arX, winWidth);
  lcdGlutRenderGrid();
  lcdGlutRenderSize(arX, arY, winWidth, winHeight);
  lcdGlutRenderHighlight(arX, arY, winWidth, winHeight);

  // Swap buffers for next redraw
  glutSwapBuffers();
}

//
// Function: lcdGlutRenderBezel
//
// Render pixel bezels in a Monochron display
//
static void lcdGlutRenderBezel(float arX, int winWidth)
{
  unsigned char x, y;
  unsigned char controller;
  float posX, posY;
  float minX, maxX;

  // Draw pixel bezel lines only when requested and when a certain minimum
  // Monochron window pixel width has been reached, or else the bezel lines
  // will blur the actual pixels
  if (winPixelBezel == GLCD_TRUE &&
      (float)winWidth * GLCD_XPIXELS / GLUT_XPIXELS / arX > GLUT_PIXBEZEL_WIDTH_PX)
  {
    // Draw from top to bottom
    glBegin(GL_LINES);
      glColor3f(0, 0, 0);
      posX = -1L + GLUT_PIX_X_SIZE;
      for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
      {
        // Skip controller draw area when the controller is switched off as
        // there is nothing to draw
        if (lcdGlutCtrl[controller].display == GLCD_FALSE)
        {
          posX = posX + GLCD_CONTROLLER_XPIXELS * GLUT_PIX_X_SIZE;
          continue;
        }

        // Draw the vertical lines per controller
        for (x = 0; x < GLCD_CONTROLLER_XPIXELS; x++)
        {
          glVertex2f(posX, -1L + GLUT_PIX_Y_SIZE);
          glVertex2f(posX,  1L - GLUT_PIX_Y_SIZE);
          posX = posX + GLUT_PIX_X_SIZE;
        }
      }

      // For drawing left to right first check if at least one controller is
      // enabled
      if (lcdGlutCtrl[0].display == GLCD_TRUE ||
          lcdGlutCtrl[1].display == GLCD_TRUE)
      {
        // Skip controller draw area when it is switched off as there is
        // nothing to draw
        minX = -1L + GLUT_PIX_X_SIZE;
        maxX = 1L - GLUT_PIX_X_SIZE;
        if (lcdGlutCtrl[0].display == GLCD_FALSE)
          minX = minX + GLCD_CONTROLLER_XPIXELS * GLUT_PIX_X_SIZE;
        if (lcdGlutCtrl[1].display == GLCD_FALSE)
          maxX = maxX - GLCD_CONTROLLER_XPIXELS * GLUT_PIX_X_SIZE;

        // Draw horizontal lines
        posY = -1L + GLUT_PIX_Y_SIZE;
        for (y = 0; y < GLCD_CONTROLLER_YPIXELS; y++)
        {
          glVertex2f(minX, posY);
          glVertex2f(maxX, posY);
          posY = posY + GLUT_PIX_Y_SIZE;
        }
      }
    glEnd();
  }
}

//
// Function: lcdGlutRenderGrid
//
// Render gridlines in a Monochron display
//
static void lcdGlutRenderGrid(void)
{
  unsigned char i;

  // Draw gridlines for testing purposes
  if (winGridLines == GLCD_TRUE)
  {
    glColor3f(GLUT_GRID_BRIGHTNESS, GLUT_GRID_BRIGHTNESS,
      GLUT_GRID_BRIGHTNESS);
    glBegin(GL_LINES);
      // Horizontal and vertical gridlines
      for (i = 1; i < 8; i++)
      {
        glVertex2f(i * 16 * GLUT_PIX_X_SIZE - 1 + GLUT_PIX_X_SIZE, -1);
        glVertex2f(i * 16 * GLUT_PIX_X_SIZE - 1 + GLUT_PIX_X_SIZE,  1);
      }
      for (i = 1; i < 4; i++)
      {
        glVertex2f(-1, i * 16 * GLUT_PIX_Y_SIZE - 1 + GLUT_PIX_Y_SIZE);
        glVertex2f( 1, i * 16 * GLUT_PIX_Y_SIZE - 1 + GLUT_PIX_Y_SIZE);
      }
      // Cross gridlines 1
      glVertex2f(-1 + GLUT_PIX_X_SIZE, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE,  1 - GLUT_PIX_Y_SIZE);
      glVertex2f(-1 + GLUT_PIX_X_SIZE,  1 - GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE, -1 + GLUT_PIX_Y_SIZE);
    glEnd();
    glBegin(GL_LINE_LOOP);
      // Cross gridlines 2
      glVertex2f(-1 + GLUT_PIX_X_SIZE, 0);
      glVertex2f(0,  1 - GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE, 0);
      glVertex2f(0, -1 + GLUT_PIX_Y_SIZE);
    glEnd();
  }
}

//
// Function: lcdGlutRenderHighlight
//
// Render a red highlighted pixel and its glcd position in a Monochron display
//
static void lcdGlutRenderHighlight(float arX, float arY, int winWidth,
  int winHeight)
{
  unsigned char controller;
  int winX, winY;
  int winLcdGlcdY;
  float posX, posY;
  float x, y;
  float dx, dy;
  float pixelSizeX, pixelSizeY;
  char sizeInfo1[14];
  char sizeInfo2[14];
  int size1, size2;
  char *c;

  // See if a pixel highlight on/off toggle event occurred
  if (winRButtonEvent == GLCD_TRUE && winPixHighlight == GLCD_TRUE)
  {
    // Disable pixel highlight
    winPixHighlight = GLCD_FALSE;
  }
  else if (winRButtonEvent == GLCD_TRUE && winPixHighlight == GLCD_FALSE)
  {
    // Obtain window position in draw pixels
    winX = (int)((float) winRButtonX / winWidth * arX * GLUT_XPIXELS -
      (arX - 1) * GLUT_XPIXELS / 2);
    winY = (int)((float) winRButtonY / winHeight * arY * GLUT_YPIXELS -
      (arY - 1) * GLUT_YPIXELS / 2);

    // Only enable pixel highlight when inside the glcd frame
    if (winX >= 1 && winX <= GLCD_XPIXELS && winY >= 1 && winY <= GLCD_YPIXELS)
    {
      // Enable pixel highlight and determine its location in glcd terms
      winPixHighlight = GLCD_TRUE;
      winPixGlcdX = winX - 1;
      controller = winPixGlcdX / GLCD_CONTROLLER_XPIXELS;
      winPixGlcdY = (winY - 1 + lcdGlutCtrl[controller].startLine) %
        GLCD_CONTROLLER_YPIXELS;
    }
  }

  // Draw highlighted glcd pixel when active
  if (winPixHighlight == GLCD_TRUE)
  {
    // Highlight the glcd pixel in red at 1.5 pixel size
    controller = winPixGlcdX / GLCD_CONTROLLER_XPIXELS;
    winLcdGlcdY = (winPixGlcdY - lcdGlutCtrl[controller].startLine +
      GLCD_CONTROLLER_YPIXELS) % GLCD_CONTROLLER_YPIXELS;
    posX = -1L + GLUT_PIX_X_SIZE * (winPixGlcdX + 1);
    posY = 1L - GLUT_PIX_Y_SIZE * (winLcdGlcdY + 1);
    glColor3f(1, 0, 0);
    glBegin(GL_QUADS);
      glVertex2f(posX - GLUT_PIX_X_SIZE / 2, posY - GLUT_PIX_Y_SIZE -
        GLUT_PIX_Y_SIZE / 2);
      glVertex2f(posX + GLUT_PIX_X_SIZE + GLUT_PIX_X_SIZE / 2, posY -
        GLUT_PIX_Y_SIZE - GLUT_PIX_Y_SIZE / 2);
      glVertex2f(posX + GLUT_PIX_X_SIZE + GLUT_PIX_X_SIZE / 2, posY +
        GLUT_PIX_Y_SIZE / 2);
      glVertex2f(posX - GLUT_PIX_X_SIZE / 2, posY + GLUT_PIX_Y_SIZE / 2);
    glEnd();

    // Show glcd pixel location info in a textbox.
    // Get strings for textbox and determine window pixel size in relation to
    // the projection.
    size1 = snprintf(sizeInfo1, 14, "glcd(x,y)");
    size2 = snprintf(sizeInfo2, 14, "(%d,%d)", winPixGlcdX, winPixGlcdY);
    pixelSizeX = 2 * arX / winWidth;
    pixelSizeY = 2 * arY / winHeight;

    // Background box size with border for text strings
    if (size1 > size2)
      x = (float)size1 * 4.5 * pixelSizeX;
    else
      x = (float)size2 * 4.5 * pixelSizeX;
    y = (float)18 * pixelSizeY;

    // Place text box near the glcd pixel, based on glcd pixel quadrant
    dx = ((float)winPixGlcdX + 1.5 - GLUT_XPIXELS / 2) / (GLUT_XPIXELS / 2);
    dy = -((float)winLcdGlcdY + 1.5 - GLUT_YPIXELS / 2) / (GLUT_YPIXELS / 2);
    if (winPixGlcdX < GLCD_XPIXELS / 2)
      dx = dx + x * 2;
    else
      dx = dx - x * 2;
    if (winLcdGlcdY < GLCD_YPIXELS / 2)
      dy = dy - y * 2;
    else
      dy = dy + y * 2;
    glColor3f(0.4, 0.4, 0.4);
    glBegin(GL_QUADS);
      glVertex2f(-x - 3 * pixelSizeX + dx, -18 * pixelSizeY + dy);
      glVertex2f( x + 3 * pixelSizeX + dx, -18 * pixelSizeY + dy);
      glVertex2f( x + 3 * pixelSizeX + dx,  18 * pixelSizeY + dy);
      glVertex2f(-x - 3 * pixelSizeX + dx,  18 * pixelSizeY + dy);
    glEnd();

    // Draw the glcd fixed text string in the box
    x = (float)size1 * 4.5 * pixelSizeX;
    y = (float)5 * pixelSizeY;
    glColor3f(0, 1, 1);
    glRasterPos2f(-x + dx, y + dy);
    for (c = sizeInfo1; *c != '\0'; c++)
      glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);

    // Draw the glcd position in the box
    x = (float)size2 * 4.5 * pixelSizeX;
    y = -(float)12 * pixelSizeY;
    glRasterPos2f(-x + dx, y + dy);
    for (c = sizeInfo2; *c != '\0'; c++)
      glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
  }
}

//
// Function: lcdGlutRenderInit
//
// Initialize a Monochron display by drawing a controller area background (if
// needed) and the display border.
//
static void lcdGlutRenderInit(void)
{
  unsigned char controller;
  float minX, maxX;

  // Find out which controllers have a majority of white pixels to draw
  for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
  {
    if (lcdGlutCtrl[controller].display == GLCD_TRUE &&
        lcdGlutCtrl[controller].winPixMajority >= 0)
      lcdGlutCtrl[controller].drawInit = GLCD_TRUE;
    else
      lcdGlutCtrl[controller].drawInit = GLCD_FALSE;
  }

  // Check the controller areas that must be initialized as white (with a
  // minority black pixels)
  if (lcdGlutCtrl[0].drawInit == GLCD_TRUE &&
      lcdGlutCtrl[1].drawInit == GLCD_TRUE)
  {
    // Both controller areas must be initialized white
    minX = -1 + GLUT_PIX_X_SIZE;
    maxX = 1 - GLUT_PIX_X_SIZE;
  }
  else if (lcdGlutCtrl[0].drawInit == GLCD_TRUE)
  {
    // Only controller 0 area must be initialized white
    minX = -1 + GLUT_PIX_X_SIZE;
    maxX = 0;
  }
  else if (lcdGlutCtrl[1].drawInit == GLCD_TRUE)
  {
    // Only controller 1 area must be initialized white
    minX = 0;
    maxX = 1 - GLUT_PIX_X_SIZE;
  }

  // Init the controller area(s) with a minority black pixels as white
  if (lcdGlutCtrl[0].drawInit == GLCD_TRUE ||
      lcdGlutCtrl[1].drawInit == GLCD_TRUE)
  {
    glBegin(GL_QUADS);
      glColor3f(winBrightness, winBrightness, winBrightness);
      glVertex2f(minX, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f(maxX, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f(maxX,  1 - GLUT_PIX_Y_SIZE);
      glVertex2f(minX,  1 - GLUT_PIX_Y_SIZE);
    glEnd();
  }

  // Draw display border in frame at 0.5 pixel from each border
  glBegin(GL_LINE_LOOP);
    glColor3f(GLUT_FRAME_BRIGHTNESS, GLUT_FRAME_BRIGHTNESS,
      GLUT_FRAME_BRIGHTNESS);
    glVertex2f(-1 + GLUT_PIX_X_SIZE / 2,-1 + GLUT_PIX_Y_SIZE / 2);
    glVertex2f(-1 + GLUT_PIX_X_SIZE / 2, 1 - GLUT_PIX_Y_SIZE / 2);
    glVertex2f( 1 - GLUT_PIX_X_SIZE / 2, 1 - GLUT_PIX_Y_SIZE / 2);
    glVertex2f( 1 - GLUT_PIX_X_SIZE / 2,-1 + GLUT_PIX_Y_SIZE / 2);
  glEnd();
}

//
// Function: lcdGlutRenderPixels
//
// Render minority pixels in a Monochron display
//
static void lcdGlutRenderPixels(void)
{
  unsigned char x, y;
  unsigned char line;
  unsigned char lcdByte;
  unsigned char pixel;
  unsigned char pixValDraw;
  unsigned char byteValIgnore;
  unsigned char controller;
  float posX, posY;
  float brightnessDraw;

  // The Monochron background and window frame are drawn and the parameters
  // for drawing either black or white pixels are set.
  // Now render the minority filled lcd pixels. Begin at left of x axis and
  // work our way to the right.
  posX = -1L + GLUT_PIX_X_SIZE;
  glBegin(GL_QUADS);
    for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
    {
      // Skip controller draw area when the controller is switched off as there
      // is nothing to draw
      if (lcdGlutCtrl[controller].display == GLCD_FALSE)
      {
        posX = posX + GLCD_CONTROLLER_XPIXELS * GLUT_PIX_X_SIZE;
        continue;
      }

      // Set draw parameters based on area background color
      if (lcdGlutCtrl[controller].drawInit == GLCD_TRUE)
      {
        // Draw black pixels in a white background
        pixValDraw = GLCD_OFF;
        byteValIgnore = 0xff;
        brightnessDraw = 0;
      }
      else
      {
        // Draw white pixels in a black background
        pixValDraw = GLCD_ON;
        byteValIgnore = 0x0;
        brightnessDraw = winBrightness;
      }

      // Move from left to right
      glColor3f(brightnessDraw, brightnessDraw, brightnessDraw);
      for (x = 0; x < GLCD_CONTROLLER_XPIXELS; x++)
      {
        // Begin painting at the y axis using the vertical offset. When we reach
        // the bottom on the glut window continue at the top for the remainaing
        // pixels.
        line = (GLCD_CONTROLLER_YPIXELS - lcdGlutCtrl[controller].startLine) &
          GLCD_CONTROLLER_YPIXMASK;
        posY = 1L - GLUT_PIX_Y_SIZE - line * GLUT_PIX_Y_SIZE;
        for (y = 0; y < GLCD_CONTROLLER_YPIXELS / 8; y++)
        {
          // Get data from image buffer with startline offset
          lcdByte =
            lcdGlutImage[x + (controller << GLCD_CONTROLLER_XPIXBITS)][y];

          if (lcdByte == byteValIgnore)
          {
            // This lcd byte does not contain any pixels to draw.
            // Shift y position for next 8 pixels.
            line = line + 8;
            if (line >= GLCD_CONTROLLER_YPIXELS)
            {
              // Due to startline offset continue at a new offset from the top
              line = line - GLCD_CONTROLLER_YPIXELS;
              posY = 1L - GLUT_PIX_Y_SIZE - line * GLUT_PIX_Y_SIZE;
            }
            else
            {
              posY = posY - 8 * GLUT_PIX_Y_SIZE;
            }
          }
          else
          {
            // Process each bit in lcd byte
            for (pixel = 0; pixel < 8; pixel++)
            {
              // Draw a pixel only when it is the draw color
              if ((lcdByte & 0x1) == pixValDraw)
              {
                // Draw a filled rectangle for the pixel
                glVertex2f(posX, posY - GLUT_PIX_Y_SIZE);
                glVertex2f(posX + GLUT_PIX_X_SIZE, posY - GLUT_PIX_Y_SIZE);
                glVertex2f(posX + GLUT_PIX_X_SIZE, posY);
                glVertex2f(posX, posY);
              }
              // Shift y position for next pixel
              line++;
              if (line == GLCD_CONTROLLER_YPIXELS)
              {
                // Due to startline offset continue at the top
                line = 0;
                posY = 1L - GLUT_PIX_Y_SIZE;
              }
              else
              {
                posY = posY - GLUT_PIX_Y_SIZE;
              }
              // Shift to next pixel
              lcdByte = (lcdByte >> 1);
            }
          }
        }
        // Shift x position for next set of vertical pixels
        posX = posX + GLUT_PIX_X_SIZE;
      }
    }
  glEnd();
}

//
// Function: lcdGlutRenderSchedule
//
// Signal a glut redraw in the main loop due to system and end-user actions
//
static void lcdGlutRenderSchedule(void)
{
  winRedraw = GLCD_TRUE;
}

//
// Function: lcdGlutRenderSize
//
// Render a centered textbox showing window size info after a resize operation
//
static void lcdGlutRenderSize(float arX, float arY, int winWidth,
  int winHeight)
{
  float x, y;
  float pixelSizeX, pixelSizeY;
  char sizeInfo1[14];
  char sizeInfo2[14];
  int size1, size2;
  char *c;

  // Draw window pixel size info after resizing the window
  if (winShowWinSize == GLCD_TRUE)
  {
    // Get strings for window pixel size and Monochron draw area pixel size
    // and determine window pixel size in relation to the projection
    size1 = snprintf(sizeInfo1, 14, "%dx%d", winWidth, winHeight);
    size2 = snprintf(sizeInfo2, 14, "(%.0fx%.0f)",
      (float)winWidth * GLCD_XPIXELS / GLUT_XPIXELS / arX,
      (float)winHeight * GLCD_YPIXELS / GLUT_YPIXELS / arY);
    pixelSizeX = 2 * arX / winWidth;
    pixelSizeY = 2 * arY / winHeight;

    // Background box with border for text strings
    if (size1 > size2)
      x = (float)size1 * 4.5 * pixelSizeX;
    else
      x = (float)size2 * 4.5 * pixelSizeX;
    glColor3f(0.4, 0.4, 0.4);
    glBegin(GL_QUADS);
      glVertex2f(-x - 3 * pixelSizeX, -18 * pixelSizeY);
      glVertex2f( x + 3 * pixelSizeX, -18 * pixelSizeY);
      glVertex2f( x + 3 * pixelSizeX,  18 * pixelSizeY);
      glVertex2f(-x - 3 * pixelSizeX,  18 * pixelSizeY);
    glEnd();

    // Draw the window size string in the box
    x = (float)size1 * 4.5 * pixelSizeX;
    y = (float)5 * pixelSizeY;
    glColor3f(0, 1, 1);
    glRasterPos2f(-x, y);
    for (c = sizeInfo1; *c != '\0'; c++)
      glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);

    // Draw the Monochron draw area pixel size string in the box
    x = (float)size2 * 4.5 * pixelSizeX;
    y = -(float)12 * pixelSizeY;
    glRasterPos2f(-x, y);
    for (c = sizeInfo2; *c != '\0'; c++)
      glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
  }
}

//
// Function: lcdGlutReshape
//
// Event handler for glut window end-user resize event.
// A resize event triggers displaying window pixel size info in a textbox.
//
void lcdGlutReshape(int x, int y)
{
  if (winRedrawFirst == GLCD_FALSE)
  {
    // At startup ignore the first resize event
    winRedrawFirst = GLCD_TRUE;
  }
  else
  {
    // Signal a resize event to show window pixel size info
    gettimeofday(&tvWinReshapeLast, NULL);
    winResize = GLCD_TRUE;
  }

  // Default glut behavior for dealing with window resize by end-user
  glViewport(0, 0, x, y);
  glutPostRedisplay();
}

//
// Function: lcdGlutReshapeProcess
//
// Process events ocurring after a glut window resize
//
void lcdGlutReshapeProcess(void)
{
  struct timeval tvNow;
  suseconds_t timeDiff;

  if (winResize == GLCD_TRUE)
  {
    // There has been a glut resize event. Clear it and begin showing
    // window pixel size info.
    winResize = GLCD_FALSE;
    winShowWinSize = GLCD_TRUE;
    winRedraw = GLCD_TRUE;
  }
  else if (winShowWinSize == GLCD_TRUE)
  {
    // We are currently showing window pixel size info.
    // Stop doing so after a timeout.
    gettimeofday(&tvNow, NULL);
    timeDiff = (tvNow.tv_sec - tvWinReshapeLast.tv_sec) * 1E6 +
      tvNow.tv_usec - tvWinReshapeLast.tv_usec;
    if (timeDiff / 1000 > GLUT_SHOW_PIXSIZE_MS)
    {
      // Timeout on showing the info so we must remove it
      winShowWinSize = GLCD_FALSE;
      winRedraw = GLCD_TRUE;
    }
  }
}

//
// Function: lcdGlutSleep
//
// Sleep amount in time (in msec)
//
static void lcdGlutSleep(int sleep)
{
  struct timeval tvWait;

  tvWait.tv_sec = 0;
  tvWait.tv_usec = sleep * 1000;
  select(0, NULL, NULL, NULL, &tvWait);
}

//
// Function: lcdGlutStartLineSet
//
// Set controller display line offset
//
void lcdGlutStartLineSet(unsigned char controller, unsigned char startLine)
{
  // Add msg to queue to set a controller display line offset
  lcdGlutMsgQueueAdd(GLUT_CMD_STARTLINE, controller, startLine, 0);
}

//
// Function: lcdGlutStatsPrint
//
// Print interface statistics
//
void lcdGlutStatsPrint(void)
{
  struct timeval tv;
  double diffDivider;

  // As this is a multi-threaded interface we need to have exclusive access
  // to the counters
  pthread_mutex_lock(&mutexStats);

  printf("glut   : lcdByteRx=%llu, ", lcdGlutStats.byteReq);
  if (lcdGlutStats.byteReq == 0)
    printf("bitEff=-%%\n");
  else
    printf("bitEff=%d%%\n",
      (int)(lcdGlutStats.bitCnf * 100 / (lcdGlutStats.byteReq * 8)));
  printf("         msgTx=%llu, msgRx=%llu, maxQLen=%llu, ",
    lcdGlutStats.msgSend, lcdGlutStats.msgRcv, lcdGlutStats.queueMax);
  if (lcdGlutStats.queueEvents == 0)
    printf("avgQLen=-\n");
  else
    printf("avgQLen=%llu\n",
      lcdGlutStats.msgSend / lcdGlutStats.queueEvents);
  printf("         redraws=%llu, cycles=%llu, updates=%llu, ",
    lcdGlutStats.redraws, lcdGlutStats.ticks, lcdGlutStats.queueEvents);
  if (lcdGlutStats.ticks == 0)
  {
    printf("fps=-\n");
  }
  else
  {
    // Get time elapsed since interface start time
    gettimeofday(&tv, NULL);
    diffDivider = (double)(((tv.tv_sec - lcdGlutStats.timeStart.tv_sec) * 1E6 +
      tv.tv_usec - lcdGlutStats.timeStart.tv_usec) / 1E4);
    printf("fps=%3.1f\n", (double)lcdGlutStats.ticks / diffDivider * 100);
  }

  pthread_mutex_unlock(&mutexStats);
}

//
// Function: lcdGlutStatsReset
//
// Reset interface statistics
//
void lcdGlutStatsReset(void)
{
  // As this is a multi-threaded interface we need to have exclusive access
  // to the counters
  pthread_mutex_lock(&mutexStats);
  memset(&lcdGlutStats, 0, sizeof(lcdGlutStats_t));
  gettimeofday(&lcdGlutStats.timeStart, NULL);
  pthread_mutex_unlock(&mutexStats);
}
