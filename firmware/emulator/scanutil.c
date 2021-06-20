//*****************************************************************************
// Filename : 'scanutil.c'
// Title    : Utility routines for mchron command line scanning, input stream
//            handling and command history caching
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <regex.h>
#include <readline/readline.h>
#include <readline/history.h>

// Monochron and emuchron defines
#include "../ks0108.h"
#include "expr.h"
#include "dictutil.h"
#include "scanutil.h"

// The command line input stream batch size for a single line
#define CMD_BUILD_LEN		128

// The readline unsaved cache and history file with size parameters
#define READLINE_CACHE_LEN	15
#define READLINE_HISFILE	"/history"
#define READLINE_MAXHISTORY	250

// External data from expression evaluator
extern double exprValue;
extern u08 exprAssign;

// Functional name of mchron command
extern char *mchronCmdName;

// This is me
extern const char *__progname;

// The variables to store the published command line argument scan results
char argChar[ARG_TYPE_COUNT_MAX];
double argDouble[ARG_TYPE_COUNT_MAX];
char *argString[ARG_TYPE_COUNT_MAX];

// Index in the several scan result arrays
static u08 argCharIdx = 0;
static u08 argDoubleIdx = 0;
static u08 argStringIdx = 0;

// The current readline history in-memory cache length and history file
static int rlCacheLen = 0;
static char *rlHistoryFile = NULL;

// Local function prototypes
static char *cmdArgCreate(char *arg, int len, int isExpr);
static u08 cmdArgValidateChar(cmdArg_t *cmdArg, char *argValue);
static u08 cmdArgValidateNum(cmdArg_t *cmdArg, double argValue);
static u08 cmdArgValidateRegex(cmdArg_t *cmdArg, char *argValue);
static u08 cmdArgValidateWord(cmdArg_t *cmdArg, char *argValue);

//
// Function: cmdArgCleanup
//
// Cleanup the split-up command arguments in a command line
//
void cmdArgCleanup(cmdLine_t *cmdLine)
{
  int i;

  // Anything to clean?
  cmdLine->initialized = GLCD_FALSE;
  if (cmdLine->args == NULL)
    return;

  // Clean each of the command argument values
  for (i = 0; i < cmdLine->cmdCommand->argCount; i++)
  {
    if (cmdLine->args[i] != NULL)
      free(cmdLine->args[i]);
  }

  // Clean the argument array
  free(cmdLine->args);
  cmdLine->args = NULL;
}

//
// Function: cmdArgCreate
//
// Allocate memory for a command argument and copy its text into it.
// The argument text to copy will not include a trailing '\0' so we'll have to
// add it ourselves.
// For an argument that is to result in a numeric value add a '\n' to its
// expression as per expression evaluator requirement.
//
static char *cmdArgCreate(char *arg, int len, int isExpr)
{
  int closeLen = 1;
  char *cmdArg;

  // Do we need to reserve space for adding a '\n' to the argument
  if (isExpr == GLCD_TRUE)
    closeLen = 2;

  // Allocate memory and copy the argument into it
  cmdArg = malloc(len + closeLen);
  memcpy(cmdArg, arg, (size_t)len);

  // Do we need to add a '\n' or just stick with adding a trailing '\0'
  if (isExpr == GLCD_TRUE)
  {
    cmdArg[len] = '\n';
    cmdArg[len + 1] = '\0';
  }
  else
  {
    cmdArg[len] = '\0';
  }

  return cmdArg;
}

//
// Function: cmdArgInit
//
// Preprocess the input string by skipping to the first non-white character,
// scan the mchron command and get its associated command dictionary.
// When done the input pointer points to the start of the first command
// argument or '\0'.
//
u08 cmdArgInit(char **input, cmdLine_t *cmdLine)
{
  char *workPtr;
  char *name;
  char nameEnd;
  int i = 0;

  // Verify empty command
  *input = cmdLine->input + strspn(cmdLine->input, " \t");
  if (**input == '\0')
  {
    cmdLine->initialized = GLCD_TRUE;
    return CMD_RET_OK;
  }

  // Get the command
  workPtr = *input;
  name = *input;

  // First find whitespace char that marks the end of the mchron command and
  // skip to next argument (if any)
  i = strcspn(workPtr, " \t");
  workPtr = workPtr + i;
  *input = workPtr + strspn(workPtr, " \t");

  // Find associated command dictionary for the command (if still unknown)
  if (cmdLine->cmdCommand == NULL)
  {
    // Temporarily mark end-of-string to obtain dictionary info
    nameEnd = name[i];
    name[i] = '\0';
    cmdLine->cmdCommand = dictCmdGet(name);
    name[i] = nameEnd;
    if (cmdLine->cmdCommand == NULL)
      return CMD_RET_ERROR;
  }
  return CMD_RET_OK;
}

//
// Function: cmdArgPublish
//
// Publish the command line arguments of a command line to the defined command
// argument variables: argChar[], argDouble[] and argString[].
// In case of a non-numeric argument type its domain profile has already been
// checked. In case of a numeric argument we need to run it through the
// expression evaluator and then check its domain profile.
//
u08 cmdArgPublish(cmdLine_t *cmdLine)
{
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;
  int argCount = cmdLine->cmdCommand->argCount;
  int i = 0;
  u08 argType;

  // Reset the argument array pointers and set number of args to publish
  argCharIdx = 0;
  argDoubleIdx = 0;
  argStringIdx = 0;

  // First publish the command name
  argString[argStringIdx] = cmdLine->cmdCommand->cmdName;
  argStringIdx++;

  // Publish all other arguments (if any remain)
  for (i = 0; i < argCount; i++)
  {
    argType = cmdArg[i].argType;
    if (argType == ARG_CHAR)
    {
      argChar[argCharIdx] = cmdLine->args[i][0];
      argCharIdx++;
    }
    else if (argType == ARG_NUM)
    {
      // Evaluate expression and validate numeric type and expression value
      if (exprEvaluate(cmdArg[i].argName, cmdLine->args[i]) != CMD_RET_OK)
        return CMD_RET_ERROR;
      if (cmdArgValidateNum(&cmdArg[i], exprValue) != CMD_RET_OK)
        return CMD_RET_ERROR;

      // Publish the resulting value of the expression
      argDouble[argDoubleIdx] = exprValue;
      argDoubleIdx++;
    }
    else if (argType == ARG_STRING)
    {
      argString[argStringIdx] = cmdLine->args[i];
      argStringIdx++;
    }
    else
    {
      printf("internal: invalid element (%d,%d)\n", i, argType);
      return CMD_RET_ERROR;
    }
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgRead
//
// Scan the argument profile for a command. Copy each argument into a malloc'ed
// command argument list pointer array in the command line.
// Note: We assume that *input points to the first non-white character after
//       the command name or to '\0'.
//
u08 cmdArgRead(char *input, cmdLine_t *cmdLine)
{
  char *workPtr = input;
  char c;
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;
  u08 argType;
  u08 domType;
  int argCount = cmdLine->cmdCommand->argCount;
  int i = 0;
  int j = 0;

  // Allocate and init pointer array for split-up command line arguments
  if (argCount > 0)
  {
    cmdLine->args = malloc(sizeof(cmdLine->args) * argCount);
    for (j = 0; j < argCount; j++)
      cmdLine->args[j] = NULL;
  }

  // Scan each command argument
  for (i = 0; i < argCount; i++)
  {
    c = *workPtr;
    argType = cmdArg[i].argType;
    domType = cmdArg[i].cmdDomain->domType;

    // Verify unexpected end-of-string
    if (domType != DOM_STRING_OPT && c == '\0')
    {
      printf("%s? missing value\n", cmdArg[i].argName);
      return CMD_RET_ERROR;
    }

    // Scan argument based on argument type
    if (argType == ARG_CHAR)
    {
      // Scan and validate a single char argument
      j = strcspn(workPtr, " \t");
      cmdLine->args[i] = cmdArgCreate(workPtr, j, GLCD_FALSE);
      if (cmdArgValidateChar(&cmdArg[i], cmdLine->args[i]) != CMD_RET_OK)
        return CMD_RET_ERROR;
    }
    else if (argType == ARG_NUM)
    {
      // Copy the flex/bison expression argument up to next delimeter.
      // Validation is done at runtime when the expression is evaluated.
      j = strcspn(workPtr, " \t");
      cmdLine->args[i] = cmdArgCreate(workPtr, j, GLCD_TRUE);
    }
    else if (argType == ARG_STRING)
    {
      // Validate the string (if a validation rule has been setup)
      if (domType == DOM_WORD_VAL)
      {
        // Copy the word argument up to next delimeter and validate value
        j = strcspn(workPtr, " \t");
        cmdLine->args[i] = cmdArgCreate(workPtr, j, GLCD_FALSE);
        if (cmdArgValidateWord(&cmdArg[i], cmdLine->args[i]) != CMD_RET_OK)
          return CMD_RET_ERROR;
      }
      else if (domType == DOM_WORD_REGEX)
      {
        // Copy the word argument up to next delimeter and validate value
        j = strcspn(workPtr, " \t");
        cmdLine->args[i] = cmdArgCreate(workPtr, j, GLCD_FALSE);
        if (cmdArgValidateRegex(&cmdArg[i], cmdLine->args[i]) != CMD_RET_OK)
          return CMD_RET_ERROR;
      }
      else if (domType == DOM_STRING || domType == DOM_STRING_OPT)
      {
        // Copy the remainder of the input string (that may be empty)
        j = strlen(workPtr);
        cmdLine->args[i] = cmdArgCreate(workPtr, j, GLCD_FALSE);
      }
      else
      {
        printf("internal: invalid element domain (%d,%d)\n", i, domType);
        return CMD_RET_ERROR;
      }
    }
    else
    {
      printf("internal: invalid element (%d,%d)\n", i, argType);
      return CMD_RET_ERROR;
    }

    // Skip to next argument in input string
    workPtr = workPtr + j;
    workPtr = workPtr + strspn(workPtr, " \t");
  }

  // Verify end-of-line
  if (*workPtr != '\0')
  {
    printf("%s? too many arguments\n", mchronCmdName);
    return CMD_RET_ERROR;
  }

  // Successful scan
  cmdLine->initialized = GLCD_TRUE;
  return CMD_RET_OK;
}

//
// Function: cmdArgValidateChar
//
// Validate a character argument with a validation profile
//
static u08 cmdArgValidateChar(cmdArg_t *cmdArg, char *argValue)
{
  int i = 0;
  u08 charFound = GLCD_FALSE;
  char *argCharList = cmdArg->cmdDomain->domTextList;

  if (cmdArg->cmdDomain->domType != DOM_CHAR_VAL)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

  // We must find a single char only
  if (argValue[1] != '\0')
  {
    printf("%s? invalid: %s\n", cmdArg->argName, argValue);
    return CMD_RET_ERROR;
  }

  // Try to find the character in the character validation list
  while (charFound == GLCD_FALSE && argCharList[i] != '\0')
  {
    if (argCharList[i] == argValue[0])
      charFound = GLCD_TRUE;
    i++;
  }

  // Return error if not found
  if (charFound == GLCD_FALSE)
  {
    printf("%s? invalid: %s\n", cmdArg->argName, argValue);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValidateNum
//
// Validate a numeric argument with a validation profile
//
static u08 cmdArgValidateNum(cmdArg_t *cmdArg, double argValue)
{
  u08 domType = cmdArg->cmdDomain->domType;

  // Validate internal integrity of validation structure
  if (domType != DOM_NUM && domType != DOM_NUM_RANGE &&
      domType != DOM_NUM_ASSIGN)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

  // Validate min/max value while allowing some math rounding errors
  if (domType == DOM_NUM_RANGE)
  {
    if (argValue <= cmdArg->cmdDomain->domNumMin - 0.1)
    {
      printf("%s? invalid: ", cmdArg->argName);
      cmdArgValuePrint(argValue, GLCD_FALSE, GLCD_TRUE);
      return CMD_RET_ERROR;
    }
    if (argValue >= cmdArg->cmdDomain->domNumMax + 0.1)
    {
      printf("%s? invalid: ", cmdArg->argName);
      cmdArgValuePrint(argValue, GLCD_FALSE, GLCD_TRUE);
      return CMD_RET_ERROR;
    }
  }

  // Validate assignment expression
  if (domType == DOM_NUM_ASSIGN)
  {
    if (exprAssign == GLCD_FALSE)
    {
      printf("%s? parse error\n", cmdArg->argName);
      return CMD_RET_ERROR;
    }
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValidateRegex
//
// Validate a string with a regex template. Currently used for scanning
// variable names.
// When used in an expression, variable names are validated in the expression
// evaluator. For commands 'vr' and 'lr' however we take the variable name as a
// word input and we must validate ourselves whether it consists of [a-zA-Z_]
// characters, and for 'vr' where it may also be a '.'.
//
// NOTE: When the scan profile for a variable name, as defined in expr.l
// [firmware/emulator] is modified, the regex pattern in all associated command
// dictionary domain entries in mchrondict.h [firmeware/emulator] using
// domaintype DOM_WORD_REGEX must be modified as well.
//
static u08 cmdArgValidateRegex(cmdArg_t *cmdArg, char *argValue)
{
  regex_t regex;
  int status = 0;

  if (cmdArg->cmdDomain->domType != DOM_WORD_REGEX)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

  // Init and validate the regex pattern
  status = regcomp(&regex, cmdArg->cmdDomain->domTextList,
    REG_EXTENDED | REG_NOSUB);
  status = regexec(&regex, argValue, (size_t)0, NULL, 0);
  regfree(&regex);
  if (status != 0)
  {
    // Invalid input according pattern
    printf("%s? invalid: %s\n", cmdArg->argName, argValue);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValidateWord
//
// Validate a word argument with a validation profile
//
static u08 cmdArgValidateWord(cmdArg_t *cmdArg, char *argValue)
{
  int i = 0;
  int j = 0;
  u08 wordFound = GLCD_FALSE;
  char *argWordList = cmdArg->cmdDomain->domTextList;

  if (cmdArg->cmdDomain->domType != DOM_WORD_VAL)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

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

  // In case we reached the end of the validation string we have a match when
  // the input string also reached the end
  if (wordFound == GLCD_FALSE && argValue[j] == '\0')
    wordFound = GLCD_TRUE;

  // Return error if not found
  if (wordFound == GLCD_FALSE)
  {
    printf("%s? invalid: %s\n", cmdArg->argName, argValue);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValuePrint
//
// Print a number in the desired format and return print length in characters
// including optional newline char
//
u08 cmdArgValuePrint(double value, u08 detail, u08 forceNewline)
{
  u08 length = 0;

  if (fabs(value) >= 10000 || fabs(value) < 0.01L)
  {
    if (detail == GLCD_TRUE)
    {
      if ((double)((long long)value) == value &&
          fabs(value) < 10E9L)
        length = printf("%lld ", (long long)value);
      else
        length = printf("%.6g ", value);
    }
    else
    {
      length = printf("%.3g ", value);
    }
  }
  else
  {
    if ((double)((long long)value) == value)
      length = printf("%lld ", (long long)value);
    else if (detail == GLCD_TRUE)
      length = printf("%.6f ", value);
    else
      length = printf("%.2f ", value);
  }

  if (forceNewline == GLCD_TRUE)
    length = length + printf("\n");

  return length;
}

//
// Function: cmdInputCleanup
//
// Cleanup the input stream by free-ing the last malloc-ed data and cleaning up
// the readline library interface (when used)
//
void cmdInputCleanup(cmdInput_t *cmdInput)
{
  // Only cleanup when initialized
  if (cmdInput->initialized == GLCD_FALSE)
    return;

  // Add last read to history cache when applicable
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    if (cmdInput->input != NULL && (cmdInput->input)[0] != '\0' &&
        (cmdInput->input)[0] != '\n')
    {
      add_history(cmdInput->input);
      rlCacheLen++;
    }
  }

  // Cleanup previous read
  if (cmdInput->input != NULL)
  {
    free(cmdInput->input);
    cmdInput->input = NULL;
  }

  // Flush pending readline cache and cleanup the readline interface
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    if (rlHistoryFile != NULL)
    {
      append_history(rlCacheLen, rlHistoryFile);
      history_truncate_file(rlHistoryFile, READLINE_MAXHISTORY);
      free(rlHistoryFile);
      rlHistoryFile = NULL;
    }
    rl_deprep_terminal();
    rl_reset_terminal(NULL);
  }

  // Cleanup complete
  cmdInput->initialized = GLCD_FALSE;
}

//
// Function: cmdInputInit
//
// Open an input stream in preparation to read its input line by line
// regardless the line size.
// Note: It is assumed that the readline method is used only once, being the
// mchron command line.
//
void cmdInputInit(cmdInput_t *cmdInput)
{
  cmdInput->input = NULL;
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    // Open/create the mchron readline history file to make sure it exists
    FILE *fp;
    char *home;

    // Get the full path to $HOME/.mchron
    home = getenv("HOME");
    if (home == NULL)
    {
      printf("%s: readline: cannot get $HOME\n", __progname);
    }
    else
    {
      // Combine $HOME and filename, open/create file and close it
      rlHistoryFile = malloc(strlen(home) + strlen(MCHRON_CONFIG) +
        strlen(READLINE_HISFILE) + 1);
      sprintf(rlHistoryFile, "%s%s%s", home, MCHRON_CONFIG, READLINE_HISFILE);
      fp = fopen(rlHistoryFile, "a");
      if (fp == NULL)
      {
        printf("%s: readline: cannot open file \"%s%s%s\"\n", __progname, "~",
          MCHRON_CONFIG, READLINE_HISFILE);
        printf("- manually create folder ~%s\n", MCHRON_CONFIG);
        free(rlHistoryFile);
        rlHistoryFile = NULL;
      }
      else
      {
        fclose(fp);
      }
    }

    // Disable auto-complete
    rl_bind_key('\t',rl_insert);

    // Truncate saved history and then load it in readline cache
    if (rlHistoryFile != NULL)
    {
      history_truncate_file(rlHistoryFile, READLINE_MAXHISTORY);
      read_history(rlHistoryFile);
    }

    // Initialize readline ahead of first read
    rl_initialize();
  }

  // Init done
  cmdInput->initialized = GLCD_TRUE;
}

//
// Function: cmdInputRead
//
// Acquire a single command line by reading the input stream part by part until
// a newline character is encountered indicating the command end-of-line.
// Note: The trailing newline character will NOT be copied to the resulting
// input buffer.
//
void cmdInputRead(char *prompt, cmdInput_t *cmdInput)
{
  // Add previous read to readline cache when applicable
  if (cmdInput->readMethod == CMD_INPUT_READLINELIB)
  {
    if (cmdInput->input != NULL && (cmdInput->input)[0] != '\0' &&
        (cmdInput->input)[0] != '\n')
    {
      add_history(cmdInput->input);
      rlCacheLen++;

      // We may need to flush unsaved readline cache into our history file
      if (rlCacheLen == READLINE_CACHE_LEN)
      {
        if (rlHistoryFile != NULL)
          append_history(READLINE_CACHE_LEN, rlHistoryFile);
        rlCacheLen = 0;
      }
    }
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
    // In case we use the readline library input method there's not much to do
    cmdInput->input = readline(prompt);
  }
  else
  {
    // Use our own input mechanism to read an input line from a text file.
    // Command line input uses the readline library.
    char inputCli[CMD_BUILD_LEN];
    u08 done = GLCD_FALSE;
    char *inputPtr = NULL;
    char *mergePtr = NULL;
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
        mergePtr = malloc(inputLenTotal + inputLen + 1);

        if (inputLenTotal > 0)
        {
          // Copy string built up so far into new buffer and add the string
          // that was read just now
          strcpy(mergePtr, (const char *)buildPtr);
          strcpy(&mergePtr[inputLenTotal], (const char *)inputCli);
          inputLenTotal = inputLenTotal + inputLen;
          free(buildPtr);
          buildPtr = mergePtr;
        }
        else
        {
          // First batch of characters
          strcpy(mergePtr, (const char *)inputCli);
          inputLenTotal = inputLen;
          buildPtr = mergePtr;
        }

        // See if we encountered the end of a line
        if (buildPtr[inputLenTotal - 1] == '\n')
        {
          // Terminating newline: EOL
          // Remove the newline from input buffer for compatibility reasons
          // with readline library functionality.
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
