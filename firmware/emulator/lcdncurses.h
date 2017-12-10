//*****************************************************************************
// Filename : 'lcdncurses.h'
// Title    : Lcd ncurses definitions for emuchron emulator
//*****************************************************************************

#ifndef LCDNCURSES_H
#define LCDNCURSES_H

// Max length ncurses tty and the file in $HOME holding the default tty
#define NCURSES_TTYLEN		100
#define NCURSES_TTYFILE		"/.mchron"

// Definition of a structure holding the ncurses lcd init parameters
typedef struct _lcdNcurInitArgs_t
{
  char tty[NCURSES_TTYLEN + 1];	// ncurses tty
  void (*winClose)(void);	// mchron callback upon ncurses window close
} lcdNcurInitArgs_t;

// Lcd device control methods
void lcdNcurCleanup(void);
void lcdNcurFlush(void);
int lcdNcurInit(lcdNcurInitArgs_t *lcdNcurInitArgs);

// Lcd device statistics methods
void lcdNcurStatsPrint(void);
void lcdNcurStatsReset(void);

// Lcd device content methods
void lcdNcurGraphicsSet(unsigned char backlight);
void lcdNcurBacklightSet(unsigned char backlight);
void lcdNcurDataWrite(unsigned char x, unsigned char y, unsigned char data);
void lcdNcurDisplaySet(unsigned char controller, unsigned char display);
void lcdNcurStartLineSet(unsigned char controller, unsigned char startline);
#endif
