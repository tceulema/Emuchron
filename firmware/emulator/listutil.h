//*****************************************************************************
// Filename : 'listvarutil.h'
// Title    : Defines for command list functionality
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
#endif
