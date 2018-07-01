//*****************************************************************************
// Filename : 'listutil.c'
// Title    : Command list and execution utility routines for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Monochron and emuchron defines
#include "../ks0108.h"
#include "../monomain.h"
#include "stub.h"
#include "scanutil.h"
#include "listutil.h"

// Current command file execution depth
extern int fileExecDepth;

// The current command echo state
extern int echoCmd;

// This is me
extern const char *__progname;

// The command list execution depth that is used to determine to actively
// switch between keyboard line mode and keypress mode
int listExecDepth = 0;

// Local function prototypes
static cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast,
  cmdLine_t **cmdLineRoot);
static int cmdLineValidate(cmdPcCtrl_t **cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine);
static int cmdListKeyboardLoad(cmdLine_t **cmdLineRoot,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdInput_t *cmdInput, int fileExecDepth);
static cmdPcCtrl_t *cmdPcCtrlCreate(cmdPcCtrl_t *cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine);
static int cmdPcCtrlLink(cmdPcCtrl_t *cmdPcCtrlLast, cmdLine_t *cmdLine);

//
// Function: cmdLineCreate
//
// Create a new cmdLine structure and add it in the command linked list
//
static cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot)
{
  cmdLine_t *cmdLine = NULL;

  // Allocate the new command line
  cmdLine = malloc(sizeof(cmdLine_t));

  // Take care of some administrative pointers
  if (*cmdLineRoot == NULL)
  {
    // This is the first list element
    *cmdLineRoot = cmdLine;
  }
  if (cmdLineLast != NULL)
  {
    // Not the first list elemant
    cmdLineLast->next = cmdLine;
  }

  // Init the new structure
  cmdLine->lineNum = 0;
  cmdLine->input = NULL;
  cmdLine->args = NULL;
  cmdLine->initialized = GLCD_FALSE;
  cmdLine->cmdCommand = NULL;
  cmdLine->cmdPcCtrlParent = NULL;
  cmdLine->cmdPcCtrlChild = NULL;
  cmdLine->next = NULL;

  return cmdLine;
}

//
// Function: cmdLineExecute
//
// Process a single line input string.
// This is the main command input handler that can be called recursively via
// the mchron 'e' command.
//
int cmdLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput)
{
  char *input = cmdLine->input;
  cmdCommand_t *cmdCommand;
  int retVal = CMD_RET_OK;

  // Have we scanned the command line arguments yet
  if (cmdLine->initialized == GLCD_FALSE)
  {
    // Start command line scanner by getting command and its dictionary entry
    retVal = cmdArgInit(&input, cmdLine);
    if (retVal != CMD_RET_OK)
      return CMD_RET_ERROR;
    if (*cmdLine->input == '\0')
    {
      // Input contains only white space chars.
      // Dump newline in the log only when we run at root command level.
      if (listExecDepth == 0)
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
    // The user has entered a program control block start command at command
    // prompt level.
    // Cache all keyboard input commands until this command is matched with a
    // corresponding end command. Then execute the command list.
    cmdLine_t *cmdLineRoot = NULL;
    cmdPcCtrl_t *cmdPcCtrlRoot = NULL;

    // Load keyboard command lines in a command list
    retVal = cmdListKeyboardLoad(&cmdLineRoot, &cmdPcCtrlRoot, cmdInput,
      fileExecDepth);
    if (retVal == CMD_RET_OK)
    {
      // Execute the commands in the command list
      int myEchoCmd = echoCmd;
      echoCmd = CMD_ECHO_NO;
      retVal = cmdListExecute(cmdLineRoot, (char *)__progname);
      echoCmd = myEchoCmd;
    }

    // We're done. Either all commands in the linked list have been executed
    // successfully or an error has occured. Do admin stuff by cleaning up the
    // linked lists.
    cmdListCleanup(cmdLineRoot, cmdPcCtrlRoot);
  }
  else
  {
    // All other control block types are invalid on the command line as they
    // need to link to either a repeat-for or if-then command. As such, they
    // can only be entered via multi line keyboard input or a command file.
    printf("command? cannot match this command: %s\n", cmdCommand->cmdName);
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
  int retVal;

  // Start command line scanner by getting command and its dictionary entry
  input = cmdLine->input;
  retVal = cmdArgInit(&input, cmdLine);
  // Command not found
  if (retVal != CMD_RET_OK)
    return -1;
  // Empty command
  if (*cmdLine->input == '\0')
    return 0;

  // Get the control block type and based on that optionally link it to a (new)
  // program control block
  cmdCommand = cmdLine->cmdCommand;
  if (cmdCommand->cmdPcCtrlType != PC_CONTINUE)
  {
    if (cmdCommand->cmdPcCtrlType == PC_REPEAT_NEXT)
    {
      // We must find a repeat-for command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLine);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR)
    {
      // Create new control block and link it to the repeat command
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
// Function: cmdListCleanup
//
// Cleanup a command linked list structure
//
void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdPcCtrl_t *cmdPcCtrlRoot)
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
// Start at the top of the list and work our way down until we find a runtime
// error or end up at the end of the list.
// We may encounter program control blocks that will influence the program
// counter by creating loops or jumps in the execution plan of the list.
//
int cmdListExecute(cmdLine_t *cmdLineRoot, char *source)
{
  char ch = '\0';
  cmdLine_t *cmdProgCounter = NULL;
  cmdLine_t *cmdProgCounterNext = NULL;
  cmdCommand_t *cmdCommand;
  char *input;
  int cmdPcCtrlType;
  int i;
  int retVal = CMD_RET_OK;

  // See if we need to switch to keypress mode. We only need to do this at the
  // top of nested list executions. Switching to keypress mode allows the
  // end-user to interrupt the list execution using a 'q' keypress.
  if (listExecDepth == 0)
    kbModeSet(KB_MODE_SCAN);
  listExecDepth++;

  // We start at the root of the linked list. Let's see where we end up.
  cmdProgCounter = cmdLineRoot;
  while (cmdProgCounter != NULL)
  {
    // Echo command
    if (echoCmd == CMD_ECHO_YES)
    {
      for (i = 1; i < fileExecDepth; i++)
        printf(":   ");
      printf(":%03d: %s\n", cmdProgCounter->lineNum, cmdProgCounter->input);
    }

    // Execute the command in the command line
    cmdCommand = cmdProgCounter->cmdCommand;
    if (cmdCommand == NULL)
    {
      // Skip a blank command line
      cmdPcCtrlType = PC_CONTINUE;
      retVal = CMD_RET_OK;
    }
    else if (cmdCommand->cmdPcCtrlType == PC_CONTINUE)
    {
      // Execute a regular command via the generic handler
      cmdPcCtrlType = PC_CONTINUE;
      retVal = cmdLineExecute(cmdProgCounter, NULL);
    }
    else
    {
      // This is a control block command from a repeat or if construct. Get the
      // command arguments (if not already done) and execute the associated
      // program counter control block handler from the command dictionary.
      input = cmdProgCounter->input;
      if (cmdProgCounter->initialized == GLCD_FALSE)
      {
        cmdArgInit(&input, cmdProgCounter);
        retVal = cmdArgRead(input, cmdProgCounter);
      }
      if (retVal == CMD_RET_OK)
      {
        cmdPcCtrlType = cmdCommand->cmdPcCtrlType;
        cmdProgCounterNext = cmdProgCounter;
        retVal = cmdCommand->cbHandler(&cmdProgCounterNext);
      }
    }

    // Verify if a command interrupt was requested
    if (retVal == CMD_RET_OK)
    {
      ch = kbKeypressScan(GLCD_TRUE);
      if (ch == 'q')
      {
        printf("quit\n");
        retVal = CMD_RET_INTERRUPT;
      }
    }

    // Check for error/interrupt
    if (retVal == CMD_RET_ERROR || retVal == CMD_RET_INTERRUPT)
    {
      // Error/interrupt occured in current level
      printf(CMD_STACK_TRACE);
      // Report current stack level and cascade to upper level
      printf(CMD_STACK_FMT, fileExecDepth, source, cmdProgCounter->lineNum,
        cmdProgCounter->input);
      retVal = CMD_RET_RECOVER;
      break;
    }
    else if (retVal == CMD_RET_RECOVER)
    {
      // Error/interrupt occured at lower level.
      // Report current stack level and cascade to upper level.
      printf(CMD_STACK_FMT, fileExecDepth, source, cmdProgCounter->lineNum,
        cmdProgCounter->input);
      break;
    }

    // Move to next command in linked list
    if (cmdPcCtrlType == PC_CONTINUE)
      cmdProgCounter = cmdProgCounter->next;
    else
      cmdProgCounter = cmdProgCounterNext;
  }

  // End of list or encountered error/interrupt.
  // Switch back to line mode when we're back at root level.
  listExecDepth--;
  if (listExecDepth == 0)
    kbModeSet(KB_MODE_LINE);

  return retVal;
}

//
// Function: cmdListFileLoad
//
// Load command file contents in a linked list structure
//
int cmdListFileLoad(cmdLine_t **cmdLineRoot, cmdPcCtrl_t **cmdPcCtrlRoot,
  char *fileName, int fileExecDepth)
{
  FILE *fp;				// Input file pointer
  cmdLine_t *cmdLineLast = NULL;	// The last cmdline in linked list
  cmdPcCtrl_t *cmdPcCtrlLast = NULL;	// The last cmdPcCtrl in linked list
  cmdPcCtrl_t *searchPcCtrl = NULL;	// Active cmdPcCtrl in search efforts
  int lineNum = 1;
  int lineNumErr = 0;
  cmdInput_t cmdInput;

  // Init the pointers to the command line and the control block lists
  *cmdLineRoot = NULL;
  *cmdPcCtrlRoot = NULL;

  // Open command file
  fp = fopen(fileName, "r");
  if (fp == NULL)
  {
    printf("cannot open command file \"%s\"\n", fileName);
    printf(CMD_STACK_TRACE);
    return CMD_RET_ERROR;
  }

  // Initialize our file readline interface method and do the first read
  cmdInput.file = fp;
  cmdInput.readMethod = CMD_INPUT_MANUAL;
  cmdInputInit(&cmdInput);
  cmdInputRead("", &cmdInput);

  // Add each line in the command file in a command linked list
  while (cmdInput.input != NULL)
  {
    // Create new command line, add it to the linked list, and fill in its
    // functional payload
    cmdLineLast = cmdLineCreate(cmdLineLast, cmdLineRoot);
    cmdLineLast->lineNum = lineNum;
    cmdLineLast->input = malloc(strlen(cmdInput.input) + 1);
    strcpy(cmdLineLast->input, cmdInput.input);

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching control blocks
    lineNumErr = cmdLineValidate(&cmdPcCtrlLast, cmdPcCtrlRoot, cmdLineLast);
    if (lineNumErr != 0)
      break;

    // Get next input line
    lineNum++;
    cmdInputRead("", &cmdInput);
  }

  // File content is loaded in linked list or error occurred while parsing
  cmdInputCleanup(&cmdInput);
  fclose(fp);

  // Check if a control block command cannot be matched or command not found
  if (lineNumErr > 0)
  {
    printf("parse: command unmatched in block starting at line %d\n",
      lineNumErr);
    printf(CMD_STACK_TRACE);
    printf(CMD_STACK_FMT, fileExecDepth, fileName, lineNum,
      cmdLineLast->input);
    return CMD_RET_ERROR;
  }
  else if (lineNumErr < 0)
  {
    printf("parse: invalid command\n");
    printf(CMD_STACK_TRACE);
    printf(CMD_STACK_FMT, fileExecDepth, fileName, lineNum,
      cmdLineLast->input);
    return CMD_RET_ERROR;
  }

  // Postprocessing the linked lists.
  // We may not find a control block that is not linked to a command line.
  searchPcCtrl = cmdPcCtrlLast;
  while (searchPcCtrl != NULL)
  {
    if (searchPcCtrl->cmdLineChild == NULL)
    {
      // Unlinked control block
      printf("parse: command unmatched in block starting at line %d\n",
        searchPcCtrl->cmdLineParent->lineNum);
      printf(CMD_STACK_TRACE);
      printf(CMD_STACK_FMT, fileExecDepth, fileName,
        searchPcCtrl->cmdLineParent->lineNum,
        searchPcCtrl->cmdLineParent->input);
      return CMD_RET_ERROR;
    }
    else
    {
      // Continue search backwards
      searchPcCtrl = searchPcCtrl->prev;
    }
  }

  // The file contents have been read into linked lists and is verified for its
  // integrity on matching program counter control block constructs
  return CMD_RET_OK;
}

//
// Function: cmdListKeyboardLoad
//
// Load keyboard commands interactively in a linked list structure
//
static int cmdListKeyboardLoad(cmdLine_t **cmdLineRoot,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdInput_t *cmdInput, int fileExecDepth)
{
  cmdLine_t *cmdLineLast = NULL;	// The last cmdline in linked list
  cmdPcCtrl_t *cmdPcCtrlLast = NULL;	// The last cmdPcCtrl in linked list
  int pcCtrlCount = 0;			// Active if+repeat command count
  char *prompt;
  int lineNum = 1;
  int lineNumDigits;
  int lineNumErr = 0;

  // Init return pointers
  *cmdLineRoot = NULL;
  *cmdPcCtrlRoot = NULL;

  // Do not read from the keyboard yet as we already have the first command in
  // the input buffer. Once we've processed that one we'll continue reading the
  // input stream using the control structure we've been handed over.

  // Add each line entered via keyboard in a command linked list.
  // The list is complete when the program control block start command (rf/iif)
  // is matched with a corresponding end command (rn/ien).
  // List build-up is interrupted when the user enters ^D on a blank line, when
  // a control block command cannot be matched or when a non-existing command
  // is entered.
  while (GLCD_TRUE)
  {
    // Create new command line, add it to the linked list, and fill in
    // its functional payload
    cmdLineLast = cmdLineCreate(cmdLineLast, cmdLineRoot);
    cmdLineLast->lineNum = lineNum;
    cmdLineLast->input = malloc(strlen(cmdInput->input) + 1);
    strcpy(cmdLineLast->input, cmdInput->input);

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching control blocks
    lineNumErr = cmdLineValidate(&cmdPcCtrlLast, cmdPcCtrlRoot, cmdLineLast);
    if (lineNumErr != 0)
      break;

    // Administer count of nested repeat and if blocks
    if (cmdLineLast->cmdCommand != NULL)
    {
      if (cmdLineLast->cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR ||
          cmdLineLast->cmdCommand->cmdPcCtrlType == PC_IF)
        pcCtrlCount++;
      else if (cmdLineLast->cmdCommand->cmdPcCtrlType == PC_REPEAT_NEXT ||
          cmdLineLast->cmdCommand->cmdPcCtrlType == PC_IF_END)
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
    sprintf(prompt, "%d", lineNum);
    strcat(prompt, ">> ");

    // Get the next keyboard input line
    cmdInputRead(prompt, cmdInput);
    free(prompt);
    if (cmdInput->input == NULL)
    {
      printf("\n<ctrl>d - quit\n");
      return CMD_RET_ERROR;
    }
  }

  // Check if a control block command cannot be matched or command not found
  if (lineNumErr > 0)
  {
    printf("parse: command unmatched in block starting at line %d\n",
      lineNumErr);
    printf(CMD_STACK_TRACE);
    printf(CMD_STACK_FMT, fileExecDepth, __progname, lineNum,
      cmdLineLast->input);
    return CMD_RET_ERROR;
  }
  else if (lineNumErr < 0)
  {
    printf("parse: invalid command\n");
    printf(CMD_STACK_TRACE);
    printf(CMD_STACK_FMT, fileExecDepth, __progname, lineNum,
      cmdLineLast->input);
    return CMD_RET_ERROR;
  }

  // We do not need to postprocess the control block linked list as we keep
  // track of the number of control block start vs end commands.
  // When we get here all control block start commands are matched with a
  // proper control block end command.
  return CMD_RET_OK;
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
  cmdPcCtrl->active = GLCD_FALSE;
  cmdPcCtrl->cmdLineParent = cmdLine;
  cmdPcCtrl->cmdLineChild = NULL;
  cmdPcCtrl->prev = cmdPcCtrlLast;
  cmdPcCtrl->next = NULL;

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
  cmdPcCtrl_t *searchPcCtrl = cmdPcCtrlLast;

  while (searchPcCtrl != NULL)
  {
    if (searchPcCtrl->cmdLineChild == NULL)
    {
      // Found an unlinked control block. Verify if its control block type is
      // compatible with the control block type of the command line.
      if (cmdLine->cmdCommand->cmdPcCtrlType == PC_REPEAT_NEXT &&
          searchPcCtrl->cmdPcCtrlType != PC_REPEAT_FOR)
        return searchPcCtrl->cmdLineParent->lineNum;
      else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_IF_ELSE_IF &&
          searchPcCtrl->cmdPcCtrlType != PC_IF &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_ELSE_IF)
        return searchPcCtrl->cmdLineParent->lineNum;
      else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_IF_ELSE &&
          searchPcCtrl->cmdPcCtrlType != PC_IF &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_ELSE_IF)
        return searchPcCtrl->cmdLineParent->lineNum;
      else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_IF_END &&
          searchPcCtrl->cmdPcCtrlType != PC_IF &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_ELSE_IF &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_ELSE)
        return searchPcCtrl->cmdLineParent->lineNum;

      // There's a valid match between the control block types of the current
      // command and the last open control block. Make a cross link between the
      // two.
      cmdLine->cmdPcCtrlParent = searchPcCtrl;
      searchPcCtrl->cmdLineChild = cmdLine;
      return 0;
    }
    else
    {
      // Search not done yet, go backward
      searchPcCtrl = searchPcCtrl->prev;
    }
  }

  // Could not find an unlinked control block in entire list.
  // Report error on line 1.
  return 1;
}
