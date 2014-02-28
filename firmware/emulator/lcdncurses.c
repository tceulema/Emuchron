//*****************************************************************************
// Filename : 'lcdncurses.c'
// Title    : LCD ncurses stub functionality for MONOCHRON Emulator
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ncurses.h>
#include "lcdncurses.h"

// This is ugly:
// Defs TRUE and FALSE are defined in both "avrlibtypes.h" (loaded via "ks0108.h")
// and <ncurses.h> and... they have different values for TRUE.
// To make sure that we build our ncurses LCD stub using the proper defs we
// must build our ncurses interface independent from the avr environment.
// And because of this we have to duplicate common stuff defines like LCD panel
// pixel sizes in here.
// This also means that we can 'communicate' with the outside world using only
// common data types such as int, char, etc and whatever we define locally and
// expose via our header.
#define GLCD_XPIXELS	128
#define GLCD_YPIXELS	64
#define NCUR_FALSE	0
#define NCUR_TRUE	1

// Fixed ncurses xterm geometry requirements
#define NCUR_X_PIXELS 	258
#define NCUR_Y_PIXELS 	66

// A private copy of the window image to optimize ncurses window updates 
unsigned char lcdNcurImage[GLCD_XPIXELS][GLCD_YPIXELS / 8];

// A copy of the init parameters.
char winNcurTty[50];            // The ncurses tty
void (*winNcurWinClose)(void);  // The mchron callback when end user closes ncurses window

// Data needed for ncurses stub LCD device
FILE *winNcurFile;
SCREEN *winNcurScreen;
WINDOW *winNcurWindow;
unsigned char doWinNcurFlush = NCUR_FALSE;
unsigned char winNcurReverse = NCUR_FALSE;

// Definition of an LCD pixel in ncurses
char winNcurPixel[] = "  ";

// Statistics on the ncurses interface
long long lcdNcurBitReq = 0;    // Nbr of LCD bits processed (from bytes with ncur update)
long long lcdNcurBitCnf = 0;    // Nbr of LCD bits leading to ncurses update
long long lcdNcurByteReq = 0;   // Nbr of LCD bytes processed
long long lcdNcurByteCnf = 0;   // Nbr of LCD bytes leading to ncurses update

//
// Function: lcdNcurBacklightSet
//
// Set backlight in LCD display in ncurses window (dummy)
//
void lcdNcurBacklightSet(unsigned char backlight)
{
  return;
}

//
// Function: lcdNcurDataWrite
//
// Set content of LCD display in ncurses window
//
void lcdNcurDataWrite(unsigned char x, unsigned char y, unsigned char data)
{
  int posX = x * 2 + 1;
  int posY = y * 8 + 1;
  unsigned char pixel = 0;
  unsigned char lcdByte = lcdNcurImage[x][y];

  // Statistics
  lcdNcurByteReq++;

  // Processing needed only when current content differs from new content
  if (lcdByte != data)
  {
    // Statistics
    lcdNcurBitReq = lcdNcurBitReq + 8;
    lcdNcurByteCnf++;

    // Sync internal window image and force window buffer flush 
    lcdNcurImage[x][y] = data;
    doWinNcurFlush = NCUR_TRUE;

    // Process each individual bit of byte in display
    for (pixel = 0; pixel < 8; pixel++)
    {
      // Check if pixel has changed
      if ((lcdByte & 0x1) != (data & 0x1))
      {
        // Statistics
        lcdNcurBitCnf++;

        // Switch between normal and reverse state when needed
        if ((data & 0x1) == 1)
        {
          if (winNcurReverse == NCUR_FALSE)
          {
            // Switch to reverse state
            wattron(winNcurWindow, A_REVERSE);
            winNcurReverse = NCUR_TRUE;
          }
        }
        else
        {
          if (winNcurReverse == NCUR_TRUE)
          {
            // Switch to normal state
            wattroff(winNcurWindow, A_REVERSE);
            winNcurReverse = NCUR_FALSE;
          }
        }

        // Draw pixel in ncurses device
        mvwprintw(winNcurWindow, posY, posX, winNcurPixel);
      }

      // Shift to next pixel
      lcdByte = (lcdByte >> 1);
      data = (data >> 1);
      posY++;
    }
  }
}

//
// Function: lcdNcurEnd
//
// Shut down the LCD display in ncurses window
//
void lcdNcurEnd(void)
{
  // Switch back to normal state
  if (winNcurReverse == NCUR_TRUE)
  {
    wattroff(winNcurWindow, A_REVERSE);
    winNcurReverse = NCUR_FALSE;
  }

  // Set ncurses tty to wait for Enter key and echo characters
  nocbreak();
  echo();

  // End the ncurses session
  endwin();
}

//
// Function: lcdNcurFlush
//
// Flush the LCD display in ncurses window
//
void lcdNcurFlush(int force)
{
  // Dump only when activity has been signalled since last refresh
  // or when a force request is issued
  if (doWinNcurFlush == NCUR_TRUE || force == 1)
  {
    struct stat buffer;   

    // Verify that the tty is actually still in use
    if (stat(winNcurTty, &buffer) != 0)
    {
      // The ncurses tty is gone so we'll force mchron to exit
      winNcurWinClose();
    }

    // When a forced flush is requested attempt to restore the
    // ncurses window
    if (force == 1)
    {
      lcdNcurRestore();
    }

    // Do the actual ncurses window refresh
    wrefresh(winNcurWindow);
    doWinNcurFlush = NCUR_FALSE;
  }
}

//
// Function: lcdNcurInit
//
// Initialize the LCD display in ncurses window
//
int lcdNcurInit(char *lcdNcurTty, void (*lcdNcurWinClose)(void))
{
  unsigned char x,y;
  int row, col;
  int retVal = 0;

  // Init our window image copy to blank screen
  for (x = 0; x < GLCD_XPIXELS; x++)
  {
    for (y = 0; y < GLCD_YPIXELS / 8; y++)
    {
      lcdNcurImage[x][y] = 0;
    }
  }

  // Do ncursus init stuff to attach to output tty and create ncurses window
  strcpy(winNcurTty, lcdNcurTty);
  winNcurWinClose = lcdNcurWinClose;
  
  // Open destination tty
  winNcurFile = fopen(winNcurTty, "r+");

  // Assign this tty file to ncurses
  winNcurScreen = newterm("xterm", winNcurFile, winNcurFile);

  // Try to set the size of the xterm tty. We do this because for some
  // reason gdb makes ncurses use the size of the mchron terminal window
  // as the actual size, which is pretty weird.   
  resizeterm(NCUR_Y_PIXELS, NCUR_X_PIXELS);

  // Set ncurses tty to not wait for Enter key and not echo characters
  cbreak();
  noecho();

  // Hide the cursor in the tty
  curs_set(0);

  // Create the Monochron ncurses window
  winNcurWindow = newwin(NCUR_Y_PIXELS, NCUR_X_PIXELS, 0, 0);

  // Get actual winsize of ncurses tty.
  // Note: This code doesn't work as getmaxyx() always returns the initial
  // screensize and not the latest.
  getmaxyx(stdscr, row, col);
  if (row < NCUR_Y_PIXELS || col < NCUR_X_PIXELS)
  {
    printf("\nWARNING: Destination tty for ncurses too small: %dx%d\n",
      col, row);
    printf("Note: The destination curses xterm size should be at least %dx%d\n",
      NCUR_X_PIXELS, NCUR_Y_PIXELS);
    printf("or else you'll get partial cut-off ncurses LCD screen output.\n");
    retVal = -1;
  }

  // Draw a box in the window
  lcdNcurRestore();

  return retVal;
}

//
// Function: lcdNcurRestore
//
// Restore layout of the LCD display in ncurses window
//
void lcdNcurRestore(void)
{
  resizeterm(NCUR_Y_PIXELS, NCUR_X_PIXELS);
  box(winNcurWindow, 0 , 0);
}

//
// Function: lcdNcurStatsGet
//
// Get interface statistics
//
void lcdNcurStatsGet(lcdNcurStats_t *lcdNcurStats)
{
  lcdNcurStats->lcdNcurBitReq = lcdNcurBitReq;
  lcdNcurStats->lcdNcurBitCnf = lcdNcurBitCnf;
  lcdNcurStats->lcdNcurByteReq = lcdNcurByteReq;
  lcdNcurStats->lcdNcurByteCnf = lcdNcurByteCnf;
}

//
// Function: lcdNcurStatsReset
//
// Reset interface statistics
//
void lcdNcurStatsReset(void)
{
  lcdNcurBitReq = 0;
  lcdNcurBitCnf = 0;
  lcdNcurByteReq = 0;
  lcdNcurByteCnf = 0;
}

