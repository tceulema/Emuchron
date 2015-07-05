//*****************************************************************************
// Filename : 'lcd.h'
// Title    : LCD stub definitions for MONOCHRON Emulator
//*****************************************************************************

#ifndef LCD_H
#define LCD_H

// The file in $HOME holding the ncurses tty
#define NCURSES_TTYFILE		"/.mchron"

// Definition of a structure to hold LCD device init related data
typedef struct _lcdDeviceParam_t
{
  u08 useNcurses;               // Will we use ncurses device
  u08 useGlut;                  // Will we use glut device
  char lcdNcurTty[50];          // The ncurses tty
  int lcdGlutPosX;              // The glut startup x position
  int lcdGlutPosY;              // The glut startup y position
  int lcdGlutSizeX;             // The glut window x size
  int lcdGlutSizeY;             // The glut window y size
  void (*winClose)(void);       // Callback when end-user closes LCD device window
} lcdDeviceParam_t;

// LCD device stubs
void lcdDeviceEnd(void);
void lcdDeviceFlush(int force);
int lcdDeviceInit(lcdDeviceParam_t lcdDeviceParam);
void lcdDeviceRestore(void);
void lcdDeviceBacklightSet(u08 brightness);

// LCD device statistics
void lcdStatsPrint(void);
void lcdStatsReset(void);

// LCD buffer stubs
u08 lcdReadStub(u08 controller);
void lcdWriteStub(u08 data);
#endif
