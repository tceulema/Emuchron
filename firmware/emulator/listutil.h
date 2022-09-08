//*****************************************************************************
// Filename : 'listutil.h'
// Title    : Defines for command list and command execution functionality
//*****************************************************************************

#ifndef LISTUTIL_H
#define LISTUTIL_H

#include "../avrlibtypes.h"
#include "interpreter.h"

// Command list echo request options
#define LIST_ECHO_ECHO		0
#define LIST_ECHO_INHERIT	1
#define LIST_ECHO_SILENT	2

// mchron single command line execution functions
void cmdLineCleanup(cmdLine_t *cmdLine);
cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot);
u08 cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput);

// mchron command list stack functions
void cmdStackCleanup(void);
void cmdStackInit(void);
u08 cmdStackIsActive(void);
void cmdStackPrintSet(u08 enable);
u08 cmdStackPush(cmdLine_t *cmdLine, u08 echoReq, char *cmdOrigin,
  cmdInput_t *cmdInput);
u08 cmdStackResume(void);
#endif
