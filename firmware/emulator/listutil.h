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

// List stack timer actions
#define LIST_TIMER_DISARM	0
#define LIST_TIMER_ARM		1

// List debug commands
// Note these work independent from debug breakpoints and the 'dcc' command
#define DEBUG_NONE		0	// No active debug command
#define DEBUG_STEP_NEXT		1	// Execute next line
#define DEBUG_STEP_IN		2	// Enter 'e' script or exec next line
#define DEBUG_STEP_OUT		3	// Halt at end of current stack level
#define DEBUG_CONT		4	// Continue execution
#define DEBUG_HALT		5	// Halt ahead of next cmd or at stack
					// level eof (internal)
#define DEBUG_HALT_EXIT		6	// Halt ahead of next cmd but not at
					// stack level eof (internal)

// mchron user-entered command shell command line execution function
u08 cmdExecute(cmdInput_t *cmdInput);

// mchron command list stack functions
u08 cmdStackListPrint(u08 level, s16 range);
u08 cmdStackActiveGet(void);
void cmdStackCleanup(void);
void cmdStackInit(void);
u08 cmdStackPush(cmdLine_t *cmdLine, u08 echoReq, char *cmdOrigin,
  cmdInput_t *cmdInput);
u08 cmdStackPrint(u08 status);
u08 cmdStackResume(char *cmdName);
void cmdStackStatsSet(u08 enable);
void cmdStackTimerSet(u08 action);

// mchron command list debug functions
u08 cmdDebugActiveGet(void);
u08 cmdDebugBpAdd(u08 level, u16 line, char *condition);
u08 cmdDebugBpDelete(u08 level, u16 breakpoint, int *count);
void cmdDebugBpPrint(void);
u08 cmdDebugCmdSet(s08 offset, u08 command);
u08 cmdDebugPcSet(u16 line);
void cmdDebugSet(u08 enable);
#endif
