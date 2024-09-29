//*****************************************************************************
// Filename : 'listutil.c'
// Title    : Command list and execution utility routines for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>

// Monochron and emuchron defines
#include "../global.h"
#include "mchronutil.h"
#include "scanutil.h"
#include "listutil.h"

// To prevent recursive file loading define a max stack depth when executing
// command files. Upon changing this also change domain domNumStackLevel in
// mchrondict.h [firmware/emulator].
#define CMD_STACK_DEPTH_MAX	8

// The command stack keyboard scan interval (msec)
#define CMD_STACK_SCAN_MSEC	100

// The command stack pop scope
#define CMD_STACK_POP_ALL	0	// Pop all levels (clear stack)
#define CMD_STACK_POP_LVL	1	// Pop current stack level only

// Stack trace header and line templates
#define CMD_STACK_NOTIFY	"stack trace:\n"
#define CMD_STACK_NFY_INTR_CMD	"stack trace (command interrupt):\n"
#define CMD_STACK_HEADER	"lvl  filename         line#  command\n"
#define CMD_STACK_EOF_FMT	" %2d  %-15s  <eof>  -\n"
#define CMD_STACK_FMT		" %2d  %-15s  %5d  %s\n"
#define CMD_STACK_INVOKE_FMT	"  -  %-15s      -  %s\n"

// Source listing header and line templates
#define CMD_SOURCE_NOTIFY	"source list:\n"
#define CMD_SOURCE_HEADER	"lvl  line#  pc   b  command\n"
#define CMD_SOURCE_LVL_LINE_FMT	" %2d  %5d  "
#define CMD_SOURCE_PC		"==>  "
#define CMD_SOURCE_NO_PC	"     "
#define CMD_SOURCE_BP		"@  "
#define CMD_SOURCE_INACT_BP	"O  "
#define CMD_SOURCE_NO_BP	"   "
#define CMD_SOURCE_COMMAND_FMT	"%s\n"
#define CMD_SOURCE_EOF_FMT	" %2d  <eof>  ==>     -\n"

// Definition of a structure holding stack execution runtime statistics
typedef struct _cmdStackStats_t
{
  struct timeval cmdTvStart;	// Stack execution start timestamp
  long long cmdCmdCount;	// Number of commands executed
  long long cmdLineCount;	// Number of lines executed
} cmdStackStats_t;

// Definition of a structure holding a single stack level
typedef struct _cmdStackLevel_t
{
  cmdLine_t *cmdProgCounter;	// Active command line
  cmdLine_t *cmdLineRoot;	// Root of command lines
  cmdLine_t *cmdLineBpRoot;	// Root of command lines with breakpoint
  u08 cmdEcho;			// Command echo
  char *cmdOrigin;		// Commands origin (file or cmdline)
  u08 cmdDebugCmd;		// Active debug command
  int cmdLines;			// Number of command lines in stack level
  int cmdDebugLines;		// Number of breakpoints in stack level
} cmdStackLevel_t;

// Definition of a structure holding the script run stack.
// What do elements level and levelResume combinations mean:
// level  levelResume  Stack status
// -----+------------+-------------
//    -1           -1  Empty stack
//    -1          >=0  Interrupted stack that can be resumed/continued
//   >=0           -1  Active stack; commands are run from the stack
//   >=0          >=0  Undefined (not supported)
typedef struct _cmdStack_t
{
  s08 level;			// Current stack run level
  s08 levelResume;		// Execution resume stack level
  cmdLine_t *cmdLineInvoke;	// Command line that initiates stack
  cmdLine_t *cmdProgCtrIntr;	// Interrupted command line
  u08 bpInterrupt;		// Execution is interrupted by breakpoint
  cmdStackStats_t cmdStackStats; // Stack runtime statistics
  cmdStackLevel_t cmdStackLevel[CMD_STACK_DEPTH_MAX]; // The run stack
} cmdStack_t;

// The current command echo state
u08 cmdEcho = CMD_ECHO_YES;

// This is me
extern const char *__progname;

// System timer data for command list 100 msec keyboard scan
static timer_t kbTimer;
static u08 kbTimerTripped;

// Command list stack and execution statistics
static cmdStack_t cmdStack;
static u08 cmdStackStatsEnable = MC_TRUE;

// List debugging master switch and debug halted flag
static u08 cmdDebugActive = MC_FALSE;		// Is list debugging active
static u08 cmdDebugHalted = MC_FALSE;		// Exec halted due to step cmd

// Local function prototypes
// Command line
static cmdLine_t *cmdLineCopy(cmdLine_t *cmdLineIn);
static cmdLine_t *cmdLineCreate(int lineNum, char *input,
  cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot);
static u08 cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput);
static int cmdLineValidate(cmdLine_t **cmdPcbTail, cmdLine_t *cmdLine);
// Command list
static void cmdListCleanup(cmdLine_t *cmdLineRoot);
static u08 cmdListExecute(cmdLine_t **cmdProgCounter,
  cmdLine_t **cmdProgCtrIntr);
static u08 cmdListFileLoad(char *argName, char *fileName);
static u08 cmdListKeyboardLoad(cmdInput_t *cmdInput);
static void cmdListRaiseScan(void);
// Program counter control block (pcb)
static int cmdPcbLink(cmdLine_t **cmdPcbTail, cmdLine_t *cmdLine);
static void cmdPcbOpen(cmdLine_t **cmdPcbTail, cmdLine_t *cmdLine);
// Command stack
static s08 cmdStackLevelGet(void);
static void cmdStackPop(u08 scope);
static void cmdStackStatsInit(void);
static void cmdStackStatsPrint(void);
// Debugging
static u08 cmdDebugCmdGet(s08 offset);

//
// Function: cmdDebugActiveGet
//
// Returns whether conditional breakpoint commands are active
//
u08 cmdDebugActiveGet(void)
{
  return cmdDebugActive;
}

//
// Function: cmdDebugBpAdd
//
// Add or update a command list breakpoint
//
u08 cmdDebugBpAdd(u08 level, u16 line, char *condition)
{
  cmdStackLevel_t *cmdStackLevel;
  cmdLine_t *cmdLineBpHead;
  cmdLine_t *cmdLineBpTail;
  cmdLine_t *cmdLineSearch;
  s08 levelTop;
  u08 lineFound = MC_FALSE;

  // See if we have an active or interupted stack and check validity of
  // requested level
  levelTop = cmdStackLevelGet();
  if (levelTop == -1 || level > levelTop)
    return MC_FALSE;

  // Check validity of requested line number
  cmdStackLevel = &cmdStack.cmdStackLevel[level];
  if ((int)line > cmdStackLevel->cmdLines)
    return MC_FALSE;

  // First find the breakpoint chain range in which the line resides
  cmdLineBpHead = cmdStackLevel->cmdLineBpRoot;
  cmdLineBpTail = NULL;
  cmdLineSearch = cmdStackLevel->cmdLineBpRoot;
  while (cmdLineSearch != NULL)
  {
    if (cmdLineSearch->lineNum > line)
    {
      cmdLineBpTail = cmdLineSearch;
      break;
    }
    else
    {
      cmdLineBpHead = cmdLineSearch;
      cmdLineSearch = cmdLineSearch->bpNext;
    }
  }

  // Next, find the exact line in the range
  if (cmdStackLevel->cmdLineBpRoot == NULL || cmdLineBpHead->lineNum > line)
  {
    // We have a first root or one with a #line lower than current one
    cmdStackLevel->cmdLineBpRoot = NULL;
    cmdLineBpHead = NULL;
    cmdLineSearch = cmdStackLevel->cmdLineRoot;
  }
  else
  {
    cmdLineSearch = cmdLineBpHead;
  }
  lineFound = MC_FALSE;
  while (cmdLineSearch != NULL)
  {
    // See if we found the requested line
    if (cmdLineSearch->lineNum == line)
    {
      // Insert the breakpoint in the chain unless it is re-used
      if (cmdLineSearch != cmdLineBpHead)
      {
        cmdStackLevel->cmdDebugLines++;
        cmdLineSearch->bpNext = cmdLineBpTail;
        if (cmdLineBpHead != NULL)
          cmdLineBpHead->bpNext = cmdLineSearch;
      }
      if (cmdStackLevel->cmdLineBpRoot == NULL)
        cmdStackLevel->cmdLineBpRoot = cmdLineSearch;

      // Add/update the breakpoint argument in the command line
      cmdArgBpCreate(condition, cmdLineSearch);
      lineFound = MC_TRUE;
      break;
    }
    else
    {
      // Skip to next line
      cmdLineSearch = cmdLineSearch->next;
    }
  }

  return lineFound;
}

//
// Function: cmdDebugBpDelete
//
// Delete a single or all command list breakpoints.
// The number of breakpoints deleted is returned in argument count.
//
u08 cmdDebugBpDelete(u08 level, u16 breakpoint, int *count)
{
  cmdStackLevel_t *cmdStackLevel;
  cmdLine_t *cmdLineBpHead;
  cmdLine_t *cmdLineSearch;
  s08 levelTop;
  int toDo = 1;
  int i;

  // Init number of deletes done regardless of request success/failure
  *count = 0;

  // See if we have an active or interupted stack and check validity of
  // requested level
  levelTop = cmdStackLevelGet();
  if (levelTop == -1 || level > levelTop)
    return MC_FALSE;

  // Check validity of breakpoint line
  cmdStackLevel = &cmdStack.cmdStackLevel[level];
  if ((int)breakpoint > cmdStackLevel->cmdDebugLines)
    return MC_FALSE;

  // If we need to remove all breakpoints delete the first breakpoint in the
  // (remaining) list multiple times
  if (breakpoint == 0)
  {
    toDo = cmdStackLevel->cmdDebugLines;
    breakpoint = 1;
  }

  // Delete the requested breakpoint(s)
  while (*count < toDo)
  {
    // Find the line in the list breakpoint chain
    cmdLineBpHead = cmdStackLevel->cmdLineBpRoot;
    cmdLineSearch = cmdStackLevel->cmdLineBpRoot;
    for (i = 1; i < (int)breakpoint; i++)
    {
      cmdLineBpHead = cmdLineSearch;
      cmdLineSearch = cmdLineSearch->bpNext;
    }

    // Remove the breakpoint from the chain and its command line argument
    cmdStackLevel->cmdDebugLines--;
    if (cmdStackLevel->cmdLineBpRoot == cmdLineSearch)
      cmdStackLevel->cmdLineBpRoot = cmdLineSearch->bpNext;
    cmdLineBpHead->bpNext = cmdLineSearch->bpNext;
    cmdArgBpCleanup(cmdLineSearch);

    // Increment number of deletes done
    (*count)++;
  }

  return MC_TRUE;
}

//
// Function: cmdDebugBpPrint
//
// Print al breakpoints
//
void cmdDebugBpPrint(void)
{
  cmdLine_t *cmdLineBp;
  s08 level;
  u08 first = MC_TRUE;
  int levelCount;

  // Run through each stack level, starting at the top
  level = cmdStackLevelGet();
  while (level >= 0)
  {
    cmdLineBp = cmdStack.cmdStackLevel[level].cmdLineBpRoot;
    levelCount = 0;

    // Print all breakpoints from the level
    while (cmdLineBp != NULL)
    {
      // Print breakpoint header only once when first breakpoint is found
      if (first == MC_TRUE)
      {
        first = MC_FALSE;
        printf("breakpoints:\n");
        printf("lvl    id  line#  condition\n");
      }

      // Print breakpoint info and move to the next
      levelCount++;
      printf(" %2d  %4d  %5d  %s", (int)level, levelCount, cmdLineBp->lineNum,
        cmdLineBp->argInfoBp->arg);
      cmdLineBp = cmdLineBp->bpNext;
    }
    level--;
  }
}

//
// Function: cmdDebugCmdGet
//
// Get the active debug command for a stacklevel relative to the top stack
// level
//
static u08 cmdDebugCmdGet(s08 offset)
{
  s08 level;
  u08 cmdDebugCmd;

  // See if we have an active or interupted stack
  level = cmdStackLevelGet();

  // Exclude irrelevant/invalid request
  if (level == -1 || offset > 0)
    return DEBUG_NONE;
  else if (level + offset < 0)
    return DEBUG_NONE;

  // Get what we asked for
  cmdDebugCmd = cmdStack.cmdStackLevel[level + offset].cmdDebugCmd;

  return cmdDebugCmd;
}

//
// Function: cmdDebugCmdSet
//
// Set the active debug command for a stacklevel relative to the top stack
// level. When the request is to clear it skip partial error checking.
//
u08 cmdDebugCmdSet(s08 offset, u08 command)
{
  s08 i;
  s08 level;

  // See if we have an active or interupted stack
  level = cmdStackLevelGet();

  // Exclude irrelevant or invalid request
  if (level == -1)
  {
    // An error only occurs when we request a debug command for an empty stack
    if (command == DEBUG_NONE)
      return MC_TRUE;
    else
      return MC_FALSE;
  }

  // Clear any active stack debug command as it may be replaced by a new one
  for (i = 0; i <= level; i++)
    cmdStack.cmdStackLevel[i].cmdDebugCmd = DEBUG_NONE;

  // Can we actually set a new debug command
  if (offset > 0 || level + offset < 0)
    return MC_FALSE;

  // Set the new debug command on the level relative to the stack top level
  cmdStack.cmdStackLevel[level + offset].cmdDebugCmd = command;

  return MC_TRUE;
}

//
// Function: cmdDebugPcSet
//
// Set the top stack level program counter to another line
//
u08 cmdDebugPcSet(u16 line)
{
  cmdStackLevel_t *cmdStackLevel;
  cmdLine_t *cmdLineSearch;
  s08 levelTop;
  u08 cmdPcbType;

  // See if we have an active or interupted stack and validate requested line
  // number
  levelTop = cmdStackLevelGet();
  if (levelTop == -1)
    return MC_FALSE;
  cmdStackLevel = &cmdStack.cmdStackLevel[levelTop];
  if ((int)line > cmdStackLevel->cmdLines)
    return MC_FALSE;

  // Clear a debug interrupted indicator (if anyway) and reset pending pcb
  // action (if anyway) on the current line
  cmdStack.bpInterrupt = MC_FALSE;
  if (cmdStackLevel->cmdProgCounter != NULL)
    cmdStackLevel->cmdProgCounter->pcbAction = PCB_ACT_DEFAULT;

  // Set the new program counter
  if (line == 0)
  {
    // Line 0 means the script <eof>
    cmdStackLevel->cmdProgCounter = NULL;
  }
  else
  {
    // Find the requested script line and set it as the new program counter
    cmdLineSearch = cmdStackLevel->cmdLineRoot;
    while (MC_TRUE)
    {
      if (cmdLineSearch->lineNum == line)
        break;
      else
        cmdLineSearch = cmdLineSearch->next;
    }
    cmdStackLevel->cmdProgCounter = cmdLineSearch;

    // Override the default behavior of a pcb when needed
    if (cmdLineSearch->cmdCommand != NULL)
    {
      cmdPcbType = cmdLineSearch->cmdCommand->cmdPcbType;
      if (cmdPcbType == PCB_IF_ELSE_IF || cmdPcbType == PCB_IF_ELSE)
        cmdStackLevel->cmdProgCounter->pcbAction = PCB_ACT_ALT_1;
    }
  }

  // Print the new top level stack trace
  cmdDebugHalted = MC_TRUE;
  cmdStackPrint(CMD_RET_INTR);

  return MC_TRUE;
}

//
// Function: cmdDebugSet
//
// Enable/disable script debugging
//
void cmdDebugSet(u08 enable)
{
  cmdDebugActive = enable;
  cmdDebugHalted = MC_FALSE;
}

//
// Function: cmdExecute
//
// Execute a user-entered mchron shell command line command
//
u08 cmdExecute(cmdInput_t *cmdInput)
{
  static int lineNum = 0;
  cmdLine_t *cmdLine;
  u08 retVal;

  lineNum++;
  cmdLine = cmdLineCreate(lineNum, cmdInput->input, NULL, NULL);
  retVal = cmdLineExecute(cmdLine, cmdInput);
  cmdListCleanup(cmdLine);

  return retVal;
}

//
// Function: cmdLineCopy
//
// Copy a command line
//
static cmdLine_t *cmdLineCopy(cmdLine_t *cmdLineIn)
{
  cmdLine_t *cmdLine = NULL;
  int argCount = cmdLineIn->cmdCommand->argCount;
  int i = 0;
  size_t size;

  // Allocate a new command line and init using an existing command line
  cmdLine = malloc(sizeof(cmdLine_t));
  *cmdLine = *cmdLineIn;

  // Allocate and copy command line text
  size = strlen(cmdLineIn->input) + 1;
  cmdLine->input = malloc(size);
  memcpy(cmdLine->input, cmdLineIn->input, size);

  // Allocate and copy individually scanned command arguments
  cmdLine->argInfo = NULL;
  if (argCount > 0)
  {
    cmdLine->argInfo = malloc(sizeof(argInfo_t) * argCount);
    for (i = 0; i < argCount; i++)
    {
      size = strlen(cmdLineIn->argInfo[i].arg) + 1;
      cmdLine->argInfo[i].arg = malloc(size);
      memcpy(cmdLine->argInfo[i].arg, cmdLineIn->argInfo[i].arg, size);
      cmdLine->argInfo[i].exprAssign = cmdLineIn->argInfo[i].exprAssign;
      cmdLine->argInfo[i].exprConst = cmdLineIn->argInfo[i].exprConst;
      cmdLine->argInfo[i].exprValue = cmdLineIn->argInfo[i].exprValue;
    }
  }

  // Allocate and copy debug arguments
  cmdLine->argInfoBp = NULL;
  if (cmdLineIn->argInfoBp != NULL)
  {
    size = strlen(cmdLineIn->argInfoBp->arg) + 1;
    cmdLine->argInfoBp->arg = malloc(size);
    memcpy(cmdLine->argInfoBp->arg, cmdLineIn->argInfoBp->arg, size);
    cmdLine->argInfoBp->exprAssign = cmdLineIn->argInfoBp->exprAssign;
    cmdLine->argInfoBp->exprConst = cmdLineIn->argInfoBp->exprConst;
    cmdLine->argInfoBp->exprValue = cmdLineIn->argInfoBp->exprValue;
  }

  return cmdLine;
}

//
// Function: cmdLineCreate
//
// Create a new cmdLine structure, copy an input string into it, and, when
// provided, add it in the command linked list
//
static cmdLine_t *cmdLineCreate(int lineNum, char *input,
  cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot)
{
  cmdLine_t *cmdLine = NULL;

  // Allocate and init a new command line
  cmdLine = malloc(sizeof(cmdLine_t));
  cmdLine->lineNum = lineNum;
  cmdLine->input = malloc(strlen(input) + 1);
  strcpy(cmdLine->input, input);
  cmdLine->argInfo = NULL;
  cmdLine->argInfoBp = NULL;
  cmdLine->initialized = MC_FALSE;
  cmdLine->cmdCommand = NULL;
  cmdLine->next = NULL;
  cmdLine->pcbPrev = NULL;
  cmdLine->pcbNext = NULL;
  cmdLine->pcbAction = PCB_ACT_DEFAULT;
  cmdLine->pcbGrpNext = NULL;
  cmdLine->pcbGrpHead = NULL;
  cmdLine->pcbGrpTail = NULL;
  cmdLine->bpNext = NULL;

  // Take care of some administrative pointers
  if (cmdLineRoot != NULL && *cmdLineRoot == NULL)
  {
    // This is the first list element
    *cmdLineRoot = cmdLine;
  }
  if (cmdLineLast != NULL)
  {
    // Not the first list element
    cmdLineLast->next = cmdLine;
  }

  return cmdLine;
}

//
// Function: cmdLineExecute
//
// Process a single line input string.
// This is the main command input handler that can be called recursively via
// the mchron 'e' command.
//
static u08 cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput)
{
  char *input = cmdLine->input;
  cmdCommand_t *cmdCommand;
  u08 retVal = CMD_RET_OK;

  // Have we scanned the command line arguments yet
  if (cmdLine->initialized == MC_FALSE)
  {
    // Start command line scanner by getting command and its dictionary entry
    retVal = cmdArgInit(&input, cmdLine);
    if (retVal != CMD_RET_OK)
      return CMD_RET_ERROR;
    if (cmdLine->cmdCommand == NULL)
    {
      // Input contains only white space chars.
      // Dump newline in the log only when we run at root command level.
      if (cmdStack.level == -1)
        DEBUGP("");
      return CMD_RET_OK;
    }

    // Scan command arguments (and validate non-numeric arguments)
    retVal = cmdArgRead(input, cmdLine);
    if (retVal != CMD_RET_OK)
      return retVal;
  }

  // See what type of command we're dealing with
  cmdCommand = cmdLine->cmdCommand;
  if (cmdCommand->cmdPcbType == PCB_CONTINUE)
  {
    // For a standard command publish the commandline argument variables and
    // execute its command handler function
    retVal = cmdArgPublish(cmdLine);
    if (retVal != CMD_RET_OK)
      return retVal;
    retVal = cmdCommand->cmdHandler(cmdLine);
  }
  else if (cmdCommand->cmdPcbType == PCB_REPEAT_FOR ||
      cmdCommand->cmdPcbType == PCB_IF)
  {
    // The user has entered a pcb start command at command prompt level.
    // Cache all keyboard input commands until this command is matched with a
    // corresponding end command and execute the command list.
    retVal = cmdStackPush(cmdLine, LIST_ECHO_SILENT, (char *)__progname,
      cmdInput);
  }
  else
  {
    // All other control block types are invalid on the command line as they
    // must be part of a repeat/if/return command. As such they can only be
    // be entered via multiline keyboard input or a command file.
    printf("%s? not part of script\n", cmdCommand->cmdName);
    retVal = CMD_RET_ERROR;
  }

  return retVal;
}

//
// Function: cmdLineValidate
//
// Validate a single command line for use in a command list and, if needed,
// open or find a program counter control block and associate with it in the
// command line.
// Return values:
// -1 : invalid command
//  0 : success (with valid command or only white space without command)
// >0 : starting line number of block in which command cannot be matched
//
static int cmdLineValidate(cmdLine_t **cmdPcbTail, cmdLine_t *cmdLine)
{
  int lineNumErr = 0;
  u08 cmdPcbType;
  char *input;
  u08 retVal;

  // Start command line scanner by getting command and its dictionary entry
  input = cmdLine->input;
  retVal = cmdArgInit(&input, cmdLine);
  // Command not found
  if (retVal != CMD_RET_OK)
    return -1;
  // Input contains only white space chars
  if (cmdLine->cmdCommand == NULL)
    return 0;

  // Get the control block type and based on that either open a new pcb group
  // or attempt to add the command in an existing open pcb group
  cmdPcbType = cmdLine->cmdCommand->cmdPcbType;
  if (cmdPcbType == PCB_REPEAT_FOR || cmdPcbType == PCB_IF ||
      cmdPcbType == PCB_RETURN)
    cmdPcbOpen(cmdPcbTail, cmdLine);
  else if (cmdPcbType != PCB_CONTINUE)
    lineNumErr = cmdPcbLink(cmdPcbTail, cmdLine);

  return lineNumErr;
}

//
// Function: cmdListCleanup
//
// Cleanup a command linked list structure
//
static void cmdListCleanup(cmdLine_t *cmdLineRoot)
{
  cmdLine_t *nextLine;

  // Free the linked list of command lines
  while (cmdLineRoot != NULL)
  {
    // Return the malloc-ed scanned command line arguments in the command, its
    // optional assigned breakpoint expression, and the command itself
    cmdArgCleanup(cmdLineRoot);
    if (cmdLineRoot->input != NULL)
      free(cmdLineRoot->input);

    // Return the malloc-ed command line and continue with the next
    nextLine = cmdLineRoot->next;
    free(cmdLineRoot);
    cmdLineRoot = nextLine;
  }
}

//
// Function: cmdListExecute
//
// Execute the commands in the command list as loaded from a file or entered
// interactively on the command line.
// We have all command lines available in a linked list. The control block
// pointers are present and have been validated on their integrity, so we don't
// need to worry about that.
// Start at the requested position in the list (usually the top of the list but
// in case of an execute resume or debug commands it may be halfway the list)
// and work our way down until we find a runtime error, debug or end-user
// interrupt, or end up at the end of the list.
// We may encounter program control blocks that will influence the program
// counter by creating loops or jumps in the execution plan of the list.
// When command list execution is end-user or debug interrupted, its interrupt
// point is returned in argument cmdProgCtrIntr. The interrupted stack itself
// is prepared to resume at the proper command in cmdProgCounter.
//
static u08 cmdListExecute(cmdLine_t **cmdProgCounter,
  cmdLine_t **cmdProgCtrIntr)
{
  int i;
  char ch = '\0';
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdLine_t *cmdProgCounterNext = NULL;
  char *input;
  u08 cmdDebugCmd;
  u08 cmdDone = MC_FALSE;
  u08 retVal = CMD_RET_OK;

  // We start at the requested position in the linked list, so let's see where
  // we end up
  while (*cmdProgCounter != NULL)
  {
    // When debugging see if we must halt at a debug supported command. Next,
    // we may hit a breakpoint condition for a command. We must skip it though
    // when the breakpoint was hit resulting in an execution break, and we now
    // resume from that break.
    if (cmdDebugActive == MC_TRUE)
    {
      cmdDone = MC_TRUE;
      cmdDebugCmd = cmdDebugCmdGet(0);
      if ((cmdDebugCmd == DEBUG_HALT || cmdDebugCmd == DEBUG_HALT_EXIT) &&
          cmdLine->cmdCommand != NULL &&
          cmdLine->cmdCommand->cmdDbSupport == MC_TRUE)
      {
        // Set interrupt point at active line of script and flag interrupt
        *cmdProgCtrIntr = *cmdProgCounter;
        cmdDebugHalted = MC_TRUE;
        return CMD_RET_INTR;
      }
      else if (cmdStack.bpInterrupt == MC_FALSE && cmdLine->argInfoBp != NULL)
      {
        // Check the breakpoint condition and interrupt execution when needed.
        // If so, flag we got interrupted by a breakpoint.
        retVal = cmdArgBpExecute(cmdLine->argInfoBp);
        if ((retVal == CMD_RET_OK && cmdLine->argInfoBp->exprValue != 0) ||
            retVal != CMD_RET_OK)
        {
          if (retVal == CMD_RET_OK)
            printf("*** breakpoint - execution halted ***\n");
          else
            printf("*** breakpoint *evaluation error* - execution halted ***\n");
          cmdStack.bpInterrupt = MC_TRUE;
          *cmdProgCtrIntr = *cmdProgCounter;
          return CMD_RET_INTR;
        }
      }
      else
      {
        // When needed clear the interrupt by breakpoint flag for next one
        cmdStack.bpInterrupt = MC_FALSE;
      }
    }

    // Echo command prefixed by the stack level line numbers
    if (cmdEcho == CMD_ECHO_YES)
    {
      for (i = 0; i < cmdStack.level; i++)
        printf(":%3d",cmdStack.cmdStackLevel[i].cmdProgCounter->lineNum);
      printf(":%3d: %s\n", cmdLine->lineNum, cmdLine->input);
    }

    // Execute the command in the command line
    cmdStack.cmdStackStats.cmdLineCount++;
    if (cmdLine->cmdCommand == NULL)
    {
      // Skip a blank command line
      cmdProgCounterNext = cmdLine->next;
      retVal = CMD_RET_OK;
    }
    else if (cmdLine->cmdCommand->cmdPcbType == PCB_CONTINUE)
    {
      // Execute a regular command via the generic handler
      cmdStack.cmdStackStats.cmdCmdCount++;
      cmdProgCounterNext = cmdLine->next;
      retVal = cmdLineExecute(cmdLine, NULL);
    }
    else
    {
      // This is a control block command from a repeat/if/return construct. Get
      // the command arguments (if not already done) and execute the associated
      // program counter control block handler from the command dictionary.
      cmdStack.cmdStackStats.cmdCmdCount++;
      input = cmdLine->input;
      if (cmdLine->initialized == MC_FALSE)
      {
        cmdArgInit(&input, cmdLine);
        retVal = cmdArgRead(input, cmdLine);
      }
      if (retVal == CMD_RET_OK)
      {
        cmdProgCounterNext = cmdLine;
        retVal = cmdLine->cmdCommand->pcbHandler(&cmdProgCounterNext);
      }
    }

    // Verify if a command interrupt was requested
    if (retVal == CMD_RET_OK && kbTimerTripped == MC_TRUE)
    {
      kbTimerTripped = MC_FALSE;
      ch = kbKeypressScan(MC_TRUE);
      if (ch == 'q')
      {
        printf("quit\n");
        retVal = CMD_RET_INTR;
      }
    }

    // When we're debugging and matching an active debug command, request to
    // halt at the next command that supports debugging or at eof
    if (cmdDebugActive == MC_TRUE && retVal == CMD_RET_OK &&
        (cmdDebugCmd == DEBUG_STEP_NEXT || cmdDebugCmd == DEBUG_STEP_IN))
      cmdDebugCmdSet(0, DEBUG_HALT);

    // If we encountered an interrupt set interrupt point and prepare stack to
    // resume
    if (retVal == CMD_RET_INTR || retVal == CMD_RET_INTR_CMD)
    {
      *cmdProgCtrIntr = *cmdProgCounter;
      *cmdProgCounter = cmdProgCounterNext;
    }

    // Abort when something is not ok
    if (retVal != CMD_RET_OK)
      return retVal;

    // Move to next command in command list and continue
    *cmdProgCounter = cmdProgCounterNext;
    cmdLine = *cmdProgCounter;
  }

  // We're at the end of the command list with successful completion. See what
  // we need to with an active debug command.
  if (cmdDebugActive == MC_TRUE)
  {
    cmdDebugCmd = cmdDebugCmdGet(0);
    if (cmdDebugCmd == DEBUG_STEP_OUT || cmdDebugCmd == DEBUG_HALT_EXIT)
    {
      // Stop at next command in lower stack level or exit at its eof
      cmdDebugCmdSet(-1, DEBUG_HALT_EXIT);
    }
    else if (cmdDone == MC_FALSE)
    {
      // Stop at next command in lower stack level or when reaching its eof
      if (cmdDebugCmd == DEBUG_STEP_NEXT || cmdDebugCmd == DEBUG_STEP_IN)
        cmdDebugCmdSet(-1, DEBUG_HALT);
    }
    else if (cmdDebugCmd == DEBUG_HALT)
    {
      // We reached eof after one or more (blank) commands and must stop
      cmdDebugHalted = MC_TRUE;
      return CMD_RET_INTR;
    }
  }

  return CMD_RET_OK;
}

//
// Function: cmdListFileLoad
//
// Load command file contents in a linked list structure on the stack.
// In case an error occurs, the cmdStackLevel->cmdProgCounter points to the
// offending command line.
//
static u08 cmdListFileLoad(char *argName, char *fileName)
{
  FILE *fp;				// Input file pointer
  cmdLine_t *cmdLineTail = NULL;	// The last cmdLine in linked list
  cmdLine_t *cmdPcbTail = NULL;	        // The last pcb in linked list
  cmdLine_t *cmdPcbSearch = NULL;	// Active pcb in search efforts
  int lineNumErr = 0;
  cmdInput_t cmdInput;
  cmdStackLevel_t *cmdStackLevel;

  // Init the pointers to the command line and the control block lists
  cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level];
  cmdStackLevel->cmdProgCounter = NULL;
  cmdStackLevel->cmdLineRoot = NULL;
  cmdStackLevel->cmdLineBpRoot = NULL;
  cmdStackLevel->cmdLines = 0;

  // Open command file
  fp = fopen(fileName, "r");
  if (fp == NULL)
  {
    printf("%s? cannot open command file \"%s\"\n", argName, fileName);
    return CMD_RET_ERROR;
  }

  // Initialize our file readline interface method and do the first read
  cmdInputInit(&cmdInput, fp, CMD_INPUT_MANUAL);
  cmdInputRead(NULL, &cmdInput);

  // Add each line in the command file in a command linked list
  while (cmdInput.input != NULL)
  {
    // Create new command line
    cmdStackLevel->cmdLines++;
    cmdLineTail = cmdLineCreate(cmdStackLevel->cmdLines, cmdInput.input,
      cmdLineTail, &cmdStackLevel->cmdLineRoot);
    cmdStackLevel->cmdProgCounter = cmdLineTail;

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching program counter control blocks
    lineNumErr = cmdLineValidate(&cmdPcbTail, cmdLineTail);
    if (lineNumErr != 0)
      break;

    // Get next input line
    cmdInputRead(NULL, &cmdInput);
  }

  // File content is loaded in linked list or error occurred while parsing
  cmdInputCleanup(&cmdInput);
  fclose(fp);

  // Check if a control block command cannot be matched or command not found
  if (lineNumErr > 0)
  {
    printf("parse: command unmatched in block starting at line %d\n",
      lineNumErr);
    return CMD_RET_ERROR;
  }
  else if (lineNumErr < 0)
  {
    printf("parse: invalid command\n");
    return CMD_RET_ERROR;
  }

  // Postprocessing the linked lists.
  // We may not find a control block that is not linked to a command line.
  cmdPcbSearch = cmdPcbTail;
  while (cmdPcbSearch != NULL)
  {
    if (cmdPcbSearch->pcbGrpTail == NULL)
    {
      // Unlinked control block
      cmdStackLevel->cmdProgCounter = cmdPcbSearch;
      printf("parse: command unmatched in block starting at line %d\n",
        cmdStackLevel->cmdProgCounter->lineNum);
      return CMD_RET_ERROR;
    }
    else
    {
      // Continue search backwards
      cmdPcbSearch = cmdPcbSearch->pcbPrev;
    }
  }
  cmdStackLevel->cmdProgCounter = cmdStackLevel->cmdLineRoot;

  // The file contents have been read into linked lists and is verified for its
  // integrity on matching program counter control block constructs
  return CMD_RET_OK;
}

//
// Function: cmdListKeyboardLoad
//
// Load keyboard commands in a linked list structure on the stack.
// In case an error occurs, the cmdStackLevel->cmdProgCounter points to the
// offending command line.
//
static u08 cmdListKeyboardLoad(cmdInput_t *cmdInput)
{
  cmdLine_t *cmdLineTail = NULL;	// The last cmdLine in linked list
  cmdLine_t *cmdPcbTail = NULL;		// The last cmdPcCtrl in linked list
  int pcCtrlCount = 0;			// Active repeat/if command count
  char *prompt;
  int lineNum = 1;
  int lineNumDigits;
  int lineNumErr = 0;
  cmdStackLevel_t *cmdStackLevel;

  // Init the pointers to the command line and the control block lists
  cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level];
  cmdStackLevel->cmdProgCounter = NULL;
  cmdStackLevel->cmdLineRoot = NULL;
  cmdStackLevel->cmdLineBpRoot = NULL;

  // Do not read from the keyboard yet as we already have the first command in
  // the input buffer. Once we've processed that one we'll continue reading the
  // input stream using the control structure we've been handed over.

  // Add each line entered via keyboard in a command linked list.
  // The list is complete when the program control block start command (rf/iif)
  // is matched with a corresponding end command (rn/ien). Note that the return
  // command (elr) is both a pcb block start and end command so we ignore them.
  // List build-up is interrupted when the user enters ^D on a blank line, when
  // a control block command cannot be matched or when a non-existing command
  // is entered.
  while (MC_TRUE)
  {
    // Create new command line
    cmdLineTail = cmdLineCreate(lineNum, cmdInput->input, cmdLineTail,
      &cmdStackLevel->cmdLineRoot);
    cmdStackLevel->cmdProgCounter = cmdLineTail;

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching control blocks
    lineNumErr = cmdLineValidate(&cmdPcbTail, cmdLineTail);
    if (lineNumErr != 0)
      break;

    // Administer count of nested repeat and if blocks
    if (cmdLineTail->cmdCommand != NULL)
    {
      if (cmdLineTail->cmdCommand->cmdPcbType == PCB_REPEAT_FOR ||
          cmdLineTail->cmdCommand->cmdPcbType == PCB_IF)
        pcCtrlCount++;
      else if (cmdLineTail->cmdCommand->cmdPcbType == PCB_REPEAT_NEXT ||
          cmdLineTail->cmdCommand->cmdPcbType == PCB_IF_END)
        pcCtrlCount--;
    }

    // If the most outer control block start is matched we're done
    if (pcCtrlCount == 0)
    {
      cmdStackLevel->cmdLines = lineNum;
      break;
    }

    // Next command line linenumber
    lineNum++;

    // Create a prompt that includes the linenumber
    lineNumDigits = (int)(log10f((float)lineNum)) + 1;
    prompt = malloc(lineNumDigits + 5);
    sprintf(prompt, "%d>> ", lineNum);

    // Get the next keyboard input line
    cmdInputRead(prompt, cmdInput);
    free(prompt);
    if (cmdInput->input == NULL)
    {
      printf("<ctrl>d - abort\n");
      return CDM_RET_LOAD_ABORT;
    }
  }

  // Check if a control block command cannot be matched or command not found
  if (lineNumErr > 0)
  {
    printf("parse: command unmatched in block starting at line %d\n",
      lineNumErr);
    return CMD_RET_ERROR;
  }
  else if (lineNumErr < 0)
  {
    printf("parse: invalid command\n");
    return CMD_RET_ERROR;
  }
  cmdStackLevel->cmdProgCounter = cmdStackLevel->cmdLineRoot;

  // We do not need to postprocess the control block linked list as we keep
  // track of the number of control block start vs end commands.
  // When we get here all control block start commands are matched with a
  // proper control block end command.
  return CMD_RET_OK;
}

//
// Function: cmdListRaiseScan
//
// Handler for the repeating 100 msec cmdlist execution keyboard scan timer
//
static void cmdListRaiseScan(void)
{
  if (kbTimerTripped == MC_FALSE)
    kbTimerTripped = MC_TRUE;
}

//
// Function: cmdPcbLink
//
// Find an unlinked control block and verify if it can match the control block
// type of the current command line. When done, add the pcb to the pcb list.
// Return values:
//  0 : success
// >0 : starting line number of block in which command cannot be matched
//
static int cmdPcbLink(cmdLine_t **cmdPcbTail, cmdLine_t *cmdLine)
{
  cmdLine_t *cmdPcbFind = *cmdPcbTail;
  u08 cmdPcbType = cmdLine->cmdCommand->cmdPcbType;
  u08 cmdFindType;

  while (cmdPcbFind != NULL)
  {
    cmdFindType = cmdPcbFind->cmdCommand->cmdPcbType;

    // Check the block to verify vs unlinked control block members we find in
    // the linked list. Some combinations must be skipped, some are invalid and
    // some will result in a valid combination.
    if (cmdPcbFind->pcbGrpTail == NULL &&
        (cmdPcbType == PCB_IF_ELSE_IF || cmdPcbType == PCB_IF_ELSE ||
         cmdPcbType == PCB_IF_END) &&
        (cmdFindType == PCB_REPEAT_BRK || cmdFindType == PCB_REPEAT_CONT))
    {
      // Skip combination if blocks vs repeat break/cont blocks, so go backward
      cmdPcbFind = cmdPcbFind->pcbPrev;
    }
    else if (cmdPcbFind->pcbGrpTail == NULL &&
        (cmdPcbType == PCB_REPEAT_BRK || cmdPcbType == PCB_REPEAT_CONT) &&
        (cmdFindType == PCB_IF || cmdFindType == PCB_IF_ELSE_IF ||
         cmdFindType == PCB_IF_ELSE || cmdFindType == PCB_IF_END))
    {
      // Skip combination repeat break/cont blocks vs if blocks, so go backward
      cmdPcbFind = cmdPcbFind->pcbPrev;
    }
    else if (cmdPcbFind->pcbGrpTail == NULL)
    {
      // Found an unlinked control block that is to be validated. Verify if its
      // control block type is compatible with the control block type of the
      // command line.
      if (cmdPcbType == PCB_REPEAT_BRK && cmdFindType != PCB_REPEAT_FOR &&
          cmdFindType != PCB_REPEAT_BRK && cmdFindType != PCB_REPEAT_CONT)
        return cmdPcbFind->lineNum;
      if (cmdPcbType == PCB_REPEAT_CONT && cmdFindType != PCB_REPEAT_FOR &&
          cmdFindType != PCB_REPEAT_BRK && cmdFindType != PCB_REPEAT_CONT)
        return cmdPcbFind->lineNum;
      if (cmdPcbType == PCB_REPEAT_NEXT && cmdFindType != PCB_REPEAT_FOR &&
          cmdFindType != PCB_REPEAT_BRK && cmdFindType != PCB_REPEAT_CONT)
        return cmdPcbFind->lineNum;
      else if (cmdPcbType == PCB_IF_ELSE_IF && cmdFindType != PCB_IF &&
          cmdFindType != PCB_IF_ELSE_IF)
        return cmdPcbFind->lineNum;
      else if (cmdPcbType == PCB_IF_ELSE && cmdFindType != PCB_IF &&
          cmdFindType != PCB_IF_ELSE_IF)
        return cmdPcbFind->lineNum;
      else if (cmdPcbType == PCB_IF_END && cmdFindType != PCB_IF &&
          cmdFindType != PCB_IF_ELSE_IF && cmdFindType != PCB_IF_ELSE)
        return cmdPcbFind->lineNum;

      // There's a valid match between the control block types of the current
      // command and the found open control block. Make a cross link between
      // the two.
      cmdLine->pcbGrpHead = cmdPcbFind->pcbGrpHead;
      cmdPcbFind->pcbGrpNext = cmdLine;

      // Fill in the tail of a complete ctrl group over all its members
      // when a ctrl group is being closed
      if (cmdPcbType == PCB_REPEAT_NEXT || cmdPcbType == PCB_IF_END)
      {
        // Start at the head of the pcb group and go down until we find ourself
        cmdPcbFind = cmdLine->pcbGrpHead;
        while (cmdPcbFind != cmdLine)
        {
          cmdPcbFind->pcbGrpTail = cmdLine;
          cmdPcbFind = cmdPcbFind->pcbGrpNext;
        }
        cmdLine->pcbGrpTail = cmdLine;
      }

      // All is well so add the pcb to the pcb list and set new last list pcb
      (*cmdPcbTail)->pcbNext = cmdLine;
      cmdLine->pcbPrev = *cmdPcbTail;
      *cmdPcbTail = cmdLine;

      return 0;
    }
    else
    {
      // Search not done yet, go backward
      cmdPcbFind = cmdPcbFind->pcbPrev;
    }
  }

  // Could not find a valid unlinked control block in entire list.
  // Report error on line 1.
  return 1;
}

//
// Function: cmdPcbOpen
//
// Add the command line to the pcb list and open a new pcb group. This group
// may also be closed immediately.
//
static void cmdPcbOpen(cmdLine_t **cmdPcbTail, cmdLine_t *cmdLine)
{
  u08 cmdPcbType = cmdLine->cmdCommand->cmdPcbType;

  // Make the current pcb tail and the command line point to one and another
  if (*cmdPcbTail != NULL)
  {
    // Not the first in the list
    (*cmdPcbTail)->pcbNext = cmdLine;
  }
  cmdLine->pcbPrev = *cmdPcbTail;

  // The head of the control group is the start command (repeat/if/return) and
  // is copied down to each other member of the control group.
  // In case of a list return the control group open is also the group close.
  cmdLine->pcbGrpHead = cmdLine;
  if (cmdPcbType == PCB_RETURN)
    cmdLine->pcbGrpTail = cmdLine;

  // Make the command line the new pcb tail
  *cmdPcbTail = cmdLine;
}

//
// Function: cmdStackActiveGet
//
// Returns whether commands are currently run from the stack
//
u08 cmdStackActiveGet(void)
{
  if (cmdStack.level >= 0)
    return MC_TRUE;
  else
    return MC_FALSE;
}

//
// Function: cmdStackCleanup
//
// Cleanup the stack and scan timer
//
void cmdStackCleanup(void)
{
  cmdStackPop(CMD_STACK_POP_ALL);
  timer_delete(kbTimer);
}

//
// Function: cmdStackInit
//
// Initialize the stack and its statistics and scan timer
//
void cmdStackInit(void)
{
  struct sigevent sEvent;
  u08 i;

  // Init the command stack
  cmdStack.level = -1;
  cmdStack.levelResume = -1;
  cmdStack.cmdLineInvoke = NULL;
  cmdStack.cmdProgCtrIntr = NULL;
  for (i = 0; i < CMD_STACK_DEPTH_MAX; i++)
  {
    cmdStack.cmdStackLevel[i].cmdProgCounter = NULL;
    cmdStack.cmdStackLevel[i].cmdLineRoot = NULL;
    cmdStack.cmdStackLevel[i].cmdEcho = CMD_ECHO_NONE;
    cmdStack.cmdStackLevel[i].cmdOrigin = NULL;
    cmdStack.cmdStackLevel[i].cmdDebugCmd = DEBUG_NONE;
  }

  // Init stack statistics
  cmdStackStatsInit();

  // Prepare a realtime timer generating signal SIGVTALRM with a handler (but
  // only arm/disarm during stack activity)
  sEvent.sigev_notify = SIGEV_SIGNAL;
  sEvent.sigev_signo = SIGVTALRM;
  sEvent.sigev_value.sival_ptr = cmdListRaiseScan;
  timer_create(CLOCK_REALTIME, &sEvent, &kbTimer);
}

//
// Function: cmdStackLevelGet
//
// Get the current command stack level of an active or interrupted stack
//
static s08 cmdStackLevelGet(void)
{
  // See if we have an active stack or an interupted stack
  if (cmdStack.level == -1 && cmdStack.levelResume == -1)
    return -1;

  // Determine top level and x-check with requested level
  if (cmdStack.levelResume >= 0)
    return cmdStack.levelResume;
  else
    return cmdStack.level;
}

//
// Function: cmdStackListPrint
//
// Print stack command source list in a range surrounding its program counter
//
u08 cmdStackListPrint(u08 level, s16 range)
{
  cmdStackLevel_t *cmdStackLevel;
  cmdLine_t *cmdLine;
  u08 levelTop;
  int lineOffset, lineMin, lineMax;

  // Ignore if we have no stack
  if (cmdStack.level == -1 && cmdStack.levelResume == -1)
    return MC_FALSE;

  // Set and check requested stack level
  if (cmdStack.levelResume >= 0)
    levelTop = cmdStack.levelResume;
  else
    levelTop = cmdStack.level;
  if (level > levelTop)
    return MC_FALSE;

  // Determine line number range to print, keeping in mind our program counter
  // may point to <eof>
  cmdStackLevel = &cmdStack.cmdStackLevel[level];
  if (cmdStackLevel->cmdProgCounter == NULL)
    lineOffset = cmdStackLevel->cmdLines + 1;
  else
    lineOffset = cmdStackLevel->cmdProgCounter->lineNum;
  if (range == -1)
  {
    lineMin = 1;
    lineMax = cmdStackLevel->cmdLines;
  }
  else
  {
    lineMin = lineOffset - range;
    lineMax = lineOffset + range;
  }

  // Print the range of command lines
  printf(CMD_SOURCE_NOTIFY);
  printf(CMD_SOURCE_HEADER);
  cmdLine = cmdStackLevel->cmdLineRoot;
  while (cmdLine != NULL)
  {
    // Is command line in-scope for printing
    if (cmdLine->lineNum >= lineMin && cmdLine->lineNum <= lineMax)
    {
      // Print command line with indicators for program counter and (in)active
      // breakpoint
      printf(CMD_SOURCE_LVL_LINE_FMT, (int)level, cmdLine->lineNum);
      if (cmdLine == cmdStackLevel->cmdProgCounter)
        printf(CMD_SOURCE_PC);
      else
        printf(CMD_SOURCE_NO_PC);
      if (cmdLine->argInfoBp == NULL)
        printf(CMD_SOURCE_NO_BP);
      else if (cmdDebugActive == MC_FALSE)
        printf(CMD_SOURCE_INACT_BP);
      else
        printf(CMD_SOURCE_BP);
      printf(CMD_SOURCE_COMMAND_FMT, cmdLine->input);
    }

    // Quit when done or get next line
    if (cmdLine->lineNum == lineMax)
      break;
    else
      cmdLine = cmdLine->next;
  }

  // Print additional line in case the listing was requested at <eof>
  if (cmdStackLevel->cmdProgCounter == NULL)
    printf(CMD_SOURCE_EOF_FMT, (int)level);

  return MC_TRUE;
}

//
// Function: cmdStackPop
//
// Reset command stack for the current or all levels
//
static void cmdStackPop(u08 scope)
{
  s08 i;
  s08 levelMin;
  s08 levelMax;
  cmdStackLevel_t *cmdStackLevel;

  // There's nothing to be done when the stack is empty
  if (cmdStack.level == -1 && cmdStack.levelResume == -1)
    return;

  // Set stack level range to be reset
  if (cmdStack.levelResume >= 0)
    cmdStack.level = cmdStack.levelResume;
  if (scope == CMD_STACK_POP_LVL)
    levelMin = cmdStack.level;
  else
    levelMin = 0;
  levelMax = cmdStack.level;

  // Reset contents of stack level(s) when requested
  for (i = levelMin; i <= levelMax; i++)
  {
    cmdStackLevel = &cmdStack.cmdStackLevel[i];
    cmdStackLevel->cmdProgCounter = NULL;
    cmdListCleanup(cmdStackLevel->cmdLineRoot);
    cmdStackLevel->cmdLineRoot = NULL;
    cmdStackLevel->cmdLineBpRoot = NULL;
    cmdStackLevel->cmdEcho = CMD_ECHO_NONE;
    free(cmdStackLevel->cmdOrigin);
    cmdStackLevel->cmdOrigin = NULL;
    cmdStackLevel->cmdDebugCmd = DEBUG_NONE;
    cmdStackLevel->cmdLines = 0;
    cmdStackLevel->cmdDebugLines = 0;
  }

  // Reset stack invoke and resume command when full reset or root level reset
  // is requested
  if (levelMin == 0)
  {
    cmdListCleanup(cmdStack.cmdLineInvoke);
    cmdStack.levelResume = -1;
    cmdStack.cmdLineInvoke = NULL;
    cmdStack.cmdProgCtrIntr = NULL;
    cmdStack.bpInterrupt = MC_FALSE;
  }

  // Do the actual pop on the stack level
  if (levelMin == 0)
    cmdStack.level = -1;
  else
    cmdStack.level--;
}

//
// Function: cmdStackPrint
//
// Print the stack. When interrupted by a debug step command only report the
// stack top level.
//
u08 cmdStackPrint(u08 status)
{
  s08 i;
  s08 level;
  char *input;
  cmdStackLevel_t *cmdStackLevel;
  cmdLine_t *cmdLine;

  // Check if no (resume) stack present
  if (cmdStack.level == -1 && cmdStack.levelResume == -1)
    return MC_FALSE;

  // Set stack level to start with
  if (cmdStack.level >= 0)
    level = cmdStack.level;
  else
    level = cmdStack.levelResume;

  // Print stack header
  if (cmdDebugHalted == MC_FALSE)
  {
    if (status == CMD_RET_INTR_CMD)
      printf(CMD_STACK_NFY_INTR_CMD);
    else
      printf(CMD_STACK_NOTIFY);
  }
  printf(CMD_STACK_HEADER);

  // Print the stack
  for (i = level; i >= 0; i--)
  {
    cmdStackLevel = &cmdStack.cmdStackLevel[i];
    cmdLine = NULL;

    // In case stack execution was interrupted in a wait or breakpoint command
    // or when an error occured we must print the associated command. In all
    // other cases the command has already been completed and then we must
    // print the next command as interrupt point.
    if (i == cmdStack.level && cmdStack.levelResume >= 0)
    {
      // This is the highest stack level and we can resume the script. Now
      // check why we were interupted.
      if (status == CMD_RET_INTR_CMD)
      {
        // Wait 'q' keypress or breakpoint command interrupt: print interrupted
        // command
        cmdLine = cmdStack.cmdProgCtrIntr;
      }
      else // CMD_RET_INTR
      {
        // User or debug/breakpoint interrupt: print next command on stack
        cmdLine = cmdStackLevel->cmdProgCounter;
      }
    }
    else
    {
      // Report the command line. At the highest stack level this may be the
      // line in error.
      cmdLine = cmdStackLevel->cmdProgCounter;
    }

    if (cmdLine == NULL && cmdStack.levelResume >= 0)
    {
      // We reached the end of the command list in the highest stack level
      printf(CMD_STACK_EOF_FMT, i, basename(cmdStackLevel->cmdOrigin));
    }
    else if (cmdLine != NULL)
    {
      // Print the designated command from the stack level where the command
      // itself is trimmed from leading spaces and tabs
      input = cmdLine->input;
      printf(CMD_STACK_FMT, i, basename(cmdStackLevel->cmdOrigin),
        cmdLine->lineNum, input + strspn(input, " \t"));
    }

    // When debugging and interrupted by a debug step command only print the
    // stack top level
    if (cmdDebugHalted == MC_TRUE)
      break;
  }

  // Print the stack invoke command, trimmed from leading spaces and tabs
  if (cmdDebugHalted == MC_FALSE && cmdStack.cmdLineInvoke != NULL)
  {
    input = cmdStack.cmdLineInvoke->input;
    printf(CMD_STACK_INVOKE_FMT, __progname, input + strspn(input, " \t"));
  }

  // Clear the debug halted flag for subsequent stack prints
  cmdDebugHalted = MC_FALSE;

  return MC_TRUE;
}

//
// Function: cmdStackPush
//
// Add a level to the current stack, execute it, and when done pop it
//
u08 cmdStackPush(cmdLine_t *cmdLine, u08 echoReq, char *cmdOrigin,
  cmdInput_t *cmdInput)
{
  cmdStackLevel_t *cmdStackLevel;
  u08 retVal = CMD_RET_OK;

  if (cmdStack.level + 1 >= CMD_STACK_DEPTH_MAX)
  {
    // Verify too deep nested list commands (prevent potential recursive call)
    printf("%s: max stack level exceeded (max=%d)\n",
      cmdLine->cmdCommand->cmdName, CMD_STACK_DEPTH_MAX - 1);
    return CMD_RET_ERROR;
  }

  // In case of a new stack build-up clear remaining command resume stack
  if (cmdStack.levelResume >= 0)
    cmdStackPop(CMD_STACK_POP_ALL);

  // Let's start: increase stack level and copy command origin (file/keyboard)
  cmdStack.level++;
  cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level];
  cmdStackLevel->cmdOrigin = malloc(strlen(cmdOrigin) + 1);
  sprintf(cmdStackLevel->cmdOrigin, "%s", cmdOrigin);

  // Determine and set commmand echo for this level
  if (echoReq == LIST_ECHO_ECHO)
    cmdStackLevel->cmdEcho = CMD_ECHO_YES;
  else if (echoReq == LIST_ECHO_SILENT)
    cmdStackLevel->cmdEcho = CMD_ECHO_NO;
  else if (cmdStack.level == 0)
    cmdStackLevel->cmdEcho = cmdEcho;
  else
    cmdStackLevel->cmdEcho =
      cmdStack.cmdStackLevel[cmdStack.level - 1].cmdEcho;
  cmdEcho = cmdStackLevel->cmdEcho;

  // See if we have an input stream, meaning that we'll take our input from the
  // keyboard
  if (cmdInput != NULL)
  {
    // This is a multi-line keyboard script starting with 'iif' or 'rf'
    retVal = cmdListKeyboardLoad(cmdInput);
  }
  else
  {
    // Commands are from a file. If this is the root level copy the mchron 'e'
    // command as stack invoke command.
    if (cmdStack.level == 0)
    {
      cmdStack.cmdLineInvoke = cmdLineCopy(cmdLine);
      cmdStack.cmdLineInvoke->lineNum = 0;
    }
    retVal = cmdListFileLoad(cmdLine->cmdCommand->cmdArg[1].argName,
      cmdStackLevel->cmdOrigin);
  }

  // When at root level switch to keypress mode, start a timer that fires every
  // 100 msec for scanning keypresses, and init stack runtime statistics
  if (cmdStack.level == 0)
  {
    kbModeSet(KB_MODE_SCAN);
    cmdStackTimerSet(LIST_TIMER_ARM);
    cmdStackStatsInit();
  }

  // If we're debugging and on the first stack level or have a step-in debug
  // command we need to halt immediately
  if (cmdDebugActive == MC_TRUE && retVal == CMD_RET_OK)
  {
    if (cmdStack.level == 0 || cmdDebugCmdGet(-1) == DEBUG_STEP_IN)
      cmdDebugCmdSet(0, DEBUG_HALT);
  }

  // When loading was ok execute the command list
  if (retVal == CMD_RET_OK)
    retVal = cmdListExecute(&cmdStackLevel->cmdProgCounter,
      &cmdStack.cmdProgCtrIntr);

  // If execution was 'q' or debug interrupted allow to resume execution at
  // this level
  if (retVal == CMD_RET_INTR || retVal == CMD_RET_INTR_CMD)
    cmdStack.levelResume = cmdStack.level;

  // Print stack when initial error/interrupt has occured
  if (retVal == CMD_RET_ERROR || retVal == CMD_RET_INTR ||
      retVal == CMD_RET_INTR_CMD)
    cmdStackPrint(retVal);

  // Report execution statistics when we're back at root level
  if (cmdStack.level == 0)
    cmdStackStatsPrint();

  // Restore command echo of parent level
  if (cmdStack.level == 0)
    cmdEcho = CMD_ECHO_YES;
  else
    cmdEcho = cmdStack.cmdStackLevel[cmdStack.level - 1].cmdEcho;

  // Pop the stack level when the stack cannot be resumed. If we can resume do
  // an administrative pop only by decreasing the stack level.
  if (cmdStack.levelResume == -1)
    cmdStackPop(CMD_STACK_POP_LVL);
  else
    cmdStack.level--;

  // When the stack is empty or we were 'q' or debug interrupted switch back to
  // line mode and stop keyboard timer
  if (cmdStack.level == -1 || retVal == CMD_RET_INTR ||
      retVal == CMD_RET_INTR_CMD)
  {
    kbModeSet(KB_MODE_LINE);
    cmdStackTimerSet(LIST_TIMER_DISARM);
  }

  // Signal that initial error handling was done
  if (retVal != CMD_RET_OK)
    retVal = CMD_RET_RECOVER;

  return retVal;
}

//
// Function: cmdStackResume
//
// Resume execution of the stack where it's been left off
//
u08 cmdStackResume(char *cmdName)
{
  cmdStackLevel_t *cmdStackLevel;
  u08 retVal = CMD_RET_OK;

  if (cmdStack.levelResume == -1)
  {
    printf("%s: no stack available\n", cmdName);
    return CMD_RET_ERROR;
  }

  // Switch to keypress mode and start a timer that fires every 100 msec after
  // which the keyboard can be scanned for keypresses
  kbModeSet(KB_MODE_SCAN);
  cmdStackTimerSet(LIST_TIMER_ARM);

  // Init statistics for this (resumed) session
  cmdStackStatsInit();

  // Clear stack resume state and continue where we left off with the requested
  // command echo setting
  cmdStack.cmdProgCtrIntr = NULL;
  cmdStack.level = cmdStack.levelResume;
  cmdStack.levelResume = -1;
  cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level];
  cmdEcho = cmdStack.cmdStackLevel[cmdStack.level].cmdEcho;
  while (cmdStack.level >= 0)
  {
    retVal = cmdListExecute(&cmdStackLevel->cmdProgCounter,
      &cmdStack.cmdProgCtrIntr);

    // If execution was 'q' or debug interrupted allow to resume execution at
    // this level
    if (retVal == CMD_RET_INTR || retVal == CMD_RET_INTR_CMD)
    {
      // Signal resume capability and print the entire stack
      cmdStack.levelResume = cmdStack.level;
      cmdStackPrint(retVal);
      cmdStack.level = -1;
    }
    else if (retVal == CMD_RET_ERROR)
    {
      // First print then clear the full stack
      cmdStackPrint(retVal);
      cmdStackPop(CMD_STACK_POP_ALL);
    }
    else if (retVal == CMD_RET_RECOVER)
    {
      // Depending on whether execution was interrupted keep the stack or pop
      // everything
      if (cmdStack.levelResume == -1)
        cmdStackPop(CMD_STACK_POP_ALL);
      else
        cmdStack.level = -1;
    }

    // Abort stack execution when error/interrupt occured
    if (retVal != CMD_RET_OK)
    {
      retVal = CMD_RET_RECOVER;
      break;
    }

    // If present, restore command echo and set resume point of parent level at
    // the line after the completed 'e' command
    if (cmdStack.level != 0)
    {
      cmdEcho = cmdStack.cmdStackLevel[cmdStack.level - 1].cmdEcho;
      cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level - 1];
      cmdStackLevel->cmdProgCounter = cmdStackLevel->cmdProgCounter->next;
    }

    // And pop the current stack level
    cmdStackPop(CMD_STACK_POP_LVL);
  }

  // Provide stack run statistics
  cmdStackStatsPrint();

  // Switch back to line mode and stop keyboard timer
  cmdEcho = CMD_ECHO_YES;
  kbModeSet(KB_MODE_LINE);
  cmdStackTimerSet(LIST_TIMER_DISARM);

  return retVal;
}

//
// Function: cmdStackStatsInit
//
// Inits the statistics data for executing the command list stack
//
static void cmdStackStatsInit(void)
{
  cmdStack.cmdStackStats.cmdCmdCount = 0;
  cmdStack.cmdStackStats.cmdLineCount = 0;
  gettimeofday(&cmdStack.cmdStackStats.cmdTvStart, NULL);
}

//
// Function: cmdStackStatsPrint
//
// Prints the statistics data for executing the command list stack
//
static void cmdStackStatsPrint(void)
{
  struct timeval cmdTvEnd;
  double secElapsed;

  // Print list execution statistics when there's something to show
  if (cmdStack.cmdStackStats.cmdLineCount > 0 &&
      cmdStackStatsEnable == MC_TRUE)
  {
    gettimeofday(&cmdTvEnd, NULL);
    secElapsed = TIMEDIFF_USEC(cmdTvEnd, cmdStack.cmdStackStats.cmdTvStart) /
      (double)1E6;
    printf("time=%.3f sec, cmd=%llu, line=%llu", secElapsed,
      cmdStack.cmdStackStats.cmdCmdCount, cmdStack.cmdStackStats.cmdLineCount);
    if (secElapsed > 0.1)
      printf(", avgLine=%.0f", cmdStack.cmdStackStats.cmdLineCount /
        secElapsed);
    printf("\n");
  }
}

//
// Function: cmdStackStatsSet
//
// Enable/disable printing stack command runtime statistics
//
void cmdStackStatsSet(u08 enable)
{
  cmdStackStatsEnable = enable;
}

//
// Function: cmdStackTimerSet
//
// Arm/disarm a repeating realtime 100 msec interval timer for scanning the
// keyboard.
//
void cmdStackTimerSet(u08 action)
{
  struct itimerspec iTimer;

  if (action == LIST_TIMER_DISARM)
  {
    // Have the timer disarmed
    iTimer.it_value.tv_sec = 0;
    iTimer.it_value.tv_nsec = 0;
  }
  else // LIST_TIMER_ARM
  {
    // Reset possible pending scan flag and set 100 msec timeout
    kbTimerTripped = MC_FALSE;
    iTimer.it_value.tv_sec = CMD_STACK_SCAN_MSEC / 1000;
    iTimer.it_value.tv_nsec = (CMD_STACK_SCAN_MSEC % 1000) * 1E6;
  }

  // Make it repeating and set timer
  iTimer.it_interval.tv_sec = iTimer.it_value.tv_sec;
  iTimer.it_interval.tv_nsec = iTimer.it_value.tv_nsec;
  timer_settime(kbTimer, 0, &iTimer, NULL);
}
