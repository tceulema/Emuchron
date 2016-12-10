//*****************************************************************************
// Filename : 'lcdglut.c'
// Title    : Lcd glut stub functionality for emuchron emulator
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <GL/glut.h>
#include <GL/freeglut_ext.h>
#include "lcdglut.h"

// This is ugly:
// Why can't we include the GLCD_* defs below from ks0108conf.h and
// ks0108.h?
// The reason for this is that headers in glut and avr define identical
// typedefs like uint32_t, making it impossible to compile glut code in
// an avr environment.
// So, we have to build glut completely independent from avr, and
// because of this we have to duplicate common stuff defines like lcd
// panel pixel sizes and colors in here.
// This also means that we can 'communicate' with the outside world
// using only common data types such as int, char, etc and whatever we
// define locally and expose via our header.
#define GLCD_XPIXELS		128
#define GLCD_YPIXELS		64
#define GLCD_CONTROLLER_XPIXELS	64
#define GLCD_CONTROLLER_YPIXELS	64
#define GLCD_NUM_CONTROLLERS \
  ((GLCD_XPIXELS + GLCD_CONTROLLER_XPIXELS - 1) / GLCD_CONTROLLER_XPIXELS)
#define GLCD_FALSE		0
#define GLCD_TRUE		1
#define GLCD_OFF		0
#define GLCD_ON			1

// We use a frame around our lcd display, being 1 pixel wide on each
// side. So, the number of GLUT pixels in our display is a bit larger
// than the number of GLCD pixels. 
#define GLUT_XPIXELS		(GLCD_XPIXELS + 2)
#define GLUT_YPIXELS		(GLCD_YPIXELS + 2)

// The size of a glut window pixel.
// Since the x and y range is from -1 to 1 we divide this
// range (=2) with the number of pixels we need.
#define GLUT_PIX_X_SIZE		((float)2 / GLUT_XPIXELS)
#define GLUT_PIX_Y_SIZE		((float)2 / GLUT_YPIXELS)

// The hor/vert aspect ratio of the glut lcd display (almost 2:1)
#define GLUT_ASPECTRATIO	((float)GLUT_XPIXELS / GLUT_YPIXELS)

// The lcd frame brightness
#define GLUT_FRAME_BRIGHTNESS	0.5

// The lcd message queue commands
#define GLUT_CMD_EXIT		0
#define GLUT_CMD_BYTEDRAW	1
#define GLUT_CMD_BACKLIGHT	2
#define GLUT_CMD_DISPLAY	3
#define GLUT_CMD_STARTLINE	4

// Definition of an lcd message to process for our glut window.
// Structure is populated depending on the message command:
// cmd = GLUT_CMD_EXIT		- <no arguments used>
// cmd = GLUT_CMD_BYTEDRAW	- arg1 = draw byte value, arg2 = x, arg3 = y
// cmd = GLUT_CMD_BACKLIGHT	- arg1 = backlight value
// cmd = GLUT_CMD_DISPLAY	- arg1 = controller, arg2 = display value
// cmd = GLUT_CMD_STARTLINE	- arg1 = controller, arg2 = startline value
typedef struct _lcdGlutMsg_t
{
  unsigned char cmd;		// Message command (draw, backlight (etc))
  unsigned char arg1;		// First command argument
  unsigned char arg2;		// Second command argument
  unsigned char arg3;		// Third acommand argument
  struct _lcdGlutMsg_t *next;	// Pointer to next msg in queue
} lcdGlutMsg_t;

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
} lcdGlutCtrl_t;

// The glut controller windows data
static lcdGlutCtrl_t lcdGlutCtrl[GLCD_NUM_CONTROLLERS];

// A private copy of the window image from which we draw our glut window 
static unsigned char lcdGlutImage[GLCD_XPIXELS][GLCD_YPIXELS / 8];

// The glut window thread we create that opens and manages the OpenGL/Glut
// window and processes messages in the lcd message queue 
static pthread_t threadGlut;
static int deviceActive = GLCD_FALSE;

// Start and end of lcd message queue
static lcdGlutMsg_t *queueStart = NULL;
static lcdGlutMsg_t *queueEnd = NULL;

// Mutex to access the lcd message queue and statistics counters
// WARNING:
// To prevent deadlock *never* lock mutexQueue within a mutexStats lock
static pthread_mutex_t mutexQueue = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutexStats = PTHREAD_MUTEX_INITIALIZER;

// Identifiers to signal certain glut tasks
static int doExit = GLCD_FALSE;
static int doFlush = GLCD_TRUE;

// The init parameters
static lcdGlutInitArgs_t lcdGlutInitArgs;

// The (dummy) message we use upon creating the glut thread 
static char *createMsg = "Monochron (glut)";

// The brightness of the pixels we draw
static float winBrightness = 1.0L;

// The number of black vs white pixels.
// <0 more black than white pixels
// =0 evently spread
// >0 more white than black pixels
static int winPixMajority = -GLCD_XPIXELS * GLCD_YPIXELS / 2;

// Statistics counters on glut and the lcd message queue
static lcdGlutStats_t lcdGlutStats;

// Local function prototypes
static void lcdGlutDelay(int x);
static void *lcdGlutMain(void *ptr);
static void lcdGlutMsgQueueAdd(unsigned char cmd, unsigned char arg1,
  unsigned char arg2, unsigned char arg3);
static void lcdGlutMsgQueueProcess(void);
static void lcdGlutRender(void);
static void lcdGlutKeyboard(unsigned char key, int x, int y);

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
// Function: lcdGlutDelay
//
// Delay time in milliseconds
//
static void lcdGlutDelay(int x)
{
  struct timeval sleepThis;

  sleepThis.tv_sec = 0;
  sleepThis.tv_usec = x * 1000;
  select(0, NULL, NULL, NULL, &sleepThis);
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
// Function: lcdGlutInit
//
// Initialize the lcd display in glut window
//
int lcdGlutInit(lcdGlutInitArgs_t *lcdGlutInitArgsSet)
{
  // Nothing to do if the glut environment is already initialized
  if (deviceActive == GLCD_TRUE)
    return 0;

  // Copy initial glut window geometry and position
  lcdGlutInitArgs = *lcdGlutInitArgsSet;

  // Create the glut thread with lcdGlutMain() as main event loop
  (void)pthread_create(&threadGlut, NULL, lcdGlutMain, (void *)createMsg);
  deviceActive = GLCD_TRUE;

  return 0;
}

//
// Function: lcdGlutKeyboard
//
// Event handler for glut keyboard event.
// Since a keyboard stroke has no function in our glut window
// briefly 'blink' the screen to gently indicate that focus should
// be put on the mchron command line terminal window.
//
static void lcdGlutKeyboard(unsigned char key, int x, int y)
{
  int k,l;

  // Invert display in buffer
  for (k = 0; k < GLCD_XPIXELS; k++)
    for (l = 0; l < GLCD_YPIXELS / 8; l++)
      lcdGlutImage[k][l] = ~(lcdGlutImage[k][l]);

  // Redraw buffer and flush it
  lcdGlutRender();
  glutSwapBuffers();

  // Wait 0.1 sec (this will lower the fps statistic)
  lcdGlutDelay(100);

  // And invert back to original
  for (k = 0; k < GLCD_XPIXELS; k++)
    for (l = 0; l < GLCD_YPIXELS / 8; l++)
      lcdGlutImage[k][l] = ~(lcdGlutImage[k][l]);

  // Redraw buffer and flush it
  lcdGlutRender();
  glutSwapBuffers();
  
  // Prevent useless reflush in main loop
  doFlush = GLCD_FALSE;
}

//
// Function: lcdGlutMain
//
// Main function for glut thread
//
static void *lcdGlutMain(void *ptr)
{
  unsigned char i;
  char *myArgv[1];
  int myArgc = 1;

  // Init our window image copy to blank screen
  memset(lcdGlutImage, 0, sizeof(lcdGlutImage));

  // Init our controller related data
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    lcdGlutCtrl[i].display = GLCD_FALSE;
    lcdGlutCtrl[i].startLine = 0;
  }

  // Init our glut environment
  myArgv[0] = (char *)ptr;
  glutInit(&myArgc, myArgv);
  glutInitDisplayMode(GLUT_DOUBLE);
  glutInitWindowSize(lcdGlutInitArgs.sizeX, lcdGlutInitArgs.sizeY);
  glutInitWindowPosition(lcdGlutInitArgs.posX, lcdGlutInitArgs.posY);
  glutCreateWindow((char *)ptr);
  glutDisplayFunc(lcdGlutRender);
  glutKeyboardFunc(lcdGlutKeyboard);
  glutCloseFunc(lcdGlutInitArgs.winClose);

  // Statistics
  gettimeofday(&lcdGlutStats.timeStart, NULL);
  
  // Main glut process loop until we signal shutdown
  while (doExit == GLCD_FALSE)
  {
    // Process glut system events.
    // Note: As lcdGlutRender() may get called in the glut loop
    // event we need to lock our statistics.
    pthread_mutex_lock(&mutexStats);
    lcdGlutStats.ticks++;
    glutMainLoopEvent();
    pthread_mutex_unlock(&mutexStats);

    // Process our application message queue
    lcdGlutMsgQueueProcess();

    // Render in case anything has changed
    if (doFlush == GLCD_TRUE)
    {
       lcdGlutRender();
       glutSwapBuffers();
       doFlush = GLCD_FALSE;
    }

    // Go to sleep to achieve low CPU usage combined with a refresh
    // rate at max ~30 fps
    lcdGlutDelay(33);
  }

  // We are about to exit the glut thread. Disable the close callback as it
  // may get triggered upon exit. Why disable? In combination with an ncurses
  // device and the readline library it may cause a race condition in readline
  // library cleanup, potentially leading to an mchron coredump.
  glutCloseFunc(NULL);

  return NULL;
}

//
// Function: lcdGlutMsgQueueAdd
//
// Add message to lcd message queue
//
static void lcdGlutMsgQueueAdd(unsigned char cmd, unsigned char arg1,
  unsigned char arg2, unsigned char arg3)
{
  void *mallocPtr;

  // Get exclusive access to the message queue
  pthread_mutex_lock(&mutexQueue);

  // Create new message and add it to the end of the queue
  mallocPtr = malloc(sizeof(lcdGlutMsg_t));
  if (queueStart == NULL)
  {
    // Start of new queue
    queueStart = (lcdGlutMsg_t *)mallocPtr;
    queueEnd = queueStart;
  }
  else
  {
    // The queue is at least of size one: link last queue member to next
    queueEnd->next = (lcdGlutMsg_t *)mallocPtr;
    queueEnd = queueEnd->next;
  }

  // Fill in functional content of message
  queueEnd->cmd = cmd;
  queueEnd->arg1 = arg1;
  queueEnd->arg2 = arg2;
  queueEnd->arg3 = arg3;
  queueEnd->next = NULL;

  // Statistics
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
  void *freePtr;
  lcdGlutMsg_t *glutMsg = queueStart;
  unsigned char lcdByte;
  unsigned char msgByte;
  unsigned char pixel = 0;
  int queueLength = 0;

  // Get exclusive access to the message queue and statistics counters
  pthread_mutex_lock(&mutexQueue);
  pthread_mutex_lock(&mutexStats);

  // Statistics
  if (glutMsg != NULL)
    lcdGlutStats.queueEvents++;

  // Eat entire queue message by message
  while (glutMsg != NULL)
  {
    // Statistics
    queueLength++;
    lcdGlutStats.msgRcv++;

    // Process the glut command
    if (glutMsg->cmd == GLUT_CMD_BYTEDRAW)
    {
      // Draw monochron pixels in window. The controller has decided
      // that the new data differs from the current lcd data.
      doFlush = GLCD_TRUE;
      msgByte = glutMsg->arg1;
      lcdByte = lcdGlutImage[glutMsg->arg2][glutMsg->arg3];

      // Sync internal window image
      lcdGlutImage[glutMsg->arg2][glutMsg->arg3] = msgByte;

      // Statistics
      lcdGlutStats.byteReq++;
      for (pixel = 0; pixel < 8; pixel++)
      {
        if ((lcdByte & 0x1) != (msgByte & 0x1))
        {
          lcdGlutStats.bitCnf++;
          if ((msgByte & 0x1) == GLCD_ON)
            winPixMajority++;
          else
            winPixMajority--;
        }
        lcdByte = lcdByte >> 1;
        msgByte = msgByte >> 1;
      }
    }
    else if (glutMsg->cmd == GLUT_CMD_BACKLIGHT)
    {
      // Set background brightness and force redraw
      if (winBrightness != (float)1 / 22 * (6 + glutMsg->arg1))
      {
        winBrightness = (float)1 / 22 * (6 + glutMsg->arg1);
        doFlush = GLCD_TRUE;
      }
    }
    else if (glutMsg->cmd == GLUT_CMD_DISPLAY)
    {
      // Set controller display and force redraw
      if (lcdGlutCtrl[glutMsg->arg1].display != glutMsg->arg2)
      {
        lcdGlutCtrl[glutMsg->arg1].display = glutMsg->arg2;
        doFlush = GLCD_TRUE;
      }
    }
    else if (glutMsg->cmd == GLUT_CMD_STARTLINE)
    {
      // Set controller display line offset and force redraw
      if (lcdGlutCtrl[glutMsg->arg1].startLine != glutMsg->arg2)
      {
        lcdGlutCtrl[glutMsg->arg1].startLine = glutMsg->arg2;
        doFlush = GLCD_TRUE;
      }
    }
    else if (glutMsg->cmd == GLUT_CMD_EXIT)
    {
      // Signal to exit glut thread (when queue is processed)
      doExit = GLCD_TRUE;
    }

    // Go to next queue member and release memory of current one
    freePtr = (void *)glutMsg;
    glutMsg = glutMsg->next;
    free(freePtr);
  }

  // Message queue is processed so let's cleanup
  queueStart = NULL;
  queueEnd = NULL;

  // Statistics
  if ((long long)queueLength > lcdGlutStats.queueMax)
    lcdGlutStats.queueMax = (long long)queueLength;

  // Release exclusive access to the statistics counters and message queue
  pthread_mutex_unlock(&mutexStats);
  pthread_mutex_unlock(&mutexQueue);
}

//
// Function: lcdGlutRender
//
// Render a full redraw of glut window in alternating buffer
//
static void lcdGlutRender(void)
{
  unsigned char x,y;
  unsigned char line;
  unsigned char lcdByte;
  unsigned char pixel, pixValDraw;
  unsigned char byteValIgnore;
  unsigned char controller;
  unsigned char on = GLCD_TRUE;
  float posX, posY;
  float viewAspectRatio;
  float brightnessDraw;

  // Statistics
  lcdGlutStats.redraws++;

  // Clear window buffer
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // We need to set the projection of our display to maintain the
  // glut lcd display aspect ratio of (almost) 2:1 
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  
  // Set the projection orthogonal
  viewAspectRatio =
    (float)glutGet(GLUT_WINDOW_WIDTH) / (float)glutGet(GLUT_WINDOW_HEIGHT);
  if (viewAspectRatio < GLUT_ASPECTRATIO)
  {
    // Use less space on the y-axis
    glOrtho(-1, 1, -(1 / (viewAspectRatio / GLUT_ASPECTRATIO)), 
      (1 / (viewAspectRatio / GLUT_ASPECTRATIO)), -1, 1);
  }
  else
  {
    // Use less space on the x-axis
    glOrtho(-viewAspectRatio / GLUT_ASPECTRATIO,
      viewAspectRatio / GLUT_ASPECTRATIO, -1, 1, -1, 1);
  }

  // We're going to draw our window.
  // We started with a fully cleared (=black) black window.
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Check if at least one of the controllers is switched off
  for (controller = 0; controller < GLCD_NUM_CONTROLLERS; controller++)
  {
    if (lcdGlutCtrl[controller].display == GLCD_FALSE)
    {
      on = GLCD_FALSE;
      break;
    }
  }

  // Determine whether we draw white on black or black on white
  if (winPixMajority < 0 || on == GLCD_FALSE)
  {
    // Majority of pixels is black so configure to draw a minority
    // number of white pixels
    pixValDraw = GLCD_ON;
    byteValIgnore = 0x0;
    brightnessDraw = winBrightness;
  }
  else
  {
    // Majority of pixels is white so configure to draw a minority
    // number of black pixels. For this we start off with a white
    // Monochron display (using a single draw only!) and then draw
    // the minority number of black pixels.
    glBegin(GL_QUADS);
      glColor3f(winBrightness, winBrightness, winBrightness);
      glVertex2f(-1 + GLUT_PIX_X_SIZE, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE,  1 - GLUT_PIX_Y_SIZE);
      glVertex2f(-1 + GLUT_PIX_X_SIZE,  1 - GLUT_PIX_Y_SIZE);
    glEnd();
    pixValDraw = GLCD_OFF;
    byteValIgnore = 0xff;
    brightnessDraw = 0;
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

  // The Monochron background and window frame are drawn and the
  // parameters for drawing either black or white pixels are set.
  // Now render the lcd pixels in our beautiful Monochron display
  // using the display image from our local lcd buffer.
  // Begin at left of x axis and work our way to the right.
  posX = -1L + GLUT_PIX_X_SIZE;
  for (x = 0; x < GLCD_XPIXELS; x++)
  {
    // Set controller belonging to the x column
    controller = x / GLCD_CONTROLLER_XPIXELS;

    // Begin painting at the y axis using the vertical offset. When we
    // reach the bottom on the glut window continue at the top for the
    // remaining pixels
    line = (GLCD_CONTROLLER_YPIXELS - lcdGlutCtrl[controller].startLine) %
      GLCD_CONTROLLER_YPIXELS;
    posY = 1L - GLUT_PIX_Y_SIZE - line * GLUT_PIX_Y_SIZE;
    for (y = 0; y < GLCD_YPIXELS / 8; y++)
    {
      // Get lcd byte to process
      if (lcdGlutCtrl[controller].display == GLCD_FALSE)
      {
        // The controller is switched off
        lcdByte = 0;
      }
      else
      {
        // Get data from lcd buffer with startline offset
        lcdByte = lcdGlutImage[x][y];
      }

      if (lcdByte == byteValIgnore)
      {
        // This lcd byte does not contain any pixels to draw.
        // Shift y position for next 8 pixels.
        line = line + 8;
        if (line >= GLCD_CONTROLLER_YPIXELS)
        {
          // Due to startline offset we will continue at a
          // new offset from the top
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
            // Draw a rectangle for the pixel
            glBegin(GL_QUADS);
              glColor3f(brightnessDraw, brightnessDraw, brightnessDraw);
              glVertex2f(posX, posY - GLUT_PIX_Y_SIZE);
              glVertex2f(posX + GLUT_PIX_X_SIZE, posY - GLUT_PIX_Y_SIZE);
              glVertex2f(posX + GLUT_PIX_X_SIZE, posY);
              glVertex2f(posX, posY);
            glEnd();
          }
          // Shift y position for next pixel
          line++;
          if (line == GLCD_CONTROLLER_YPIXELS)
          {
            // Due to startline offset we will continue at the top
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

  // Force window buffer flush
  doFlush = GLCD_TRUE;
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
    
  // As this is a multi-threaded interface we need to have exclusive
  // access to the counters
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
  // As this is a multi-threaded interface we need to have exclusive
  // access to the counters
  pthread_mutex_lock(&mutexStats);
  memset(&lcdGlutStats, 0, sizeof(lcdGlutStats_t));
  gettimeofday(&lcdGlutStats.timeStart, NULL);
  pthread_mutex_unlock(&mutexStats);
}

