//*****************************************************************************
// Filename : 'lcdncurses.h'
// Title    : Lcd ncurses definitions for emuchron emulator
//*****************************************************************************

#ifndef LCDNCURSES_H
#define LCDNCURSES_H

// Max length ncurses tty and the file in MCHRON_CONFIG holding the default tty
#define NCURSES_TTYLEN	100
#define LCD_TTYFILE	"/tty"
#define MON_TTYFILE	"/ttyMon"

// The ncurses monitor activity mode
#define MON_MODE_AGGREGATE	0
#define MON_MODE_PAUSED		1
#define MON_MODE_SNAPSHOT	2

// Definition of a structure holding the ncurses lcd and monitor init parameters
typedef struct _ncurInitArgs_t
{
  u08 useLcd;				// Do we use an lcd tty
  u08 useMon;				// Do we use a monitor tty
  char ttyLcd[NCURSES_TTYLEN + 1];	// ncurses lcd tty
  char ttyMon[NCURSES_TTYLEN + 1];	// ncurses monitor tty
  char monColor;			// monitor color scheme
  void (*winClose)(void);		// mchron callback upon window close
} ncurInitArgs_t;

// Lcd device control methods
void lcdNcurCleanup(void);
void lcdNcurFlush(void);
u08 ncurInit(ncurInitArgs_t *ncurInitArgs);

// Lcd device statistics methods
void lcdNcurStatsPrint(void);
void lcdNcurStatsReset(void);

// Lcd device content methods
void lcdNcurBacklightSet(u08 backlight);
void lcdNcurDataWrite(u08 x, u08 y, u08 data);
void lcdNcurDisplaySet(u08 controller, u08 display);
void lcdNcurGraphicsSet(u08 useBacklight);
void lcdNcurStartLineSet(u08 controller, u08 startline);

// Emuchron monitor methods
void monModeSet(u08 mode);
#endif
