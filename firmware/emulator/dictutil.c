//*****************************************************************************
// Filename : 'dictutil.c'
// Title    : Utility routines for emuchron mchron command dictionary
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <string.h>
#include <regex.h>

// Emuchron defines
#include "dictutil.h"

// Load the mchron command dictionary
#include "mchrondict.h"

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
    printf("%s? unknown: %s\n", argCmd[0].argName, cmdName);
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
  printf("%s? unknown: %s\n", argCmd[0].argName, cmdName);
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
  cmdArgDomain_t *cmdArgDomain;
  char *domainChar;

  // Command name and description
  printf("command: %s (%s)\n", cmdCommand->cmdName, cmdCommand->cmdNameDescr);

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
    cmdArgDomain = cmdArg->cmdArgDomain;
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
      if (cmdArgDomain->argDomainType == DOM_WORD)
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
          if (cmdArgDomain->argNumMax - cmdArgDomain->argNumMin == 1)
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
        // Detailed domain profile
        switch (cmdArgDomain->argDomainType)
        {
        case DOM_NUM_RANGE:
          if (cmdArgDomain->argNumMax - cmdArgDomain->argNumMin == 1)
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
        cmdArgDomain->argDomainType == DOM_VAR_NAME ||
        cmdArgDomain->argDomainType == DOM_VAR_NAME_ALL)
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

  // Print the actual command handler function name
  printf("handler: %s()\n", cmdCommand->cmdHandlerName);
}

//
// Function: dictPrint
//
// Print mchron command dictionary entries using a regexp pattern (where '.'
// matches every command)
//
u08 dictPrint(char *pattern)
{
  regex_t regex;
  int status = 0;
  int i;
  int j;
  int commandCount = 0;

  // Validate regexp pattern
  if (regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB) != 0)
    return CMD_RET_ERROR;

  // Loop through each command group
  for (i = 0; i < cmdDictCount; i++)
  {
    // Loop through each command in the command group
    for (j = 0; j < cmdDictMchron[i].commandCount; j++)
    {
      // See if command name matches regexp pattern
      status = regexec(&regex, cmdDictMchron[i].cmdCommand[j].cmdName,
        (size_t)0, NULL, 0);
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

  // Cleanup regexp
  regfree(&regex);

  return CMD_RET_OK;
}
