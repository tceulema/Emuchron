//*****************************************************************************
// Filename : 'listutil.c'
// Title    : Command list utility routines for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Monochron and emuchron defines
#include "../ks0108.h"
#include "scanutil.h"
#include "listutil.h"

// From the variables that store the processed command line arguments
// we only need the word arguments
extern char *argWord[];

// The command profile for an mchron command
extern cmdArg_t argCmd[];

// This is me
extern const char *__progname;

// Local function prototypes
static int cmdLineComplete(cmdPcCtrl_t **cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLineLast);
static cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast,
  cmdLine_t **cmdLineRoot);
static cmdPcCtrl_t *cmdPcCtrlCreate(cmdPcCtrl_t *cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine);
static int cmdPcCtrlLink(cmdPcCtrl_t *cmdPcCtrlLast, cmdLine_t *cmdLine);

//
// Function: cmdLineComplete
//
// Complete a single command line and, if needed, add or find a program
// counter control block and associate it with the command line.
// Return values:
// -1 : invalid command
//  0 : success
// >0 : starting line number of block in which command cannot be matched
//
static int cmdLineComplete(cmdPcCtrl_t **cmdPcCtrlLast, cmdPcCtrl_t **cmdPcCtrlRoot,
  cmdLine_t *cmdLineLast)
{
  int retVal;
  int lineNumErr = 0;
  cmdCommand_t *cmdCommand;
  char *input;

  // Scan the command name in this command line
  input = cmdLineLast->input;
  cmdArgInit(&input);
  cmdArgScan(argCmd, 1, &input, GLCD_TRUE);
  if (argWord[0] == NULL)
  {
    // Most likely an empty/whitespace line, which is not an error.
    // It also means we're done.
    cmdLineLast->cmdCommand = NULL;
    return 0;
  }

  // Get the dictionary entry for the command
  retVal = cmdDictCmdGet(argWord[0], &cmdCommand);
  if (retVal != CMD_RET_OK)
  {
    // Unknown command
    cmdLineLast->cmdCommand = NULL;
    return -1;
  }

  // Set the control block type and based on that optionally
  // link it a (new) program control block
  cmdLineLast->cmdCommand = cmdCommand;

  // Only in case of a program counter control block command
  // we need to administer control blocks
  if (cmdCommand->cmdPcCtrlType != PC_CONTINUE)
  {
    if (cmdCommand->cmdPcCtrlType == PC_REPEAT_NEXT)
    {
      // We must find a repeat-for command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLineLast);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR)
    {
      // Create new control block and link it to the repeat command
      *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
        cmdLineLast);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF_ELSE_IF)
    {
      // We must find an if or else-if command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLineLast);

      // Create new control block and link it to the if-else-if command
      if (lineNumErr == 0)
        *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
          cmdLineLast);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF_END)
    {
      // We must find an if, else-if or else command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLineLast);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF_ELSE)
    {
      // We must find an if or else-if command to associate with
      lineNumErr = cmdPcCtrlLink(*cmdPcCtrlLast, cmdLineLast);

      // Create new control block and link it to the if-else command
      if (lineNumErr == 0)
        *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
          cmdLineLast);
    }
    else if (cmdCommand->cmdPcCtrlType == PC_IF)
    {
      // Create new control block and link it to the if-then command
      *cmdPcCtrlLast = cmdPcCtrlCreate(*cmdPcCtrlLast, cmdPcCtrlRoot,
        cmdLineLast);
    }
  }

  return lineNumErr;
}

//
// Function: cmdLineCreate
//
// Create a new cmdLine structure and add it in the command linked list
//
static cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot)
{
  cmdLine_t *cmdLine = NULL;

  // Allocate the new command line
  cmdLine = (cmdLine_t *)malloc(sizeof(cmdLine_t));

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
  cmdLine->cmdCommand = NULL;
  cmdLine->cmdPcCtrlParent = NULL;
  cmdLine->cmdPcCtrlChild = NULL;
  cmdLine->next = NULL;

  return cmdLine;
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
    // Return the malloc-ed command line
    free(nextLine->input);
    nextLine = cmdLineRoot->next;

    // Return the malloc-ed list element itself
    free(cmdLineRoot);
    cmdLineRoot = nextLine;
  }

  // Free the linked list of program counter control blocks
  nextPcCtrl = cmdPcCtrlRoot;
  while (nextPcCtrl != NULL)
  {
    // Return the malloc-ed control block argument expressions
    if (nextPcCtrl->cbArg1 != NULL)
      free(nextPcCtrl->cbArg1);
    if (nextPcCtrl->cbArg2 != NULL)
      free(nextPcCtrl->cbArg2);
    if (nextPcCtrl->cbArg3 != NULL)
      free(nextPcCtrl->cbArg3);
    nextPcCtrl = cmdPcCtrlRoot->next;

    // Return the malloc-ed list element itself
    free(cmdPcCtrlRoot);
    cmdPcCtrlRoot = nextPcCtrl;
  }
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
    // Create new command line, add it to the linked list, and fill in
    // its functional payload
    cmdLineLast = cmdLineCreate(cmdLineLast, cmdLineRoot);
    cmdLineLast->lineNum = lineNum;
    cmdLineLast->input = malloc(strlen(cmdInput.input) + 1);
    strcpy(cmdLineLast->input, cmdInput.input);

    // Scan and validate the command name (but not its arguments) as well as
    // validating matching control blocks
    lineNumErr = cmdLineComplete(&cmdPcCtrlLast, cmdPcCtrlRoot, cmdLineLast);
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

  // The file contents have been read into linked lists and is verified for
  // its integrity on matching program counter control block constructs
  return CMD_RET_OK;
}

//
// Function: cmdListKeyboardLoad
//
// Load keyboard commands interactively in a linked list structure
//
int cmdListKeyboardLoad(cmdLine_t **cmdLineRoot, cmdPcCtrl_t **cmdPcCtrlRoot,
  cmdInput_t *cmdInput, int fileExecDepth)
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

  // Do not read from the keyboard yet as we already have the first
  // command in the input buffer. Once we've processed that one we'll
  // continue reading the input stream using the control structure we've
  // been handed over.

  // Add each line entered via keyboard in a command linked list.
  // The list is complete when the program control block start command
  // (rf/iif) is matched with a corresponding program control block end
  // command (rn/ien) command.
  // List build-up is interrupted when the user enters ^D on a blank line,
  // when a control block command cannot be matched or when a non-existing
  // command is entered.
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
    lineNumErr = cmdLineComplete(&cmdPcCtrlLast, cmdPcCtrlRoot, cmdLineLast);
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
  cmdPcCtrl_t *cmdPcCtrlMalloc = NULL;

  // Allocate the new control block runtime
  cmdPcCtrlMalloc = (cmdPcCtrl_t *)malloc(sizeof(cmdPcCtrl_t));

  // Take care of administrative pointers
  if (*cmdPcCtrlRoot == NULL)
  {
    // First structure in the list
    *cmdPcCtrlRoot = cmdPcCtrlMalloc;
  }
  if (cmdPcCtrlLast != NULL)
  {
    // Not the first in the list
    cmdPcCtrlLast->next = cmdPcCtrlMalloc;
  }

  // Init the new program counter control block
  cmdPcCtrlMalloc->cmdPcCtrlType = cmdLine->cmdCommand->cmdPcCtrlType;
  cmdPcCtrlMalloc->initialized = GLCD_FALSE;
  cmdPcCtrlMalloc->active = GLCD_FALSE;
  cmdPcCtrlMalloc->cbArg1 = NULL;
  cmdPcCtrlMalloc->cbArg2 = NULL;
  cmdPcCtrlMalloc->cbArg3 = NULL;
  cmdPcCtrlMalloc->cmdLineParent = cmdLine;
  cmdPcCtrlMalloc->cmdLineChild = NULL;
  cmdPcCtrlMalloc->prev = cmdPcCtrlLast;
  cmdPcCtrlMalloc->next = NULL;

  // Link the program counter control block to the current command line
  cmdLine->cmdPcCtrlChild = cmdPcCtrlMalloc;

  return cmdPcCtrlMalloc;
}

//
// Function: cmdPcCtrlArgCreate
//
// Allocate memory of a program counter control block argument and copy
// the argument expression into it.
// Note that a '\n' is added at the end of the expression as per
// expression evaluator requirement.
//
char *cmdPcCtrlArgCreate(char *argExpr)
{
  int argExprLen = 0;
  char *mallocPtr = NULL;

  argExprLen = strlen(argExpr) + 2;
  mallocPtr = malloc(argExprLen);
  strcpy(mallocPtr, argExpr);
  mallocPtr[argExprLen - 2] = '\n';
  mallocPtr[argExprLen - 1] = '\0';

  return mallocPtr;
}

//
// Function: cmdPcCtrlLink
//
// Find an unlinked control block and verify if it can match the control
// block type of the current command line.
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
      // Found an unlinked control block. Verify if its control block type
      // is compatible with the control block type of the command line.
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
      // command and the last open control block. Make a cross link between
      // the two.
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
