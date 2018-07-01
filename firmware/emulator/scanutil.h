//*****************************************************************************
// Filename : 'scanutil.h'
// Title    : Defines for command line input and scanning functionality
//*****************************************************************************

#ifndef SCANUTIL_H
#define SCANUTIL_H

#include "interpreter.h"

// mchron input stream reader functions
void cmdInputCleanup(cmdInput_t *cmdInput);
void cmdInputInit(cmdInput_t *cmdInput);
void cmdInputRead(char *prompt, cmdInput_t *cmdInput);

// mchron command line argument scanning functions
void cmdArgCleanup(cmdLine_t *cmdLine);
int cmdArgInit(char **input, cmdLine_t *cmdLine);
int cmdArgPublish(cmdLine_t *cmdLine);
int cmdArgRead(char *input, cmdLine_t *cmdLine);
int cmdArgValuePrint(double value, int detail);
#endif
