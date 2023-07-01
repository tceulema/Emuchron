//*****************************************************************************
// Filename : 'lcdncurses.c'
// Title    : Lcd ncurses stub functionality for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <ncurses.h>
#include "lcdncurses.h"

// This is ugly:
// Why can't we include the GLCD_*/MC_* defs below from ks0108conf.h, glcd.h
// and global.h?
// Defs TRUE and FALSE are defined in both "avrlibtypes.h" (loaded via
// global.h) and <ncurses.h> and... they have different values for TRUE.
// To make sure that we build our ncurses lcd stub using the proper defs we
// must build our ncurses interface independent from the avr environment.
// And because of this we have to duplicate common glcd defines like lcd panel
// pixel sizes and colors in here.
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
#define GLCD_CONTROLLER_XPIXMASK 0x3f
#define GLCD_OFF		0
#define GLCD_ON			1
#define MC_FALSE		0
#define MC_TRUE			1

// Fixed ncurses xterm geometry requirements
#define NCUR_XY_BORDERSIZE	1	// Draw area border is one term char
#define NCUR_X_PIXSIZE		2	// A glcd x pixel is two terminal chars
#define NCUR_Y_PIXSIZE		1	// A glcd y pixel is one terminal char

// Terminal size requirements for our Monochron display, being 258x66 chars
#define NCUR_X_PIXELS \
  (GLCD_NUM_CONTROLLERS * GLCD_CONTROLLER_XPIXELS * NCUR_X_PIXSIZE + \
  NCUR_XY_BORDERSIZE * 2)
#define NCUR_Y_PIXELS \
  (GLCD_CONTROLLER_YPIXELS * NCUR_Y_PIXSIZE + NCUR_XY_BORDERSIZE * 2)

// The ncurses color index used for controllers and border windows background
// and foreground colors
#define NCUR_COLOR_WIN		128
#define NCUR_COLOR_BORDER	129

// Get ncurses brightness greyscale based on display backlight level 0..16
#define NCUR_BRIGHTNESS(level)	(int)(1000 * (6 + (float)(level)) / 22)

// The string to be used to obtain a single square ncurses lcd pixel
#define NCUR_PIXEL		"  "

// This is me
extern const char *__progname;

// Definition of a structure holding ncurses lcd device statistics
typedef struct _lcdNcurStats_t
{
  long long bitCnf;		// Lcd bits leading to ncurses update
  long long byteReq;		// Lcd bytes processed
} lcdNcurStats_t;

// Definition of a structure to hold ncurses window related data
typedef struct _lcdNcurCtrl_t
{
  WINDOW *winCtrl;		// The controller ncurses window
  unsigned char display;	// Indicates if controller display is active
  unsigned char startLine;	// Controller data display line offset
  unsigned char color;		// Indicates the current draw color
  unsigned char flush;		// Indicates the flush state
} lcdNcurCtrl_t;

// The ncurses controller windows data
static lcdNcurCtrl_t lcdNcurCtrl[GLCD_NUM_CONTROLLERS];

// A private copy of the window image from which we draw our ncurses window
static unsigned char lcdNcurImage[GLCD_XPIXELS][GLCD_YPIXELS / 8];

// The init parameters
static lcdNcurInitArgs_t lcdNcurInitArgs;
static unsigned char deviceActive = MC_FALSE;

// Data needed for ncurses stub lcd device
static FILE *ttyFile = NULL;
static SCREEN *ttyScreen;
static WINDOW *winBorder;

// Backlight support (init=yes) and brightness (init=full brightness)
static unsigned char lcdUseBacklight = MC_TRUE;
static unsigned char lcdBacklight = 16;

// Statistics counters on ncurses
static lcdNcurStats_t lcdNcurStats;

// Timestamps used to verify existence of ncurses tty
static struct timeval tvNow;
static struct timeval tvThen;

// Local function prototypes
static void lcdNcurClose(void);
static void lcdNcurDrawModeSet(unsigned char controller, unsigned char color);
static void lcdNcurRedraw(unsigned char controller, int startY, int rows);

//
// Function: lcdNcurBacklightSet
//
// Set backlight brightness
//
void lcdNcurBacklightSet(unsigned char backlight)
{
  int brightness;
  unsigned char i;

  // No need to update when backlight remains unchanged
  if (lcdBacklight == backlight)
    return;

  // Sync backlight
  lcdBacklight = backlight;

  // See if update controller windows is required
  if (lcdUseBacklight == MC_FALSE)
    return;

  // Set new brightness in controller windows and flag update
  brightness = NCUR_BRIGHTNESS(backlight);
  init_color(NCUR_COLOR_WIN, brightness, brightness, brightness);
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    lcdNcurCtrl[i].flush = MC_TRUE;
}

//
// Function: lcdNcurCleanup
//
// Shut down the lcd display in ncurses window
//
void lcdNcurCleanup(void)
{
  // Nothing to do if the ncurses environment is not initialized
  if (deviceActive == MC_FALSE)
    return;

  // Reset settings applied during init of ncurses display
  nocbreak();
  echo();
  curs_set(1);

  // End the ncurses session
  endwin();

  // Unlink ncurses from tty
  delscreen(ttyScreen);
  fclose(ttyFile);
  ttyFile = NULL;
  deviceActive = MC_FALSE;
}

//
// Function: lcdNcurClose
//
// Callback for ncurses window close event
//
static void lcdNcurClose(void)
{
  // Attempt to shutdown gracefully
  lcdNcurInitArgs.winClose();
}

//
// Function: lcdNcurDataWrite
//
// Set content of lcd display in ncurses window. The controller has decided
// that the new data differs from the current lcd data.
//
void lcdNcurDataWrite(unsigned char x, unsigned char y, unsigned char data)
{
  unsigned char pixel = 0;
  unsigned char controller = x >> GLCD_CONTROLLER_XPIXBITS;
  int posX = (x & GLCD_CONTROLLER_XPIXMASK) * 2;
  int posY = y * 8;
  unsigned char lcdByte = lcdNcurImage[x][y];

  // Sync y with startline
  if (posY >= lcdNcurCtrl[controller].startLine)
    posY = posY - lcdNcurCtrl[controller].startLine;
  else
    posY = GLCD_YPIXELS - lcdNcurCtrl[controller].startLine + posY;

  // Statistics
  lcdNcurStats.byteReq++;

  // Sync internal window image and force window buffer flush
  lcdNcurImage[x][y] = data;
  lcdNcurCtrl[controller].flush = MC_TRUE;

  // Process each individual bit of byte in display
  for (pixel = 0; pixel < 8; pixel++)
  {
    // Check if pixel has changed
    if ((lcdByte & 0x1) != (data & 0x1))
    {
      // Statistics
      lcdNcurStats.bitCnf++;

      // Only draw when the controller display is on
      if (lcdNcurCtrl[controller].display == MC_TRUE)
      {
        // Switch between draw color and draw pixel
        lcdNcurDrawModeSet(controller, data & GLCD_ON);
        mvwprintw(lcdNcurCtrl[controller].winCtrl, posY, posX, NCUR_PIXEL);
      }
    }

    // Shift to next pixel
    lcdByte = (lcdByte >> 1);
    data = (data >> 1);
    posY++;
    if (posY >= GLCD_YPIXELS)
      posY = posY - GLCD_YPIXELS;
  }
}

//
// Function: lcdNcurDisplaySet
//
// Switch controller display off or on
//
void lcdNcurDisplaySet(unsigned char controller, unsigned char display)
{
  // Sync display state and update window
  if (lcdNcurCtrl[controller].display != display)
  {
    lcdNcurCtrl[controller].display = display;
    if (display == 0)
    {
      // Clear out the controller window
      werase(lcdNcurCtrl[controller].winCtrl);
      lcdNcurCtrl[controller].flush = MC_TRUE;
    }
    else
    {
      // Repaint the entire controller window
      lcdNcurRedraw(controller, 0, GLCD_YPIXELS);
    }
  }
}

//
// Function: lcdNcurDrawModeSet
//
// Set the ncurses draw color
//
static void lcdNcurDrawModeSet(unsigned char controller, unsigned char color)
{
  // Switch between draw colors when needed
  if (color == GLCD_ON)
  {
    if (lcdNcurCtrl[controller].color == GLCD_OFF)
    {
      // Draw white pixels
      wattron(lcdNcurCtrl[controller].winCtrl, A_REVERSE);
      lcdNcurCtrl[controller].color = GLCD_ON;
    }
  }
  else
  {
    if (lcdNcurCtrl[controller].color == GLCD_ON)
    {
      // Draw black pixels
      wattroff(lcdNcurCtrl[controller].winCtrl, A_REVERSE);
      lcdNcurCtrl[controller].color = GLCD_OFF;
    }
  }
}

//
// Function: lcdNcurFlush
//
// Flush the lcd display in ncurses window
//
void lcdNcurFlush(void)
{
  unsigned char i;
  unsigned char refreshDone = MC_FALSE;
  struct stat buffer;

  // Check the ncurses tty if the previous check was in a preceding second
  gettimeofday(&tvNow, NULL);
  if (tvNow.tv_sec > tvThen.tv_sec)
  {
    // Sync check time and check ncurses tty
    tvThen = tvNow;
    if (stat(lcdNcurInitArgs.tty, &buffer) != 0)
    {
      // The ncurses tty is gone so we'll force mchron to exit
      lcdNcurClose();
    }
  }

  // Dump only when activity has been signalled since last refresh
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    if (lcdNcurCtrl[i].flush == MC_TRUE)
    {
      // Flush changes in ncurses window but do not redraw yet
      refreshDone = MC_TRUE;
      wnoutrefresh(lcdNcurCtrl[i].winCtrl);
      lcdNcurCtrl[i].flush = MC_FALSE;
    }
  }

  // Do the actual redraw
  if (refreshDone == MC_TRUE)
    doupdate();
}

//
// Function: lcdNcurGraphicsSet
//
// Enable/disable variable backlight
//
void lcdNcurGraphicsSet(unsigned char useBacklight)
{
  unsigned char refresh = MC_FALSE;
  int brightness;
  unsigned char i;

  // No need to update when brightness support is unchanged
  if (lcdUseBacklight == useBacklight)
    return;

  // Sync brightness support
  lcdUseBacklight = useBacklight;

  // Depending on support value set new controller window brightness
  if (useBacklight == MC_FALSE && lcdBacklight != 16)
  {
    // When unsupported fall back to full brightness
    brightness = NCUR_BRIGHTNESS(16);
    refresh = MC_TRUE;
  }
  else if (useBacklight == MC_TRUE && lcdBacklight != 16)
  {
    // When supported use the current backlight
    brightness = NCUR_BRIGHTNESS(lcdBacklight);
    refresh = MC_TRUE;
  }

  // Set new brightness in controller windows and flag update
  if (refresh == MC_TRUE)
  {
    init_color(NCUR_COLOR_WIN, brightness, brightness, brightness);
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
      lcdNcurCtrl[i].flush = MC_TRUE;
  }
}

//
// Function: lcdNcurInit
//
// Initialize the lcd display in ncurses window
//
unsigned char lcdNcurInit(lcdNcurInitArgs_t *lcdNcurInitArgsSet)
{
  int brightWin;
  int brightBorder;
  unsigned char i;
  FILE *fp;
  struct winsize sizeTty;
  struct stat statTty;

  // Nothing to do if the ncurses environment is already initialized
  if (deviceActive == MC_TRUE)
    return MC_TRUE;

  // Reset the ncurses statistics
  lcdNcurStatsReset();

  // Copy ncursus init parameters
  lcdNcurInitArgs = *lcdNcurInitArgsSet;

  // Check if the ncurses tty is actually in use
  if (stat(lcdNcurInitArgs.tty, &statTty) != 0)
  {
    printf("%s: -t: destination ncurses tty \"%s\" is not in use\n",
      __progname, lcdNcurInitArgs.tty);
    return MC_FALSE;
  }

  // Open tty and check if it has a minimum size
  fp = fopen(lcdNcurInitArgs.tty, "r");
  if (fp == (FILE *)NULL)
  {
    printf("%s: -t: cannot open destination ncurses tty \"%s\"\n", __progname,
      lcdNcurInitArgs.tty);
    return MC_FALSE;
  }
  if (ioctl(fileno(fp), TIOCGWINSZ, (char *)&sizeTty) >= 0)
  {
    if (sizeTty.ws_col < NCUR_X_PIXELS || sizeTty.ws_row < NCUR_Y_PIXELS)
    {
      printf("%s: -t: destination ncurses tty \"%s\" size (%dx%d) is too\n",
        __progname, lcdNcurInitArgs.tty, sizeTty.ws_col, sizeTty.ws_row);
      printf("small for use as monochron ncurses terminal (min = %dx%d chars)\n",
        NCUR_X_PIXELS, NCUR_Y_PIXELS);
      fclose(fp);
      return MC_FALSE;
    }
  }
  fclose(fp);

  // Init our window lcd image copy to blank
  memset(lcdNcurImage, 0, sizeof(lcdNcurImage));

  // Open destination tty, assign to ncurses screen and allow using colors
  // by forcing a 256 color profile
  ttyFile = fopen(lcdNcurInitArgs.tty, "r+");
  ttyScreen = newterm("xterm-256color", ttyFile, ttyFile);
  start_color();

  // Try to set the size of the xterm tty. We do this because for some
  // reason gdb makes ncurses use the size of the mchron terminal window
  // as the actual size, which is pretty weird.
  resize_term(NCUR_Y_PIXELS, NCUR_X_PIXELS);

  // Set ncurses tty to not wait for Enter key and not echo characters
  cbreak();
  noecho();

  // Hide the cursor in the tty
  curs_set(0);

  // Create the outer border window and the controller section windows
  winBorder = newwin(NCUR_Y_PIXELS, NCUR_X_PIXELS, 0, 0);
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    lcdNcurCtrl[i].winCtrl = newwin(GLCD_CONTROLLER_YPIXELS * NCUR_Y_PIXSIZE,
      GLCD_CONTROLLER_XPIXELS * NCUR_X_PIXSIZE, 1,
      1 + i * GLCD_CONTROLLER_XPIXELS * NCUR_X_PIXSIZE);
    lcdNcurCtrl[i].display = MC_FALSE;
    lcdNcurCtrl[i].startLine = 0;
    lcdNcurCtrl[i].color = GLCD_OFF;
    lcdNcurCtrl[i].flush = MC_FALSE;
  }

  // Define black background and greyscale foreground colors and link them in
  // color pairs. Upon changing the lcd backlight, NCUR_COLOR_WIN will be
  // redefined into a new greyscale.
  // When changing a color in a color pair that is used in a window, ncurses
  // will redraw the affected window contents upon refreshing, so we don't need
  // to do this ourselves (which is very nice).
  brightWin = NCUR_BRIGHTNESS(16);
  brightBorder = NCUR_BRIGHTNESS(6);
  init_color(COLOR_BLACK, 0, 0, 0);
  init_color(NCUR_COLOR_WIN, brightWin, brightWin, brightWin);
  init_color(NCUR_COLOR_BORDER, brightBorder, brightBorder, brightBorder);
  init_pair(1, NCUR_COLOR_WIN, COLOR_BLACK);
  init_pair(2, NCUR_COLOR_BORDER, COLOR_BLACK);

  // Assign color pair to controllers and border windows
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    wattron(lcdNcurCtrl[i].winCtrl, COLOR_PAIR(1));
  wattron(winBorder, COLOR_PAIR(2));

  // Draw and show a box in the outer border window
  box(winBorder, 0 , 0);
  wrefresh(winBorder);

  // Set time reference for checking ncurses tty
  gettimeofday(&tvThen, NULL);

  // We're initialized
  deviceActive = MC_TRUE;

  return MC_TRUE;
}

//
// Function: lcdNcurRedraw
//
// Redraw (a part of) the lcd display in ncurses window
//
static void lcdNcurRedraw(unsigned char controller, int startY, int rows)
{
  unsigned char x,y;
  unsigned char rowsToDo;
  unsigned char bitsToDo;
  unsigned char lcdByte;
  unsigned char lcdLine;
  int posY;

  // Redraw all x columns for (a limited set of) rows. We either start at line
  // 0 for a number of rows, or start at a non-0 line and draw all up to the
  // the end of the window. We only need to draw the white (or grey-tone)
  // pixels.
  lcdNcurDrawModeSet(controller, GLCD_ON);
  for (x = 0; x < GLCD_CONTROLLER_XPIXELS; x++)
  {
    rowsToDo = rows;
    posY = startY;
    lcdLine = (startY + lcdNcurCtrl[controller].startLine) %
      GLCD_CONTROLLER_YPIXELS;

    // Draw pixels in pixel byte
    for (y = (lcdLine >> 3); rowsToDo > 0; y = (y + 1) % 8)
    {
      // Get pixel byte and shift to the first pixel to be drawn
      lcdByte = lcdNcurImage[x + controller * GLCD_CONTROLLER_XPIXELS][y];
      lcdByte = (lcdByte >> (lcdLine & 0x7));

      // Draw all applicable pixel bit in byte
      for (bitsToDo = 8 - (lcdLine & 0x7); bitsToDo > 0 && rowsToDo > 0;
          bitsToDo--)
      {
        // Only draw when needed
        if ((lcdByte & 0x1) == GLCD_ON)
          mvwprintw(lcdNcurCtrl[controller].winCtrl, posY, x * 2, NCUR_PIXEL);

        // Shift to next pixel in byte and line in display
        lcdByte = (lcdByte >> 1);
        lcdLine++;
        posY++;
        rowsToDo--;
      }
    }
  }

  // Signal redraw
  lcdNcurCtrl[controller].flush = MC_TRUE;
}

//
// Function: lcdNcurStartLineSet
//
// Set controller display line offset
//
void lcdNcurStartLineSet(unsigned char controller, unsigned char startLine)
{
  int scroll;
  int fillStart;
  int rows;

  // If the display is off or when there's no change in the startline there's
  // no reason to redraw so only sync new value
  if (lcdNcurCtrl[controller].display == MC_FALSE ||
      lcdNcurCtrl[controller].startLine == startLine)
  {
    lcdNcurCtrl[controller].startLine = startLine;
    return;
  }

  // Determine the parameters that minimize ncurses scroll and redraw efforts
  if (lcdNcurCtrl[controller].startLine > startLine)
  {
    if (lcdNcurCtrl[controller].startLine - startLine < GLCD_CONTROLLER_YPIXELS / 2)
    {
      scroll = (int)startLine - (int)lcdNcurCtrl[controller].startLine;
      fillStart = 0;
      rows = -scroll;
    }
    else
    {
      scroll = (int)startLine - (int)lcdNcurCtrl[controller].startLine +
        GLCD_CONTROLLER_YPIXELS;
      fillStart = GLCD_YPIXELS - scroll;
      rows = scroll;
    }
  }
  else
  {
    if (startLine - lcdNcurCtrl[controller].startLine < GLCD_CONTROLLER_YPIXELS / 2)
    {
      scroll = (int)startLine - (int)lcdNcurCtrl[controller].startLine;
      fillStart = GLCD_CONTROLLER_YPIXELS - scroll;
      rows = scroll;
    }
    else
    {
      scroll = (int)startLine - (int)lcdNcurCtrl[controller].startLine -
        GLCD_CONTROLLER_YPIXELS;
      fillStart = 0;
      rows = -scroll;
    }
  }

  // Set new startline
  lcdNcurCtrl[controller].startLine = startLine;

  // Scroll the window up or down and fill-up emptied scroll space.
  // To avoid nasty scroll side effects that may result in scroll remnants at
  // the bottom of the window allow scrolling only during the actual scroll.
  scrollok(lcdNcurCtrl[controller].winCtrl, TRUE);
  wscrl(lcdNcurCtrl[controller].winCtrl, scroll);
  scrollok(lcdNcurCtrl[controller].winCtrl, FALSE);
  lcdNcurRedraw(controller, fillStart, rows);
}

//
// Function: lcdNcurStatsPrint
//
// Print interface statistics
//
void lcdNcurStatsPrint(void)
{
  printf("ncurses: lcdByteRx=%llu, ", lcdNcurStats.byteReq);
  if (lcdNcurStats.byteReq == 0)
    printf("bitEff=-%%\n");
  else
    printf("bitEff=%.0f%%\n",
      lcdNcurStats.bitCnf * 100 / ((double)lcdNcurStats.byteReq * 8));
}

//
// Function: lcdNcurStatsReset
//
// Reset interface statistics
//
void lcdNcurStatsReset(void)
{
  memset(&lcdNcurStats, 0, sizeof(lcdNcurStats_t));
}
