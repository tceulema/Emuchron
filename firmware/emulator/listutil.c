//*****************************************************************************
// Filename : 'listutil.c'
// Title    : Command list and execution utility routines for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>

// Monochron and emuchron defines
#include "../global.h"
#include "scanutil.h"
#include "mchronutil.h"
#include "listutil.h"

// To prevent recursive file loading define a max stack depth when executing
// command files
#define CMD_STACK_DEPTH_MAX	8

// The command stack keyboard scan interval (msec)
#define CMD_STACK_SCAN_MSEC	100

// The command stack pop scope
#define CMD_STACK_POP_ALL	0	// Pop all levels (clear stack)
#define CMD_STACK_POP_LVL	1	// Pop current stack level only

// Generic stack trace header and line
#define CMD_STACK_TRACE		"--- stack trace ---\n"
#define CMD_STACK_FMT		"%d:%s:%d:%s\n"
#define CMD_STACK_INVOKE_FMT	"-:%s:-:%s\n"

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
  cmdPcCtrl_t *cmdPcCtrlRoot;	// Root of control blocks
  u08 cmdEcho;			// Command echo
  char *cmdOrigin;		// Commands origin (file or cmdline)
} cmdStackLevel_t;

// Definition of a structure holding the run stack
typedef struct _cmdStack_t
{
  s08 level;			// Current stack run level (-1 = free)
  s08 levelResume;		// Execution resume stack level (-1 = no)
  cmdLine_t *cmdLineInvoke;	// Command line that initiates stack
  cmdLine_t *cmdProgCtrIntr;	// Interrupted command line upon completion
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

// Local function prototypes
// Command line
static cmdLine_t *cmdLineCopy(cmdLine_t *cmdLineIn);
static int cmdLineValidate(cmdPcCtrl_t **cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine);
// Command list
static void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdPcCtrl_t *cmdPcCtrlRoot);
static u08 cmdListExecute(cmdLine_t **cmdProgCounter,
  cmdLine_t **cmdProgCtrIntr);
static u08 cmdListFileLoad(char *argName, char *fileName);
static u08 cmdListKeyboardLoad(cmdInput_t *cmdInput);
static void cmdListRaiseScan(void);
// Program counter control block
static cmdPcCtrl_t *cmdPcCtrlCreate(cmdPcCtrl_t *cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine);
static int cmdPcCtrlLink(cmdPcCtrl_t *cmdPcCtrlLast, cmdLine_t *cmdLine);
// Command stack
static void cmdStackPop(u08 scope);
static void cmdStackPrint(void);
static void cmdStackStatsInit(void);
static void cmdStackStatsPrint(void);

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

  // Allocate a new command line and init using an existing command line
  cmdLine = malloc(sizeof(cmdLine_t));
  *cmdLine = *cmdLineIn;

  // Allocate and copy command line text
  cmdLine->input = malloc(strlen(cmdLineIn->input) + 1);
  memcpy(cmdLine->input, cmdLineIn->input, strlen(cmdLineIn->input) + 1);

  // Allocate and copy individually scanned command arguments
  cmdLine->argInfo = NULL;
  if (argCount > 0)
  {
    cmdLine->argInfo = malloc(sizeof(argInfo_t) * argCount);
    for (i = 0; i < argCount; i++)
    {
      cmdLine->argInfo[i].arg = malloc(strlen(cmdLineIn->argInfo[i].arg) + 1);
      memcpy(cmdLine->argInfo[i].arg, cmdLineIn->argInfo[i].arg,
        strlen(cmdLineIn->argInfo[i].arg) + 1);
      cmdLine->argInfo[i].exprAssign = cmdLineIn->argInfo[i].exprAssign;
      cmdLine->argInfo[i].exprConst = cmdLineIn->argInfo[i].exprConst;
      cmdLine->argInfo[i].exprValue = cmdLineIn->argInfo[i].exprValue;
    }
  }

  return cmdLine;
}

//
// Function: cmdLineCreate
//
// Create a new cmdLine structure and, when provided, add it in the command
// linked list
//
cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot)
{
  cmdLine_t *cmdLine = NULL;

  // Allocate and init a new command line
  cmdLine = malloc(sizeof(cmdLine_t));
  cmdLine->lineNum = 0;
  cmdLine->input = NULL;
  cmdLine->argInfo = NULL;
  cmdLine->initialized = MC_FALSE;
  cmdLine->cmdCommand = NULL;
  cmdLine->cmdPcCtrlParent = NULL;
  cmdLine->cmdPcCtrlChild = NULL;
  cmdLine->next = NULL;

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
u08 cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput)
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
  if (cmdCommand->cmdPcCtrlType == PC_CONTINUE)
  {
    // For a standard command publish the commandline argument variables and
    // execute its command handler function
    retVal = cmdArgPublish(cmdLine);
    if (retVal != CMD_RET_OK)
      return retVal;
    retVal = cmdCommand->cmdHandler(cmdLine);
  }
  else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR ||
    cmdCommand->cmdPcCtrlType == PC_IF)
  {
    // The user has entered a program counter control block start command at
    // command prompt level.
    // Cache all keyboard input commands until this command is matched with a
    // corresponding end command and execute the command list.
    retVal = cmdStackPush(cmdLine, LIST_ECHO_SILENT, (char *)__progname,
      cmdInput);
  }
  else
  {
    // All other control block types are invalid on the command line as they
    // need to link to either a repeat-for or if-then command. As such, they
    // can only be entered via multiline keyboard input or a command file.
    printf("%s? cannot match this command\n", cmdCommand->cmdName);
    retVal = CMD_RET_ERROR;
  }

  return retVal;
}

//
// Function: cmdLineValidate
//
// Validate a single command line for use in a command list and, if needed,
// add or find a program counter control block and associate it with the
// command line.
// Return values:
// -1 : invalid command
//  0 : success (with valid command or only white space without command)
// >0 : starting line number of block in which command cannot be matched
//
static int cmdLineValidate(cmdPcCtrl_t **cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine)
{
  int lineNumErr = 0;
  cmdCommand_t *cmdCommand;
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

  // Get the control block type and based on that optionally link it to a (new)
  // program control block
  cmdCommand = cmdLine->cmdCommand;
  if (cmdCommand->cmdPcCtrlType != PC_CONTINUE)
  {
    if (cmdCommand->cmdPcCtrlType == PC_REPEAT_NEXT)
    {
      // We must find a repeat for/break/cont command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR)
    {
      // Create new control block and link it to the repeat command
      *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
        cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_BRK)
    {
      // We must find a repeat for/break/cont to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);

      // Create new control block and link it to repeat break command
      if (lineNumErr == 0)
        *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
          cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_CONT)
    {
      // We must find a repeat for/break/cont to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);

      // Create new control block and link it to repeat continue command
      if (lineNumErr == 0)
        *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
          cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF_ELSE_IF)
    {
      // We must find an if or else-if command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);

      // Create new control block and link it to the if-else-if command
      if (lineNumErr == 0)
        *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
          cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF_END)
    {
      // We must find an if, else-if or else command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF_ELSE)
    {
      // We must find an if or else-if command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);

      // Create new control block and link it to the if-else command
      if (lineNumErr == 0)
        *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
          cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF)
    {
      // Create new control block and link it to the if-then command
      *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
        cmdLine);
    }
  }

  return lineNumErr;
}

//
// Function: cmdLineCleanup
//
// Cleanup a command line
//
void cmdLineCleanup(cmdLine_t *cmdLine)
{
  cmdListCleanup(cmdLine, NULL);
}

//
// Function: cmdListCleanup
//
// Cleanup a command linked list structure
//
static void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdPcCtrl_t *cmdPcCtrlRoot)
{
  cmdLine_t *nextLine;
  cmdPcCtrl_t *nextPcCtrl;

  // Free the linked list of command lines
  nextLine = cmdLineRoot;
  while (nextLine != NULL)
  {
    // Return the malloc-ed scanned command line arguments in the command and
    // the command itself
    cmdArgCleanup(cmdLineRoot);
    if (cmdLineRoot->input != NULL)
      free(cmdLineRoot->input);

    // Return the malloc-ed command line
    nextLine = cmdLineRoot->next;
    free(cmdLineRoot);
    cmdLineRoot = nextLine;
  }

  // Free the linked list of program counter control blocks
  nextPcCtrl = cmdPcCtrlRoot;
  while (nextPcCtrl != NULL)
  {
    nextPcCtrl = cmdPcCtrlRoot->next;
    free(cmdPcCtrlRoot);
    cmdPcCtrlRoot = nextPcCtrl;
  }
}

//
// Function: cmdListExecute
//
// Execute the commands in the command list as loaded from a file or entered
// interactively on the command line.
// We have all command lines and control blocks available in linked lists. The
// control block list has been checked on its integrity, meaning that all
// pointers between command lines and control blocks are present and validated
// so we don't need to worry about that.
// Start at the requested position in the list (usually the top of the list but
// in case of an execute resume it may be halfway the list) and work our way
// down until we find a runtime error or end up at the end of the list.
// We may encounter program control blocks that will influence the program
// counter by creating loops or jumps in the execution plan of the list.
// When command list execution is end-user interrupted, its interrupt point is
// returned in argument cmdProgCtrIntr. The interrupted stack itself is already
// prepared to resume at the next command.
//
static u08 cmdListExecute(cmdLine_t **cmdProgCounter,
  cmdLine_t **cmdProgCtrIntr)
{
  int i;
  char ch = '\0';
  cmdLine_t *cmdLine = NULL;
  cmdLine_t *cmdProgCounterNext = NULL;
  char *input;
  u08 retVal = CMD_RET_OK;

  // We start at the requested position in the linked list, so let's see where
  // we end up.
  while (*cmdProgCounter != NULL)
  {
    cmdLine = *cmdProgCounter;

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
    else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_CONTINUE)
    {
      // Execute a regular command via the generic handler
      cmdStack.cmdStackStats.cmdCmdCount++;
      cmdProgCounterNext = cmdLine->next;
      retVal = cmdLineExecute(cmdLine, NULL);
    }
    else
    {
      // This is a control block command from a repeat or if construct. Get the
      // command arguments (if not already done) and execute the associated
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
        retVal = cmdLine->cmdCommand->cbHandler(&cmdProgCounterNext);
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
        retVal = CMD_RET_INTERRUPT;
      }
    }

    // If we encountered an interrupt set interrupt point and prepare stack
    // to resume
    if (retVal == CMD_RET_INTERRUPT)
    {
      *cmdProgCtrIntr = *cmdProgCounter;
      *cmdProgCounter = cmdProgCounterNext;
    }

    // Abort when something is not ok
    if (retVal != CMD_RET_OK)
      break;

    // Move to next command in linked list and continue
    *cmdProgCounter = cmdProgCounterNext;
  }

  return retVal;
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
  cmdLine_t *cmdLine = NULL;		// The last cmdLine in linked list
  cmdPcCtrl_t *cmdPcCtrlLast = NULL;	// The last cmdPcCtrl in linked list
  cmdPcCtrl_t *cmdPcCtrlSearch = NULL;	// Active cmdPcCtrl in search efforts
  int lineNum = 1;
  int lineNumErr = 0;
  cmdInput_t cmdInput;
  cmdStackLevel_t *cmdStackLevel;

  // Init the pointers to the command line and the control block lists
  cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level];
  cmdStackLevel->cmdProgCounter = NULL;
  cmdStackLevel->cmdLineRoot = NULL;
  cmdStackLevel->cmdPcCtrlRoot = NULL;

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
    // Create new command line, add it to the linked list, and fill in its
    // functional payload
    cmdLine = cmdLineCreate(cmdLine, &cmdStackLevel->cmdLineRoot);
    cmdStackLevel->cmdProgCounter = cmdLine;
    cmdLine->lineNum = lineNum;
    cmdLine->input = malloc(strlen(cmdInput.input) + 1);
    strcpy(cmdLine->input, cmdInput.input);

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching control blocks
    lineNumErr = cmdLineValidate(&cmdPcCtrlLast, &cmdStackLevel->cmdPcCtrlRoot,
      cmdLine);
    if (lineNumErr != 0)
      break;

    // Get next input line
    lineNum++;
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
  cmdPcCtrlSearch = cmdPcCtrlLast;
  while (cmdPcCtrlSearch != NULL)
  {
    if (cmdPcCtrlSearch->cmdLineChild == NULL)
    {
      // Unlinked control block
      cmdStackLevel->cmdProgCounter = cmdPcCtrlSearch->cmdLineParent;
      printf("parse: command unmatched in block starting at line %d\n",
        cmdStackLevel->cmdProgCounter->lineNum);
      return CMD_RET_ERROR;
    }
    else
    {
      // Continue search backwards
      cmdPcCtrlSearch = cmdPcCtrlSearch->prev;
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
  cmdLine_t *cmdLine = NULL;		// The last cmdLine in linked list
  cmdPcCtrl_t *cmdPcCtrlLast = NULL;	// The last cmdPcCtrl in linked list
  int pcCtrlCount = 0;			// Active if+repeat command count
  char *prompt;
  int lineNum = 1;
  int lineNumDigits;
  int lineNumErr = 0;
  cmdStackLevel_t *cmdStackLevel;

  // Init the pointers to the command line and the control block lists
  cmdStackLevel = &cmdStack.cmdStackLevel[cmdStack.level];
  cmdStackLevel->cmdProgCounter = NULL;
  cmdStackLevel->cmdLineRoot = NULL;
  cmdStackLevel->cmdPcCtrlRoot = NULL;

  // Do not read from the keyboard yet as we already have the first command in
  // the input buffer. Once we've processed that one we'll continue reading the
  // input stream using the control structure we've been handed over.

  // Add each line entered via keyboard in a command linked list.
  // The list is complete when the program control block start command (rf/iif)
  // is matched with a corresponding end command (rn/ien).
  // List build-up is interrupted when the user enters ^D on a blank line, when
  // a control block command cannot be matched or when a non-existing command
  // is entered.
  while (MC_TRUE)
  {
    // Create new command line, add it to the linked list, and fill in
    // its functional payload
    cmdLine = cmdLineCreate(cmdLine, &cmdStackLevel->cmdLineRoot);
    cmdStackLevel->cmdProgCounter = cmdLine;
    cmdLine->lineNum = lineNum;
    cmdLine->input = malloc(strlen(cmdInput->input) + 1);
    strcpy(cmdLine->input, cmdInput->input);

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching control blocks
    lineNumErr = cmdLineValidate(&cmdPcCtrlLast, &cmdStackLevel->cmdPcCtrlRoot,
      cmdLine);
    if (lineNumErr != 0)
      break;

    // Administer count of nested repeat and if blocks
    if (cmdLine->cmdCommand != NULL)
    {
      if (cmdLine->cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR ||
          cmdLine->cmdCommand->cmdPcCtrlType == PC_IF)
        pcCtrlCount++;
      else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_REPEAT_NEXT ||
          cmdLine->cmdCommand->cmdPcCtrlType == PC_IF_END)
        pcCtrlCount--;
    }

    // If the most outer control block start is matched we're done
    if (pcCtrlCount == 0)
      break;

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
// Function: cmdPcCtrlCreate
//
// Create a new cmdPcCtrl structure, add it in the control block list,
// initialize it and link it to the command line
//
static cmdPcCtrl_t *cmdPcCtrlCreate(cmdPcCtrl_t *cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine)
{
  cmdPcCtrl_t *cmdPcCtrl = NULL;

  // Allocate the new control block runtime
  cmdPcCtrl = malloc(sizeof(cmdPcCtrl_t));

  // Take care of administrative pointers
  if (*cmdPcCtrlRoot == NULL)
  {
    // First structure in the list
    *cmdPcCtrlRoot = cmdPcCtrl;
  }
  if (cmdPcCtrlLast != NULL)
  {
    // Not the first in the list
    cmdPcCtrlLast->next = cmdPcCtrl;
  }

  // Init the new program counter control block
  cmdPcCtrl->cmdPcCtrlType = cmdLine->cmdCommand->cmdPcCtrlType;
  cmdPcCtrl->active = MC_FALSE;
  cmdPcCtrl->cmdLineParent = cmdLine;
  cmdPcCtrl->cmdLineChild = NULL;
  cmdPcCtrl->prev = cmdPcCtrlLast;
  cmdPcCtrl->next = NULL;
  // The head of the control group is the start command (repeat or if) and is
  // copied down to each other member of the control group
  if (cmdPcCtrl->cmdPcCtrlType == PC_REPEAT_FOR ||
      cmdPcCtrl->cmdPcCtrlType == PC_IF)
    cmdPcCtrl->cmdLineGrpHead = cmdLine;
  else
    cmdPcCtrl->cmdLineGrpHead = cmdLine->cmdPcCtrlParent->cmdLineGrpHead;
  // The tail of the control group is only known when the control group is
  // completed (repeat-next or if-end) and must then be copied into each
  // control group member
  cmdPcCtrl->cmdLineGrpTail = NULL;

  // Link the program counter control block to the current command line
  cmdLine->cmdPcCtrlChild = cmdPcCtrl;

  return cmdPcCtrl;
}

//
// Function: cmdPcCtrlLink
//
// Find an unlinked control block and verify if it can match the control block
// type of the current command line.
// Return values:
//  0 : success
// >0 : starting line number of block in which command cannot be matched
//
static int cmdPcCtrlLink(cmdPcCtrl_t *cmdPcCtrlLast, cmdLine_t *cmdLine)
{
  cmdPcCtrl_t *cmdCtrlFind = cmdPcCtrlLast;
  u08 cmdPcCtrlType = cmdLine->cmdCommand->cmdPcCtrlType;
  u08 cmdFindType;

  while (cmdCtrlFind != NULL)
  {
    cmdFindType = cmdCtrlFind->cmdPcCtrlType;

    // Check the block to verify vs unlinked control block members we find in
    // the linked list. Some combinations must be skipped, some are invalid and
    // some will result in a valid combination.
    if (cmdCtrlFind->cmdLineChild == NULL &&
        (cmdPcCtrlType == PC_IF_ELSE_IF || cmdPcCtrlType == PC_IF_ELSE ||
         cmdPcCtrlType == PC_IF_END) &&
        (cmdFindType == PC_REPEAT_BRK || cmdFindType == PC_REPEAT_CONT))
    {
      // Skip combination if blocks vs repeat break/cont blocks, so go backward
      cmdCtrlFind = cmdCtrlFind->prev;
    }
    else if (cmdCtrlFind->cmdLineChild == NULL &&
        (cmdPcCtrlType == PC_REPEAT_BRK || cmdPcCtrlType == PC_REPEAT_CONT) &&
        (cmdFindType == PC_IF || cmdFindType == PC_IF_ELSE_IF ||
         cmdFindType == PC_IF_ELSE || cmdFindType == PC_IF_END))
    {
      // Skip combination repeat break/cont blocks vs if blocks, so go backward
      cmdCtrlFind = cmdCtrlFind->prev;
    }
    else if (cmdCtrlFind->cmdLineChild == NULL)
    {
      // Found an unlinked control block that is to be validated. Verify if its
      // control block type is compatible with the control block type of the
      // command line.
      if (cmdPcCtrlType == PC_REPEAT_BRK && cmdFindType != PC_REPEAT_FOR &&
          cmdFindType != PC_REPEAT_BRK && cmdFindType != PC_REPEAT_CONT)
        return cmdCtrlFind->cmdLineParent->lineNum;
      if (cmdPcCtrlType == PC_REPEAT_CONT && cmdFindType != PC_REPEAT_FOR &&
          cmdFindType != PC_REPEAT_BRK && cmdFindType != PC_REPEAT_CONT)
        return cmdCtrlFind->cmdLineParent->lineNum;
      if (cmdPcCtrlType == PC_REPEAT_NEXT && cmdFindType != PC_REPEAT_FOR &&
          cmdFindType != PC_REPEAT_BRK && cmdFindType != PC_REPEAT_CONT)
        return cmdCtrlFind->cmdLineParent->lineNum;
      else if (cmdPcCtrlType == PC_IF_ELSE_IF && cmdFindType != PC_IF &&
          cmdFindType != PC_IF_ELSE_IF)
        return cmdCtrlFind->cmdLineParent->lineNum;
      else if (cmdPcCtrlType == PC_IF_ELSE && cmdFindType != PC_IF &&
          cmdFindType != PC_IF_ELSE_IF)
        return cmdCtrlFind->cmdLineParent->lineNum;
      else if (cmdPcCtrlType == PC_IF_END && cmdFindType != PC_IF &&
          cmdFindType != PC_IF_ELSE_IF && cmdFindType != PC_IF_ELSE)
        return cmdCtrlFind->cmdLineParent->lineNum;

      // There's a valid match between the control block types of the current
      // command and the found open control block. Make a cross link between
      // the two.
      cmdLine->cmdPcCtrlParent = cmdCtrlFind;
      cmdCtrlFind->cmdLineChild = cmdLine;

      // Fill in the tail of a complete ctrl group over all its members
      if (cmdPcCtrlType == PC_REPEAT_NEXT || cmdPcCtrlType == PC_IF_END)
      {
        cmdCtrlFind->cmdLineGrpTail = cmdLine;
        while (cmdCtrlFind->cmdPcCtrlType != PC_REPEAT_FOR &&
              cmdCtrlFind->cmdPcCtrlType != PC_IF)
        {
          cmdCtrlFind = cmdCtrlFind->cmdLineParent->cmdPcCtrlParent;
          cmdCtrlFind->cmdLineGrpTail = cmdLine;
        }
      }
      return 0;
    }
    else
    {
      // Search not done yet, go backward
      cmdCtrlFind = cmdCtrlFind->prev;
    }
  }

  // Could not find a valid unlinked control block in entire list.
  // Report error on line 1.
  return 1;
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
    cmdStack.cmdStackLevel[i].cmdPcCtrlRoot = NULL;
    cmdStack.cmdStackLevel[i].cmdEcho = CMD_ECHO_NONE;
    cmdStack.cmdStackLevel[i].cmdOrigin = NULL;
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
// Function: cmdStackIsActive
//
// Returns whether commands are currently run from the stack
//
u08 cmdStackIsActive(void)
{
  if (cmdStack.level >= 0)
    return MC_TRUE;
  else
    return MC_FALSE;
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
    cmdListCleanup(cmdStackLevel->cmdLineRoot, cmdStackLevel->cmdPcCtrlRoot);
    cmdStackLevel->cmdProgCounter = NULL;
    cmdStackLevel->cmdLineRoot = NULL;
    cmdStackLevel->cmdPcCtrlRoot = NULL;
    cmdStackLevel->cmdEcho = CMD_ECHO_NONE;
    free(cmdStackLevel->cmdOrigin);
    cmdStackLevel->cmdOrigin = NULL;
  }

  // Reset stack invoke and resume command when full reset or root level reset
  // is requested
  if (levelMin == 0)
  {
    cmdListCleanup(cmdStack.cmdLineInvoke, NULL);
    cmdStack.cmdLineInvoke = NULL;
    cmdStack.cmdProgCtrIntr = NULL;
    cmdStack.levelResume = -1;
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
// Print the stack
//
static void cmdStackPrint(void)
{
  s08 i;
  cmdStackLevel_t *cmdStackLevel;
  cmdLine_t *cmdLine;

  // Check if no (resume) stack present
  if (cmdStack.level == -1)
    return;

  // Print stack
  printf(CMD_STACK_TRACE);
  for (i = cmdStack.level; i >= 0; i--)
  {
    // In case stack execution was interrupted we must print the command that
    // signalled the interrupt, else take the command that failed or that is
    // active on a higher stack level. Also, only print stacklevel when we can
    // report on it. This will exclude only the top level when loading the
    // file/keyboard list failed.
    cmdStackLevel = &cmdStack.cmdStackLevel[i];
    cmdLine = NULL;
    if (i == cmdStack.level && cmdStack.levelResume >= 0)
      cmdLine = cmdStack.cmdProgCtrIntr;
    else if (cmdStackLevel->cmdProgCounter != NULL)
      cmdLine = cmdStackLevel->cmdProgCounter;

    if (cmdLine != NULL)
    {
      // Print command that signalled the end-user interrupt or error
      printf(CMD_STACK_FMT, i, cmdStackLevel->cmdOrigin, cmdLine->lineNum,
        cmdLine->input);
    }
  }
  // And report stack invoke command
  if (cmdStack.cmdLineInvoke != NULL)
    printf(CMD_STACK_INVOKE_FMT, __progname, cmdStack.cmdLineInvoke->input);
}

//
// Function: cmdStackPrintSet
//
// Enable/disable printing stack command runtime statistics
//
void cmdStackPrintSet(u08 enable)
{
  cmdStackStatsEnable = enable;
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

  // See if we have an input stream, meaning that we'll take our input from
  // the keyboard
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

  // When loading was ok execute the command list
  if (retVal == CMD_RET_OK)
    retVal = cmdListExecute(&cmdStackLevel->cmdProgCounter,
      &cmdStack.cmdProgCtrIntr);

  // If execution was 'q'-interrupted allow to resume execution at this level
  if (retVal == CMD_RET_INTERRUPT)
    cmdStack.levelResume = cmdStack.level;

  // Print stack when initial error/interrupt has occured
  if (retVal == CMD_RET_ERROR || retVal == CMD_RET_INTERRUPT)
    cmdStackPrint();

  // Report execution statistics when we're back at root level
  if (cmdStack.level == 0)
    cmdStackStatsPrint();

  // Restore command echo of parent level
  if (cmdStack.level == 0)
    cmdEcho = CMD_ECHO_YES;
  else
    cmdEcho = cmdStack.cmdStackLevel[cmdStack.level - 1].cmdEcho;

  // Pop the stack level when the stack cannot be resumed. If we can resume
  // do an administrative pop only by decreasing the stack level.
  if (cmdStack.levelResume == -1)
    cmdStackPop(CMD_STACK_POP_LVL);
  else
    cmdStack.level--;

  // When the stack is empty or we were user-interrupted switch back to line
  // mode and stop keyboard timer
  if (cmdStack.level == -1 || retVal == CMD_RET_INTERRUPT)
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
    printf("%s: no resume command stack available\n", cmdName);
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

    // When execution failed to complete initiate recovery or cleanup
    if (retVal == CMD_RET_INTERRUPT)
    {
      // Signal resume capability and print the entire stack
      cmdStack.levelResume = cmdStack.level;
      cmdStackPrint();
      cmdStack.level = -1;
    }
    else if (retVal == CMD_RET_ERROR)
    {
      // First print then clear the full stack
      cmdStackPrint();
      cmdStackPop(CMD_STACK_POP_ALL);
    }
    else if (retVal == CMD_RET_RECOVER)
    {
      // Depending on whether execution was interrupted keep the stack or
      // pop everything
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

    // Current stack level is completed successfully
    if (cmdStack.level == 0)
    {
      // We're back at root level and the stack becomes empty
      cmdEcho = CMD_ECHO_YES;
    }
    else
    {
      // Restore command echo and set resume point of parent level (to continue
      // at the line after the completed 'e' command)
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
