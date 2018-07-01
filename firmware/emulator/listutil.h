//*****************************************************************************
// Filename : 'listutil.h'
// Title    : Defines for command list and command execution functionality
//*****************************************************************************

#ifndef LISTUTIL_H
#define LISTUTIL_H

#include "interpreter.h"

// mchron interpreter command line/list execution functions
int cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput);
int cmdListExecute(cmdLine_t *cmdLineRoot, char *source);

// mchron command list functions
void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdPcCtrl_t *cmdPcCtrlRoot);
int cmdListFileLoad(cmdLine_t **cmdLineRoot, cmdPcCtrl_t **cmdPcCtrlRoot,
  char *fileName, int fileExecDepth);
#endif
