//*****************************************************************************
// Filename : 'listvarutil.h'
// Title    : Defines for command lists and variable support functionality
//*****************************************************************************

#ifndef LISTVARUTIL_H
#define LISTVARUTIL_H

#include "interpreter.h"

// mchron command list functions
void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdPcCtrl_t *cmdPcCtrlRoot);
int cmdListFileLoad(cmdLine_t **cmdLineRoot, cmdPcCtrl_t **cmdPcCtrlRoot,
  char *fileName, int fileExecDepth);
int cmdListKeyboardLoad(cmdLine_t **cmdLineRoot, cmdPcCtrl_t **cmdPcCtrlRoot,
  cmdInput_t *cmdInput, int fileExecDepth);
char *cmdPcCtrlArgCreate(char *argExpr);

// mchron variable support functions
int varClear(char *argName, char *var);
void varInit(void);
int varPrint(char *argName, char *var);
void varReset(void);

// Functions for referencing and manipulating variables
int varIdGet(char *var);
double varValGet(int varId, int *varError);
double varValSet(int varId, double value);
#endif
