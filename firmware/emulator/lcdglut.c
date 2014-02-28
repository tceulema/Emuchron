//*****************************************************************************
// Filename : 'lcdglut.c'
// Title    : LCD glut stub functionality for MONOCHRON Emulator
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
// Why can't we include the two GLCD defs below from ks0108conf.h?
// The reason for this is that headers in glut and avr define identical
// typedefs like uint32_t, making it impossible to compile glut code in
// an avr environment.
// So, we have to build glut completely independent from avr, and
// because of this we have to duplicate common stuff defines like LCD
// panel pixel sizes in here.
// This also means that we can 'communicate' with the outside world
// using only common data types such as int, char, etc and whatever we
// define locally and expose via our header.
#define GLCD_XPIXELS		128
#define GLCD_YPIXELS		64
#define GLUT_FALSE		0
#define GLUT_TRUE		1

// We use a frame around our LCD display, being 1 pixel wide on each
// side. So, the number of GLUT pixels in our display is a bit larger
// than the number of GLCD pixels. 
#define GLUT_XPIXELS		(GLCD_XPIXELS + 2)
#define GLUT_YPIXELS		(GLCD_YPIXELS + 2)

// The size of a glut window pixel.
// Since the x and y range is from -1 to 1 we divide this
// range (=2) with the number of pixels we need.
#define GLUT_PIX_X_SIZE		((float)2 / GLUT_XPIXELS)
#define GLUT_PIX_Y_SIZE		((float)2 / GLUT_YPIXELS)

// The hor/vert aspect ratio of the glut LCD display (almost 2:1)
#define GLUT_ASPECTRATIO	((float)GLUT_XPIXELS / GLUT_YPIXELS)

// The lcd frame brightness
#define GLUT_FRAME_BRIGHTNESS	0.5

// The glut message queue commands
#define GLUT_CMD_EXIT		0
#define GLUT_CMD_BYTEDRAW	1
#define GLUT_CMD_BACKLIGHT	2

// Definition of a message to process for our glut window
typedef struct _winGlutMsg_t
{
  unsigned char cmd;		// Message type indicator (draw, backlight, end)
  unsigned char data;		// Functional data (8 pixel bits, backlight)
  int x;			// The Monochron x pos where to draw
  int y;			// The Monochron y byte pos where to draw
  struct _winGlutMsg_t *next;	// Pointer to next msg in queue
} winGlutMsg_t;

// The glut window thread we create that opens and manages the OpenGL/Glut
// window and processes messages in the glut message queue 
pthread_t winGlutThread;

// Start and end of glut message queue
winGlutMsg_t *winGlutQueueStart = NULL;
winGlutMsg_t *winGlutQueueEnd = NULL;

// Mutex to access the glut message queue and statistics counters
// WARNING:
// To prevent deadlock never ever lock winGlutQueueMutex within a
// winGlutStatsMutex lock
pthread_mutex_t winGlutQueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t winGlutStatsMutex = PTHREAD_MUTEX_INITIALIZER;

// A private copy of the window image to optimize glut window updates 
unsigned char lcdGlutImage[GLCD_XPIXELS][GLCD_YPIXELS / 8];

// Identifiers to signal certain glut tasks
int doWinGlutExit = GLUT_FALSE;
int doWinGlutFlush = GLUT_TRUE;

// A copy of the init parameters
int winGlutPosX;		// The initial glut x position
int winGlutPosY;		// The initial glut y position
int winGlutSizeX;		// The initial glut x size
int winGlutSizeY;		// The initial glut y size
void (*winGlutWinClose)(void);  // The mchron callback when end user closes glut window

// The (dummy) message we use upon creating the glut thread 
char *createMsg = "Monochron (glut)";

// The brightness of the pixels we draw
float winGlutBrightness = 1.0L;

// The number of black vs white pixels.
// <0 more black than white pixels
// =0 evently spread
// >0 more white than black pixels
int winPixMajority = -GLCD_XPIXELS * GLCD_YPIXELS / 2;

// Statistics counters on glut and the glut message queue
long long lcdGlutMsgSend = 0;   // Nbr of msgs sent
long long lcdGlutMsgRcv = 0;    // Nbr of msgs received
long long lcdGlutBitReq = 0;    // Nbr of LCD bits processed (from bytes with glut update)
long long lcdGlutBitCnf = 0;    // Nbr of LCD bits leading to glut update
long long lcdGlutByteReq = 0;   // Nbr of LCD bytes processed
long long lcdGlutByteCnf = 0;   // Nbr of LCD bytes leading to glut update
int lcdGlutRedraws = 0;	        // Nbr of glut window redraws (by internal glut event)
int lcdGlutQueueMax = 0;        // Max length of glut message queue
int lcdGlutQueueEvents = 0;     // Nbr of times the glut message queue is processed
struct timeval lcdGlutTimeStart;// Timestamp start of glut interface
long long lcdGlutTicks = 0;     // Nbr of glut thread cycles

// Local function prototypes
void winGlutDelay(int x);
void *winGlutMain(void *ptr);
void winGlutMsgQueueAdd(int x, int y, unsigned char data, unsigned char cmd);
void winGlutMsgQueueProcess(void);
void winGlutRender(void);
void winGlutKeyboard(unsigned char key, int x, int y);

//
// Function: lcdGlutBacklightSet
//
// Set backlight in LCD display in glut window
//
void lcdGlutBacklightSet(unsigned char backlight)
{
  // Add msg to queue to set backlight brightness
  winGlutMsgQueueAdd(0, 0, backlight, GLUT_CMD_BACKLIGHT);
}

//
// Function: lcdGlutEnd
//
// Shut down the LCD display in glut window
//
void lcdGlutEnd(void)
{
  // Add msg to queue to exit glut thread
  winGlutMsgQueueAdd(0, 0, 0, GLUT_CMD_EXIT);

  // Wait for glut thread to exit
  pthread_join(winGlutThread, NULL);
}

//
// Function: lcdGlutFlush
//
// Flush the LCD display in glut window (dummy)
//
void lcdGlutFlush(int force)
{
  return;
}

//
// Function: lcdGlutInit
//
// Initialize the LCD display in glut window
//
int lcdGlutInit(int lcdGlutPosX, int lcdGlutPosY, int lcdGlutSizeX, int lcdGlutSizeY,
  void (*lcdGlutWinClose)(void))
{
  // Copy initial glut window geometry and position
  winGlutPosX = lcdGlutPosX;
  winGlutPosY = lcdGlutPosY;
  winGlutSizeX = lcdGlutSizeX;
  winGlutSizeY = lcdGlutSizeY;
  winGlutWinClose = lcdGlutWinClose;

  // Create the glut thread with winGlutMain() as main event loop
  (void)pthread_create(&winGlutThread, NULL, winGlutMain, (void *)createMsg);

  return 0;
}

//
// Function: lcdGlutRestore
//
// Restore layout of the LCD display in glut window (dummy)
//
void lcdGlutRestore(void)
{
  return;
}

//
// Function: lcdGlutDataWrite
//
// Draw pixels in LCD display in glut window
//
void lcdGlutDataWrite(unsigned char x, unsigned char y, unsigned char data)
{
  // Add msg to queue to draw a pixel byte (8 vertical pixels)
  winGlutMsgQueueAdd(x, y, data, GLUT_CMD_BYTEDRAW);
}

//
// Function: winGlutDelay
//
// Delay time in milliseconds
//
void winGlutDelay(int x)
{
  struct timeval sleepThis;

  sleepThis.tv_sec = 0;
  sleepThis.tv_usec = x * 1000; 
  select(0, NULL, NULL, NULL, &sleepThis);
}

//
// Function: winGlutKeyboard
//
// Event handler for glut keyboard event.
// Since a keyboard stroke has no function in our glut window
// briefly 'blink' the screen to gently indicate that focus should
// be put on the mchron command line terminal window.
//
void winGlutKeyboard(unsigned char key, int x, int y)
{
  int k,l;

  // Invert display in buffer
  for (k = 0; k < GLCD_XPIXELS; k++)
  {
    for (l = 0; l < GLCD_YPIXELS / 8; l++)
    {
      // Invert LCD byte
      lcdGlutImage[k][l] = ~(lcdGlutImage[k][l]);
    }
  }

  // Redraw buffer and flush it
  winGlutRender();
  glutSwapBuffers();

  // Wait 0.1 sec (this will lower the fps statistic)
  winGlutDelay(100);

  // And invert back to original
  for (k = 0; k < GLCD_XPIXELS; k++)
  {
    for (l = 0; l < GLCD_YPIXELS / 8; l++)
    {
      // Invert LCD byte back to original
      lcdGlutImage[k][l] = ~(lcdGlutImage[k][l]);
    }
  }

  // Redraw buffer and flush it
  winGlutRender();
  glutSwapBuffers();
  
  // Prevent useless reflush in main loop
  doWinGlutFlush = GLUT_FALSE;
}

//
// Function: winGlutMain
//
// Main function for glut thread
//
void *winGlutMain(void *ptr)
{
  unsigned char x,y;
  char *myArgv[1];
  int myArgc = 1;

  // Init our window image copy to blank screen
  for (x = 0; x < GLCD_XPIXELS; x++)
  {
    for (y = 0; y < GLCD_YPIXELS / 8; y++)
    {
      lcdGlutImage[x][y] = 0;
    }
  }

  // Init our glut environment
  myArgv[0] = (char *)ptr;
  glutInit(&myArgc, myArgv);
  glutInitDisplayMode(GLUT_DOUBLE);
  glutInitWindowSize(winGlutSizeX, winGlutSizeY);
  glutInitWindowPosition(winGlutPosX, winGlutPosY);
  glutCreateWindow((char *)ptr);
  glutDisplayFunc(winGlutRender);
  glutKeyboardFunc(winGlutKeyboard);
  glutCloseFunc(winGlutWinClose);

  // Statistics
  gettimeofday(&lcdGlutTimeStart, NULL);
  
  // Main glut process loop until we signal shutdown
  while (doWinGlutExit == GLUT_FALSE)
  {
    // Process glut system events.
    // Note: As winGlutRender() may get called in the glut loop
    // event we need to lock our statistics.
    pthread_mutex_lock(&winGlutStatsMutex);
    lcdGlutTicks++;
    glutMainLoopEvent();
    pthread_mutex_unlock(&winGlutStatsMutex);

    // Process our application message queue
    winGlutMsgQueueProcess();

    // Render in case anything has changed
    if (doWinGlutFlush == GLUT_TRUE)
    {
       winGlutRender();
       glutSwapBuffers();
       doWinGlutFlush = GLUT_FALSE;
    }

    // Go to sleep to achieve low CPU usage combined with a refresh
    // rate at max ~30 fps
    winGlutDelay(33);
  }

  return NULL;
}

//
// Function: winGlutMsgQueueAdd
//
// Add message to glut message queue
//
void winGlutMsgQueueAdd(int x, int y, unsigned char data, unsigned char cmd)
{
  void *mallocPtr;

  // Get exclusive access to the message queue
  pthread_mutex_lock(&winGlutQueueMutex);

  // Create new message and add it to the end of the queue
  mallocPtr = malloc(sizeof(winGlutMsg_t));
  if (winGlutQueueStart == NULL)
  {
    // Start of new queue
    winGlutQueueStart = (winGlutMsg_t *)mallocPtr;
    winGlutQueueEnd = winGlutQueueStart;    
  }
  else
  {
    // The queue is at least of size one: link last queue member to next
    winGlutQueueEnd->next = (winGlutMsg_t *)mallocPtr;
    winGlutQueueEnd = winGlutQueueEnd->next;    
  }

  // Fill in functional content of message
  winGlutQueueEnd->cmd = cmd;
  winGlutQueueEnd->data = data;
  winGlutQueueEnd->x = x;
  winGlutQueueEnd->y = y;
  winGlutQueueEnd->next = NULL;

  // Statistics
  pthread_mutex_lock(&winGlutStatsMutex);
  lcdGlutMsgSend++;
  pthread_mutex_unlock(&winGlutStatsMutex);

  // Release exclusive access to the message queue
  pthread_mutex_unlock(&winGlutQueueMutex);
}

//
// Function: winGlutMsgQueueProcess
//
// Process all messages in the glut message queue
//
void winGlutMsgQueueProcess(void)
{
  void *freePtr;
  winGlutMsg_t *glutMsg = winGlutQueueStart;
  unsigned char lcdByte;
  unsigned char msgByte;
  unsigned char pixel = 0;
  int queueLength = 0;

  // Get exclusive access to the message queue and statistics counters
  pthread_mutex_lock(&winGlutQueueMutex);
  pthread_mutex_lock(&winGlutStatsMutex); 

  // Statistics
  if (glutMsg != NULL)
  {
    lcdGlutQueueEvents++;
  }

  // Eat entire queue message by message
  while (glutMsg != NULL)
  {
    // Statistics
    queueLength++;
    lcdGlutMsgRcv++;

    // Process the glut command
    if (glutMsg->cmd == GLUT_CMD_BYTEDRAW)
    {
      // Draw monochron pixels in window
      lcdByte = lcdGlutImage[glutMsg->x][glutMsg->y];
      msgByte = glutMsg->data;

      // Statistics
      lcdGlutByteReq++;

      // Force redraw when new content differs from current content
      if (lcdByte != msgByte)
      {
        // Sync internal window image and force window buffer flush 
        lcdGlutImage[glutMsg->x][glutMsg->y] = msgByte;
        doWinGlutFlush = GLUT_TRUE;

        // Statistics
        lcdGlutByteCnf++;
        lcdGlutBitReq = lcdGlutBitReq + 8;
        for (pixel = 0; pixel < 8; pixel++)
        {
          if ((lcdByte & 0x1) != (msgByte & 0x1))
          {
            lcdGlutBitCnf++;
            if ((msgByte & 0x1) == 0x0)
              winPixMajority--;
            else
              winPixMajority++;
          }
          lcdByte = lcdByte >> 1;
          msgByte = msgByte >> 1;
        }
      }
    }
    else if (glutMsg->cmd == GLUT_CMD_BACKLIGHT)
    {
      // Set background brightness and force redraw
      winGlutBrightness = (float)1 / 22 * (6 + glutMsg->data);
      doWinGlutFlush = GLUT_TRUE;
    }
    else if (glutMsg->cmd == GLUT_CMD_EXIT)
    {
      // Signal to exit glut thread (when queue is processed)
      doWinGlutExit = GLUT_TRUE;
    }

    // Go to next queue member and release memory of current one
    freePtr = (void *)glutMsg;
    glutMsg = glutMsg->next;
    free(freePtr);
  }

  // Message queue is processed so let's cleanup
  winGlutQueueStart = NULL;
  winGlutQueueEnd = NULL;

  // Statistics
  if (queueLength > lcdGlutQueueMax)
    lcdGlutQueueMax = queueLength;

  // Release exclusive access to the statistics counters and message queue
  pthread_mutex_unlock(&winGlutStatsMutex);
  pthread_mutex_unlock(&winGlutQueueMutex);
}

//
// Function: winGlutRender
//
// Render a full redraw of glut window in alternating buffer
//
void winGlutRender(void)
{
  unsigned char x,y,lcdByte;
  unsigned char pixel, pixValDraw;
  unsigned char byteValIgnore;
  float posX, posY;
  float viewAspectRatio;
  float brightnessDraw;

  // Statistics
  lcdGlutRedraws++;

  // Clear window buffer
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  // We need to set the projection of our display to maintain the
  // glut LCD display aspect ratio of (almost) 2:1 
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

  if (winPixMajority < 0)
  {
    // Majority of pixels is black so configure to draw a minority
    // number of white pixels
    pixValDraw = 1;
    byteValIgnore = 0x0;
    brightnessDraw = winGlutBrightness;
  }
  else
  {
    // Majority of pixels is white so configure to draw a minority
    // number of black pixels. For this we start off with a white
    // Monochron display (using a single draw only!) and then draw
    // the minority number of black pixels.
    glBegin(GL_QUADS);
      glColor3f(winGlutBrightness, winGlutBrightness, winGlutBrightness);
      glVertex2f(-1 + GLUT_PIX_X_SIZE, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE, -1 + GLUT_PIX_Y_SIZE);
      glVertex2f( 1 - GLUT_PIX_X_SIZE,  1 - GLUT_PIX_Y_SIZE);
      glVertex2f(-1 + GLUT_PIX_X_SIZE,  1 - GLUT_PIX_Y_SIZE);
    glEnd();
    pixValDraw = 0;
    byteValIgnore = 0xff;
    brightnessDraw = 0;
  }

  // Draw display border in frame at 0.5 pixel from each border
  glBegin(GL_LINE_LOOP);
    glColor3f(GLUT_FRAME_BRIGHTNESS, GLUT_FRAME_BRIGHTNESS, GLUT_FRAME_BRIGHTNESS);
    glVertex2f(-1 + GLUT_PIX_X_SIZE / 2,-1 + GLUT_PIX_Y_SIZE / 2);
    glVertex2f(-1 + GLUT_PIX_X_SIZE / 2, 1 - GLUT_PIX_Y_SIZE / 2);
    glVertex2f( 1 - GLUT_PIX_X_SIZE / 2, 1 - GLUT_PIX_Y_SIZE / 2);
    glVertex2f( 1 - GLUT_PIX_X_SIZE / 2,-1 + GLUT_PIX_Y_SIZE / 2);
  glEnd();

  // The Monochron background and window frame are drawn and the
  // parameters for drawing either black or white pixels are set.
  // Now render the LCD pixels in our beautiful Monochron display
  // using the display image from our local LCD buffer.
  // Begin at left of x axis and work our way to the right.
  posX = -1L + GLUT_PIX_X_SIZE;
  for (x = 0; x < GLCD_XPIXELS; x++)
  {
    // Begin at the top of y axis and work our way down
    posY = 1L - GLUT_PIX_Y_SIZE;
    for (y = 0; y < GLCD_YPIXELS / 8; y++)
    {
      // Get LCD byte to process
      lcdByte = lcdGlutImage[x][y];

      if (lcdByte == byteValIgnore)
      {
        // This LCD byte does not contain any pixels to draw.
        // Shift y position for next 8 pixels.
        posY = posY - 8 * GLUT_PIX_Y_SIZE;
      }
      else
      {
        // Process each bit in LCD byte
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
          posY = posY - GLUT_PIX_Y_SIZE;
          // Shift to next pixel
          lcdByte = (lcdByte >> 1);
        }
      }
    }
    // Shift x position for next set of vertical pixels
    posX = posX + GLUT_PIX_X_SIZE;
  }

  // Force window buffer flush
  doWinGlutFlush = GLUT_TRUE;
}

//
// Function: lcdGlutStatsGet
//
// Get interface statistics
//
void lcdGlutStatsGet(lcdGlutStats_t *lcdGlutStats)
{
  // As this is a multi-threaded interface we need to have exclusive
  // access to the counters
  pthread_mutex_lock(&winGlutStatsMutex);
  lcdGlutStats->lcdGlutMsgSend = lcdGlutMsgSend;
  lcdGlutStats->lcdGlutMsgRcv = lcdGlutMsgRcv;
  lcdGlutStats->lcdGlutBitReq = lcdGlutBitReq;
  lcdGlutStats->lcdGlutBitCnf = lcdGlutBitCnf;
  lcdGlutStats->lcdGlutByteReq = lcdGlutByteReq;
  lcdGlutStats->lcdGlutByteCnf = lcdGlutByteCnf;
  lcdGlutStats->lcdGlutRedraws = lcdGlutRedraws;
  lcdGlutStats->lcdGlutQueueMax = lcdGlutQueueMax;
  lcdGlutStats->lcdGlutQueueEvents = lcdGlutQueueEvents;
  lcdGlutStats->lcdGlutTimeStart = lcdGlutTimeStart;
  lcdGlutStats->lcdGlutTicks = lcdGlutTicks;
  pthread_mutex_unlock(&winGlutStatsMutex);
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
  pthread_mutex_lock(&winGlutStatsMutex);
  lcdGlutMsgSend = 0;
  lcdGlutMsgRcv = 0;
  lcdGlutBitReq = 0;
  lcdGlutBitCnf = 0;
  lcdGlutByteReq = 0;
  lcdGlutByteCnf = 0;
  lcdGlutRedraws = 0;
  lcdGlutQueueMax = 0;
  lcdGlutQueueEvents = 0;
  (void) gettimeofday(&lcdGlutTimeStart, NULL);
  lcdGlutTicks = 0;
  pthread_mutex_unlock(&winGlutStatsMutex);
}

