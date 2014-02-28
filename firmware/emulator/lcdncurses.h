//*****************************************************************************
// Filename : 'lcdncurses.h'
// Title    : LCD ncurses definitions for MONOCHRON Emulator
//*****************************************************************************

#ifndef LCDNCURSES_H
#define LCDNCURSES_H

// Definition of a structure to hold ncurses LCD device statistics
typedef struct _lcdNcurStats_t
{
  long long lcdNcurBitReq;    // Nbr of LCD bits processed (from bytes with ncur update)
  long long lcdNcurBitCnf;    // Nbr of LCD bits leading to ncurses update
  long long lcdNcurByteReq;   // Nbr of LCD bytes processed
  long long lcdNcurByteCnf;   // Nbr of LCD bytes leading to ncurses update
} lcdNcurStats_t;

// LCD device control methods
void lcdNcurEnd(void);
void lcdNcurFlush(int force);
int lcdNcurInit(char *lcdNcurTty, void (*lcdNcurWinClose)(void));
void lcdNcurRestore(void);
void lcdNcurStatsGet(lcdNcurStats_t *lcdNcurStats);
void lcdNcurStatsReset(void);

// LCD device content methods
void lcdNcurBacklightSet(unsigned char backlight);
void lcdNcurDataWrite(unsigned char x, unsigned char y, unsigned char data);
#endif
