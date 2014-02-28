//*****************************************************************************
// Filename : 'lcdglut.h'
// Title    : LCD glut definitions for MONOCHRON Emulator
//*****************************************************************************

#ifndef LCDGLUT_H
#define LCDGLUT_H

// Definition of a structure to hold glut LCD device statistics
typedef struct _lcdGlutStats_t
{
  long long lcdGlutMsgSend;       // Nbr of msgs sent
  long long lcdGlutMsgRcv;        // Nbr of msgs received
  long long lcdGlutBitReq;        // Nbr of LCD bits processed (from bytes with glut update)
  long long lcdGlutBitCnf;        // Nbr of LCD bits leading to glut update
  long long lcdGlutByteReq;       // Nbr of LCD bytes processed
  long long lcdGlutByteCnf;       // Nbr of LCD bytes leading to glut update
  int lcdGlutRedraws;	          // Nbr of glut window redraws (by glut event or glut msg)
  int lcdGlutQueueMax;            // Max length of glut message queue
  long long lcdGlutQueueEvents;   // Nbr of times the glut message queue is processed
  struct timeval lcdGlutTimeStart;// Timestamp start of glut interface
  long long lcdGlutTicks;         // Nbr of glut thread cycles
} lcdGlutStats_t;

// LCD device control methods
void lcdGlutEnd(void);
void lcdGlutFlush(int force);
int lcdGlutInit(int lcdGlutPosX, int lcdGlutPosY, int lcdGlutSizeX, int lcdGlutSizeY,
  void (*lcdGlutWinClose)(void));
void lcdGlutRestore(void);
void lcdGlutStatsGet(lcdGlutStats_t *lcdGlutStats);
void lcdGlutStatsReset(void);

// LCD device content methods
void lcdGlutBacklightSet(unsigned char backlight);
void lcdGlutDataWrite(unsigned char x, unsigned char y, unsigned char data);
#endif
