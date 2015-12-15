//*****************************************************************************
// Filename : 'lcd.h'
// Title    : Lcd stub definitions for MONOCHRON Emulator
//*****************************************************************************

#ifndef LCD_H
#define LCD_H

// The file in $HOME holding the ncurses tty
#define NCURSES_TTYFILE		"/.mchron"
#define NCURSES_TTYLEN		100

// Definition of a structure to hold lcd device init related data
typedef struct _lcdDeviceParam_t
{
  u08 useNcurses;               // Will we use ncurses device
  u08 useGlut;                  // Will we use glut device
  char lcdNcurTty[NCURSES_TTYLEN + 1]; // The ncurses tty
  int lcdGlutPosX;              // The glut startup x position
  int lcdGlutPosY;              // The glut startup y position
  int lcdGlutSizeX;             // The glut window x size
  int lcdGlutSizeY;             // The glut window y size
  void (*winClose)(void);       // Callback when end-user closes lcd device window
} lcdDeviceParam_t;

// Lcd device stubs
void lcdDeviceEnd(void);
void lcdDeviceFlush(int force);
int lcdDeviceInit(lcdDeviceParam_t lcdDeviceParam);
void lcdDeviceRestore(void);
void lcdDeviceBacklightSet(u08 brightness);

// Lcd device statistics
void lcdStatsPrint(void);
void lcdStatsReset(void);

// Lcd buffer stubs
u08 lcdReadStub(u08 controller);
void lcdWriteStub(u08 data);
#endif
