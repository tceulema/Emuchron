//*****************************************************************************
// Filename : 'lcdncurses.c'
// Title    : ncurses lcd stub and monitor functionality for emuchron emulator
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
#include <pthread.h>
#include "../global.h"
#include "../glcd.h"
#include "../ks0108conf.h"
#include "../monomain.h"
#include "controller.h"
#include "lcdncurses.h"

// Fixed ncurses lcd xterm geometry requirements
#define LCD_XY_BORDERSIZE	1	// Draw area border is one term char
#define LCD_X_PIXSIZE		2	// A glcd x pixel is two terminal chars
#define LCD_Y_PIXSIZE		1	// A glcd y pixel is one terminal char

// Lcd terminal size requirements for our Monochron display, being 258x66 chars
#define LCD_X_PIXELS \
  (GLCD_NUM_CONTROLLERS * GLCD_CONTROLLER_XPIXELS * LCD_X_PIXSIZE + \
  LCD_XY_BORDERSIZE * 2)
#define LCD_Y_PIXELS \
  (GLCD_CONTROLLER_YPIXELS * LCD_Y_PIXSIZE + LCD_XY_BORDERSIZE * 2)

// The lcd ncurses color index used for controllers and border windows
// background and foreground colors
#define LCD_COLOR_WIN		128
#define LCD_COLOR_BORDER	129

// Get lcd ncurses brightness greyscale based on display backlight level 0..16
#define LCD_BRIGHTNESS(level)	\
  (int)(1000 * (6 + (float)(level)) / (OCR2A_MAX + 6))

// The string to be used to obtain a single square ncurses lcd pixel
#define LCD_PIXEL		"  "

// The ncurses monitor terminal display size, being 124x24
#define MON_X_SIZE		124
#define MON_Y_SIZE		24

// The ncursus monitor info sections x/y location
#define MON_TD_X		2	// Date/time/alarm
#define MON_TD_Y		2
#define MON_M1_X		2	// Monochron vars 1
#define MON_M1_Y		6
#define MON_M2_X		26	// Monochron vars 2
#define MON_M2_Y		6
#define MON_LC_X		26	// Lcd display
#define MON_LC_Y		2
#define MON_ST_X		48	// Runtime stub
#define MON_ST_Y		2
#define MON_GL_X		48	// Runtime glcd
#define MON_GL_Y		10
#define MON_C0_X		48	// Runtime controller 0
#define MON_C0_Y		16
#define MON_C1_X		75	// Runtime controller 1
#define MON_C1_Y		16
#define MON_CR_X		75	// Controller 0/1 registers
#define MON_CR_Y		2
#define MON_EE_X		102	// Eeprom
#define MON_EE_Y		2

// The ncurses monitor info thread main loop sleep duration (msec)
#define MON_THREAD_SLEEP_MS	65

// This is me
extern const char *__progname;

// External data we need for the Emuchron monitor
extern volatile rtcDateTime_t rtcDateTime;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcAlarmSwitch, mcAlarming, mcSnoozing;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint16_t mcTickerAlarm, mcTickerSnooze;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcClockInit;
extern volatile uint8_t mcMchronClock;
extern volatile uint8_t mcFgColor, mcBgColor;
extern volatile uint8_t mcU8Util1, mcU8Util2, mcU8Util3, mcU8Util4;
extern volatile uint16_t mcU16Util1, mcU16Util2, mcU16Util3, mcU16Util4;

// The eeprom dictionary used for printing it in the monitor
extern eepDict_t eepDict[];
extern int eepDictCount;

// Definition of a structure holding ncurses lcd device statistics
typedef struct _lcdNcurStats_t
{
  long long bitCnf;		// Lcd bits leading to ncurses update
  long long byteReq;		// Lcd bytes processed
} lcdNcurStats_t;

// Definition of a structure to hold ncurses controller window related data
typedef struct _lcdNcurCtrl_t
{
  WINDOW *winLcdCtrl;		// The controller ncurses window
  u08 display;			// Indicates if controller display is active
  u08 startLine;		// Controller data display line offset
  u08 color;			// Indicates the current draw color
  u08 flush;			// Indicates the flush state
} lcdNcurCtrl_t;

// Definition of a structure holding the stubbed controller hardware registers
// for use in the Emuchron monitor. It is essentially a copy of ctrlRegister_t
// [emulator/controller.h], but declares its values as int for uniform monitor
// value processing purposes.
typedef struct _monCtrlReg_t
{
  int display;			// Display on/off switch (0=off, 1=on)
  int x;			// Cursor x pos (0..63)
  int y;			// Cursor y page (0..7)
  int startLine;		// Vertical display start line (0..63)
  int dataRead;			// Last data read from the lcd
  int dataWrite;		// Last data written to the lcd
} monCtrlReg_t;

// Definition of a structure to hold the current Emuchron monitor window values
typedef struct _monInfo_t
{
  uint8_t monClockData[8];
  int monAlarmH1, monAlarmM1;
  int monAlarmH2, monAlarmM2;
  int monAlarmSwitch1, monAlarmSwitch2;
  int monMchronClock;
  int monCycleCounter;
  int monAlarming, monSnoozing;
  int monTickerAlarm, monTickerSnooze;
  int monFgColor, monBgColor;
  int monClockOldTS, monClockOldTM, monClockOldTH;
  int monClockNewTS, monClockNewTM, monClockNewTH;
  int monClockOldDD, monClockOldDM, monClockOldDY;
  int monClockNewDD, monClockNewDM, monClockNewDY;
  int monU8Util1, monU8Util2, monU8Util3, monU8Util4;
  int monU16Util1, monU16Util2, monU16Util3, monU16Util4;
  int monCycle;
  stubStats_t monStubStats;
  ctrlGlcdStats_t monCtrlGlcd;
  int monBrightness;
  int monCtrlActive;
  int monCtrlState0, monCtrlState1;
  monCtrlReg_t monCtrlReg0, monCtrlReg1;
  ctrlStats_t monCtrlStats0, monCtrlStats1;
  int *monEeprom;
} monInfo_t;

// The ncurses controller windows data
static lcdNcurCtrl_t lcdNcurCtrl[GLCD_NUM_CONTROLLERS];

// A private copy of the window image from which we draw our ncurses window
static u08 lcdNcurImage[GLCD_XPIXELS][GLCD_YPIXELS / 8];

// The init parameters
static ncurInitArgs_t ncurInitArgs;
static u08 deviceActive = MC_FALSE;
static u08 useLcd = MC_FALSE;
static u08 useMon = MC_FALSE;

// Data needed for ncurses stub lcd device
static FILE *ttyLcd = NULL;
static SCREEN *scrnLcd = NULL;
static WINDOW *winLcdBorder;

// Backlight support (init=yes) and brightness (init=full brightness)
static u08 lcdUseBacklight = MC_TRUE;
static u08 lcdBacklight = OCR2A_VALUE;

// Statistics counters on ncurses
static lcdNcurStats_t lcdNcurStats;

// Data needed for Emuchron monitor thread
static FILE *ttyMon = NULL;
static SCREEN *scrnMon = NULL;
static WINDOW *winMon;
static monInfo_t monInfo;
static pthread_t monThread;
static u08 monExit = MC_FALSE;
static char *createMsg = "Emuchron monitor (ncur)";
static u08 monInit = MC_TRUE;
static u08 monModeReq = MON_MODE_AGGREGATE;
static u08 monMode = MON_MODE_PAUSED;
static pthread_mutex_t mutexNcur = PTHREAD_MUTEX_INITIALIZER;

// Copies of Emuchron statistics data to compare with what we show in the
// monitor so we'll know what to change and sync
static u08 ctrlActive;
static u08 ctrlState0, ctrlState1;
static ctrlRegister_t ctrlReg0, ctrlReg1;
static ctrlStats_t ctrlStats0, ctrlStats1;
static ctrlGlcdStats_t ctrlGlcd;
static stubStats_t stubStats;

// Local function prototypes for lcd device
static void lcdNcurClose(void);
static void lcdNcurDrawModeSet(u08 controller, u08 color);
static void lcdNcurRedraw(u08 controller, int startY, int rows);

// Local function prototypes for Emuchron monitor
static void monInfoInit(void);
static void monLabelSet(char *label);
static void *monMain(void *ptr);
static void monSleep(int sleep);
static void monSyncI(int *local, int source, int x, int y, char *format,
  u08 *update);
static void monSyncLl1(long long *local, long long source, int x, int y,
  char *format, u08 *update);
static void monSyncLl2(long long *localReq, long long *localCnf,
  long long sourceReq, long long sourceCnf, int xReq, int xPct, int y,
  char *formatReq, char *formatPct, u08 *update);

// Local function prototypes for generic ncurses purposes
static void ncurColorInit(void);
static u08 ncurTtyInit(char *tty, char cmdArg, FILE **ttyFile, SCREEN **screen,
  int xSize, int ySize);

//
// Function: lcdNcurBacklightSet
//
// Set backlight brightness
//
void lcdNcurBacklightSet(u08 backlight)
{
  int brightness;
  u08 i;

  // No need to update when backlight remains unchanged
  if (lcdBacklight == backlight)
    return;

  // Sync backlight
  lcdBacklight = backlight;

  // See if update controller windows is required
  if (lcdUseBacklight == MC_FALSE)
    return;

  // Set new brightness in controller windows and flag update
  brightness = LCD_BRIGHTNESS(backlight);
  pthread_mutex_lock(&mutexNcur);
  init_color(LCD_COLOR_WIN, brightness, brightness, brightness);
  pthread_mutex_unlock(&mutexNcur);
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

  // Stop ncurses monitor thread
  monExit = MC_TRUE;
  pthread_join(monThread, NULL);

  // Reset lcd terminal and unlink tty
  if (useLcd == MC_TRUE)
  {
    set_term(scrnLcd);
    nocbreak();
    echo();
    curs_set(1);
    endwin();
    delscreen(scrnLcd);
    fclose(ttyLcd);
    ttyLcd = NULL;
  }

  // Reset monitor terminal and unlink tty
  if (useMon == MC_TRUE)
  {
    set_term(scrnMon);
    nocbreak();
    echo();
    curs_set(1);
    endwin();
    delscreen(scrnMon);
    fclose(ttyMon);
    ttyMon = NULL;
  }

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
  ncurInitArgs.winClose();
}

//
// Function: lcdNcurDataWrite
//
// Set content of lcd display in ncurses window. The controller has decided
// that the new data differs from the current lcd data.
//
void lcdNcurDataWrite(u08 x, u08 y, u08 data)
{
  u08 pixel = 0;
  u08 controller = x >> GLCD_CONTROLLER_XPIXBITS;
  int posX = (x & GLCD_CONTROLLER_XPIXMASK) * 2;
  int posY = y * 8;
  u08 lcdByte = lcdNcurImage[x][y];

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
  pthread_mutex_lock(&mutexNcur);
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
        lcdNcurDrawModeSet(controller, data & 0x1);
        mvwprintw(lcdNcurCtrl[controller].winLcdCtrl, posY, posX, LCD_PIXEL);
      }
    }

    // Shift to next pixel
    lcdByte = (lcdByte >> 1);
    data = (data >> 1);
    posY++;
    if (posY >= GLCD_YPIXELS)
      posY = posY - GLCD_YPIXELS;
  }
  pthread_mutex_unlock(&mutexNcur);
}

//
// Function: lcdNcurDisplaySet
//
// Switch controller display off or on
//
void lcdNcurDisplaySet(u08 controller, u08 display)
{
  // Sync display state and update window
  if (lcdNcurCtrl[controller].display != display)
  {
    lcdNcurCtrl[controller].display = display;
    pthread_mutex_lock(&mutexNcur);
    if (display == 0)
    {
      // Clear out the controller window
      werase(lcdNcurCtrl[controller].winLcdCtrl);
      lcdNcurCtrl[controller].flush = MC_TRUE;
    }
    else
    {
      // Repaint the entire controller window
      lcdNcurRedraw(controller, 0, GLCD_YPIXELS);
    }
    pthread_mutex_unlock(&mutexNcur);
  }
}

//
// Function: lcdNcurDrawModeSet
//
// Set the ncurses draw color
//
static void lcdNcurDrawModeSet(u08 controller, u08 color)
{
  // Switch between draw colors when needed
  if (color == GLCD_ON)
  {
    if (lcdNcurCtrl[controller].color == GLCD_OFF)
    {
      // Draw white pixels
      wattron(lcdNcurCtrl[controller].winLcdCtrl, A_REVERSE);
      lcdNcurCtrl[controller].color = GLCD_ON;
    }
  }
  else
  {
    if (lcdNcurCtrl[controller].color == GLCD_ON)
    {
      // Draw black pixels
      wattroff(lcdNcurCtrl[controller].winLcdCtrl, A_REVERSE);
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
  u08 i;
  u08 doRedraw = MC_FALSE;

  // Redraw only when activity has been signalled since last redraw
  for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
  {
    if (lcdNcurCtrl[i].flush == MC_TRUE)
    {
      // Flush changes in ncurses window but do not redraw yet
      if (doRedraw == MC_FALSE)
      {
        doRedraw = MC_TRUE;
        pthread_mutex_lock(&mutexNcur);
      }
      wnoutrefresh(lcdNcurCtrl[i].winLcdCtrl);
      lcdNcurCtrl[i].flush = MC_FALSE;
    }
  }

  // Do the actual redraw
  if (doRedraw == MC_TRUE)
  {
    doupdate();
    pthread_mutex_unlock(&mutexNcur);
  }
}

//
// Function: lcdNcurGraphicsSet
//
// Enable/disable variable backlight
//
void lcdNcurGraphicsSet(u08 useBacklight)
{
  u08 refresh = MC_FALSE;
  int brightness;
  u08 i;

  // No need to update when brightness support is unchanged
  if (lcdUseBacklight == useBacklight)
    return;

  // Sync brightness support
  lcdUseBacklight = useBacklight;

  // Depending on support value set new controller window brightness
  if (useBacklight == MC_FALSE && lcdBacklight != OCR2A_VALUE)
  {
    // When unsupported fall back to default brightness
    brightness = LCD_BRIGHTNESS(OCR2A_VALUE);
    refresh = MC_TRUE;
  }
  else if (useBacklight == MC_TRUE && lcdBacklight != OCR2A_VALUE)
  {
    // When supported use the current backlight
    brightness = LCD_BRIGHTNESS(lcdBacklight);
    refresh = MC_TRUE;
  }

  // Set new brightness in controller windows and flag update
  if (refresh == MC_TRUE)
  {
    pthread_mutex_lock(&mutexNcur);
    init_color(LCD_COLOR_WIN, brightness, brightness, brightness);
    pthread_mutex_unlock(&mutexNcur);
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
      lcdNcurCtrl[i].flush = MC_TRUE;
  }
}

//
// Function: lcdNcurRedraw
//
// Redraw (a part of) the lcd display in ncurses window
//
static void lcdNcurRedraw(u08 controller, int startY, int rows)
{
  u08 x,y;
  u08 rowsToDo;
  u08 bitsToDo;
  u08 lcdByte;
  u08 lcdLine;
  int posY;

  // Redraw all x columns for (a limited set of) rows. We either start at line
  // 0 for a number of rows, or start at a non-0 line and draw all up to the
  // the end of the window. We only need to draw the white (or grey-tone)
  // pixels.
  lcdNcurDrawModeSet(controller, MC_TRUE);
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
        if ((lcdByte & 0x1) == MC_TRUE)
          mvwprintw(lcdNcurCtrl[controller].winLcdCtrl, posY, x * 2,
            LCD_PIXEL);

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
void lcdNcurStartLineSet(u08 controller, u08 startLine)
{
  int scroll;
  u08 fillStart;
  u08 rows;

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
  pthread_mutex_lock(&mutexNcur);
  scrollok(lcdNcurCtrl[controller].winLcdCtrl, TRUE);
  wscrl(lcdNcurCtrl[controller].winLcdCtrl, scroll);
  scrollok(lcdNcurCtrl[controller].winLcdCtrl, FALSE);
  lcdNcurRedraw(controller, fillStart, rows);
  pthread_mutex_unlock(&mutexNcur);
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
  pthread_mutex_lock(&mutexNcur);
  monInit = MC_TRUE;
  pthread_mutex_unlock(&mutexNcur);
}

//
// Function: monInfoInit
//
// Init layout of ncurses monitor info window
//
void monInfoInit(void)
{
  int i;

  // Start from scratch
  wclear(winMon);
  int versionLen = strlen(EMUCHRON_VERSION);

  // Border
  box(winMon,0 , 0);
  wattron(winMon, A_BOLD);
  mvwprintw(winMon, 0, (MON_X_SIZE - 18) / 2, " Emuchron monitor ");
  if (monMode == MON_MODE_PAUSED)
    monLabelSet("Paused");
  else if (monMode == MON_MODE_AGGREGATE)
    monLabelSet("Aggregate");
  else
    monLabelSet("Snapshot");
  mvwprintw(winMon, 0, MON_X_SIZE - 4 - versionLen, " %s ",
    EMUCHRON_VERSION);
  wattroff(winMon, A_BOLD);

  // KPI's
  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_TD_Y +  0, MON_TD_X, "Time");
  mvwprintw(winMon, MON_TD_Y +  1, MON_TD_X, "Date");
  mvwprintw(winMon, MON_TD_Y +  2, MON_TD_X, "Alarm");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_TD_Y +  0, MON_TD_X + 6, ":");
  mvwprintw(winMon, MON_TD_Y +  1, MON_TD_X + 6, ":");
  mvwprintw(winMon, MON_TD_Y +  2, MON_TD_X + 6, ":   :");
  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_M1_Y +  0, MON_M1_X, "Monochron variables");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_M1_Y +  1, MON_M1_X, "mcMchronClock :");
  mvwprintw(winMon, MON_M1_Y +  2, MON_M1_X, "mcAlarmSwitch :");
  mvwprintw(winMon, MON_M1_Y +  3, MON_M1_X, "mcAlarmH      :");
  mvwprintw(winMon, MON_M1_Y +  4, MON_M1_X, "mcAlarmM      :");
  mvwprintw(winMon, MON_M1_Y +  5, MON_M1_X, "mcAlarming    :");
  mvwprintw(winMon, MON_M1_Y +  6, MON_M1_X, "mcTickerAlarm :");
  mvwprintw(winMon, MON_M1_Y +  7, MON_M1_X, "mcSnoozing    :");
  mvwprintw(winMon, MON_M1_Y +  8, MON_M1_X, "mcTickerSnooze:");
  mvwprintw(winMon, MON_M1_Y +  9, MON_M1_X, "mcU8Util1     :");
  mvwprintw(winMon, MON_M1_Y + 10, MON_M1_X, "mcU8Util2     :");
  mvwprintw(winMon, MON_M1_Y + 11, MON_M1_X, "mcU8Util3     :");
  mvwprintw(winMon, MON_M1_Y + 12, MON_M1_X, "mcU8Util4     :");
  mvwprintw(winMon, MON_M1_Y + 13, MON_M1_X, "mcU16Util1    :");
  mvwprintw(winMon, MON_M1_Y + 14, MON_M1_X, "mcU16Util2    :");
  mvwprintw(winMon, MON_M1_Y + 15, MON_M1_X, "mcU16Util3    :");
  mvwprintw(winMon, MON_M1_Y + 16, MON_M1_X, "mcU16Util4    :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_M2_Y +  0, MON_M2_X, "Monochron variables");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_M2_Y +  1, MON_M2_X, "mcClockNewTH  :");
  mvwprintw(winMon, MON_M2_Y +  2, MON_M2_X, "mcClockNewTM  :");
  mvwprintw(winMon, MON_M2_Y +  3, MON_M2_X, "mcClockNewTS  :");
  mvwprintw(winMon, MON_M2_Y +  4, MON_M2_X, "mcClockNewDD  :");
  mvwprintw(winMon, MON_M2_Y +  5, MON_M2_X, "mcClockNewDM  :");
  mvwprintw(winMon, MON_M2_Y +  6, MON_M2_X, "mcClockNewDY  :");
  mvwprintw(winMon, MON_M2_Y +  7, MON_M2_X, "mcCycleCounter:");
  mvwprintw(winMon, MON_M2_Y +  8, MON_M2_X, "mcBgColor     :");
  mvwprintw(winMon, MON_M2_Y +  9, MON_M2_X, "mcFgColor     :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_LC_Y +  0, MON_LC_X, "Lcd display");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_LC_Y +  1, MON_LC_X, "backlight     :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_ST_Y +  0, MON_ST_X, "Runtime stub");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_ST_Y +  1, MON_ST_X, "cycle     :");
  mvwprintw(winMon, MON_ST_Y +  2, MON_ST_X, "inTime    :");
  mvwprintw(winMon, MON_ST_Y +  3, MON_ST_X, "outTime   :");
  mvwprintw(winMon, MON_ST_Y +  4, MON_ST_X, "singleCycl:");
  mvwprintw(winMon, MON_ST_Y +  5, MON_ST_X, "avgSleep  :");
  mvwprintw(winMon, MON_ST_Y +  6, MON_ST_X, "minSleep  :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_GL_Y +  0, MON_GL_X, "Runtime glcd");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_GL_Y +  1, MON_GL_X, "dataWrite :");
  mvwprintw(winMon, MON_GL_Y +  2, MON_GL_X, "dataRead  :");
  mvwprintw(winMon, MON_GL_Y +  3, MON_GL_X, "addressSet:");
  mvwprintw(winMon, MON_GL_Y +  4, MON_GL_X, "ctrlSet   :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_CR_Y +  0, MON_CR_X, "Controller registers");
  mvwprintw(winMon, MON_CR_Y +  1, MON_CR_X + 12, "Ct-0  Ct-1");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_CR_Y +  2, MON_CR_X, "active    :");
  mvwprintw(winMon, MON_CR_Y +  3, MON_CR_X, "state     :");
  mvwprintw(winMon, MON_CR_Y +  4, MON_CR_X, "display   :");
  mvwprintw(winMon, MON_CR_Y +  5, MON_CR_X, "startline :");
  mvwprintw(winMon, MON_CR_Y +  6, MON_CR_X, "x         :");
  mvwprintw(winMon, MON_CR_Y +  7, MON_CR_X, "y         :");
  mvwprintw(winMon, MON_CR_Y +  8, MON_CR_X, "write     :");
  mvwprintw(winMon, MON_CR_Y +  9, MON_CR_X, "read      :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_C0_Y +  0, MON_C0_X, "Runtime controller 0");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_C0_Y +  1, MON_C0_X, "display   :");
  mvwprintw(winMon, MON_C0_Y +  2, MON_C0_X, "startline :");
  mvwprintw(winMon, MON_C0_Y +  3, MON_C0_X, "x         :");
  mvwprintw(winMon, MON_C0_Y +  4, MON_C0_X, "y         :");
  mvwprintw(winMon, MON_C0_Y +  5, MON_C0_X, "write     :");
  mvwprintw(winMon, MON_C0_Y +  6, MON_C0_X, "read      :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_C1_Y +  0, MON_C1_X, "Runtime controller 1");
  wattroff(winMon, A_BOLD);
  mvwprintw(winMon, MON_C1_Y +  1, MON_C1_X, "display   :");
  mvwprintw(winMon, MON_C1_Y +  2, MON_C1_X, "startline :");
  mvwprintw(winMon, MON_C1_Y +  3, MON_C1_X, "x         :");
  mvwprintw(winMon, MON_C1_Y +  4, MON_C1_X, "y         :");
  mvwprintw(winMon, MON_C1_Y +  5, MON_C1_X, "write     :");
  mvwprintw(winMon, MON_C1_Y +  6, MON_C1_X, "read      :");

  wattron(winMon, A_BOLD);
  mvwprintw(winMon, MON_EE_Y + 0, MON_EE_X, "Eeprom");
  wattroff(winMon, A_BOLD);
  for (i = 0; i< eepDictCount; i++)
  {
    mvwprintw(winMon, MON_EE_Y + 1 + eepDict[i].eepItemId - EE_OFFSET,
      MON_EE_X, "%s", eepDict[i].eepItemName);
    mvwprintw(winMon, MON_EE_Y + 1 + eepDict[i].eepItemId - EE_OFFSET,
      MON_EE_X + 15, ":");
  }
}

//
// Function: monLabelSet
//
// Set label of emuchron monitor form
//
static void monLabelSet(char *label)
{
  int len = (int)strlen(label);

  mvwprintw(winMon, 0, 2, "%*c", len + 4, ' ');
  wattron(winMon, A_REVERSE);
  mvwprintw(winMon, 0, 3, " %s ", label);
  wattroff(winMon, A_REVERSE);
}

//
// Function: monMain
//
// Main function for ncurses Emuchron monitor thread. It serves to update the
// Emuchron monitor as well as being a watchdog for the continued existence of
// the ncurses Monochron lcd and Emuchron monitor ttys.
//
static void *monMain(void *ptr)
{
  uint8_t rtcClockData[8];
  u08 monUpd = MC_FALSE;
  u08 monTimeUpd = MC_FALSE;
  int eeprom[eepDictCount];
  int i;
  uint8_t eprombyte;
  struct stat buffer;

  // Init monitor mirror data and link to eeprom data
  memset(&monInfo, 0, sizeof(monInfo_t));
  monInfo.monEeprom = &eeprom[0];

  // In case we're only using the monitor default the screen to use
  if (useLcd == MC_FALSE)
    set_term(scrnMon);

  // Loop unit exit is requested
  while (monExit == MC_FALSE)
  {
    // Keep track of what we have to update/check
    monUpd = MC_FALSE;
    monTimeUpd = MC_FALSE;

    // Get the mchron internal time
    i2cMasterReceiveNI(0xd0, 7, rtcClockData);
    if (monInit == MC_TRUE ||
        memcmp(monInfo.monClockData, rtcClockData, sizeof(rtcClockData)) != 0)
    {
      // We have a new date/time (usually once per second)
      monTimeUpd = MC_TRUE;
      memcpy(monInfo.monClockData, rtcClockData, sizeof(rtcClockData));
    }

    // Update monitor mode and info data when enabled
    if (useMon == MC_TRUE)
    {
      // Init update to the monitor ncurses tty
      pthread_mutex_lock(&mutexNcur);
      if (useLcd == MC_TRUE)
        set_term(scrnMon);

      // See if we need to change the monitor mode
      if (monMode != monModeReq)
      {
        // Sync request state and prepare monitor screen
        monMode = monModeReq;
        monInit = MC_TRUE;
        monInfoInit();
      }

      // Get the controller+glcd and stub statistics
      if (monMode != MON_MODE_PAUSED)
      {
        ctrlStatsGet(monMode, &ctrlActive, &ctrlState0, &ctrlState1, &ctrlReg0,
          &ctrlReg1, &ctrlStats0, &ctrlStats1, &ctrlGlcd);
        stubStatsGet(&stubStats);
      }

      // 1) Time + Date + Alarm
      if (monTimeUpd == MC_TRUE || monInit == MC_TRUE)
      {
        mvwprintw(winMon, MON_TD_Y + 0, MON_TD_X + 8, "%02d:%02d:%02d",
          (int)bcdDecode(rtcClockData[2], 0x3),
          (int)bcdDecode(rtcClockData[1], 0x7),
          (int)bcdDecode(rtcClockData[0], 0x7));
        mvwprintw(winMon, MON_TD_Y + 1, MON_TD_X + 8, "%02d/%02d/%04d",
          (int)bcdDecode(rtcClockData[4], 0x3),
          (int)bcdDecode(rtcClockData[5], 0x1),
          (int)bcdDecode(rtcClockData[6], 0xf) + 2000);
        monUpd = MC_TRUE;
      }
      monSyncI(&monInfo.monAlarmH1, (int)mcAlarmH, MON_TD_X + 8, MON_TD_Y + 2,
        "%02d", &monUpd);
      monSyncI(&monInfo.monAlarmM1, (int)mcAlarmM, MON_TD_X + 11, MON_TD_Y + 2,
        "%02d", &monUpd);

      if (monInfo.monAlarmSwitch1 != (ALARM_PIN & _BV(ALARM)) ||
          monInit == MC_TRUE)
      {
        if (ALARM_PIN & _BV(ALARM))
          mvwprintw(winMon, MON_TD_Y + 2, MON_TD_X + 14, "(off)");
        else
        mvwprintw(winMon, MON_TD_Y + 2, MON_TD_X + 14, "(on) ");
        monInfo.monAlarmSwitch1 = (ALARM_PIN & _BV(ALARM));
        monUpd = MC_TRUE;
      }

      // Update the other elements only when enabled
      if (monMode != MON_MODE_PAUSED)
      {
        // 2a) Monochron variables
        monSyncI(&monInfo.monMchronClock, (int)mcMchronClock,
          MON_M1_X + 18, MON_M1_Y +  1, "%3d", &monUpd);
        monSyncI(&monInfo.monAlarmSwitch2, (int)mcAlarmSwitch,
          MON_M1_X + 18, MON_M1_Y +  2, "%3d", &monUpd);
        monSyncI(&monInfo.monAlarmH2, (int)mcAlarmH,
          MON_M1_X + 18, MON_M1_Y +  3, "%3d", &monUpd);
        monSyncI(&monInfo.monAlarmM2, (int)mcAlarmM,
          MON_M1_X + 18, MON_M1_Y +  4, "%3d", &monUpd);
        monSyncI(&monInfo.monAlarming, (int)mcAlarming,
          MON_M1_X + 18, MON_M1_Y +  5, "%3d", &monUpd);
        monSyncI(&monInfo.monTickerAlarm, (int)mcTickerAlarm,
          MON_M1_X + 18, MON_M1_Y +  6, "%3d", &monUpd);
        monSyncI(&monInfo.monSnoozing, (int)mcSnoozing,
          MON_M1_X + 18, MON_M1_Y +  7, "%3d", &monUpd);
        monSyncI(&monInfo.monTickerSnooze, (int)mcTickerSnooze,
          MON_M1_X + 18, MON_M1_Y +  8, "%3d", &monUpd);
        monSyncI(&monInfo.monU8Util1, (int)mcU8Util1,
          MON_M1_X + 18, MON_M1_Y +  9, "%3d", &monUpd);
        monSyncI(&monInfo.monU8Util2, (int)mcU8Util2,
          MON_M1_X + 18, MON_M1_Y + 10, "%3d", &monUpd);
        monSyncI(&monInfo.monU8Util3, (int)mcU8Util3,
          MON_M1_X + 18, MON_M1_Y + 11, "%3d", &monUpd);
        monSyncI(&monInfo.monU8Util4, (int)mcU8Util4,
          MON_M1_X + 18, MON_M1_Y + 12, "%3d", &monUpd);
        monSyncI(&monInfo.monU16Util1, (int)mcU16Util1,
          MON_M1_X + 16, MON_M1_Y + 13, "%5d", &monUpd);
        monSyncI(&monInfo.monU16Util2, (int)mcU16Util2,
          MON_M1_X + 16, MON_M1_Y + 14, "%5d", &monUpd);
        monSyncI(&monInfo.monU16Util3, (int)mcU16Util3,
          MON_M1_X + 16, MON_M1_Y + 15, "%5d", &monUpd);
        monSyncI(&monInfo.monU16Util4, (int)mcU16Util4,
          MON_M1_X + 16, MON_M1_Y + 16, "%5d", &monUpd);

        // 2b) Monochron variables
        monSyncI(&monInfo.monClockNewTH, (int)mcClockNewTH,
          MON_M2_X + 16, MON_M2_Y +  1, "%3d", &monUpd);
        monSyncI(&monInfo.monClockNewTM, (int)mcClockNewTM,
          MON_M2_X + 16, MON_M2_Y +  2, "%3d", &monUpd);
        monSyncI(&monInfo.monClockNewTS, (int)mcClockNewTS,
          MON_M2_X + 16, MON_M2_Y +  3, "%3d", &monUpd);
        monSyncI(&monInfo.monClockNewDD, (int)mcClockNewDD,
          MON_M2_X + 16, MON_M2_Y +  4, "%3d", &monUpd);
        monSyncI(&monInfo.monClockNewDM, (int)mcClockNewDM,
          MON_M2_X + 16, MON_M2_Y +  5, "%3d", &monUpd);
        monSyncI(&monInfo.monClockNewDY, (int)mcClockNewDY,
          MON_M2_X + 16, MON_M2_Y +  6, "%3d", &monUpd);
        monSyncI(&monInfo.monCycleCounter, (int)mcCycleCounter,
          MON_M2_X + 16, MON_M2_Y +  7, "%3d", &monUpd);
        monSyncI(&monInfo.monBgColor, (int)mcBgColor,
          MON_M2_X + 16, MON_M2_Y +  8, "%3d", &monUpd);
        monSyncI(&monInfo.monFgColor, (int)mcFgColor,
          MON_M2_X + 16, MON_M2_Y +  9, "%3d", &monUpd);

        // 3) Lcd display
        monSyncI(&monInfo.monBrightness, (int)(OCR2B >> OCR2B_BITSHIFT),
          MON_LC_X + 16, MON_LC_Y + 1, "%3d", &monUpd);

        // 4) Runtime stub
        monSyncI(&monInfo.monCycle,
          (int)stubStats.cycle, MON_ST_X + 16, MON_ST_Y + 1, "%3d", &monUpd);
        if (monInfo.monStubStats.inTimeCount != stubStats.inTimeCount ||
            monInit == MC_TRUE)
        {
          monSyncI(&monInfo.monStubStats.inTimeCount,
            stubStats.inTimeCount, MON_ST_X + 12, MON_ST_Y + 2, "%7d",
            &monUpd);
          if (stubStats.inTimeCount == 0)
            mvwprintw(winMon, MON_ST_Y + 5, MON_ST_X + 12, "      -");
          else
            mvwprintw(winMon, MON_ST_Y + 5, MON_ST_X + 12, "%7.0f",
              stubStats.waitTotal / (double)stubStats.inTimeCount / 1000);
          monUpd = MC_TRUE;
        }
        monSyncI(&monInfo.monStubStats.outTimeCount,
          stubStats.outTimeCount, MON_ST_X + 12, MON_ST_Y + 3, "%7d",
          &monUpd);
        monSyncI(&monInfo.monStubStats.singleCycleCount,
          stubStats.singleCycleCount, MON_ST_X + 12, MON_ST_Y + 4, "%7d",
          &monUpd);
        if (monInfo.monStubStats.minSleep != stubStats.minSleep ||
            monInit == MC_TRUE)
        {
          if (stubStats.minSleep == ANIM_TICK_CYCLE_MS + 1)
            mvwprintw(winMon, MON_ST_Y + 6, MON_ST_X + 16, "  -");
          else
            mvwprintw(winMon, MON_ST_Y + 6, MON_ST_X + 16, "%3d",
              stubStats.minSleep);
          monInfo.monStubStats.minSleep = stubStats.minSleep;
          monUpd = MC_TRUE;
        }

        // 5) Runtime glcd (aggregate or snapshot data)
        monSyncLl1(&monInfo.monCtrlGlcd.dataWrite, ctrlGlcd.dataWrite,
          MON_GL_X + 12, MON_GL_Y + 1, "%7llu", &monUpd);
        monSyncLl1(&monInfo.monCtrlGlcd.dataRead, ctrlGlcd.dataRead,
          MON_GL_X + 12, MON_GL_Y + 2, "%7llu", &monUpd);
        monSyncLl1(&monInfo.monCtrlGlcd.addressSet, ctrlGlcd.addressSet,
          MON_GL_X + 12, MON_GL_Y + 3, "%7llu", &monUpd);
        monSyncLl1(&monInfo.monCtrlGlcd.ctrlSet, ctrlGlcd.ctrlSet,
          MON_GL_X + 12, MON_GL_Y + 4, "%7llu", &monUpd);

        // 6) Controller registers
        if (monInfo.monCtrlActive != ctrlActive || monInit == MC_TRUE)
        {
          if (ctrlActive == 0)
            mvwprintw(winMon, MON_CR_Y + 2, MON_CR_X + 15, "*     -");
          else
            mvwprintw(winMon, MON_CR_Y + 2, MON_CR_X + 15, "-     *");
          monInfo.monCtrlActive = ctrlActive;
          monUpd = MC_TRUE;
        }
        monSyncI(&monInfo.monCtrlState0, (int)ctrlState0,
          MON_CR_X + 15, MON_CR_Y + 3, "%1d", &monUpd);
        monSyncI(&monInfo.monCtrlState1, (int)ctrlState1,
          MON_CR_X + 21, MON_CR_Y + 3, "%1d", &monUpd);
        monSyncI(&monInfo.monCtrlReg0.display, (int)ctrlReg0.display,
          MON_CR_X + 15, MON_CR_Y + 4, "%1d", &monUpd);
        monSyncI(&monInfo.monCtrlReg1.display, (int)ctrlReg1.display,
          MON_CR_X + 21, MON_CR_Y + 4, "%1d", &monUpd);
        monSyncI(&monInfo.monCtrlReg0.startLine, (int)ctrlReg0.startLine,
          MON_CR_X + 14, MON_CR_Y + 5, "%2d", &monUpd);
        monSyncI(&monInfo.monCtrlReg1.startLine, (int)ctrlReg1.startLine,
          MON_CR_X + 20, MON_CR_Y + 5, "%2d", &monUpd);
        monSyncI(&monInfo.monCtrlReg0.x, (int)ctrlReg0.x,
          MON_CR_X + 13, MON_CR_Y + 6, "%3d", &monUpd);
        monSyncI(&monInfo.monCtrlReg1.x, (int)ctrlReg1.x,
          MON_CR_X + 19, MON_CR_Y + 6, "%3d", &monUpd);
        monSyncI(&monInfo.monCtrlReg0.y, (int)ctrlReg0.y,
          MON_CR_X + 15, MON_CR_Y + 7, "%1d", &monUpd);
        monSyncI(&monInfo.monCtrlReg1.y, (int)ctrlReg1.y,
          MON_CR_X + 21, MON_CR_Y + 7, "%1d", &monUpd);
        monSyncI(&monInfo.monCtrlReg0.dataWrite, (int)ctrlReg0.dataWrite,
          MON_CR_X + 12, MON_CR_Y + 8, "0x%02x", &monUpd);
        monSyncI(&monInfo.monCtrlReg1.dataWrite, (int)ctrlReg1.dataWrite,
          MON_CR_X + 18, MON_CR_Y + 8, "0x%02x", &monUpd);
        monSyncI(&monInfo.monCtrlReg0.dataRead, (int)ctrlReg0.dataRead,
          MON_CR_X + 12, MON_CR_Y + 9, "0x%02x", &monUpd);
        monSyncI(&monInfo.monCtrlReg1.dataRead, (int)ctrlReg1.dataRead,
          MON_CR_X + 18, MON_CR_Y + 9, "0x%02x", &monUpd);

        // 7) Runtime controller 0  (aggregate or snapshot data)
        monSyncLl2(&monInfo.monCtrlStats0.displayReq,
          &monInfo.monCtrlStats0.displayCnf, ctrlStats0.displayReq,
          ctrlStats0.displayCnf, MON_C0_X + 12, MON_C0_X + 20, MON_C0_Y + 1,
          "%7llu", "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats0.startLineReq,
          &monInfo.monCtrlStats0.startLineCnf, ctrlStats0.startLineReq,
          ctrlStats0.startLineCnf, MON_C0_X + 12, MON_C0_X + 20, MON_C0_Y + 2,
          "%7llu", "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats0.xReq,
          &monInfo.monCtrlStats0.xCnf, ctrlStats0.xReq,
          ctrlStats0.xCnf, MON_C0_X + 12, MON_C0_X + 20, MON_C0_Y + 3, "%7llu",
          "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats0.yReq,
          &monInfo.monCtrlStats0.yCnf, ctrlStats0.yReq,
          ctrlStats0.yCnf, MON_C0_X + 12, MON_C0_X + 20, MON_C0_Y + 4, "%7llu",
          "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats0.writeReq,
          &monInfo.monCtrlStats0.writeCnf, ctrlStats0.writeReq,
          ctrlStats0.writeCnf, MON_C0_X + 12, MON_C0_X + 20, MON_C0_Y + 5,
          "%7llu", "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats0.readReq,
          &monInfo.monCtrlStats0.readCnf, ctrlStats0.readReq,
          ctrlStats0.readCnf, MON_C0_X + 12, MON_C0_X + 20, MON_C0_Y + 6,
          "%7llu", "%3.0f%%", &monInit);

        // 8) Runtime controller 1 (aggregate or snapshot data)
        monSyncLl2(&monInfo.monCtrlStats1.displayReq,
          &monInfo.monCtrlStats1.displayCnf, ctrlStats1.displayReq,
          ctrlStats1.displayCnf, MON_C1_X + 12, MON_C1_X + 20, MON_C0_Y + 1,
          "%7llu", "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats1.startLineReq,
          &monInfo.monCtrlStats1.startLineCnf, ctrlStats1.startLineReq,
          ctrlStats1.startLineCnf, MON_C1_X + 12, MON_C1_X + 20, MON_C0_Y + 2,
          "%7llu", "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats1.xReq,
          &monInfo.monCtrlStats1.xCnf, ctrlStats1.xReq,
          ctrlStats1.xCnf, MON_C1_X + 12, MON_C1_X + 20, MON_C0_Y + 3, "%7llu",
          "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats1.yReq,
          &monInfo.monCtrlStats1.yCnf, ctrlStats1.yReq,
          ctrlStats1.yCnf, MON_C1_X + 12, MON_C1_X + 20, MON_C0_Y + 4, "%7llu",
          "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats1.writeReq,
          &monInfo.monCtrlStats1.writeCnf, ctrlStats1.writeReq,
          ctrlStats1.writeCnf, MON_C1_X + 12, MON_C1_X + 20, MON_C0_Y + 5,
          "%7llu", "%3.0f%%", &monInit);
        monSyncLl2(&monInfo.monCtrlStats1.readReq,
          &monInfo.monCtrlStats1.readCnf, ctrlStats1.readReq,
          ctrlStats1.readCnf, MON_C1_X + 12, MON_C1_X + 20, MON_C0_Y + 6,
          "%7llu", "%3.0f%%", &monInit);

        // 9) eeprom
        for (i = 0; i< eepDictCount; i++)
        {
          eprombyte = eeprom_read_byte((uint8_t *)eepDict[i].eepItemId);
          monSyncI(&monInfo.monEeprom[i], (int)eprombyte,
            MON_EE_X + 17, MON_EE_Y + eepDict[i].eepItemId - EE_OFFSET + 1,
            "%3d", &monUpd);
        }
      }

      // Update only if something was updated
      if (monUpd == MC_TRUE)
        wrefresh(winMon);

      // Switch back to monochron lcd, if needed, as default
      if (useLcd == MC_TRUE)
        set_term(scrnLcd);

      // Wrap up update
      pthread_mutex_unlock(&mutexNcur);
    }

    // Every second do a watchdog on the existence of the lcd and/or monitor
    // terminal and when gone force mchron to exit
    if (monTimeUpd == MC_TRUE &&
        ((useLcd == MC_TRUE && stat(ncurInitArgs.ttyLcd, &buffer) != 0) ||
         (useMon == MC_TRUE && stat(ncurInitArgs.ttyMon, &buffer) != 0)))
    {
      // Make the main thread refuse further access to ncurses using the lock.
      // This way we can do a graceful shutdown of the (ncurses) environment in
      // this thread.
      pthread_mutex_lock(&mutexNcur);
      lcdNcurClose();
    }

    // Go to sleep to achieve low cpu usage combined with ncurses refresh rate
    // at max ~16 fps
    monInit = MC_FALSE;
    monSleep(MON_THREAD_SLEEP_MS);
  }

  return 0;
}

//
// Function: monModeSet
//
// Set activity mode for the Emuchron monitor (paused, aggregate or snapshot)
//
void monModeSet(u08 mode)
{
  monModeReq = mode;
}

//
// Function: monSleep
//
// Sleep amount of time (in msec) without keyboard interaction with a max value
// of 999 msec.
//
static void monSleep(int sleep)
{
  struct timespec timeSleep = { 0, sleep * 1000000 };
  nanosleep(&timeSleep, NULL);
}

//
// Function: monSyncI
//
// Update a single integer based item in the Emuchron monitor
//
static void monSyncI(int *local, int source, int x, int y, char *format,
  u08 *update)
{
  if ((*local != source || monInit == MC_TRUE) &&
      (*local < 1E7 || source < 1E7))
  {
    if (source < 1E7)
      mvwprintw(winMon, y, x, format, source);
    else if (*local < 1E7)
      mvwprintw(winMon, y, x, "  >=1E7");
    *local = source;
    *update = MC_TRUE;
  }
}

//
// Function: monSyncLl1
//
// Update a single long long based item in the Emuchron monitor
//
static void monSyncLl1(long long *local, long long source, int x, int y,
  char *format, u08 *update)
{
  if ((*local != source || monInit == MC_TRUE) &&
      (*local < 1E7 || source < 1E7))
  {
    if (source < 1E7)
      mvwprintw(winMon, y, x, format, source);
    else if (*local < 1E7)
      mvwprintw(winMon, y, x, "  >=1E7");
    *local = source;
    *update = MC_TRUE;
  }
}

//
// Function: monSyncLl2
//
// Update two long long based items in the Emuchron monitor
//
static void monSyncLl2(long long *localReq, long long *localCnf,
  long long sourceReq, long long sourceCnf, int xReq, int xPct, int y,
  char *formatReq, char *formatPct, u08 *update)
{
  if (*localReq != sourceReq || *localCnf != sourceCnf || monInit == MC_TRUE)
  {
    if (sourceReq < 1E7)
      mvwprintw(winMon, y, xReq, formatReq, sourceReq);
    else if (*localReq < 1E7)
      mvwprintw(winMon, y, xReq, "  >=1E7");
    *localReq = sourceReq;
    *localCnf = sourceCnf;
    if (*localReq == 0)
      mvwprintw(winMon, y, xPct, "  -%%");
    else
      mvwprintw(winMon, y, xPct, formatPct,
       sourceCnf * 100 / (double)sourceReq);
    *update = MC_TRUE;
  }
}

//
// Function: ncurColorInit
//
// Init the ncurses colors used in the lcd and monitor windows. Use this
// function once at the init of an ncurses tty.
//
static void ncurColorInit(void)
{
  int brightWin;
  int brightBorder;

  // Define black background and greyscale foreground colors and link them in
  // color pairs. Upon changing the lcd backlight, LCD_COLOR_WIN will be
  // redefined into a new greyscale.
  // When changing a color in a color pair that is used in a window, ncurses
  // will redraw the affected window contents upon refreshing, so we don't need
  // to do this ourselves (which is very nice).
  start_color();

  // Common defines for lcd and monitor windows
  init_color(COLOR_BLACK, 0, 0, 0);
  init_color(COLOR_WHITE, 1000, 1000, 1000);

  // Defines for ncurses lcd window
  brightWin = LCD_BRIGHTNESS(16);
  brightBorder = LCD_BRIGHTNESS(6);
  init_color(LCD_COLOR_WIN, brightWin, brightWin, brightWin);
  init_color(LCD_COLOR_BORDER, brightBorder, brightBorder, brightBorder);
  init_pair(1, LCD_COLOR_WIN, COLOR_BLACK);
  init_pair(2, LCD_COLOR_BORDER, COLOR_BLACK);

  // Defines for the Emuchron monitor window
  if (ncurInitArgs.monColor == 'b')
    init_pair(3, COLOR_BLACK, COLOR_WHITE);
  else
    init_pair(3, COLOR_WHITE, COLOR_BLACK);
}

//
// Function: ncurInit
//
// Initialize the lcd display in ncurses window
//
u08 ncurInit(ncurInitArgs_t *ncurInitArgsSet)
{
  u08 i;
  u08 retVal = MC_TRUE;

  // Nothing to do if the ncurses environment is already initialized
  if (deviceActive == MC_TRUE)
    return MC_TRUE;

  // Copy ncursus init parameters
  ncurInitArgs = *ncurInitArgsSet;
  useLcd = ncurInitArgs.useLcd;
  useMon = ncurInitArgs.useMon;

  // Do we need to init anything anyway
  if (useLcd == MC_FALSE && useMon == MC_FALSE)
    return MC_FALSE;

  // Reset the ncurses statistics
  lcdNcurStatsReset();

  // Init our window lcd image copy to blank
  memset(lcdNcurImage, 0, sizeof(lcdNcurImage));

  // Check and init the ncurses lcd when requested
  if (useLcd == MC_TRUE)
  {
    retVal = ncurTtyInit(ncurInitArgs.ttyLcd, 't', &ttyLcd, &scrnLcd,
      LCD_X_PIXELS, LCD_Y_PIXELS);
    if (retVal == MC_FALSE)
      return retVal;
  }

  // Check and init the ncurses monitor when requested
  if (useMon == MC_TRUE)
  {
    retVal = ncurTtyInit(ncurInitArgs.ttyMon, 'm', &ttyMon, &scrnMon,
      MON_X_SIZE, MON_Y_SIZE);
    if (retVal == MC_FALSE)
      return retVal;
  }

  // Init the Monochron ncurses lcd device if requested
  if (useLcd == MC_TRUE)
  {
    // Create the outer border and the controller section windows
    deviceActive = MC_TRUE;
    set_term(scrnLcd);
    winLcdBorder = newwin(LCD_Y_PIXELS, LCD_X_PIXELS, 0, 0);
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
    {
      lcdNcurCtrl[i].winLcdCtrl =
        newwin(GLCD_CONTROLLER_YPIXELS * LCD_Y_PIXSIZE,
          GLCD_CONTROLLER_XPIXELS * LCD_X_PIXSIZE, 1,
          1 + i * GLCD_CONTROLLER_XPIXELS * LCD_X_PIXSIZE);
      lcdNcurCtrl[i].display = MC_FALSE;
      lcdNcurCtrl[i].startLine = 0;
      lcdNcurCtrl[i].color = GLCD_OFF;
      lcdNcurCtrl[i].flush = MC_FALSE;
    }

    // Assign color pair to controllers and border windows
    for (i = 0; i < GLCD_NUM_CONTROLLERS; i++)
      wbkgd(lcdNcurCtrl[i].winLcdCtrl, COLOR_PAIR(1));
    wbkgd(winLcdBorder, COLOR_PAIR(2));

    // Draw and show a box in the outer border window
    box(winLcdBorder, 0 , 0);
    wrefresh(winLcdBorder);

    // Set time reference for checking ncurses tty
    //gettimeofday(&tvThen, NULL);
  }

  // Init the Emuchron ncurses monitor device if requested
  if (useMon == MC_TRUE)
  {
    // Create the monitor window
    deviceActive = MC_TRUE;
    set_term(scrnMon);
    winMon = newwin(MON_Y_SIZE, MON_X_SIZE, 0, 0);

    // Assign color pair to monitor window
    wbkgd(winMon, COLOR_PAIR(3));

    // Init monitor screen
    //monInfoInit();
    //wrefresh(winMon);

    // Init done. Switch back to lcd terminal as default, when used.
    if (useLcd == MC_TRUE)
      set_term(scrnLcd);
  }

  // At least one ncurses window is active.
  // Create the ncurses monitor thread with monMain() as its main event loop.
  // As of now, this thread takes care of updating the monitor (if it is used)
  // while also being the watchdog for the Monochron and/or Emuchron ncurses
  // tty(s).
  (void)pthread_create(&monThread, NULL, monMain, (void *)createMsg);

  // Init successfully completed
  return MC_TRUE;
}

//
// Function: ncurTtyInit
//
// Check a tty and create a basic ncurses screen in it
//
static u08 ncurTtyInit(char *tty, char cmdArg, FILE **ttyFile, SCREEN **screen,
 int xSize, int ySize)
{
  struct stat statTty;
  struct winsize sizeTty;

  // Check if the tty is actually in use
  if (stat(tty, &statTty) != 0)
  {
    printf("%s: -%c: destination ncurses tty \"%s\" is not in use\n",
      __progname, cmdArg, tty);
    return MC_FALSE;
  }

  // Open tty and check if it has a specific size
  *ttyFile = fopen(tty, "r+");
  if (*ttyFile == (FILE *)NULL)
  {
    printf("%s: -%c: cannot open destination ncurses tty \"%s\"\n", __progname,
      cmdArg, tty);
    return MC_FALSE;
  }
  if (ioctl(fileno(*ttyFile), TIOCGWINSZ, (char *)&sizeTty) >= 0)
  {
    if (sizeTty.ws_col != xSize || sizeTty.ws_row != ySize)
    {
      printf("%s: -%c: destination ncurses tty \"%s\" size (%dx%d) is incorrect,\n",
        __progname, cmdArg, tty, sizeTty.ws_col, sizeTty.ws_row);
      printf("required size is %dx%d\n", xSize, ySize);
      fclose(*ttyFile);
      return MC_FALSE;
    }
  }

  // Assign tty to ncurses screen and allow using colors by forcing a 256
  // color profile. Set options to not wait for Enter key, not echo chars, hide
  // cursor, and init (use of) colors.
  *screen = newterm("xterm-256color", *ttyFile, *ttyFile);
  nocbreak();
  noecho();
  curs_set(0);
  ncurColorInit();

  // Try to set the size of the xterm tty. We do this because for some reason
  // gdb may make ncurses use the size of the mchron terminal window as the
  // actual size, which is pretty weird.
  set_term(*screen);
  resize_term(ySize, xSize);

  return MC_TRUE;
}
