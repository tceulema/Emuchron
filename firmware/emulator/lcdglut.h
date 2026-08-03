//*****************************************************************************
// Filename : 'lcdglut.h'
// Title    : Lcd glut definitions for emuchron emulator
//*****************************************************************************

#ifndef LCDGLUT_H
#define LCDGLUT_H

// Definition of a structure holding the glut lcd init parameters
typedef struct _lcdGlutInitArgs_t
{
  int posX;			// Glut window x position
  int posY;			// Glut window y position
  int sizeX;			// Glut window x size in px
  int sizeY;			// Glut window y size in px
  void (*winClose)(void);	// mchron callback upon glut window close
} lcdGlutInitArgs_t;

// Definition of a structure to communicate about a double-clicked pixel
typedef struct _lcdGlutGlcdPix_t
{
  u08 active;			// Is toggle pixel functionality active
  u08 pixelLock;		// Double-click event occured
  u08 glcdX;			// The glcd x pixel coordinate
  u08 glcdY;			// The glcd y pixel coordinate
} lcdGlutGlcdPix_t;

// Lcd device control methods
void lcdGlutCleanup(void);
void lcdGlutFlush(void);
u08 lcdGlutInit(lcdGlutInitArgs_t *lcdGlutInitArgsSet);

// Lcd device statistics methods
void lcdGlutStatsPrint(void);
void lcdGlutStatsReset(void);

// Lcd device content methods
void lcdGlutBacklightSet(u08 backlight);
void lcdGlutDataWrite(u08 x, u08 y, u08 data);
void lcdGlutDisplaySet(u08 controller, u08 display);
void lcdGlutGraphicsSet(u08 bezel, u08 grid);
void lcdGlutHighlightSet(u08 highlight, u08 x, u08 y);
void lcdGlutSizeSet(char axis, u16 size);
void lcdGlutStartLineSet(u08 controller, u08 startline);
#endif
