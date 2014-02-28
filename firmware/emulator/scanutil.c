//*****************************************************************************
// Filename : 'scanutil.c'
// Title    : Various interpreter utility routines for mchron command line tool
//*****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "../ks0108.h"
#include "expr.h"
#include "scanutil.h"

// The cmdline input stream batch size for a single line
#define CMD_BUILD_LEN	128

// A structure to hold runtime information for a named numeric variable
typedef struct _variable_t
{
  int active;		// Whether character variable is in use
  int varValue;		// The current numeric value of the variable
} variable_t;

// Array holding numeric variable values identified by variable a..z, aa..zz
variable_t varVal[26][27];

// The variables to store the command line argument scan results
char argChar[ARG_TYPE_COUNT_MAX];
int argInt[ARG_TYPE_COUNT_MAX];
char *argWord[ARG_TYPE_COUNT_MAX];
char *argString = NULL;

// Index in the several scan result arrays
int argCharIdx = 0;
int argIntIdx = 0;
int argWordIdx = 0;

// Generic profile for command
argItem_t argRepeatCmd[] =
{ { ARG_WORD, "command" } };

// External data from expression evaluator
extern int exprValue;
extern int varError;

// Local function prototypes
int argValidateChar(argItem_t argItem, char argValue, int silent);
int argValidateNum(argItem_t argItem, int argValue, int silent);
int argValidateWord(argItem_t argItem, char *argValue, int silent);
cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot);
cmdRepeat_t *cmdRepeatCreate(cmdRepeat_t *cmdRepeatLast,
  cmdRepeat_t **cmdRepeatRoot, cmdLine_t *cmdLine);
int cmdRepeatLinkWhile(cmdRepeat_t *cmdRepeatLast, cmdLine_t *cmdLine);
int varNameIdxGet(char *var, int *varIdx1, int *varIdx2);

//
// Function: argInit
//
// Preprocess the input string by skipping to the first non-white
// character. And, clear previous scan results by initializing the
// indices in the scan result arrays and freeing malloc-ed string
// space.
//
void argInit(char **input)
{
  int i = 0;
  int argFound = GLCD_FALSE;
  char *argInput = *input;

  // Skip to first non-white character or stop at end of string
  while (*argInput != '\0' && argFound == GLCD_FALSE)
  {
    if (*argInput != ' ' && *argInput != '\t')
      argFound = GLCD_TRUE;
    else
      argInput++;
  }
  *input = argInput;

  // Reset the contents of the scan result arrays and scan string
  for (i = 0; i < 10; i++)
  {
    argChar[i] = '\0';
    argInt[i] = 0;
    if (argWord[i] != NULL)
    {
      free(argWord[i]);
      argWord[i] = NULL;
    }
  }
  if (argString != NULL)
  {
    free(argString);
    argString = NULL;
  }

  // Reset the indices in the scan result arrays
  argCharIdx = 0;
  argIntIdx = 0;
  argWordIdx = 0;
}

//
// Function: argScan
//
// Scan a (partial) argument profile.
// For a char, word or string profile just copy the char/string value
// into the designated array or string argument variable.
// For a numeric profile though copy the string expression and push
// it through the expression evaluator to get a resulting numeric
// value. When a valid numeric value is obtained copy it into the
// designated integer array variable.
// It returns parmater input that is modified to point to the remaining
// string to be scanned.
//
int argScan(argItem_t argItem[], int argCount, char **input, int silent)
{
  char *workPtr = *input;
  char *evalString;
  int i = 0;
  int j = 0;
  int retVal = CMD_RET_OK;

  while (i < argCount)
  {
    char c = *workPtr;
    
    if (argItem[i].argType == ARG_CHAR)
    {
      // Scan a single character argument profile
      char nextChar;

      // Verify argument count overflow
      if (argCharIdx == ARG_TYPE_COUNT_MAX)
      {
        printf("%s? internal: overflow char argument count\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }
 
      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }

      // The next character must be white space or end of string
      nextChar = *(workPtr + 1);
      if (nextChar != ' ' && nextChar != '\t' && nextChar != '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? invalid value: too long\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }

      // Validate the character (if a validation rule has been setup)
      retVal = argValidateChar(argItem[i], c, silent);
      if (retVal != CMD_RET_OK)
        return retVal;

      // Value approved so copy the char argument
      argChar[argCharIdx] = c;
      argCharIdx++;

      // Skip to next argument element in string
      workPtr++;
      while (*workPtr == ' ' || *workPtr == '\t')
      {
        workPtr++;
      }

      // Next argument profile
      i++;
    }
    else if (argItem[i].argType == ARG_UINT || argItem[i].argType == ARG_INT)
    {
      // Scan an (unsigned) integer argument profile.
      // An integer profile is an expression that can be a constant,
      // a variable or a combination of both in a mathematical
      // expression. We'll use flex/bison to evaluate the expression.

      // Verify argument count overflow
      if (argCharIdx == ARG_TYPE_COUNT_MAX)
      {
        printf("%s? internal: overflow int argument count\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }
 
      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }

      // Copy the integer expression upto next delimeter
      j = 0;
      while (workPtr[j] != ' ' && workPtr[j] != '\t' && workPtr[j] != '\0')
      {
        j++;
      }
      evalString = malloc(j + 2);

      j = 0;
      while (*workPtr != ' ' && *workPtr != '\t' && *workPtr != '\0')
      {
        evalString[j] = *workPtr;
        workPtr++;
        j++;
      }
      evalString[j] = '\n';
      evalString[j + 1] = '\0';

      // Evaluate the integer expression 
      retVal = exprEvaluate(evalString);
      free(evalString);
      if (varError == 1)
      {
        printf("%s? parse error\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }
      else if (retVal == 1)
      {
        printf("%s? syntax error\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }

      // Validate unsigned integer
      if (argItem[i].argType == ARG_UINT && exprValue < 0)
      {
        if (silent == GLCD_FALSE)
          printf("%s? invalid value: %d\n", argItem[i].argName, exprValue);
        return CMD_RET_ERROR;
      }

      // Validate the number (if a validation rule has been setup)
      retVal = argValidateNum(argItem[i], exprValue, silent);
      if (retVal != CMD_RET_OK)
        return retVal;

      // Value approved so copy the resulting value of the expression
      argInt[argIntIdx] = exprValue;
      argIntIdx++;

      // Skip to next argument element in string
      while (*workPtr == ' ' || *workPtr == '\t')
      {
        workPtr++;
      }

      // Next argument profile
      i++;
    }
    else if (argItem[i].argType == ARG_WORD)
    {
      // Scan a character word argument profile
      
      // Verify argument count overflow
      if (argCharIdx == ARG_TYPE_COUNT_MAX)
      {
        printf("%s? internal: overflow word argument count\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }
 
      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }

      // Value exists so copy the word upto next delimeter
      j = 0;
      while (workPtr[j] != ' ' && workPtr[j] != '\t' && workPtr[j] != '\0')
      {
        j++;
      }
      argWord[argWordIdx] = malloc(j + 1);

      j = 0;
      while (*workPtr != ' ' && *workPtr != '\t' && *workPtr != '\0')
      {
        argWord[argWordIdx][j] = *workPtr;
        workPtr++;
        j++;
      }
      argWord[argWordIdx][j] = '\0';
      argWordIdx++;

      // Validate the word (if a validation rule has been setup)
      retVal = argValidateWord(argItem[i], argWord[argWordIdx - 1], silent);
      if (retVal != CMD_RET_OK)
        return retVal;

      // Skip to next argument element in string
      while (*workPtr == ' ' || *workPtr == '\t')
      {
        workPtr++;
      }

      // Next argument profile
      i++;
    }
    else if (argItem[i].argType == ARG_STRING)
    {
      // Scan a character string argument profile

      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", argItem[i].argName);
        return CMD_RET_ERROR;
      }

      // As the remainder of the input string is to be considered
      // the string argument we skip to the end of the input string
      j = 0;
      while (workPtr[j] != '\0')
      {
        j++;
      }
      argString = malloc(j + 1);

      j = 0;
      while (*workPtr != '\0')
      {
        argString[j] = *workPtr;
        workPtr++;
        j++;
      }

      // Set end of string
      argString[j] = '\0';

      // Next argument profile
      i++;
    }
    else if (argItem[i].argType == ARG_END)
    {
      // Scan an end-of-line argument profile

      // Verify end-of-line
      if (c != '\0')
      {
        if (silent == GLCD_FALSE)
          printf("command %s? too many arguments\n", argWord[0]);
        return CMD_RET_ERROR;
      }

      // Next argument profile
      i++;
    }
    else
    {
      printf("internal: invalid element: %d %d\n", i, argItem[i].argType);
      return CMD_RET_ERROR;
    }
  }
  
  // Successful scan
  *input = workPtr;
  return CMD_RET_OK;
}

//
// Function: argValidateChar
//
// Validate a character argument with a validation profile
//
int argValidateChar(argItem_t argItem, char argValue, int silent)
{
  // If there is no validation rule we're done
  if (argItem.argItemDomain == NULL)
    return CMD_RET_OK;

  if ((argItem.argItemDomain)->argDomainType == DOM_CHAR_LIST)
  {
    int i = 0;
    int charFound = GLCD_FALSE;
    char *argCharList = (argItem.argItemDomain)->argTextList;

    // Try to find the character in the character validation list
    while (charFound == GLCD_FALSE && argCharList[i] != '\0')
    {
      if (argCharList[i] == argValue)
        charFound = GLCD_TRUE;
      i++;
    }

    // Return error if not found
    if (charFound == GLCD_FALSE)
    {
      if (silent == GLCD_FALSE)
        printf("%s? invalid value: %c\n", argItem.argName, argValue);
      return CMD_RET_ERROR;
    }
  }
  else
  {
    printf("%s? internal: invalid domain validation type\n", argItem.argName);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: argValidateNum
//
// Validate a numeric argument with a validation profile
//
int argValidateNum(argItem_t argItem, int argValue, int silent)
{
  int argDomainType = 0;

  // If there is no validation rule we're done
  if (argItem.argItemDomain == NULL)
    return CMD_RET_OK;

  // Validate internal integrity of validation structure
  argDomainType = (argItem.argItemDomain)->argDomainType;
  if (argDomainType != DOM_NUM_MIN && argDomainType != DOM_NUM_MAX &&
      argDomainType != DOM_NUM_RANGE)
  {
    printf("%s? internal: invalid domain validation type\n", argItem.argName);
    return CMD_RET_ERROR;
  }

  // Validate minimum value
  if (argDomainType == DOM_NUM_MIN || argDomainType == DOM_NUM_RANGE)
  {
    if (argValue < (argItem.argItemDomain)->argNumMin)
    {
      if (silent == GLCD_FALSE)
        printf("%s? invalid value: %d\n", argItem.argName, argValue);
      return CMD_RET_ERROR;
    }
  }

  // Validate maximum value
  if (argDomainType == DOM_NUM_MAX || argDomainType == DOM_NUM_RANGE)
  {
    if (argValue > (argItem.argItemDomain)->argNumMax)
    {
      if (silent == GLCD_FALSE)
        printf("%s? invalid value: %d\n", argItem.argName, argValue);
      return CMD_RET_ERROR;
    }
  }

  return CMD_RET_OK;
}

//
// Function: argValidateWord
//
// Validate a word argument with a validation profile
//
int argValidateWord(argItem_t argItem, char *argValue, int silent)
{
  // If there is no validation rule we're done
  if (argItem.argItemDomain == NULL)
    return CMD_RET_OK;

  if ((argItem.argItemDomain)->argDomainType == DOM_WORD_LIST)
  {
    int i = 0;
    int j = 0;
    int wordFound = GLCD_FALSE;
    char *argWordList = (argItem.argItemDomain)->argTextList;

    // Try to find the word in the word validation list
    while (wordFound == GLCD_FALSE && argWordList[i] != '\0')
    {
      if (argWordList[i] == '\n')
      {
        // End of a validation word in validation list
        if (argValue[j] == '\0')
        {
          // Input value and domain word are identical 
          wordFound = GLCD_TRUE;
        }
        else
        {
          // Input has additional characters. Reset validation.
          j = 0;
          i++;
        }
      }
      else if (argWordList[i] != argValue[j])
      {
        // Character mismatch. Reset position in input value.
        j = 0;
        // Skip to next word in word validation list
        while (argWordList[i] != '\0' && argWordList[i] != '\n')
          i++;
        if (argWordList[i] != '\n')
          i++;
      }
      else
      {
        // Current character matches so continue with next
        i++;
        j++;
      }
    }
    // In case we reached the end of the validation string we have a match
    // when the input string also reached the end
    if (wordFound == GLCD_FALSE && argValue[j] == '\0')
      wordFound = GLCD_TRUE;

    // Return error if not found
    if (wordFound == GLCD_FALSE)
    {
      if (silent == GLCD_FALSE)
        printf("%s? invalid value: %s\n", argItem.argName, argValue);
      return CMD_RET_ERROR;
    }
  }
  else
  {
    printf("%s? internal: invalid domain validation type\n", argItem.argName);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: cmdFileLoad
//
// Load command file contents in a linked list structure
//
int cmdFileLoad(cmdLine_t **cmdLineRoot, cmdRepeat_t **cmdRepeatRoot,
  char *fileName)
{
  FILE *fp;				// Input file pointer
  cmdLine_t *cmdLineLast = NULL;	// The last cmdline in linked list
  cmdRepeat_t *cmdRepeatLast = NULL;	// The last cmdrepeat in linked list
  cmdRepeat_t *searchRepeat = NULL;	// Active cmdrepeat in search efforts
  int unlinkedFound = GLCD_FALSE;	// 'rw' command not linked to 'rn'
  int lineNum = 1;
  char *input = NULL;
  cmdInput_t cmdInput;
  int retVal = CMD_RET_OK;

  // Init the pointers to the command line and the repeat lists
  *cmdLineRoot = NULL;
  *cmdRepeatRoot = NULL;

  // Open command file
  fp = fopen(fileName, "r");
  if (fp == NULL)
  {
    printf("cannot open command file \"%s\"\n", fileName);
    return CMD_RET_ERROR;
  }

  // Initialize our file readline interface and do the first read
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

    // See if this is a repeat while or repeat next command 
    argInit(&input);
    retVal = argScan(argRepeatCmd, sizeof(argRepeatCmd) / sizeof(argItem_t),
      &input, GLCD_TRUE);

    if (retVal != CMD_RET_OK)
    {
      // Most likely an empty line
      cmdLineLast->cmdRepeatType = CMD_REPEAT_NONE;
    }
    else if (strcmp(argWord[0], "rn") == 0)
    {
      // This is a repeat next command
      cmdLineLast->cmdRepeatType = CMD_REPEAT_NEXT;

      // We must find a repeat while command to associate with
      retVal = cmdRepeatLinkWhile(cmdRepeatLast, cmdLineLast);
      if (retVal != CMD_RET_OK)
      {
        printf("parse:%s:%d:no associated rw for this rn\n", fileName, lineNum);
        cmdInputCleanup(&cmdInput);
        fclose(fp);
        return CMD_RET_ERROR;
      }
    }
    else if (strcmp(argWord[0], "rw") == 0)
    {
      // This is a repeat while command
      cmdLineLast->cmdRepeatType = CMD_REPEAT_WHILE;

      // Create new repeat structure, add it to the linked list and link
      // it to the repeat while command
      cmdRepeatLast = cmdRepeatCreate(cmdRepeatLast, cmdRepeatRoot, cmdLineLast);
    }
    else
    {
      // Anything else
      cmdLineLast->cmdRepeatType = CMD_REPEAT_NONE;
    }

    // Next file line number
    lineNum++;

    // And get next input line
    cmdInputRead("", &cmdInput);
  }

  // File content is loaded in a linked list
  cmdInputCleanup(&cmdInput);
  fclose(fp);

  // Postprocessing the linked lists.
  // We may not find a repeat while command without an associated
  // repeat next command.
  unlinkedFound = GLCD_FALSE;
  searchRepeat = cmdRepeatLast;
  while (searchRepeat != NULL && unlinkedFound == GLCD_FALSE)
  {
    if (searchRepeat->cmdLineRn == NULL)
    {
      // Found a repeat while not associated with a repeat next
      unlinkedFound = GLCD_TRUE;
    }
    else
    {
      // Continue search backwards
      searchRepeat = searchRepeat->prev;
    }
  }
  if (unlinkedFound == GLCD_TRUE)
  {
    printf("parse:%s:%d:no associated rn for this rw\n", fileName,
      searchRepeat->cmdLineRw->lineNum);
    return CMD_RET_ERROR;
  }

  // The file contents have been read into linked lists and is verified
  // for its integrity on matching 'rw'/'rn' repeat constructs
  return CMD_RET_OK;
}

//
// Function: cmdInputCleanup
//
// Cleanup the input stream by free-ing the last malloc-ed data
// and cleaning up the readline library interface
//
void cmdInputCleanup(cmdInput_t *cmdInput)
{
  // Add previous read to history when applicable
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    if (cmdInput->input != NULL && (cmdInput->input)[0] != '\0' && 
        (cmdInput->input)[0] != '\n')
      add_history(cmdInput->input);
  }

  // Cleanup previous read
  if (cmdInput->input != NULL)
  {
    free(cmdInput->input);
    cmdInput->input = NULL;
  }

  // Cleanup the readline interface
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    rl_deprep_terminal();
    rl_reset_terminal(NULL);
  }
}

//
// Function: cmdInputInit
//
// Open an input stream in preparation to read its input line by line
// regardless the line size
//
void cmdInputInit(cmdInput_t *cmdInput)
{
  cmdInput->input = NULL;
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    // Disable auto-complete
    rl_bind_key('\t',rl_insert);
  }
}

//
// Function: cmdInputRead
//
// Acquire a single command line by reading the input stream part by
// part until a newline character is encountered indicating the
// command end-of-line.
// Note: The trailing newline character will NOT be copied to the 
// resulting input buffer.
//
void cmdInputRead(char *prompt, cmdInput_t *cmdInput)
{
  // Add previous read to history when applicable
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    if (cmdInput->input != NULL && (cmdInput->input)[0] != '\0' && 
        (cmdInput->input)[0] != '\n')
      add_history(cmdInput->input);
  }

  // Cleanup previous read
  if (cmdInput->input != NULL)
  {
    free(cmdInput->input);
    cmdInput->input = NULL;
  }

  // Execute the requested input method
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    // In case we use the readline library input method there's
    // not much to do
    cmdInput->input = readline(prompt);
  }
  else
  {
    // Use our own input mechanism to read an input line. It is
    // mainly used for file input and commandline input with an
    // ncurses device.
    char inputCli[CMD_BUILD_LEN];
    int done = GLCD_FALSE;
    char *inputPtr = NULL;
    char *mallocPtr = NULL;
    char *buildPtr = NULL;
    int inputLen = 0;
    int inputLenTotal = 0;

    // First start with providing a prompt, when specified.
    if (prompt[0] != '\0')
      printf("%s", prompt);

    // Get first character batch
    inputPtr = fgets(inputCli, CMD_BUILD_LEN, cmdInput->file);
    while (done == GLCD_FALSE)
    {
      if (inputPtr == NULL)
      {
        // EOF: we're done
        cmdInput->input = buildPtr;
        done = GLCD_TRUE;
      }
      else
      {
        // Prepare new combined input string
        inputLen = strlen(inputCli);
        mallocPtr = malloc(inputLenTotal + inputLen + 1);

        if (inputLenTotal > 0)
        {
          // Copy string built up so far into new buffer and add
          // the string that was read just now
          strcpy(mallocPtr, (const char *)buildPtr);
          strcpy(&(mallocPtr[inputLenTotal]), (const char *)inputCli);
          inputLenTotal = inputLenTotal + inputLen;
          free(buildPtr);
          buildPtr = mallocPtr;
        }
        else
        {
          // First batch of characters
          strcpy(mallocPtr, (const char *)inputCli);
          inputLenTotal = inputLen;
          buildPtr = mallocPtr;
        }

        // See if we encountered the end of a line
        if (buildPtr[inputLenTotal - 1] == '\n')
        {
          // Terminating newline: EOL
          // Remove the newline from input buffer for compatibility
          // reasons with readline library functionality.
          buildPtr[inputLenTotal - 1] = '\0';
          cmdInput->input = buildPtr;
          done = GLCD_TRUE;
        }
        else
        {
          // No EOL encountered so get next batch of characters
          inputPtr = fgets(inputCli, CMD_BUILD_LEN, cmdInput->file);
        }
      }
    }
  }
}

//
// Function: cmdKeyboardLoad
//
// Load keyboard commands interactively in a linked list structure
//
int cmdKeyboardLoad(cmdLine_t **cmdLineRoot, cmdRepeat_t **cmdRepeatRoot,
  cmdInput_t *cmdInput)
{
  cmdLine_t *cmdLineLast = NULL;	// The last cmdline in linked list
  cmdRepeat_t *cmdRepeatLast = NULL;	// The last cmdrepeat in linked list
  int repeatCount = 0;			// rw vs rn command count
  char prompt[] = ">> ";
  int init = GLCD_TRUE;
  int lineNum = 1;
  char *input = NULL;
  int retVal = CMD_RET_OK;

  // Init return pointers
  *cmdLineRoot = NULL;
  *cmdRepeatRoot = NULL;

  // Do not read from the keyboard yet as we already have the first
  // command in the input buffer. Once we've processed that one we'll
  // continue reading the input stream using the control structure we've
  // been handed over.

  // Add each line entered via keyboard in a command linked list.
  // The list is complete when all 'rw' commands are matched with a 'rn'
  // command, or when the user entered ^D.
  while ((repeatCount != 0 && retVal == CMD_RET_OK) || init == GLCD_TRUE)
  {
    // Create new command line and add it to the linked list
    // by administering lots of pointers
    cmdLineLast = cmdLineCreate(cmdLineLast, cmdLineRoot);

    // Fill in functional payload of the list element
    cmdLineLast->lineNum = lineNum;
    cmdLineLast->input = malloc(strlen(cmdInput->input) + 1);
    strcpy(cmdLineLast->input, (const char *)cmdInput->input);

    // We need to know if this is a repeat while or repeat next command
    input = cmdLineLast->input;
    argInit(&input);
    retVal = argScan(argRepeatCmd, sizeof(argRepeatCmd) / sizeof(argItem_t),
      &input, GLCD_TRUE);

    // Process the scan result
    if (retVal != CMD_RET_OK)
    {
      // Most likely an empty line
      cmdLineLast->cmdRepeatType = CMD_REPEAT_NONE;
      retVal = CMD_RET_OK;
    }
    else if (strcmp(argWord[0], "rn") == 0)
    {
      // This is a repeat next command
      repeatCount--;
      cmdLineLast->cmdRepeatType = CMD_REPEAT_NEXT;

      // We must find a repeat while command to associate with
      retVal = cmdRepeatLinkWhile(cmdRepeatLast, cmdLineLast);
      if (retVal != CMD_RET_OK)
      {
        printf("parse:%s:%d:internal: no associated rw for this rn\n", "-",
          lineNum);
        return CMD_RET_ERROR;
      }

      // If this 'rn' associates with the most outer 'rw' we're done
      if (repeatCount == 0)
      {
        break;
      }
    }
    else if (strcmp(argWord[0], "rw") == 0)
    {
      // This is a repeat while command
      repeatCount++;
      cmdLineLast->cmdRepeatType = CMD_REPEAT_WHILE;

      // Create new repeat structure, add it to the linked list and
      // link it to the repeat while command
      cmdRepeatLast = cmdRepeatCreate(cmdRepeatLast, cmdRepeatRoot,
        cmdLineLast);
    }
    else
    {
      // Anything else
      cmdLineLast->cmdRepeatType = CMD_REPEAT_NONE;
    }

    // Next command line linenumber
    lineNum++;

    // If this is the first loop entry we've just processed the
    // initial repeat while command. We must now switch to our own
    // command input scanner.
    init = GLCD_FALSE;

    // Get the next keyboard input line
    cmdInputRead(prompt, cmdInput);
    if (cmdInput->input == NULL)
    {
      printf("\n<ctrl>d - quit\n");
      return CMD_RET_ERROR;
    }
  }

  // We do not need to postprocess the repeat linked list as we keep
  // track of the number of 'rw' vs 'rn' commands.
  // When we get here all 'rw' commands are matched with a 'rn' command.
  return CMD_RET_OK;
}

//
// Function: cmdLineCreate
//
// Create a new cmdLine structure and add it in the command line list
//
cmdLine_t *cmdLineCreate(cmdLine_t *cmdLineLast, cmdLine_t **cmdLineRoot)
{
  cmdLine_t *cmdLineMalloc = NULL;

  // Allocate the new command line
  cmdLineMalloc = (cmdLine_t *)malloc(sizeof(cmdLine_t));

  // Take care of some administrative pointers
  if (*cmdLineRoot == NULL)
  {
    // This is the first list element
    *cmdLineRoot = cmdLineMalloc;
  }
  if (cmdLineLast != NULL)
  {
    // Not the first list elemant
    cmdLineLast->next = cmdLineMalloc;
  }

  // Init the new structure
  cmdLineMalloc->lineNum = 0;
  cmdLineMalloc->input = NULL;
  cmdLineMalloc->cmdRepeatType = CMD_REPEAT_NONE;
  cmdLineMalloc->cmdRepeat = NULL;
  cmdLineMalloc->next = NULL;

  return cmdLineMalloc;
}

//
// Function: cmdListCleanup
//
// Cleanup a command linked list structure
//
void cmdListCleanup(cmdLine_t *cmdLineRoot, cmdRepeat_t *cmdRepeatRoot)
{
  cmdLine_t *nextLine;
  cmdRepeat_t *nextRepeat;

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

  // Free the linked list of repeat loop variables
  nextRepeat = cmdRepeatRoot;
  while (nextRepeat != NULL)
  {
    // Return the malloc-ed end and step expressions
    if (nextRepeat->end != NULL)
      free(nextRepeat->end);
    if (nextRepeat->step != NULL)
      free(nextRepeat->step);
    nextRepeat = cmdRepeatRoot->next;

    // Return the malloc-ed list element itself
    free(cmdRepeatRoot);
    cmdRepeatRoot = nextRepeat;
  }
}

//
// Function: cmdRepeatCreate
//
// Create a new cmdRepeat structure, add it in the repeat list and link it
// to the repeat while command line
//
cmdRepeat_t *cmdRepeatCreate(cmdRepeat_t *cmdRepeatLast,
  cmdRepeat_t **cmdRepeatRoot, cmdLine_t *cmdLine)
{
  cmdRepeat_t *cmdRepeatMalloc = NULL;

  // Allocate the new repeat runtime
  cmdRepeatMalloc = (cmdRepeat_t *)malloc(sizeof(cmdRepeat_t));

  // Take care of lots of administrative pointers
  if (*cmdRepeatRoot == NULL)
  {
    // First structure in the list
    *cmdRepeatRoot = cmdRepeatMalloc;
  }
  if (cmdRepeatLast != NULL)
  {
    // Not the first in the list
    cmdRepeatLast->next = cmdRepeatMalloc;
  }
  cmdLine->cmdRepeat = cmdRepeatMalloc;

  // Init the new structure
  cmdRepeatMalloc->active = GLCD_FALSE;
  cmdRepeatMalloc->initialized = GLCD_FALSE;
  cmdRepeatMalloc->var[0] = '\0';
  cmdRepeatMalloc->condition = RU_NONE;
  cmdRepeatMalloc->start = 0;
  cmdRepeatMalloc->end = 0;
  cmdRepeatMalloc->step = 0;
  cmdRepeatMalloc->cmdLineRw = cmdLine;
  cmdRepeatMalloc->cmdLineRn = NULL;
  cmdRepeatMalloc->prev = cmdRepeatLast;
  cmdRepeatMalloc->next = NULL;

  return cmdRepeatMalloc;
}

//
// Function: cmdRepeatItemInit
//
// Allocate memory of a repeat item expression and copy its value
// into it.
// Note that a '\n' is added at the end of the expression as per
// expression scanner requirement.
//
void argRepeatItemInit(char **repeatItem, char *argExpr)
{
  int argExprLen = 0;
  char *mallocPtr = NULL;

  argExprLen = strlen(argExpr) + 2;
  mallocPtr = malloc(argExprLen);
  strcpy(mallocPtr, argExpr);
  mallocPtr[argExprLen - 2] = '\n';
  mallocPtr[argExprLen - 1] = '\0';
  *repeatItem = mallocPtr;
}

//
// Function: cmdRepeatLinkWhile
//
// Find and link an unlinked 'rw' repeat line to the 'rn' command line
//
int cmdRepeatLinkWhile(cmdRepeat_t *cmdRepeatLast, cmdLine_t *cmdLine)
{
  int foundRw = GLCD_FALSE;
  cmdRepeat_t *searchRepeat = cmdRepeatLast;

  while (searchRepeat != NULL && foundRw == GLCD_FALSE)
  {
    if (searchRepeat->cmdLineRn == NULL)
    {
      // Found a repeat while to be associated with the repeat next
      foundRw = GLCD_TRUE;
      cmdLine->cmdRepeat = searchRepeat;
      searchRepeat->cmdLineRn = cmdLine;
      return CMD_RET_OK;
    }
    else
    {
      searchRepeat = searchRepeat->prev;
    }
  }

  // Could not find an unlinked 'rw'
  return CMD_RET_ERROR;
}

//
// Function: varClear
//
// Clear a named variable
//
int varClear(char *var)
{
  int varIdx1;
  int varIdx2;

  // Validate variable name
  if (varNameIdxGet(var, &varIdx1, &varIdx2) != CMD_RET_OK)
    return CMD_RET_ERROR;

  // Reset variable by inactivating it.
  // Also be so kind to reset its value to 0.
  varVal[varIdx1][varIdx2].active = GLCD_FALSE;
  varVal[varIdx1][varIdx2].varValue = 0;

  return CMD_RET_OK;
}

//
// Function: varNameIdxGet
//
// Validate the name of a named variable. When correct return the
// indices in the storage array for the variable name.
//
int varNameIdxGet(char *var, int *varIdx1, int *varIdx2)
{
  // Validate variable name char 1
  if (var[0] < 'a' || var[0] > 'z')
  {
    printf("invalid variable: %s\n", var);
    return CMD_RET_ERROR;
  }
  else
  {
    *varIdx1 = (int)(var[0] - 'a');
  }

  // Validate variable name char 2
  if (var[1] == '\0')
  {
    *varIdx2 = 0;
  }
  else if (var[1] < 'a' || var[1] > 'z' || var[2] != '\0')
  {
    printf("invalid variable: %s\n", var);
    return CMD_RET_ERROR;
  }
  else
  {
    *varIdx2 = (int)(var[1] - 'a' + 1);
  }

  // Var name is validated and its indices in var array are returned
  return CMD_RET_OK;
}

//
// Function: varReset
//
// Reset all named variable data
//
void varReset(void)
{
  int i = 0;
  int j = 0;

  for (i = 0; i < 26; i++)
  {
    for (j = 0; j < 27; j++)
    {
      varVal[i][j].active = GLCD_FALSE;
      varVal[i][j].varValue = 0;
    }
  }
}

//
// Function: varStateGet
//
// Get the active state of a named variable
//
int varStateGet(char *var, int *active)
{
  int varIdx1;
  int varIdx2;

  // Validate variable name
  if (varNameIdxGet(var, &varIdx1, &varIdx2) != CMD_RET_OK)
    return CMD_RET_ERROR;

  // Get the active state of the variable
  *active = varVal[varIdx1][varIdx2].active;

  return CMD_RET_OK;
}

//
// Function: varValGet
//
// Get the value of a named variable
//
int varValGet(char *var, int *value)
{
  int varIdx1;
  int varIdx2;

  // Validate variable name
  if (varNameIdxGet(var, &varIdx1, &varIdx2) != CMD_RET_OK)
    return CMD_RET_ERROR;

  if (varVal[varIdx1][varIdx2].active == GLCD_FALSE)
  {
    // When inactive make it active and init to 0
    varVal[varIdx1][varIdx2].active = GLCD_TRUE;
    varVal[varIdx1][varIdx2].varValue = 0;
    *value = 0;
  }
  else
  {
    // Get the value of the variable
    *value = varVal[varIdx1][varIdx2].varValue;
  }

  return CMD_RET_OK;
}

//
// Function: varValSet
//
// Set the value of a named variable
//
int varValSet(char *var, int value)
{
  int varIdx1;
  int varIdx2;

  // Validate variable name
  if (varNameIdxGet(var, &varIdx1, &varIdx2) != CMD_RET_OK)
    return CMD_RET_ERROR;

  // Make variable active when not in use
  varVal[varIdx1][varIdx2].active = GLCD_TRUE;

  // Set new value
  varVal[varIdx1][varIdx2].varValue = value;

  return CMD_RET_OK;
}

