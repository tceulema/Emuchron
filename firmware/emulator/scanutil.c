//*****************************************************************************
// Filename : 'scanutil.c'
// Title    : Utilities for mchron command line scanning, command dictionary
//            and readline command cache with history
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <readline/readline.h>
#include <readline/history.h>

// Monochron and emuchron defines
#include "../ks0108.h"
#include "scanutil.h"
#include "mchrondict.h"
#include "expr.h"

// The command line input stream batch size for a single line
#define CMD_BUILD_LEN		128

// The readline unsaved cache and history file with size parameters
#define READLINE_CACHE_LEN	15
#define READLINE_HISFILE	"/.mchron_history"
#define READLINE_MAXHISTORY	250

// External data from expression evaluator
extern double exprValue;
extern unsigned char exprAssign;

// This is me
extern const char *__progname;

// The variables to store the command line argument scan results
char argChar[ARG_TYPE_COUNT_MAX];
double argDouble[ARG_TYPE_COUNT_MAX];
char *argWord[ARG_TYPE_COUNT_MAX];
char *argString = NULL;

// Index in the several scan result arrays
static int argCharIdx = 0;
static int argDoubleIdx = 0;
static int argWordIdx = 0;

// The current readline history in-memory cache length and history file
static int rlCacheLen = 0;
static char *rlHistoryFile = NULL;

// Local function prototypes
static int cmdArgValidateChar(cmdArg_t *cmdArg, char argValue, int silent);
static int cmdArgValidateNum(cmdArg_t *cmdArg, double argValue, int silent);
static int cmdArgValidateVar(cmdArg_t *cmdArg, char *argValue, int silent);
static int cmdArgValidateWord(cmdArg_t *cmdArg, char *argValue, int silent);

//
// Function: cmdArgInit
//
// Preprocess the input string by skipping to the first non-white
// character. And, clear previous scan results by initializing the
// indices in the scan result arrays and freeing malloc-ed string
// space.
//
void cmdArgInit(char **input)
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
  for (i = 0; i < ARG_TYPE_COUNT_MAX; i++)
  {
    argChar[i] = '\0';
    argDouble[i] = 0;
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
  argDoubleIdx = 0;
  argWordIdx = 0;
}

//
// Function: cmdArgScan
//
// Scan a (partial) argument profile.
// For a char, word or string profile just copy the char/string value
// into the designated array or string argument variable.
// For a numeric profile though copy the string expression and push
// it through the expression evaluator to get a resulting numeric
// value. When a valid numeric value is obtained copy it into the
// designated double array variable.
// Parameter input is modified to point to the remaining string to be
// scanned.
//
int cmdArgScan(cmdArg_t cmdArg[], int argCount, char **input, int silent)
{
  char *workPtr = *input;
  char *evalString;
  int i = 0;
  int j = 0;
  int retVal = CMD_RET_OK;

  while (i < argCount)
  {
    char c = *workPtr;

    if (cmdArg[i].argType == ARG_CHAR)
    {
      // Scan a single character argument profile

      // Verify argument count overflow
      if (argCharIdx == ARG_TYPE_COUNT_MAX)
      {
        printf("%s? internal: overflow char argument count\n",
          cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // The next character must be white space or end of string
      if (strcspn(workPtr, " \t") > 1)
      {
        if (silent == GLCD_FALSE)
          printf("%s? invalid: not a single character\n",
            cmdArg[i].argName);
        return CMD_RET_ERROR;
      }
      workPtr++;

      // Validate the character (if a validation rule has been setup)
      if (cmdArg[i].cmdArgDomain->argDomainType != DOM_NULL_INFO)
      {
        retVal = cmdArgValidateChar(&cmdArg[i], c, silent);
        if (retVal != CMD_RET_OK)
          return retVal;
      }

      // Value approved so copy the char argument
      argChar[argCharIdx] = c;
      argCharIdx++;

      // Skip to next argument element in string
      workPtr = workPtr + strspn(workPtr, " \t");

      // Next argument profile
      i++;
    }
    else if (cmdArg[i].argType == ARG_UNUM || cmdArg[i].argType == ARG_NUM ||
             cmdArg[i].argType == ARG_ASSIGN)
    {
      // Scan an (unsigned) number argument profile.
      // A number profile is an expression that can be a constant, a variable,
      // a combination of both or a variable value assignment in a mathematical
      // expression. We'll use flex/bison to evaluate the expression.

      // Verify argument count overflow
      if (argCharIdx == ARG_TYPE_COUNT_MAX)
      {
        printf("%s? internal: overflow numeric argument count\n",
          cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // Copy the expression upto next delimeter
      j = strcspn(workPtr, " \t");
      evalString = malloc(j + 2);
      memcpy(evalString, workPtr, j);
      evalString[j] = '\n';
      evalString[j+1] = '\0';
      workPtr = workPtr + j;

      // Evaluate the expression
      retVal = exprEvaluate(cmdArg[i].argName, evalString, j + 1);
      free(evalString);
      if (retVal != CMD_RET_OK)
        return CMD_RET_ERROR;

      // Validate unsigned number
      if (cmdArg[i].argType == ARG_UNUM && exprValue < 0)
      {
        if (silent == GLCD_FALSE)
        {
          printf("%s? invalid: ", cmdArg[i].argName);
          cmdArgValuePrint(exprValue, GLCD_FALSE);
          printf("\n");
        }
        return CMD_RET_ERROR;
      }

      // Validate assignment expression
      if (cmdArg[i].argType == ARG_ASSIGN && exprAssign == 0)
      {
        if (silent == GLCD_FALSE)
          printf("%s? syntax error\n", cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // Validate the number (if a validation rule has been setup)
      if (cmdArg[i].cmdArgDomain->argDomainType != DOM_NULL_INFO)
      {
        retVal = cmdArgValidateNum(&cmdArg[i], exprValue, silent);
        if (retVal != CMD_RET_OK)
          return retVal;
      }

      // Value approved so copy the resulting value of the expression
      argDouble[argDoubleIdx] = exprValue;
      argDoubleIdx++;

      // Skip to next argument element in string
      workPtr = workPtr + strspn(workPtr, " \t");

      // Next argument profile
      i++;
    }
    else if (cmdArg[i].argType == ARG_WORD)
    {
      // Scan a character word argument profile

      // Verify argument count overflow
      if (argCharIdx == ARG_TYPE_COUNT_MAX)
      {
        printf("%s? internal: overflow word argument count\n",
          cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // Verify end-of-string
      if (c == '\0')
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // Value exists so copy the word upto next delimeter
      j = strcspn(workPtr, " \t");
      argWord[argWordIdx] = malloc(j + 1);
      memcpy(argWord[argWordIdx], workPtr, j);
      argWord[argWordIdx][j] = '\0';
      workPtr = workPtr + j;
      argWordIdx++;

      // Validate the word (if a validation rule has been setup)
      if (cmdArg[i].argType == ARG_WORD)
      {
        if (cmdArg[i].cmdArgDomain->argDomainType == DOM_WORD_LIST)
        {
          retVal = cmdArgValidateWord(&cmdArg[i], argWord[argWordIdx - 1],
            silent);
          if (retVal != CMD_RET_OK)
            return retVal;
        }
        else if (cmdArg[i].cmdArgDomain->argDomainType == DOM_VAR_NAME)
        {
          retVal = cmdArgValidateVar(&cmdArg[i], argWord[argWordIdx - 1],
            silent);
          if (retVal != CMD_RET_OK)
            return retVal;
        }
      }

      // Skip to next argument element in string
      workPtr = workPtr + strspn(workPtr, " \t");

      // Next argument profile
      i++;
    }
    else if (cmdArg[i].argType == ARG_STRING ||
             cmdArg[i].argType == ARG_STR_OPT)
    {
      // Scan a character string argument profile

      // Verify end-of-string that will be an error only in case
      // a string must be specified (is optional for comments)
      if (c == '\0' && cmdArg[i].argType == ARG_STRING)
      {
        if (silent == GLCD_FALSE)
          printf("%s? missing value\n", cmdArg[i].argName);
        return CMD_RET_ERROR;
      }

      // As the remainder of the input string is to be considered
      // the string argument we skip to the end of the input string
      j = strlen(workPtr);
      argString = malloc(j + 1);
      memcpy(argString, workPtr, j + 1);
      workPtr = workPtr + j;

      // Next argument profile
      i++;
    }
    else if (cmdArg[i].argType == ARG_END)
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
      printf("internal: invalid element: %d %d\n", i, cmdArg[i].argType);
      return CMD_RET_ERROR;
    }
  }

  // Successful scan
  *input = workPtr;
  return CMD_RET_OK;
}

//
// Function: cmdArgValidateChar
//
// Validate a character argument with a validation profile
//
static int cmdArgValidateChar(cmdArg_t *cmdArg, char argValue, int silent)
{
  int i = 0;
  int charFound = GLCD_FALSE;
  char *argCharList = cmdArg->cmdArgDomain->argTextList;

  if (cmdArg->cmdArgDomain->argDomainType != DOM_CHAR_LIST)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

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
      printf("%s? unknown: %c\n", cmdArg->argName, argValue);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValidateNum
//
// Validate a numeric argument with a validation profile
//
static int cmdArgValidateNum(cmdArg_t *cmdArg, double argValue, int silent)
{
  int argDomainType = cmdArg->cmdArgDomain->argDomainType;

  // Validate internal integrity of validation structure
  if (argDomainType != DOM_NUM_MIN && argDomainType != DOM_NUM_MAX &&
      argDomainType != DOM_NUM_RANGE)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

  // Validate minimum value
  if (argDomainType == DOM_NUM_MIN || argDomainType == DOM_NUM_RANGE)
  {
    if (argValue < cmdArg->cmdArgDomain->argNumMin)
    {
      if (silent == GLCD_FALSE)
      {
        printf("%s? invalid: ", cmdArg->argName);
        cmdArgValuePrint(argValue, GLCD_FALSE);
        printf("\n");
      }
      return CMD_RET_ERROR;
    }
  }

  // Validate maximum value
  if (argDomainType == DOM_NUM_MAX || argDomainType == DOM_NUM_RANGE)
  {
    if (argValue - cmdArg->cmdArgDomain->argNumMax >= 1)
    {
      if (silent == GLCD_FALSE)
      {
        printf("%s? invalid: ", cmdArg->argName);
        cmdArgValuePrint(argValue, GLCD_FALSE);
        printf("\n");
      }
      return CMD_RET_ERROR;
    }
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValidateVar
//
// Validate a variable name. When used in an expression, variable names are
// validated in the expression evaluator. However, for the var print and var
// reset commands we take the variable name as a word input and we must
// validate whether it consists of a '*' or a string containing only [a-zA-Z_]
// characters.
// Note that when the scan profile for a variable name (as defined in expr.l
// [firmware/emulator]) is modified, this function must be changed as well.
//
static int cmdArgValidateVar(cmdArg_t *cmdArg, char *argValue, int silent)
{
  char *varName = argValue;

  if (cmdArg->cmdArgDomain->argDomainType != DOM_VAR_NAME)
  {
    printf("%s? internal: invalid domain validation type\n", cmdArg->argName);
    return CMD_RET_ERROR;
  }

  // Find either a '*' or a string of [a-zA-Z_] characters
  if (strcmp(argValue, "*") != 0)
  {
    // A variable name is [a-zA-Z_]+
    while (*varName != '\0')
    {
      if ((*varName >= 'a' && *varName <='z') ||
          (*varName >= 'A' && *varName <='Z') ||
          *varName == '_')
      {
        varName++;
      }
      else
      {
        // Invalid character found
        if (silent == GLCD_FALSE)
          printf("%s? invalid\n", cmdArg->argName);
        return CMD_RET_ERROR;
      }
    }
  }

  return CMD_RET_OK;
}

//
// Function: cmdArgValidateWord
//
// Validate a word argument with a validation profile
//
static int cmdArgValidateWord(cmdArg_t *cmdArg, char *argValue, int silent)
{
  int i = 0;
  int j = 0;
  int wordFound = GLCD_FALSE;
  char *argWordList = cmdArg->cmdArgDomain->argTextList;

  if (cmdArg->cmdArgDomain->argDomainType != DOM_WORD_LIST)
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

  // In case we reached the end of the validation string we have a match
  // when the input string also reached the end
  if (wordFound == GLCD_FALSE && argValue[j] == '\0')
    wordFound = GLCD_TRUE;

  // Return error if not found
  if (wordFound == GLCD_FALSE)
  {
    if (silent == GLCD_FALSE)
      printf("%s? unknown: %s\n", cmdArg->argName, argValue);
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}


//
// Function: cmdArgValuePrint
//
// Print a number in the desired format and return print length
//
int cmdArgValuePrint(double value, int detail)
{
  if (fabs(value) >= 10000 || fabs(value) < 0.01L)
  {
    if (detail == GLCD_TRUE)
    {
      if ((double)((long long)value) == value &&
          fabs(value) < 10E9L)
        return printf("%lld ", (long long)value);
      else
        return printf("%.6g ", value);
    }
    else
    {
      return printf("%.3g ", value);
    }
  }
  else
  {
    if ((double)((long long)value) == value)
      return printf("%lld ", (long long)value);
    else if (detail == GLCD_TRUE)
      return printf("%.6f ", value);
    else
      return printf("%.2f ", value);
  }
}

//
// Function: cmdDictCmdGet
//
// Get the dictionary entry for an mchron command
//
int cmdDictCmdGet(char *cmd, cmdCommand_t **cmdCommand)
{
  int i;
  int dictIdx = 0;
  int retVal;

  // Get index in dictionary for command group (#, a..z) or return an
  // error when not a valid command group is provided
  if (*cmd == '#')
    dictIdx = 0;
  else if (*cmd >= 'a' && *cmd <= 'z')
    dictIdx = *cmd - 'a' + 1;
  else
    return CMD_RET_ERROR;

  // Get first commmand in command group dictionary and loop
  // through available commands
  *cmdCommand = cmdDictMchron[dictIdx].cmdCommand;
  for (i = 0; i < cmdDictMchron[dictIdx].commandCount; i++)
  {
    retVal = strcmp(cmd, (*cmdCommand)->cmdName);
    if (retVal == 0)
    {
      // Found the command we're looking for
      return CMD_RET_OK;
    }
    else if (retVal < 0)
    {
      // Not found and will not find it based on alphabetical order
      return CMD_RET_ERROR;
    }

    // Not found yet; get next entry
    (*cmdCommand)++;
  }

  // Dictionary entry not found
  return CMD_RET_ERROR;
}

//
// Function: cmdDictCmdPrint
//
// Print the dictionary contents of a command in the mchron command dictionary
//
int cmdDictCmdPrint(char *cmd)
{
  int argCount = 0;
  int retVal;
  cmdCommand_t *cmdCommand;
  cmdArg_t *cmdArg;
  cmdArgDomain_t *cmdArgDomain;
  char *domainChar;

  // Get the dictionary for the command
  retVal = cmdDictCmdGet(cmd, &cmdCommand);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Command name and description
  printf("command: %s (%s)\n", cmdCommand->cmdName, cmdCommand->cmdNameDescr);

  // Command usage
  printf("usage  : %s ", cmdCommand->cmdName);
  for (argCount = 0; argCount < cmdCommand->argCount; argCount++)
  {
    if (cmdCommand->cmdArg[argCount].argType != ARG_END)
      printf("<%s> ", cmdCommand->cmdArg[argCount].argName);
  }
  printf("\n");

  // Command argument info (name + domain)
  for (argCount = 0; argCount < cmdCommand->argCount; argCount++)
  {
    // Get argument
    cmdArg = &cmdCommand->cmdArg[argCount];

    if (cmdArg->argType != ARG_END)
    {
      // Argument name
      printf("         %s: ", cmdArg->argName);

      // Get argument domain structure
      cmdArgDomain = cmdArg->cmdArgDomain;

      // Argument domain info
      switch (cmdArg->argType)
      {
      case ARG_CHAR:
        if (cmdArgDomain->argDomainType != DOM_NULL_INFO)
        {
          // Detailed domain profile
          domainChar = cmdArgDomain->argTextList;
          while (*domainChar != '\0')
          {
            printf("'%c'", *domainChar);
            domainChar++;
            if (*domainChar != '\0')
              printf(",");
          }
        }
        break;
      case ARG_WORD:
        if (cmdArgDomain->argDomainType == DOM_WORD_LIST)
        {
          // Detailed domain profile
          printf("'");
          domainChar = cmdArgDomain->argTextList;
          while (*domainChar != '\0')
          {
            if (*domainChar == '\n')
              printf("','");
            else
              printf("%c", *domainChar);
            domainChar++;
          }
          printf("'");
        }
        break;
      case ARG_UNUM:
        if (cmdArgDomain->argDomainType != DOM_NULL_INFO)
        {
          // Detailed domain profile
          switch (cmdArgDomain->argDomainType)
          {
          case DOM_NUM_RANGE:
            if (fabs(cmdArgDomain->argNumMax - cmdArgDomain->argNumMin) == 1)
              printf("%d, %d", (int)cmdArgDomain->argNumMin,
                (int)cmdArgDomain->argNumMax);
            else
              printf("%d..%d", (int)cmdArgDomain->argNumMin,
                (int)cmdArgDomain->argNumMax);
            break;
          case DOM_NUM_MAX:
            if (cmdArgDomain->argNumMax == 1)
              printf("0, %d", (int)cmdArgDomain->argNumMax);
            else
              printf("0..%d", (int)cmdArgDomain->argNumMax);
            break;
          case DOM_NUM_MIN:
            printf(">=%d", (int)cmdArgDomain->argNumMin);
            break;
          default:
            printf("*** internal: invalid domain profile");
            break;
          }
        }
        break;
      case ARG_NUM:
        if (cmdArgDomain->argDomainType != DOM_NULL_INFO)
        {
          switch (cmdArgDomain->argDomainType)
          {
          case DOM_NUM_RANGE:
            if (fabs(cmdArgDomain->argNumMax - cmdArgDomain->argNumMin) == 1)
              printf("%d, %d", (int)cmdArgDomain->argNumMin,
                (int)cmdArgDomain->argNumMax);
            else
              printf("%d..%d", (int)cmdArgDomain->argNumMin,
                (int)cmdArgDomain->argNumMax);
            break;
          case DOM_NUM_MAX:
            printf("<=%d", (int)cmdArgDomain->argNumMax);
            break;
          case DOM_NUM_MIN:
            printf(">=%d", (int)cmdArgDomain->argNumMin);
            break;
          default:
            printf("*** internal: invalid domain profile");
            break;
          }
        }
        break;
      case ARG_STRING:
      case ARG_STR_OPT:
      case ARG_ASSIGN:
        // An (optional) string will only have an info domain profile
        break;
      default:
        printf("*** internal: invalid domain profile");
        break;
      }

      // Provide argument info
      if (cmdArgDomain->argDomainType == DOM_NULL_INFO ||
          cmdArgDomain->argDomainType == DOM_VAR_NAME)
      {
        // Provide generic domain info
        printf("%s", cmdArgDomain->argDomainInfo);
      }
      else if (cmdArgDomain->argDomainInfo != NULL)
      {
        // Provide detailed domain info when present
        printf(" (%s)", cmdArgDomain->argDomainInfo);
      }
      printf("\n");
    }
  }

  // Print the actual command handler function name
  printf("handler: %s()\n", cmdCommand->cmdHandlerName);

  return CMD_RET_OK;
}

//
// Function: cmdDictCmdPrintAll
//
// Print the full mchron command dictionary
//
int cmdDictCmdPrintAll(void)
{
  int i;
  int j;
  int commandCount = 0;

  // Loop through each command group
  for (i = 0; i < cmdDictCount; i++)
  {
    // Loop through each command in the command group
    for (j = 0; j < cmdDictMchron[i].commandCount; j++)
    {
      // Print its dictionary
      printf("------------------------\n");
      cmdDictCmdPrint(cmdDictMchron[i].cmdCommand[j].cmdName);
      commandCount++;
    }
  }

  // Statistics
  if (commandCount > 0)
    printf("------------------------\n");
  printf("registered commands: %d\n", commandCount);

  return CMD_RET_OK;
}

//
// Function: cmdInputCleanup
//
// Cleanup the input stream by free-ing the last malloc-ed data
// and cleaning up the readline library interface (when used)
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
// Note: It is assumed that the readline method is used only once, being
// the mchron command line.
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
      printf("%s: readline: Cannot get $HOME\n", __progname);
    }
    else
    {
      // Combine $HOME and filename, open/create file and close it
      rlHistoryFile = malloc(strlen(home) + strlen(READLINE_HISFILE) + 1);
      sprintf(rlHistoryFile, "%s%s", home, READLINE_HISFILE);
      fp = fopen(rlHistoryFile, "a");
      if (fp == NULL)
      {
        printf("%s: readline: Cannot open file \"%s%s\".\n", __progname,
         "$HOME", READLINE_HISFILE);
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
// Acquire a single command line by reading the input stream part by
// part until a newline character is encountered indicating the
// command end-of-line.
// Note: The trailing newline character will NOT be copied to the
// resulting input buffer.
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
    // In case we use the readline library input method there's
    // not much to do
    cmdInput->input = readline(prompt);
  }
  else
  {
    // Use our own input mechanism to read an input line from a
    // text file. Command line input uses the readline library.
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
          strcpy(&mallocPtr[inputLenTotal], (const char *)inputCli);
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
