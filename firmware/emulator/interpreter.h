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

// The program counter control block (pcb) execution logic types
#define PCB_CONTINUE		0  // Non-program counter control block command
#define PCB_REPEAT_FOR		1
#define PCB_REPEAT_BRK		2
#define PCB_REPEAT_CONT		3
#define PCB_REPEAT_NEXT		4
#define PCB_IF			5
#define PCB_IF_ELSE_IF		6
#define PCB_IF_ELSE		7
#define PCB_IF_END		8
#define PCB_RETURN		9

// The program counter control block (pcb) execution actions.
// Each pcb logic type has a default action that will be executed when the pcb
// predecessor command is a non-pcb command. When the predecessor command is
// a pcb from the same repeat or if group, that predecessor pcb may set an
// alternative action to be executed by the pcb.
#define PCB_ACT_DEFAULT		0 // Execute default action in pcb
#define PCB_ACT_ALT_1		1 // Execute alternative action 1 in pcb

// The max number of mchron command line arguments per argument publishing type
#define ARG_TYPE_COUNT_MAX	10

// The command argument publishing types
#define ARG_CHAR		0  // A char in argChar[]
#define ARG_STRING		1  // A string in argString[]
#define ARG_NUM			2  // A double in argDouble[]

// The argument domain value validation types
// 1) Use in combination with ARG_CHAR command argument publishing type
#define DOM_CHAR_VAL		0  // Validated single character
// 2) Use in combination with ARG_STRING command argument publishing type
#define DOM_WORD_VAL		10 // Validated string delimited by whitespace
#define DOM_WORD_REGEX		11 // Regex validated string delim by whitespace
#define DOM_STRING		12 // Non-empty string with whitespace chars
#define DOM_STRING_OPT		13 // Optional string with whitespace chars
// 3) Use in combination with ARG_NUM command argument publishing type
#define DOM_NUM			20 // Expression for double
#define DOM_NUM_RANGE		21 // Expression for double in min/max range
#define DOM_NUM_ASSIGN		22 // Assignment expression for double

// The command input read methods
#define CMD_INPUT_READLINELIB	0  // Use readline library to create lines
#define CMD_INPUT_MANUAL	1  // Read and create input lines manually

// The command echo options
#define CMD_ECHO_NONE		0  // Undefined
#define CMD_ECHO_NO		1  // Do not echo command
#define CMD_ECHO_YES		2  // Echo command

// The command return values
#define CMD_RET_OK		0  // Success
#define CMD_RET_EXIT		1  // End-user mchron exit
#define CMD_RET_ERROR		2  // Error occured (scan/syntax/parse/internal)
#define CMD_RET_INTR		3  // End-user 'q' or breakpoint interrupt
#define CMD_RET_INTR_CMD	4  // Wait cmd 'q' or breakpoint cmd interrupt
#define CDM_RET_LOAD_ABORT	5  // Interactive loading of script aborted
#define CMD_RET_RECOVER		6  // Stack recover from error/interrupt/abort

// Definition of a structure holding an argument value and several numeric
// expression result properties
typedef struct _argInfo_t
{
  char *arg;				// The malloc-ed command argument
  u08 exprAssign;			// Is argument an assignment expression
  u08 exprConst;			// Is result a constant numeric value
  double exprValue;			// The resulting expression value
} argInfo_t;

// Definition of a structure holding a single command line, originating from
// the command line prompt or from a command file
typedef struct _cmdLine_t
{
  int lineNum;				// Line number
  char *input;				// Malloc-ed command from file/prompt
  argInfo_t *argInfo;			// Malloc-ed command arguments
  argInfo_t *argInfoBp;			// Malloc-ed breakpoint argument
  u08 initialized;			// Are command arguments initialized
  struct _cmdCommand_t *cmdCommand;	// The associated command dict entry
  // Chaining all list commands
  struct _cmdLine_t *next;		// Next command in list
  // Chaining all program counter control block (pcb) list commands
  struct _cmdLine_t *pcbPrev;		// Previous pcb command in list
  struct _cmdLine_t *pcbNext;		// Next pcb command in list
  // Chaining all pcb commands belonging to a single if or repeat pcb group
  u08 pcbAction;			// Pcb command action to be executed
  struct _cmdLine_t *pcbGrpNext;	// Next pcb in group
  struct _cmdLine_t *pcbGrpHead;	// First in group (if/repeat)
  struct _cmdLine_t *pcbGrpTail;	// Last in group (if-end/repeat-next)
  // Chaining all breakpoints
  struct _cmdLine_t *bpNext;		// Next breakpoint in list
} cmdLine_t;

// Definition of a structure holding the parameters for reading input lines
// from an input stream, being either command line or file
typedef struct _cmdInput_t
{
  FILE *file;				// Input stream (stdin/file)
  char *input;				// Resulting single input line
  u08 readMethod;			// Input read method (readline/manual)
  u08 initialized;			// Structure initialized indicator
} cmdInput_t;

// Definition of a structure holding domain info for a command argument
typedef struct _cmdDomain_t
{
  char *domName;			// Domain structure name
  char *domTypeName;			// Domain type name
  u08 domType;				// Domain type
  char *domTextList;			// Char/word/regex value list
  double domNumMin;			// Numeric domain min value
  double domNumMax;			// Numeric domain max value
  char *domInfo;			// Additional domain info
} cmdDomain_t;

// Definition of a structure holding a command line argument
typedef struct _cmdArg_t
{
  char *argTypeName;			// Argument type name
  u08 argType;				// Argument type
  char *argName;			// Argument name
  cmdDomain_t *cmdDomain;		// Argument domain
} cmdArg_t;

// Definition of a structure holding a single mchron command with argument
// profiles and command handler function
typedef struct _cmdCommand_t
{
  char *cmdName;			// The mchron command name
  char *cmdPcbTypeName;			// Program counter ctrl block type name
  u08 cmdPcbType;			// Program counter ctrl block type
  u08 cmdDbSupport;			// Supports debug and debug step
  char *cmdArgName;			// Argument structure name
  cmdArg_t *cmdArg;			// Array of command argument profiles
  int argCount;				// Profile argument count
  char *cmdHandlerName;			// Execution handler name
  u08 (*cmdHandler)(cmdLine_t *);	// Handler for regular commands
  u08 (*pcbHandler)(cmdLine_t **);	// Handler for control block commands
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
  int commandCount;			// Command group command count
} cmdDict_t;
#endif
