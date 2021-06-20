//*****************************************************************************
// Filename : 'listutil.h'
// Title    : Defines for command list and command execution functionality
//*****************************************************************************

#ifndef LISTUTIL_H
#define LISTUTIL_H

#include "../global.h"
#include "interpreter.h"

// mchron interpreter command line/list execution functions
u08 cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput);
u08 cmdListExecute(cmdLine_t *cmdLineRoot, char *source);

// mchron interpreter command list execution statistics functions
void cmdListStatsInit(void);
void cmdListStatsPrint(void);

// mchron command line/list functions
cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot);
void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdPcCtrl_t *cmdPcCtrlRoot);
u08 cmdListFileLoad(cmdLine_t **cmdLineRoot, cmdPcCtrl_t **cmdPcCtrlRoot,
  char *argName, char *fileName, int fileExecDepth);
#endif
