//*****************************************************************************
// Filename : 'listvarutil.c'
// Title    : Command list and variable utility routines for mchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Monochron and emuchron defines
#include "../ks0108.h"
#include "scanutil.h"
#include "listvarutil.h"

// From the variables that store the processed command line arguments
// we only need the word arguments
extern char *argWord[];

// The command profile for an mchron command
extern cmdArg_t argCmd[];

// This is me
extern const char *__progname;

// A structure to hold runtime information for a named numeric variable
typedef struct _variable_t
{
  char *name;			// Variable name
  int active;			// Whether variable is in use
  double varValue;		// The current numeric value of the variable
  struct _variable_t *prev;	// Pointer to preceding bucket member
  struct _variable_t *next;	// Pointer to next bucket member
} variable_t;

// A structure to hold a list of numeric variables
typedef struct _varbucket_t
{
  int count;			// Number of bucket members
  struct _variable_t *var;	// Pointer to first bucket member
} varbucket_t;

// The administration of mchron variables.
// Variables are spread over buckets. There are VAR_BUCKETS buckets.
// Each bucket can contain up to VAR_BUCKET_SIZE variables.
// Also administer the total number of bucket members in use.
#define VAR_BUCKETS		26
#define VAR_BUCKET_SIZE		512
static varbucket_t varBucket[VAR_BUCKETS];
static int varCount = 0;

// Local function prototypes
static int cmdLineComplete(cmdPcCtrl_t **cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLineLast);
static cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast,
  cmdLine_t **cmdLineRoot);
static cmdPcCtrl_t *cmdPcCtrlCreate(cmdPcCtrl_t *cmdPcCtrlLast,
  cmdPcCtrl_t **cmdPcCtrlRoot, cmdLine_t *cmdLine);
static int cmdPcCtrlLink(cmdPcCtrl_t *cmdPcCtrlLast, cmdLine_t *cmdLine);
static int varPrintValue(char *var, double value, int detail);

//
// Function: cmdLineComplete
//
// Complete a single command line and, if needed, add or find a program
// counter control block and associate it with the command line.
// Return values:
// -1 : invalid command 
//  0 : success
// >0 : line number of block in which command cannot be matched
//
static int cmdLineComplete(cmdPcCtrl_t **cmdPcCtrlLast, cmdPcCtrl_t **cmdPcCtrlRoot,
  cmdLine_t *cmdLineLast)
{
  int retVal;
  int lineNumErr = 0;
  cmdCommand_t *cmdCommand;

  // Process the command name scan result
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
    else if (cmdCommand->cmdPcCtrlType == PC_IF_THEN)
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
  char *input = NULL;
  cmdInput_t cmdInput;

  // Init the pointers to the command line and the control block lists
  *cmdLineRoot = NULL;
  *cmdPcCtrlRoot = NULL;

  // Open command file
  fp = fopen(fileName, "r");
  if (fp == NULL)
  {
    printf("cannot open command file \"%s\"\n", fileName);
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
    // Set work pointer
    input = cmdInput.input;

    // Create new command line and add it to the linked list
    cmdLineLast = cmdLineCreate(cmdLineLast, cmdLineRoot);

    // Fill in functional payload of the list element
    cmdLineLast->lineNum = lineNum;
    cmdLineLast->input = malloc(strlen(input) + 1);
    strcpy(cmdLineLast->input, (const char *)input);

    // Scan the command name in this line
    cmdArgInit(&input);
    cmdArgScan(argCmd, 1, &input, GLCD_TRUE);

    // Process the command name scan result that includes validating
    // the command name (but not its arguments), retrieving the command
    // dictionary and matching control blocks
    lineNumErr = cmdLineComplete(&cmdPcCtrlLast, cmdPcCtrlRoot,
      cmdLineLast);
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
    printf("%d:%s:%d:%s\n", fileExecDepth, fileName, lineNum,
      cmdLineLast->input);
    printf("parse: command unmatched in block starting at line %d\n",
      lineNumErr);
    return CMD_RET_ERROR;
  }
  else if (lineNumErr < 0)
  {
    printf("%d:%s:%d:%s\n", fileExecDepth, fileName, lineNum,
      cmdLineLast->input);
    printf("parse: invalid command\n");
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
      printf("%d:%s:%d:%s\n", fileExecDepth, fileName,
        searchPcCtrl->cmdLineParent->lineNum,
        searchPcCtrl->cmdLineParent->input);
      printf("parse: command unmatched in block starting at line %d\n",
        searchPcCtrl->cmdLineParent->lineNum);
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
  char *input = NULL;

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
    // Create new command line and add it to the linked list
    // by administering lots of pointers
    cmdLineLast = cmdLineCreate(cmdLineLast, cmdLineRoot);

    // Fill in functional payload of the list element
    cmdLineLast->lineNum = lineNum;
    cmdLineLast->input = malloc(strlen(cmdInput->input) + 1);
    strcpy(cmdLineLast->input, (const char *)cmdInput->input);

    // Scan the command name in this line
    input = cmdLineLast->input;
    cmdArgInit(&input);
    cmdArgScan(argCmd, 1, &input, GLCD_TRUE);

    // Process the command name scan result that includes validating
    // the command name (but not its arguments) and matching control
    // blocks
    lineNumErr = cmdLineComplete(&cmdPcCtrlLast, cmdPcCtrlRoot,
      cmdLineLast);
    if (lineNumErr != 0)
      break;

    // Administer count of nested repeat and if blocks
    if (cmdLineLast->cmdCommand != NULL)
    {
      if (cmdLineLast->cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR ||
          cmdLineLast->cmdCommand->cmdPcCtrlType == PC_IF_THEN)
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
    printf("%d:%s:%d:%s\n", fileExecDepth, __progname, lineNum,
      cmdLineLast->input);
    printf("parse: command unmatched in block starting at line %d\n",
      lineNumErr);
    return CMD_RET_ERROR;
  }
  else if (lineNumErr < 0)
  {
    printf("%d:%s:%d:%s\n", fileExecDepth, __progname, lineNum,
      cmdLineLast->input);
    printf("parse: invalid command\n");
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
// Returns 0 in case of success. In case of failure it returns the linenumber
// marking the execution block in which no matching link could be found.
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
          searchPcCtrl->cmdPcCtrlType != PC_IF_THEN &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_ELSE_IF)
        return searchPcCtrl->cmdLineParent->lineNum;
      else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_IF_ELSE &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_THEN &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_ELSE_IF)
        return searchPcCtrl->cmdLineParent->lineNum;
      else if (cmdLine->cmdCommand->cmdPcCtrlType == PC_IF_END &&
          searchPcCtrl->cmdPcCtrlType != PC_IF_THEN &&
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

//
// Function: varClear
//
// Clear a named variable
//
int varClear(char *argName, char *var)
{
  int varId;
  int bucketId;
  int bucketListId;
  int i = 0;
  variable_t *delVar;

  varId = varIdGet(var);
  if (varId == -1)
  {
    printf("%s? internal bucket overflow\n", argName);
    return CMD_RET_ERROR;
  }

  // Get reference to variable
  bucketId = varId & 0xff;
  bucketListId = (varId >> 8);
  delVar = varBucket[bucketId].var;
  for (i = 0; i < bucketListId; i++)
    delVar = delVar->next;

  // Remove variable from the list
  if (delVar->prev != NULL)
    delVar->prev->next = delVar->next;
  else
    varBucket[bucketId].var = delVar->next;
  if (delVar->next != NULL)
    delVar->next->prev = delVar->prev;

  // Correct variable counters
  (varBucket[bucketId].count)--;
  varCount--;

  // Return the malloced variable name and the structure itself
  free(delVar->name);
  free(delVar);

  return CMD_RET_OK;
}

//
// Function: varIdGet
//
// Get the id of a named variable using its name. When the name is scanned by
// the flex lexer it is guaranteed to consist of any combination of [a-zA-Z]
// characters. When used in commmand 'vp' or 'vr' the command handler function
// is responsible for checking whether the var name consists of [a-zA-Z] chars
// or a '*' string.
// Return values:
// >=0 : variable id (combination of bucket id and index in bucket)
//  -1 : variable bucket overflow
//
int varIdGet(char *var)
{
  int bucketId;
  int bucketListId;
  int found = GLCD_FALSE;
  int compare = 0;
  int i = 0;
  variable_t *checkVar;
  variable_t *myVar;

  // Create hash of first and optionally second character of var name to
  // obtain variable bucket number
  if (var[0] < 'a')
    bucketId = var[0] - 'A';
  else
    bucketId = var[0] - 'a';
  if (var[1] != '\0')
  {
    if (var[1] < 'a')
      bucketId = bucketId + (var[1] - 'A');
    else
      bucketId = bucketId + (var[1] - 'a');
    if (bucketId >= VAR_BUCKETS)
      bucketId = bucketId - VAR_BUCKETS;
  }

  // Find the variable in the bucket
  checkVar = varBucket[bucketId].var;
  while (i < varBucket[bucketId].count && found == GLCD_FALSE)
  {
    compare = strcmp(checkVar->name, var);
    if (compare == 0)
    {
      found = GLCD_TRUE;
      bucketListId = i;
    }
    else
    {
      // Not done searching
      if (i != varBucket[bucketId].count - 1)
      {
        // Next list member
        checkVar = checkVar->next;
        i++;
      }
      else
      {
        // Searched all list members and did not find variable
        break;
      }
    }
  }

  if (found == GLCD_TRUE)
  {
    // Var name found
    bucketListId = i;
  }
  else
  {
    // Var name not found in the bucket so let's add it.
    // However, first check for bucket overflow.
    if (varBucket[bucketId].count == VAR_BUCKET_SIZE)
    {
      printf("cannot register variable: %s\n", var);
      return -1;
    }

    // Add variable to end of the bucket list
    myVar = malloc(sizeof(variable_t));
    myVar->name = malloc(strlen(var) + 1);
    strcpy(myVar->name, var);
    myVar->active = GLCD_FALSE;
    myVar->varValue = 0;
    myVar->next = NULL;

    if (varBucket[bucketId].count == 0)
    {
      // Bucket is empty
      varBucket[bucketId].var = myVar;
      myVar->prev = NULL;
      bucketListId = 0;
    }
    else
    {
      // Add to end of bucket list
      checkVar->next = myVar;
      myVar->prev = checkVar;
      bucketListId = i + 1;
    }

    // Admin counters
    (varBucket[bucketId].count)++;
    varCount++;
  }

  // Var name is validated and its hashed bucket with bucket list index
  // is returned
  //printf("%s: bucket=%d index=%d\n", var, bucketId, bucketListId);
  return (bucketListId << 8) + bucketId;
}

//
// Function: varInit
//
// Initialize the named variable buckets
//
void varInit(void)
{
  int i;

  for (i = 0; i < VAR_BUCKETS; i++)
  {
    varBucket[i].count = 0;
    varBucket[i].var = NULL;
  }
  varCount = 0;
}

//
// Function: varPrint
//
// Print the value of a single or all named variables
//
int varPrint(char *argName, char *var)
{
  // Get and print the value of one or all variables
  if (strcmp(var, "*") == 0)
  {
    const int spaceCountMax = 60;
    int spaceCount = 0;
    int varInUse = 0;
    int i;
    int varIdx = 0;
    int allSorted = GLCD_FALSE;

    if (varCount != 0)
    {
      // Get pointers to all variables and then sort them
      variable_t *myVar;
      variable_t *varSort[varCount];

      for (i = 0; i < VAR_BUCKETS; i++)
      {
        // Copy references to all bucket list members
        myVar = varBucket[i].var;
        while (myVar != NULL)
        {
          varSort[varIdx] = myVar;
          myVar = myVar->next;
          varIdx++;
        }
      }

      // Sort the array based on var name
      while (allSorted == GLCD_FALSE)
      {
        allSorted = GLCD_TRUE;
        for (i = 0; i < varIdx - 1; i++)
        {
          if (strcmp(varSort[i]->name, varSort[i + 1]->name) > 0)
          {
            myVar = varSort[i];
            varSort[i] = varSort[i + 1];
            varSort[i + 1] = myVar;
            allSorted = GLCD_FALSE;
          }
        }
        varIdx--;
      }

      // Print the vars from the sorted array
      for (i = 0; i < varCount; i++)
      {
        if (varSort[i]->active == GLCD_TRUE)
        {
          varInUse++;
          spaceCount = spaceCount +
            varPrintValue(varSort[i]->name, varSort[i]->varValue, GLCD_FALSE);
          if (spaceCount % 10 != 0)
          {
            printf("%*s", 10 - spaceCount % 10, "");
            spaceCount = spaceCount + 10 - spaceCount % 10;
          }
          if (spaceCount >= spaceCountMax)
          {
            spaceCount = 0;
            printf("\n");
          }
        }
      }
    }

    // End on newline if needed and provide variable summary
    if (spaceCount != 0)
      printf("\n");
    printf("variables in use: %d\n", varInUse);
  }
  else
  {
    // Get and print the value of a single variable, when active
    int varId;
    double varValue = 0;
    int varError = 0;

    // Get var id
    varId = varIdGet(var);
    if (varId == -1)
    {
      printf("%s? internal bucket overflow\n", argName);
      return CMD_RET_ERROR;
    }

    // Get var value
    varValue = varValGet(varId, &varError);
    if (varError == 1)
      return CMD_RET_ERROR;

    // Print var value
    varPrintValue(var, varValue, GLCD_TRUE);
    printf("\n");
  }

  return CMD_RET_OK;
}

//
// Function: varPrintValue
//
// Print a single variable value and return the length
// of the printed string
//
static int varPrintValue(char *var, double value, int detail)
{
  return printf("%s=", var) + cmdArgValuePrint(value, detail);
}

//
// Function: varReset
//
// Reset all named variable data
//
void varReset(void)
{
  int i = 0;
  variable_t *delVar;
  variable_t *nextVar;

  // Clear each bucket
  for (i = 0; i < VAR_BUCKETS; i++)
  {
    // Clear all variable in bucket
    nextVar = varBucket[i].var;
    while (nextVar != NULL)
    {
      delVar = nextVar;
      nextVar = delVar->next;
      free(delVar->name);
      free(delVar);
    }
    varBucket[i].count = 0;
    varBucket[i].var = NULL;
  }
  varCount = 0;
}

//
// Function: varValGet
//
// Get the value of a named variable using its id
//
double varValGet(int varId, int *varError)
{
  int bucketId;
  int bucketListId;
  int i = 0;
  variable_t *myVar;

  // Check if we have a valid id
  if (varId < 0)
  {
    *varError = 1;
    return 0;
  }

  // Get reference to variable
  bucketId = varId & 0xff;
  bucketListId = (varId >> 8);
  myVar = varBucket[bucketId].var;
  for (i = 0; i < bucketListId; i++)
    myVar = myVar->next;

  // Only an active variable has a value
  if (myVar->active == 0)
  {
    printf("variable not in use: %s\n", myVar->name);
    *varError = 1;
    return 0;
  }

  // Return value
  *varError = 0;
  return myVar->varValue;
}

//
// Function: varValSet
//
// Set the value of a named variable using its id.
//
// In case a scanner/parser error occurs during the expression evaluation
// process we won't even get in here. However, we still need to check the
// end result value for anomalies, in this case NaN and infinite. If
// something is wrong, do not assign the value to the variable.
// Further error handling for the expression evaluator will take place in
// exprEvaluate() that will provide an error code to its caller.
//
double varValSet(int varId, double value)
{
  // Check end result of expression
  if (isnan(value) == 0 && isfinite(value) != 0)
  {
    int bucketId;
    int bucketListId;
    int i = 0;
    variable_t *myVar;

    // Get reference to variable
    bucketId = varId & 0xff;
    bucketListId = (varId >> 8);
    myVar = varBucket[bucketId].var;
    for (i = 0; i < bucketListId; i++)
      myVar = myVar->next;

    // Make variable active (if not already) and assign value
    myVar->active = GLCD_TRUE;
    myVar->varValue = value;
  }

  return value;
}

