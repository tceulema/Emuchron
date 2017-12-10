//*****************************************************************************
// Filename : 'scanutil.h'
// Title    : Defines for command line input, scan and dictionary functionality
//*****************************************************************************

#ifndef SCANUTIL_H
#define SCANUTIL_H

#include "interpreter.h"

// mchron input stream reader functions
void cmdInputCleanup(cmdInput_t *cmdInput);
void cmdInputInit(cmdInput_t *cmdInput);
void cmdInputRead(char *prompt, cmdInput_t *cmdInput);

// mchron command dictionary functions
int cmdDictCmdGet(char *cmd, cmdCommand_t **cmdCommand);
int cmdDictPrint(char *pattern);

// mchron command line argument scanning functions
void cmdArgInit(char **input);
int cmdArgScan(cmdArg_t cmdArg[], int argCount, char **input, int silent);
int cmdArgValuePrint(double value, int detail);
#endif
