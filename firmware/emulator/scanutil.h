//*****************************************************************************
// Filename : 'scanutil.h'
// Title    : Defines for command line input and scanning functionality
//*****************************************************************************

#ifndef SCANUTIL_H
#define SCANUTIL_H

#include "../avrlibtypes.h"
#include "interpreter.h"

// mchron input stream reader functions
void cmdInputCleanup(cmdInput_t *cmdInput);
void cmdInputInit(cmdInput_t *cmdInput);
void cmdInputRead(char *prompt, cmdInput_t *cmdInput);

// mchron command line argument scanning functions
void cmdArgCleanup(cmdLine_t *cmdLine);
u08 cmdArgInit(char **input, cmdLine_t *cmdLine);
u08 cmdArgPublish(cmdLine_t *cmdLine);
u08 cmdArgRead(char *input, cmdLine_t *cmdLine);
u08 cmdArgValuePrint(double value, u08 detail, u08 forceNewline);
#endif
