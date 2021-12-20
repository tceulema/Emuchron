//*****************************************************************************
// Filename : 'dictutil.c'
// Title    : Utility routines for emuchron mchron command dictionary
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <string.h>
#include <regex.h>

// Emuchron defines
#include "../global.h"
#include "dictutil.h"

// Load the mchron command dictionary
#include "mchrondict.h"

// Functional name of mchron command
char *mchronCmdName = "command";

// This is me
extern const char *__progname;

// Local function prototypes
static void dictCmdPrint(cmdCommand_t *cmdCommand);

//
// Function: dictCmdGet
//
// Get the command dictionary entry for an mchron command
//
cmdCommand_t *dictCmdGet(char *cmdName)
{
  int i;
  int dictIdx = 0;
  cmdCommand_t *cmdCommand;
  int retVal;

  // Get index in dictionary for command group (#, a..z) or return an error
  // when unknown command group is provided
  if (*cmdName == '#')
  {
    dictIdx = 0;
  }
  else if (*cmdName >= 'a' && *cmdName <= 'z')
  {
    dictIdx = *cmdName - 'a' + 1;
  }
  else
  {
    printf("%s? invalid: %s\n", mchronCmdName, cmdName);
    return NULL;
  }

  // Get first commmand in command group dictionary and loop through available
  // commands
  cmdCommand = cmdDictMchron[dictIdx].cmdCommand;
  for (i = 0; i < cmdDictMchron[dictIdx].commandCount; i++)
  {
    retVal = strcmp(cmdName, cmdCommand->cmdName);
    if (retVal == 0)
    {
      // Found the command we're looking for
      return cmdCommand;
    }
    else if (retVal < 0)
    {
      // Not found and will not find it based on alphabetical order
      break;
    }
    cmdCommand++;
  }

  // Dictionary entry not found
  printf("%s? invalid: %s\n", mchronCmdName, cmdName);
  return NULL;
}

//
// Function: dictCmdPrint
//
// Print the dictionary contents of a command in the mchron command dictionary
//
static void dictCmdPrint(cmdCommand_t *cmdCommand)
{
  int argCount = 0;
  cmdArg_t *cmdArg;
  cmdDomain_t *cmdDomain;
  char *domChar;

  // Command name and description
  printf("%s: %s (%s)\n", mchronCmdName, cmdCommand->cmdName,
    cmdCommand->cmdNameDescr);

  // Command usage
  printf("usage  : %s ", cmdCommand->cmdName);
  for (argCount = 0; argCount < cmdCommand->argCount; argCount++)
    printf("<%s> ", cmdCommand->cmdArg[argCount].argName);
  printf("\n");

  // Command argument info (name + domain)
  for (argCount = 0; argCount < cmdCommand->argCount; argCount++)
  {
    // Get argument
    cmdArg = &cmdCommand->cmdArg[argCount];

    // Argument name
    printf("         %s: ", cmdArg->argName);

    // Argument domain info
    cmdDomain = cmdArg->cmdDomain;
    switch (cmdDomain->domType)
    {
    case DOM_CHAR_VAL:
      domChar = cmdDomain->domTextList;
      while (*domChar != '\0')
      {
        printf("'%c'", *domChar);
        domChar++;
        if (*domChar != '\0')
          printf(",");
      }
      if (cmdDomain->domInfo != NULL)
        printf(" (%s)", cmdDomain->domInfo);
      break;
    case DOM_WORD_VAL:
      printf("'");
      domChar = cmdDomain->domTextList;
      while (*domChar != '\0')
      {
        if (*domChar == '\n')
          printf("','");
        else
          printf("%c", *domChar);
        domChar++;
      }
      printf("'");
      if (cmdDomain->domInfo != NULL)
        printf(" (%s)", cmdDomain->domInfo);
      break;
    case DOM_NUM_RANGE:
      if (cmdDomain->domNumMax - cmdDomain->domNumMin == 1)
        printf("%d, %d", (int)cmdDomain->domNumMin,
          (int)cmdDomain->domNumMax);
      else
        printf("%d..%d", (int)cmdDomain->domNumMin,
          (int)cmdDomain->domNumMax);
      if (cmdDomain->domInfo != NULL)
        printf(" (%s)", cmdDomain->domInfo);
      break;
    case DOM_WORD_REGEX:
    case DOM_STRING:
    case DOM_STRING_OPT:
    case DOM_NUM:
    case DOM_NUM_ASSIGN:
      // These domain types do NOT contain human readable domain value check
      // info so we must provide generic domain info
      printf("%s", cmdDomain->domInfo);
      break;
    default:
      printf(" *** internal: unknown domain profile");
      break;
    }
    printf("\n");
  }

  // Print the actual command handler function name
  printf("handler: %s()\n", cmdCommand->cmdHandlerName);
}

//
// Function: dictPrint
//
// Print mchron command dictionary entries using a regex pattern (where '.'
// matches every command)
//
u08 dictPrint(char *pattern, u08 searchType)
{
  regex_t regex;
  cmdCommand_t *cmdCommand;
  int status = 0;
  int i, j, k;
  int commandCount = 0;

  // Validate regex pattern
  status = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0;
  if (status != 0)
  {
    regfree(&regex);
    return CMD_RET_ERROR;
  }

  // Loop through each command group
  for (i = 0; i < cmdDictCount; i++)
  {
    // Loop through each command in the command group
    for (j = 0; j < cmdDictMchron[i].commandCount; j++)
    {
      // Try to match the command property with the regex pattern
      cmdCommand = &cmdDictMchron[i].cmdCommand[j];
      if (searchType == CMD_SEARCH_NAME)
      {
        status = regexec(&regex, cmdCommand->cmdName, (size_t)0, NULL, 0);
      }
      else if (searchType == CMD_SEARCH_DESCR)
      {
        status = regexec(&regex, cmdCommand->cmdNameDescr, (size_t)0, NULL, 0);
      }
      else if (searchType == CMD_SEARCH_ARG)
      {
        // Check each command argument. In case a command does not have
        // arguments try to match the pattern with an empty string.
        status = -1;
        if (cmdCommand->argCount == 0)
          status = regexec(&regex, "", (size_t)0, NULL, 0);
        for (k = 0; k < cmdCommand->argCount && status != 0; k++)
        {
          status = regexec(&regex, cmdCommand->cmdArg[k].argName, (size_t)0,
            NULL, 0);
        }
      }
      else // CMD_SEARCH_ALL: search name + description + arguments
      {
        status = regexec(&regex, cmdCommand->cmdName, (size_t)0, NULL, 0);
        if (status != 0)
          status = regexec(&regex, cmdCommand->cmdNameDescr, (size_t)0, NULL,
            0);

        // Check each command argument. In case a command does not have
        // arguments try to match the pattern with an empty string.
        if (status != 0)
        {
          if (cmdCommand->argCount == 0)
            status = regexec(&regex, "", (size_t)0, NULL, 0);
          for (k = 0; k < cmdCommand->argCount && status != 0; k++)
          {
            status = regexec(&regex, cmdCommand->cmdArg[k].argName, (size_t)0,
              NULL, 0);
          }
        }
      }

      if (status == 0)
      {
        // Print its dictionary
        printf("------------------------\n");
        dictCmdPrint(&cmdDictMchron[i].cmdCommand[j]);
        commandCount++;
      }
    }
  }

  // Statistics
  if (commandCount > 0)
    printf("------------------------\n");
  printf("registered commands: %d\n", commandCount);

  // Cleanup regex
  regfree(&regex);

  return CMD_RET_OK;
}

//
// Function: dictVerify
//
// Verify the integrity of the command dictionary where possible.
// Return: MC_TRUE (success) or MC_FALSE (failure).
//
u08 dictVerify(void)
{
  regex_t regex;
  cmdCommand_t *cmdCommand;
  cmdArg_t *cmdArg;
  cmdDomain_t *cmdDomain;
  int i,j,k;
  int charCount;
  int stringCount;
  int numCount;
  int issueCount = 0;

  // Loop through each command group
  for (i = 0; i < cmdDictCount; i++)
  {
    // Loop through each command in the command group
    for (j = 0; j < cmdDictMchron[i].commandCount; j++)
    {
      cmdCommand = &cmdDictMchron[i].cmdCommand[j];

      // Verify alphabetical order of the command
      if (j > 0)
      {
        if (strcmp(cmdDictMchron[i].cmdCommand[j - 1].cmdName,
            cmdCommand->cmdName) >= 0)
        {
          printf("%s: dict: commands not in alphabetical order or identical\n",
            __progname);
          printf("  command-1 = '%s', command-2 = '%s'\n",
            cmdDictMchron[i].cmdCommand[j - 1].cmdName, cmdCommand->cmdName);
          issueCount++;
        }
      }

      // Verify we have a command description
      if (cmdCommand->cmdNameDescr == NULL ||
          cmdCommand->cmdNameDescr[0] == '\0')
      {
        printf("%s: dict: command is missing command description\n",
          __progname);
        printf("  command = '%s'\n", cmdCommand->cmdName);
        issueCount++;
      }

      // Verify that a proper command handler has been assigned
      if (cmdCommand->cmdPcCtrlType == PC_CONTINUE &&
          cmdCommand->cmdHandler == NULL)
      {
        printf("%s: dict: regular command is missing command handler\n",
          __progname);
        printf("  command = '%s'\n", cmdCommand->cmdName);
        issueCount++;
      }
      if (cmdCommand->cmdPcCtrlType != PC_CONTINUE &&
          cmdCommand->cbHandler == NULL)
      {
        printf("%s: dict: control block command is missing control block handler\n",
          __progname);
        printf("  command = '%s'\n", cmdCommand->cmdName);
        issueCount++;
      }

      // Reset argument counters.
      // Note that stringCount already includes an entry for the command name.
      charCount = 0;
      stringCount = 1;
      numCount = 0;

      // Loop through each command argument
      for (k = 0; k < cmdCommand->argCount; k++)
      {
        // Get argument and its domain
        cmdArg = &cmdCommand->cmdArg[k];
        cmdDomain = cmdArg->cmdDomain;

        // Count number of argument types per command
        if (cmdArg->argType == ARG_CHAR)
          charCount++;
        else if (cmdArg->argType == ARG_STRING)
          stringCount++;
        else if (cmdArg->argType == ARG_NUM)
          numCount++;

        // Verify argument type and argument domain and its combination
        if (cmdArg->argType != ARG_CHAR && cmdArg->argType != ARG_STRING &&
            cmdArg->argType != ARG_NUM)
        {
          printf("%s: dict: invalid argtype\n", __progname);
          printf("  command = '%s', argument = '%s'\n", cmdCommand->cmdName,
            cmdArg->argName);
          issueCount++;
        }
        else if (cmdDomain->domType != DOM_CHAR_VAL &&
            cmdDomain->domType != DOM_WORD_VAL &&
            cmdDomain->domType != DOM_WORD_REGEX &&
            cmdDomain->domType != DOM_STRING &&
            cmdDomain->domType != DOM_STRING_OPT &&
            cmdDomain->domType != DOM_NUM &&
            cmdDomain->domType != DOM_NUM_RANGE &&
            cmdDomain->domType != DOM_NUM_ASSIGN)
        {
          printf("%s: dict: invalid domaintype\n", __progname);
          printf("  command = '%s', argument = '%s' (argName = '%s', domName = '%s')\n",
            cmdCommand->cmdName, cmdArg->argName, cmdCommand->cmdArgName,
            cmdDomain->domName);
          issueCount++;
        }
        else if ((cmdArg->argType == ARG_CHAR &&
            cmdDomain->domType != DOM_CHAR_VAL) ||
            (cmdArg->argType == ARG_STRING &&
            cmdDomain->domType != DOM_WORD_VAL &&
            cmdDomain->domType != DOM_WORD_REGEX &&
            cmdDomain->domType != DOM_STRING &&
            cmdDomain->domType != DOM_STRING_OPT) ||
            (cmdArg->argType == ARG_NUM &&
            cmdDomain->domType != DOM_NUM &&
            cmdDomain->domType != DOM_NUM_RANGE &&
            cmdDomain->domType != DOM_NUM_ASSIGN))
        {
          printf("%s: dict: invalid combination argtype + domaintype\n",
            __progname);
          printf("  command = '%s', argument = '%s' (argName = '%s', domName = '%s')\n",
            cmdCommand->cmdName, cmdArg->argName, cmdCommand->cmdArgName,
            cmdDomain->domName);
          issueCount++;
        }

        // Verify domain types with validation checks
        if ((cmdDomain->domType == DOM_CHAR_VAL ||
            cmdDomain->domType == DOM_WORD_VAL ||
            cmdDomain->domType == DOM_WORD_REGEX) &&
            (cmdDomain->domTextList == NULL ||
            cmdDomain->domTextList[0] == '\0'))
        {
          printf("%s: dict: missing domain validation info\n", __progname);
          printf("  command = '%s', argument = '%s' (argName = '%s', domName = '%s')\n",
            cmdCommand->cmdName, cmdArg->argName, cmdCommand->cmdArgName,
            cmdDomain->domName);
          issueCount++;
        }
        else if (cmdDomain->domType == DOM_WORD_REGEX)
        {
          // Validate regex pattern
          if (regcomp(&regex, cmdDomain->domTextList,
              REG_EXTENDED | REG_NOSUB) != 0)
          {
            printf("%s: dict: invalid regex domain validation info\n",
              __progname);
            printf("  command = '%s', argument = '%s' (argName = '%s', domName = '%s')\n",
              cmdCommand->cmdName, cmdArg->argName, cmdCommand->cmdArgName,
              cmdDomain->domName);
            issueCount++;
          }
          regfree(&regex);
        }

        // Verify domain types without human readable validation checks
        if (cmdDomain->domType != DOM_CHAR_VAL &&
            cmdDomain->domType != DOM_WORD_VAL &&
            cmdDomain->domType != DOM_NUM_RANGE &&
            (cmdDomain->domInfo == NULL || cmdDomain->domInfo[0] == '\0'))
        {
          printf("%s: dict: missing domain info for printing command dictionary\n",
            __progname);
          printf("  command = '%s', argument = '%s' (argName = '%s', domName = '%s')\n",
            cmdCommand->cmdName, cmdArg->argName, cmdCommand->cmdArgName,
            cmdDomain->domName);
          issueCount++;
        }
      }

      // Detect overflow of command arguments
      if (charCount >= ARG_TYPE_COUNT_MAX)
      {
        printf("%s: dict: too many char arguments\n", __progname);
        printf("  command = '%s', count = %d\n", cmdCommand->cmdName, charCount);
        issueCount++;
      }
      if (stringCount >= ARG_TYPE_COUNT_MAX)
      {
        printf("%s: dict: too many string arguments\n", __progname);
        printf("  command = '%s', count = %d\n", cmdCommand->cmdName, stringCount);
        issueCount++;
      }
      if (numCount >= ARG_TYPE_COUNT_MAX)
      {
        printf("%s: dict: too many numeric arguments\n", __progname);
        printf("  command = '%s', count = %d\n", cmdCommand->cmdName, numCount);
        issueCount++;
      }
    }
  }

  if (issueCount != 0)
  {
    printf("%s: dict: issues found = %d\n", __progname, issueCount);
    printf("make corrections in mchrondict.h [firmware/emulator]\n");
    return MC_FALSE;
  }

  return MC_TRUE;
}
