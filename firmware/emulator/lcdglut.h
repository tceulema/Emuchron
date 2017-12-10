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

// Lcd device control methods
void lcdGlutCleanup(void);
void lcdGlutFlush(void);
int lcdGlutInit(lcdGlutInitArgs_t *lcdGlutInitArgsSet);

// Lcd device statistics methods
void lcdGlutStatsPrint(void);
void lcdGlutStatsReset(void);

// Lcd device content methods
void lcdGlutBacklightSet(unsigned char backlight);
void lcdGlutDataWrite(unsigned char x, unsigned char y, unsigned char data);
void lcdGlutDisplaySet(unsigned char controller, unsigned char display);
void lcdGlutGraphicsSet(unsigned char bezel, unsigned char grid);
void lcdGlutStartLineSet(unsigned char controller, unsigned char startline);
#endif
