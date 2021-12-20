//*****************************************************************************
// Filename : 'interpreter.h'
// Title    : Common defines for mchron interpreter
//*****************************************************************************

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <stdio.h>
#include "../avrlibtypes.h"

// The mchron configuration folder relative to $HOME
#define MCHRON_CONFIG		"/.config/mchron"

// The program counter control block execution logic types
#define PC_CONTINUE		0
#define PC_REPEAT_FOR		1
#define PC_REPEAT_NEXT		2
#define PC_IF			3
#define PC_IF_ELSE_IF		4
#define PC_IF_ELSE		5
#define PC_IF_END		6

// The command argument publishing types
#define ARG_CHAR		0  // A char in argChar[]
#define ARG_STRING		1  // A string in argString[]
#define ARG_NUM			2  // A double in argDouble[]

// The argument domain value validation types
// 1) Use in combination with ARG_CHAR command argument
#define DOM_CHAR_VAL		0  // Validated single character
// 2) Use in combination with ARG_STRING command argument
#define DOM_WORD_VAL		10 // Validated string delimited by whitespace
#define DOM_WORD_REGEX		11 // Regex validated string delim by whitespace
#define DOM_STRING		12 // Non-empty string with whitespace chars
#define DOM_STRING_OPT		13 // Optional string with whitespace chars
// 3) Use in combination with ARG_NUM command argument
#define DOM_NUM			20 // Expression for double
#define DOM_NUM_RANGE		21 // Expression for double in min/max range
#define DOM_NUM_ASSIGN		22 // Assignment expression for double

// The command input read methods
#define CMD_INPUT_READLINELIB	0
#define CMD_INPUT_MANUAL	1

// The command echo options
#define CMD_ECHO_NO		0
#define CMD_ECHO_YES		1
#define CMD_ECHO_INHERIT	2

// The command return values
#define CMD_RET_OK		0
#define CMD_RET_EXIT		1
#define CMD_RET_ERROR		2
#define CMD_RET_INTERRUPT	3
#define CMD_RET_RECOVER		4

// To prevent recursive file loading define a max file depth when executing
// command files
#define CMD_FILE_DEPTH_MAX	8

// The max number of mchron command line arguments per argument type
#define ARG_TYPE_COUNT_MAX	10

// Generic stack trace header and line
#define CMD_STACK_TRACE		"--- stack trace ---\n"
#define CMD_STACK_FMT		"%d:%s:%d:%s\n"
#define CMD_STACK_ROOT_FMT	"%d:%s:-:%s\n"

// Definition of a structure holding a single command line, originating from
// the command line prompt or from a command file
typedef struct _cmdLine_t
{
  int lineNum;				// Line number
  char *input;				// The malloc-ed command from file/prompt
  char **args;				// The command arguments
  u08 initialized;			// Are command arguments initialized
  struct _cmdCommand_t *cmdCommand;	// The associated command dictionary entry
  struct _cmdPcCtrl_t *cmdPcCtrlParent;	// Control block completed by this line
  struct _cmdPcCtrl_t *cmdPcCtrlChild;	// Control block started by this line
  struct _cmdLine_t *next;		// Pointer to next list element
} cmdLine_t;

// Definition of a structure holding a program counter control block for
// if-then-else and repeat commands
typedef struct _cmdPcCtrl_t
{
  u08 cmdPcCtrlType;			// The program counter control block type
  u08 active;				// Is current block the active code block
  cmdLine_t *cmdLineParent;		// Pointer to associated parent command
  cmdLine_t *cmdLineChild;		// Pointer to associated child command
  struct _cmdPcCtrl_t *prev;		// Pointer to previous list element
  struct _cmdPcCtrl_t *next;		// Pointer to next list element
} cmdPcCtrl_t;

// Definition of a structure holding the parameters for reading input
// lines from an input stream, being either command line or file
typedef struct _cmdInput_t
{
  FILE *file;				// Input stream (stdin or file)
  char *input;				// Pointer to resulting single input line
  u08 readMethod;			// Input read method (readline or manual)
  u08 initialized;			// Structure initialized indicator
} cmdInput_t;

// Definition of a structure holding domain info for a command argument
typedef struct _cmdDomain_t
{
  char *domName;			// Domain structure name
  u08 domType;				// Domain type
  char *domTextList;			// Char/word/regex value list
  double domNumMin;			// Numeric domain min value
  double domNumMax;			// Numeric domain max value
  char *domInfo;			// Additional domain info
} cmdDomain_t;

// Definition of a structure holding a command line argument
typedef struct _cmdArg_t
{
  u08 argType;				// Argument type
  char *argName;			// Argument name
  cmdDomain_t *cmdDomain;		// Argument domain
} cmdArg_t;

// Definition of a structure holding a single mchron command with argument
// profiles and command handler function
typedef struct _cmdCommand_t
{
  char *cmdName;			// The mchron command name
  u08 cmdPcCtrlType;			// Program counter control block type
  char *cmdArgName;			// Argument structure name
  cmdArg_t *cmdArg;			// Array of command argument profiles
  int argCount;				// Profile argument count
  char *cmdHandlerName;			// Execution handler name
  u08 (*cmdHandler)(cmdLine_t *);	// Handler for regular commands
  u08 (*cbHandler)(cmdLine_t **);	// Handler for control block commands
  char *cmdNameDescr;			// Command name description
} cmdCommand_t;

// Definition of a structure holding a command group containing all mchron
// commands for the group.
// In array form this structure will create the mchron command dictionary.
typedef struct _cmdDict_t
{
  char cmdGroup;			// The mchron command group identifier
  char *cmdGroupDescr;			// The command group description
  cmdCommand_t *cmdCommand;		// Array of mchron commands in group
  int commandCount;			// Command group size
} cmdDict_t;
#endif
