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
void cmdInputInit(cmdInput_t *cmdInput, FILE *file, u08 readMethod);
void cmdInputRead(char *prompt, cmdInput_t *cmdInput);

// mchron command line argument scanning functions
void cmdArgCleanup(cmdLine_t *cmdLine);
u08 cmdArgInit(char **input, cmdLine_t *cmdLine);
u08 cmdArgPublish(cmdLine_t *cmdLine);
u08 cmdArgRead(char *input, cmdLine_t *cmdLine);

// mchron command line breakpoint argument scanning functions
void cmdArgBpCleanup(cmdLine_t *cmdline);
u08 cmdArgBpExecute(argInfo_t *argInfoBp);
void cmdArgBpCreate(char *condition, cmdLine_t *cmdline);
#endif
