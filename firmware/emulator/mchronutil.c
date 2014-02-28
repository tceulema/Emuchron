//*****************************************************************************
// Filename : 'mchronutil.c'
// Title    : mchron utility routines
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

// Monochron defines including hardware stubs
#include "stub.h"
#include "lcd.h"
#include "../ks0108.h"
#include "../ratt.h"
#include "../anim.h"

// Mchron utilities
#include "expr.h"
#include "scanutil.h"
#include "mchronutil.h"

// Monochron defined data
extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t date_d, date_m, date_y;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t mcMchronClock;
extern clockDriver_t *mcClockPool;

// The variables that store the processed command line arguments
extern char argChar[];
extern int argInt[];
extern char *argWord[];
extern char *argString;

// External data from expression evaluator
extern int exprValue;
extern int varError;

// Current command file execution depth
extern int fileExecDepth;

// The mchron alarm time
extern uint8_t emuAlarmH;
extern uint8_t emuAlarmM;

// Debug file
extern FILE *debugFile;

// The command line input stream control structure
extern cmdInput_t cmdInput;

// This is me
extern const char *__progname;

// The command list execution depth that is used to determine to
// actively switch between keyboard line mode and keypress mode
int listExecDepth = 0;

// Repeat command argument profiles
argItem_t argRepeatWhile[] =
{ { ARG_WORD,   "command",     0 },
  { ARG_WORD,   "variable",    0 },
  { ARG_WORD,   "condition",   0 },
  { ARG_WORD,   "end",         0 },
  { ARG_INT,    "start",       0 },
  { ARG_WORD,   "step",        0 },
  { ARG_END,    "",            0 } };

argItem_t argRepeatNext[] =
{ { ARG_WORD,   "command",     0 },
  { ARG_END,    "",            0 } };

// Local function prototypes
int cmdRepeatArgsGet(cmdRepeat_t *cmdRepeat, int *valVar, int *valEnd,
  int *valStep);
int emuRepeatInit(cmdRepeat_t *cmdRepeat, char *input);
int emuRepeatNext(cmdLine_t *cmdLine, int *doNextLoop);
int emuRepeatWhile(cmdLine_t *cmdLine, int *doNextLoop);
void emuSigCatch(int sig, siginfo_t *siginfo, void *context);

//
// Function: emuArgcArgv
//
// Process mchron strartup command line arguments
//
int emuArgcArgv(int argc, char *argv[], argcArgv_t *argcArgv)
{
  char input[250];
  int argCount = 1;

  // Init references to command line argument positions
  argcArgv->argDebug = 0;
  argcArgv->argGlutGeometry = 0;
  argcArgv->argGlutPosition = 0;
  argcArgv->argTty = 0;
  argcArgv->argLcdType = 0;

  // Init the LCD device data
  argcArgv->lcdDeviceParam.lcdNcurTty[0] = '\0';
  argcArgv->lcdDeviceParam.useNcurses = GLCD_FALSE;
  argcArgv->lcdDeviceParam.useGlut = GLCD_TRUE;
  argcArgv->lcdDeviceParam.lcdGlutPosX = 100;
  argcArgv->lcdDeviceParam.lcdGlutPosY = 100;
  argcArgv->lcdDeviceParam.lcdGlutSizeX = 520;
  argcArgv->lcdDeviceParam.lcdGlutSizeY = 264;
  argcArgv->lcdDeviceParam.winClose = emuWinClose;

  // Do archaic command line processing to obtain the LCD output device(s),
  // LCD output configs and debug logfile 
  while (argCount < argc)
  {
    if (strncmp(argv[argCount],"-d",4) == 0)
    {
      // Debug output file name
      argcArgv->argDebug = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount],"-g",4) == 0)
    {
      // Glut window geometry
      argcArgv->argGlutGeometry = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount],"-l",4) == 0)
    {
      // LCD stub device type
      argcArgv->argLcdType = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount],"-p",4) == 0)
    {
      // Glut window position
      argcArgv->argGlutPosition = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount],"-t",4) == 0)
    {
      // Ncurses tty device
      argcArgv->argTty = argCount + 1;
      argCount = argCount + 2;
    }
    else
    {
      // Anything else: force to quit
      argcArgv->argDebug = argc;
      argCount = argc;
    }
  }

  // Check result of command line processing
  if (argcArgv->argLcdType >= argc || argcArgv->argDebug >= argc ||
      argcArgv->argGlutGeometry >= argc || argcArgv->argTty >= argc ||
      argcArgv->argGlutPosition >= argc)
  {
    printf("Use: %s [-l <device>] [-t <tty>] [-g <geometry>] [-p <position>]\n", __progname);
    printf("            [-d <logfile>] [-h]\n");
    printf("  -d <logfile>  - Debug logfile name\n");
    printf("  -g <geometry> - Geometry (x,y) of glut window\n");
    printf("                  Default: \"520x264\"\n");
    printf("                  Examples: \"130x66\" or \"260x132\"\n");
    printf("  -h            - Give usage help\n");
    printf("  -l <device>   - LCD stub device type\n");
    printf("                  Values: \"glut\" or \"ncurses\" or \"all\"\n");
    printf("                  Default: \"glut\"\n");
    printf("  -p <position> - Position (x,y) of glut window\n");
    printf("                  Default: \"100,100\"\n");
    printf("  -t <tty>      - tty device for ncurses of 258x66 sized terminal\n");
    printf("                  Default: get <tty> from $HOME/.mchron\n");
    printf("Examples:\n");
    printf("  ./%s\n", __progname);
    printf("  ./%s -l glut -p \"768,128\"\n", __progname);
    printf("  ./%s -l ncurses\n", __progname);
    printf("  ./%s -l ncurses -t /dev/pts/1 -d debug.log\n", __progname);
    return CMD_RET_ERROR;
  }

  // Validate LCD stub output device
  if (argcArgv->argLcdType > 0)
  {
    if (strcmp("glut", argv[argcArgv->argLcdType]) == 0)
    {
      argcArgv->lcdDeviceParam.useGlut = GLCD_TRUE;
      argcArgv->lcdDeviceParam.useNcurses = GLCD_FALSE;
    }
    else if (strcmp("ncurses", argv[argcArgv->argLcdType]) == 0)
    {
      argcArgv->lcdDeviceParam.useGlut = GLCD_FALSE;
      argcArgv->lcdDeviceParam.useNcurses = GLCD_TRUE;
    }
    else if (strcmp("all", argv[argcArgv->argLcdType]) == 0)
    {
      argcArgv->lcdDeviceParam.useGlut = GLCD_TRUE;
      argcArgv->lcdDeviceParam.useNcurses = GLCD_TRUE;
    }
    else
    {
      printf("%s: -l: Unknown LCD stub device type: %s\n", __progname,
        argv[argcArgv->argLcdType]);
      return CMD_RET_ERROR;
    }
  }

  // Validate glut window geometry
  if (argcArgv->argGlutGeometry > 0)
  {
    char *separator;

    strcpy(input, argv[argcArgv->argGlutGeometry]);
    separator = strchr(input, 'x');
    if (separator == NULL)
    {
      printf("%s: -g: Invalid format glut geometry\n", __progname);
      return CMD_RET_ERROR;
    }
    *separator = '\0';
    argcArgv->lcdDeviceParam.lcdGlutSizeX = atoi(input);
    argcArgv->lcdDeviceParam.lcdGlutSizeY = atoi(separator + 1);
  }

  // Validate glut window position
  if (argcArgv->argGlutPosition > 0)
  {
    char *separator;

    strcpy(input, argv[argcArgv->argGlutPosition]);
    separator = strchr(input, ',');
    if (separator == NULL)
    {
      printf("%s: -p: Invalid format glut position\n", __progname);
      return CMD_RET_ERROR;
    }
    *separator = '\0';
    argcArgv->lcdDeviceParam.lcdGlutPosX = atoi(input);
    argcArgv->lcdDeviceParam.lcdGlutPosY = atoi(separator + 1);
  }

  // Get the ncurses output device
  if (argcArgv->argTty != 0)
  {
    // Got it from the command line
    struct stat buffer;   

    // Copy to our runtime and verify if the tty is actually in use
    strcpy(argcArgv->lcdDeviceParam.lcdNcurTty, argv[argcArgv->argTty]);
    if (stat(argcArgv->lcdDeviceParam.lcdNcurTty, &buffer) != 0)
    {
      printf("%s: -t: tty \"%s\" is not in use\n", __progname,
        argcArgv->lcdDeviceParam.lcdNcurTty);
      return CMD_RET_ERROR;
    }
  }
  else if (argcArgv->lcdDeviceParam.useNcurses == 1)
  {
    // Get the tty device if not specified on the command line
    FILE *fp;
    char *home;
    char fullPath[200];
    int ttyLen = 0;
    struct stat buffer;   

    // Get the full path to $HOME/.mchron
    home = getenv("HOME");
    if (home == NULL)
    {
      printf("%s: Cannot get $HOME\n", __progname);
      printf("Use switch '-o <tty>' to set LCD output device\n");
      return CMD_RET_ERROR;
    }
    strcpy(fullPath, home);
    strcat(fullPath, "/.mchron");

    // Open the file with the tty device
    fp = fopen(fullPath, "r");
    if (fp == NULL)
    {
      printf("%s: Cannot open file \"%s\".\n", __progname, "$HOME/.mchron");
      printf("Use switch '-t <tty>' to set ncurses LCD output device\n");
      return CMD_RET_ERROR;
    }

    // Read output device in first line
    fgets(argcArgv->lcdDeviceParam.lcdNcurTty, 50, fp);

    // Kill all \r or \n in the tty string as ncurses doesn't like
    // this. Both \r and \n are normally trailing characters.
    for (ttyLen = strlen(argcArgv->lcdDeviceParam.lcdNcurTty); ttyLen > 0; ttyLen--)
    {
      if (argcArgv->lcdDeviceParam.lcdNcurTty[ttyLen-1] == '\n' ||
          argcArgv->lcdDeviceParam.lcdNcurTty[ttyLen-1] == '\r')
      {
        argcArgv->lcdDeviceParam.lcdNcurTty[ttyLen-1] = '\0';
      }
    }

    // Clean up our stuff
    fclose(fp);
    
    // Verify if the tty is actually in use
    if (stat(argcArgv->lcdDeviceParam.lcdNcurTty, &buffer) != 0)
    {
      printf("%s: $HOME/.mchron: tty \"%s\" is not in use\n",
        __progname, argcArgv->lcdDeviceParam.lcdNcurTty);
      printf("Start a new Monochron ncurses terminal or use switch '-t <tty>' to set\n");
      printf("ncurses LCD output device\n");
      return CMD_RET_ERROR;
    }
  }
  
  // All seems to be ok
  return CMD_RET_OK;
}

//
// Function: emuClockRelease
//
// Release a selected clock
//
void emuClockRelease(int echoCmd)
{
  // Clear clock time and detach from current selected clock
  mcClockOldTS=mcClockOldTM=mcClockOldTH=mcClockOldDD=mcClockOldDM=mcClockOldDY=0;
  if (mcClockPool[mcMchronClock].clockId != CHRON_NONE && echoCmd == CMD_ECHO_YES)
  {
    emuPrefixEcho();
    printf("released clock\n");
  }
  mcMchronClock = 0;

  // Kill alarm (if sounding anyway) and reset it
  alarmSoundKill();
  alarmClear();
}

//
// Function: emuColorGet
//
// Get the requested color
//
int emuColorGet(char colorId, int *color)
{
  // Validate color
  if (colorId == 'b')
  {
    *color = mcBgColor;
  }
  else if (colorId == 'f')
  {
    *color = mcFgColor;
  }
  else
  {
    printf("color? invalid value\n");
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: emuListExecute
//
// Execute the commands in the program list as loaded via a file or
// interactively on the command line.
// We have all applicable commands and repeat loops available in linked
// lists. The repeat loop list has been checked on its integrity so we
// don't need to worry about that. 
// Start at the top of the list and work our way down until we find an
// error or end up at the end of the list.
// We may encounter repeat while and repeat next commands that will
// influence the program counter by creating loops in the execution
// plan of the list.
//
int emuListExecute(cmdLine_t *cmdLineRoot, char *source, int echoCmd,
  int (*cmdHandler)(cmdInput_t *, int))
{
  char ch = '\0';
  cmdLine_t *cmdProgCounter = NULL;
  cmdInput_t cmdInput;
  int retVal = CMD_RET_OK;

  // See if we need to switch to keypress mode. We only need to do this
  // at the top of nested list executions. Switching to keypress mode
  // allows the end-user to interrupt the list execution using a 'q'
  // keypress.
  if (listExecDepth == 0)
    kbModeSet(KB_MODE_SCAN);
  listExecDepth++;

  // Fill in a dummy command input control structure for the command handler
  cmdInput.file = 0;
  cmdInput.readMethod = CMD_INPUT_MANUAL;

  // We start at the root of the linked list. Let's see where we end up.
  cmdProgCounter = cmdLineRoot;
  while (cmdProgCounter != NULL)
  {
    // Echo command
    if (echoCmd == CMD_ECHO_YES)
    {
      emuPrefixEcho();
      printf("%s\n", cmdProgCounter->input);
    }

    // Execute the command
    if (cmdProgCounter->cmdRepeatType == CMD_REPEAT_WHILE)
    {
      // Special command: repeat while
      int doNextLoop = GLCD_FALSE;

      if (cmdProgCounter->cmdRepeat->active == GLCD_FALSE)
      {
        // We're going to start a new repeat loop
        retVal = emuRepeatWhile(cmdProgCounter, &doNextLoop);
        if (retVal == CMD_RET_OK)
        {
          // If the repeat while condition fails continue at the command
          // after the repeat next command
          if (doNextLoop == GLCD_FALSE)
          {
            cmdProgCounter = cmdProgCounter->cmdRepeat->cmdLineRn;
          }
        }
      }
      else
      {
        // We're already using the loop. As the repeat next will take
        // care of continuing or ending the loop there's nothing left
        // for us to do.
        retVal = CMD_RET_OK;
      }
    }
    else if (cmdProgCounter->cmdRepeatType == CMD_REPEAT_NEXT)
    {
      // Special command: repeat next
      int doNextLoop = GLCD_FALSE;

      retVal = emuRepeatNext(cmdProgCounter, &doNextLoop);
      if (retVal == CMD_RET_OK)
      {
        // If the repeat while condition fails there's nothing else to
        // do for us and the program counter should continue after the
        // 'rn' command. If the repeat loop continues set the program
        // counter to continue at the beginning of the 'rw' command.
        if (doNextLoop == GLCD_TRUE)
        {
          // Next loop: continue at the repeat while command
          cmdProgCounter = cmdProgCounter->cmdRepeat->cmdLineRw;
        }
      }
    }
    else
    {
      // Process regular command via the command handler (usually doInput())
      cmdInput.input = cmdProgCounter->input;
      retVal = cmdHandler(&cmdInput, echoCmd);
    }

    // Verify if a command interrupt was requested
    if (retVal == CMD_RET_OK)
    {
      while (kbHit())
      {
        ch = getchar();
        if (ch == 'q' || ch == 'Q')
        {
          printf("quit\n");
          retVal = CMD_RET_INTERRUPT;
          break;
        }
      }
    }

    // Check for errors
    if (retVal == CMD_RET_ERROR || retVal == CMD_RET_INTERRUPT)
    {
      // Error/interrupt occured in current level
      printf("--- stack trace ---\n");
      // Report current stack level and cascade to upper level
      printf("%d:%s:%d:%s\n", fileExecDepth, source, cmdProgCounter->lineNum,
        cmdProgCounter->input);
      retVal = CMD_RET_ERROR_RECOVER;
      break;
    }
    else if (retVal == CMD_RET_ERROR_RECOVER)
    {
      // Error/interrupt occured at lower level.
      // Report current stack level and cascade to upper level.
      printf("%d:%s:%d:%s\n", fileExecDepth, source, cmdProgCounter->lineNum,
        cmdProgCounter->input);
      break;
    }

    // Command executed successfully: move to next in linked list
    cmdProgCounter = cmdProgCounter->next;
  }

  // End of list or encountered error/interrupt.
  // Switch back to line mode when we're back at root level.
  listExecDepth--;
  if (listExecDepth == 0)
    kbModeSet(KB_MODE_LINE);

  return retVal;
}

//
// Function: emuLogfileClose
//
// Close the debug logfile
//
void emuLogfileClose(void)
{
  if (debugFile != NULL)
    fclose(debugFile);
}

//
// Function: emuLogfileOpen
//
// Open the debug logfile
//
void emuLogfileOpen(char fileName[])
{
  if (!DEBUGGING)
  {
    printf("\nWARNING: -d <file> ignored as master debugging is Off.\n");
    printf("Assign value 1 to \"#define DEBUGGING\" in ratt.h [firmware] rebuild mchron.\n");
  }
  else
  {
    debugFile = fopen(fileName, "a");
    if (debugFile == NULL)
    {
      // Something went wrong opening the logfile
      printf("Cannot open debug output file \"%s\".\n", fileName);
    }
    else
    {
      // Disable buffering so we can properly 'tail -f' the file
      setbuf(debugFile, NULL);
      DEBUGP("**** logging started");
    }
  }
}

//
// Function: emuPrefixEcho
//
// Print a prefix string in command echo based on command depth level
//
void emuPrefixEcho(void)
{
  int i;

  for (i = 1; i < fileExecDepth; i++)
    printf(":  ");
}

//
// Function: emuRepeatArgsGet
//
// Get the value of the repeat loop variable and evaluate the repeat end
// and step values
//
int emuRepeatArgsGet(cmdRepeat_t *cmdRepeat, int *valVar, int *valEnd,
  int *valStep)
{
  int varActive = 0;
  int retVal = CMD_RET_OK;

  // Get the value of the repeat loop variable
  retVal = varStateGet(cmdRepeat->var, &varActive);
  if (retVal != CMD_RET_OK)
    return retVal;
  if (varActive == GLCD_FALSE)
  {
    printf("variable not in use: %s\n", cmdRepeat->var);
    return CMD_RET_ERROR;
  }
  varValGet(cmdRepeat->var, valVar);

  // Get the repeat end value 
  retVal = exprEvaluate(cmdRepeat->end);
  if (varError == 1)
  {
    printf("%s? parse error\n", argRepeatWhile[3].argName);
    return CMD_RET_ERROR;
  }
  else if (retVal == 1)
  {
    printf("%s? syntax error\n", argRepeatWhile[3].argName);
    return CMD_RET_ERROR;
  }
  *valEnd = exprValue;

  // Get the repeat step value 
  retVal = exprEvaluate(cmdRepeat->step);
  if (varError == 1)
  {
    printf("%s? parse error\n", argRepeatWhile[5].argName);
    return CMD_RET_ERROR;
  }
  else if (retVal ==1)
  {
    printf("%s? syntax error\n", argRepeatWhile[5].argName);
    return CMD_RET_ERROR;
  }
  *valStep = exprValue;

  return CMD_RET_OK;
}

//
// Function: emuRepeatInit
//
// Initialize a cmdRepeat structure with arguments from a repeat
// while command ('rw')
//
int emuRepeatInit(cmdRepeat_t *cmdRepeat, char *input)
{
  int varActive = GLCD_FALSE;
  int retVal = CMD_RET_OK;

  // Scan the command line for the repeat parameters
  argInit(&input);
  _argScan(argRepeatWhile, &input);

  // Validate repeat var name
  retVal = varStateGet(argWord[1], &varActive);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Copy the repeat variable name
  strcpy(cmdRepeat->var, argWord[1]);

  // Get the repeat while condition
  if (strcmp(argWord[2], ">") == 0)
  {
    cmdRepeat->condition = RU_GT;
  }
  else if (strcmp(argWord[2], "<") == 0)
  {
    cmdRepeat->condition = RU_LT;
  }
  else if (strcmp(argWord[2], "<=") == 0)
  {
    cmdRepeat->condition = RU_LET;
  }
  else if (strcmp(argWord[2], ">=") == 0)
  {
    cmdRepeat->condition = RU_GET;
  }
  else if (strcmp(argWord[2], "<>") == 0)
  {
    cmdRepeat->condition = RU_NEQ;
  }
  else
  {
    printf("%s? invalid value: %s\n", argRepeatWhile[2].argName, argWord[2]);
    return CMD_RET_ERROR;
  }

  // Copy the start value
  cmdRepeat->start = argInt[0];

  // Copy the expressions for the end and step value.
  // Note that a '\n' is added at the end of the expression
  // as per parser requirement.
  argRepeatItemInit(&(cmdRepeat->end), argWord[3]);
  argRepeatItemInit(&(cmdRepeat->step), argWord[4]);

  // Indicate that a full init has been completed
  cmdRepeat->initialized = GLCD_TRUE;

  return CMD_RET_OK;
}

//
// Function: emuRepeatNext
//
// Generate next loop value and determine end-of-loop
//
int emuRepeatNext(cmdLine_t *cmdLine, int *doNextLoop)
{
  cmdRepeat_t *cmdRepeat = cmdLine->cmdRepeat;
  char *input = cmdLine->input;
  int valVar = 0;
  int valEnd = 0;
  int valStep = 0;
  int retVal = CMD_RET_OK;

  // Scan the command line for the repeat parameters
  argInit(&input);
  _argScan(argRepeatNext, &input);

  *doNextLoop = GLCD_TRUE;
  retVal = emuRepeatArgsGet(cmdRepeat, &valVar, &valEnd, &valStep);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Calculate and register the new loop variable value
  valVar = valVar + valStep;
  retVal = varValSet(cmdRepeat->var, valVar);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Verify if the repeat loop while condition has been met
  if ((cmdRepeat->condition == RU_GT && valVar <= valEnd) ||
      (cmdRepeat->condition == RU_LT && valVar >= valEnd) ||
      (cmdRepeat->condition == RU_GET && valVar < valEnd) ||
      (cmdRepeat->condition == RU_LET && valVar > valEnd) ||
      (cmdRepeat->condition == RU_NEQ && valVar == valEnd))
  {
    // The while condition has failed, so end the loop by
    // inactivating the repeat loop
    *doNextLoop = GLCD_FALSE;
    cmdRepeat->active = GLCD_FALSE;
  }

  return CMD_RET_OK;
}

//
// Function: emuRepeatWhile
//
// Initiate a new repeat loop
//
int emuRepeatWhile(cmdLine_t *cmdLine, int *doNextLoop)
{
  cmdRepeat_t *cmdRepeat = cmdLine->cmdRepeat;
  char *input = cmdLine->input;
  int valVar = 0;
  int valEnd = 0;
  int valStep = 0;
  int retVal = CMD_RET_OK;

  // For now init the indicator to continue the repeat loop
  *doNextLoop = GLCD_TRUE;

  // Verify if the repeat structure has already been initialized.
  // A repeat loop is hit multiple times when it is nested in another
  // repeat loop. Once initialized its arguments are static until the
  // repeat list is destroyed.
  if (cmdRepeat->initialized == GLCD_FALSE)
  {
    retVal = emuRepeatInit(cmdRepeat, input);
    if (retVal != CMD_RET_OK)
      return retVal;
  }

  // Set the repeat variable start value
  retVal = varValSet(cmdRepeat->var, cmdRepeat->start);
  if (retVal != CMD_RET_OK)
  {
    printf("%s?: invalid value\n", "start");
    return retVal;
  }

  // We need to verify whether the repeat loop has already ended.
  // For this we need the evaluated values of the repeat arguments.
  retVal = emuRepeatArgsGet(cmdRepeat, &valVar, &valEnd, &valStep);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Verify if the repeat loop while condition has been met
  if ((cmdRepeat->condition == RU_GT && valVar <= valEnd) ||
      (cmdRepeat->condition == RU_LT && valVar >= valEnd) ||
      (cmdRepeat->condition == RU_GET && valVar < valEnd) ||
      (cmdRepeat->condition == RU_LET && valVar > valEnd) ||
      (cmdRepeat->condition == RU_NEQ && valVar == valEnd))
  {
    // The while condition has failed, so signal to end the loop and
    // inactivate it (before it got even started)
    *doNextLoop = GLCD_FALSE;
    cmdRepeat->active = GLCD_FALSE;
  }
  else
  {
    // The while condition has been met. Make the repeat loop active.
    *doNextLoop = GLCD_TRUE;
    cmdRepeat->active = GLCD_TRUE;
  }

  return CMD_RET_OK;
}

//
// Function: emuSigCatch
//
// Signal handler. Mainly used to implement a graceful shutdown so
// we do not need to 'reset' the mchron shell, when it no longer echoes
// input characters due to its keypress mode, or to kill the alarm
// audio PID.
// However, do not close the LCD device (works for ncurses only) since
// we may want to keep the latest screen layout for analytic purposes.
//
void emuSigCatch(int sig, siginfo_t *siginfo, void *context)
{
  //printf ("Signo     -  %d\n",siginfo->si_signo);
  //printf ("SigCode   -  %d\n",siginfo->si_code);

  // For signals we should interpret to make the application quit, switch
  // back to keyboard line mode and kill audio before we actually exit
  if (sig == SIGINT)
  {
    // Keyboard: "^C"
    kbModeSet(KB_MODE_LINE);
    alarmSoundKill();
    printf("\n<ctrl>c - interrupt\n");
    exit(-1);
  }
  else if (sig == SIGTSTP)
  {
    // Keyboard: "^Z"
    kbModeSet(KB_MODE_LINE);
    alarmSoundKill();
    printf("\n<ctrl>z - stop\n");
    exit(-1);
  }
  else if (sig == SIGABRT)
  {
    struct sigaction sigAction;

    kbModeSet(KB_MODE_LINE);
    alarmSoundKill();

    // We must clear the sighandler for SIGABRT or else we'll get an
    // infinite recursive loop due to abort() below that triggers a new
    // SIGABRT that triggers a new (etc)...
    memset(&sigAction, '\0', sizeof(sigaction));
    sigAction.sa_sigaction = NULL;
    sigAction.sa_flags = SA_SIGINFO;
    if (sigaction(SIGABRT, &sigAction, NULL) < 0)
    {
      printf("Cannot clear handler %d\n", SIGABRT);
      printf("Not able to coredump\n");
      exit(-1);
    }

    // Let's abort and optionally coredump. Note that in order to get a
    // coredump it requires to run shell command "ulimit -c unlimited"
    // once in the mchron shell.
    abort();
  }
  else if (sig == SIGQUIT)
  {
    // Keyboard: "^\"
    // Note that abort() below will trigger a SIGABRT that will be handled
    // separately and will take care of a graceful shutdown
    printf("\n<ctrl>\\ - quit\n");
    abort();
  }
  else if (sig == SIGWINCH)
  {
    // Ignore reshape of mchron command line xterm
    return;
  }
}

//
// Function: emuSigSetup
//
// Signal handler setup that attaches a handler to dedicated signal(s).  
//
void emuSigSetup(void)
{
  struct sigaction sigAction;

  // Setup signal handler structure
  memset(&sigAction, '\0', sizeof(sigaction));
  sigAction.sa_sigaction = &emuSigCatch;
  sigAction.sa_flags = SA_SIGINFO;

  if (sigaction(SIGINT, &sigAction, NULL) < 0)
    printf("Cannot set handler %d\n", SIGINT);
  if (sigaction(SIGTSTP, &sigAction, NULL) < 0)
    printf("Cannot set handler %d\n", SIGTSTP);
  if (sigaction(SIGQUIT, &sigAction, NULL) < 0)
    printf("Cannot set handler %d\n", SIGQUIT);
  if (sigaction(SIGABRT, &sigAction, NULL) < 0)
    printf("Cannot set handler %d\n", SIGABRT);
  // For SIGWINCH force to restart system calls, mainly meant for fgets()
  // in main loop (that otherwise will end with EOF)
  sigAction.sa_flags = sigAction.sa_flags | SA_RESTART;
  if (sigaction(SIGWINCH, &sigAction, NULL) < 0)
    printf("Cannot set handler %d\n", SIGWINCH);
}

//
// Function: emuStartModeGet
//
// Get the requested start mode
//
int emuStartModeGet(char startId, int *start)
{
  // Validate start mode
  if (startId == 'c')
  {
    *start = CYCLE_REQ_WAIT;
  }
  else if (startId == 'n')
  {
    *start = CYCLE_REQ_NOWAIT;
  }
  else
  {
    printf("start? invalid value\n");
    return CMD_RET_ERROR;
  }

  return CMD_RET_OK;
}

//
// Function: emuTimePrint
//
// Print the time/dat/alarm
//
void emuTimePrint(void)
{
  emuPrefixEcho();
  printf("time  : %02d:%02d:%02d (hh:mm:ss)\n", time_h, time_m, time_s);
  emuPrefixEcho();
  printf("date  : %02d/%02d/%04d (dd/mm/yyyy)\n", date_d, date_m, date_y + 2000);
  emuPrefixEcho();
  printf("alarm : %02d:%02d (hh:mm)\n", emuAlarmH, emuAlarmM);
  emuPrefixEcho();
  alarmSwitchShow();
}

//
// Function: emuWinClose
//
// Callback function for the LCD device when the end user closes its
// window.
// It is primarily used to implement a graceful shutdown in case the glut
// window is closed.
// When that occurs it will kill both the glut thread and the mchron
// application unless a glut handler is setup to exit gracefully.
// We need such a handler, or else our mchron terminal may need to be
// 'reset' when it no longer echoes characters due to an active keypress
// mode or an unproperly closed readline session. In addition to that,
// audible alarm may keep playing. So, this handler should attempt to exit
// gracefully.
//
void emuWinClose(void)
{
  kbModeSet(KB_MODE_LINE);
  alarmSoundKill();
  cmdInputCleanup(&cmdInput);
  printf("\nlcd device closed - exit\n");
  exit(-1);
}

