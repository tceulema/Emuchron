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
  unsigned char active;		// Is toggle pixel functionality active
  unsigned char pixelLock;	// Double-click event occured
  unsigned char glcdX;		// The glcd x pixel coordinate
  unsigned char glcdY;		// The glcd y pixel coordinate
  unsigned char glcdPix;	// The glcd pixel value
} lcdGlutGlcdPix_t;

// Lcd device control methods
void lcdGlutCleanup(void);
void lcdGlutFlush(void);
unsigned char lcdGlutInit(lcdGlutInitArgs_t *lcdGlutInitArgsSet);

// Lcd device statistics methods
void lcdGlutStatsPrint(void);
void lcdGlutStatsReset(void);

// Lcd device content methods
void lcdGlutBacklightSet(unsigned char backlight);
void lcdGlutDataWrite(unsigned char x, unsigned char y, unsigned char data);
void lcdGlutDisplaySet(unsigned char controller, unsigned char display);
void lcdGlutGraphicsSet(unsigned char bezel, unsigned char grid);
void lcdGlutHighlightSet(unsigned char highlight, unsigned char x,
  unsigned char y);
void lcdGlutStartLineSet(unsigned char controller, unsigned char startline);
#endif
