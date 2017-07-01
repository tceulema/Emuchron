//*****************************************************************************
// Filename : 'mchronutil.c'
// Title    : Utility routines for emuchron emulator command line tool
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

// Monochron defines
#include "../ks0108.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"

// Dedicated clock defines
#include "../clock/qr.h"

// Emuchron stubs and utilities
#include "stub.h"
#include "listutil.h"
#include "scanutil.h"
#include "varutil.h"
#include "mchronutil.h"

// Monochron defined data
extern volatile rtcDateTime_t rtcDateTime;
extern volatile rtcDateTime_t rtcDateTimeNext;
extern volatile uint8_t rtcTimeEvent;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern clockDriver_t *mcClockPool;

// From the variables that store the processed command line arguments
// we only need the word arguments
extern char *argWord[];

// The command profile for an mchron command
extern cmdArg_t argCmd[];

// Current command file execution depth
extern int fileExecDepth;

// The mchron alarm time
extern uint8_t emuAlarmH;
extern uint8_t emuAlarmM;

// Debug file
extern FILE *stubDebugStream;

// The command line input stream control structure
extern cmdInput_t cmdInput;

// The current command echo state
extern int echoCmd;

// This is me
extern const char *__progname;

// Data resulting from mchron startup command line processing
emuArgcArgv_t emuArgcArgv;

// The command list execution depth that is used to determine to
// actively switch between keyboard line mode and keypress mode
int listExecDepth = 0;

// Flags indicating active state upon exit
int invokeExit = GLCD_FALSE;
static int closeWinMsg = GLCD_FALSE;

// Local function prototypes
static int emuDigitsCheck(char *input);
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context);

//
// Function: emuArgcArgvGet
//
// Process mchron startup command line arguments
//
int emuArgcArgvGet(int argc, char *argv[])
{
  FILE *fp;
  int argCount = 1;
  char *tty = emuArgcArgv.ctrlDeviceArgs.lcdNcurInitArgs.tty;

  // Init references to command line argument positions
  emuArgcArgv.argDebug = 0;
  emuArgcArgv.argGlutGeometry = 0;
  emuArgcArgv.argGlutPosition = 0;
  emuArgcArgv.argTty = 0;
  emuArgcArgv.argLcdType = 0;

  // Init the lcd device data
  emuArgcArgv.ctrlDeviceArgs.useNcurses = GLCD_FALSE;
  emuArgcArgv.ctrlDeviceArgs.useGlut = GLCD_TRUE;
  emuArgcArgv.ctrlDeviceArgs.lcdNcurInitArgs.tty[0] = '\0';
  emuArgcArgv.ctrlDeviceArgs.lcdNcurInitArgs.winClose = emuWinClose;
  emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.posX = 100;
  emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.posY = 100;
  emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.sizeX = 520;
  emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.sizeY = 264;
  emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.winClose = emuWinClose;

  // Do archaic command line processing to obtain the lcd output device(s),
  // lcd output configs and debug logfile
  while (argCount < argc)
  {
    if (strncmp(argv[argCount], "-d", 4) == 0)
    {
      // Debug output file name
      emuArgcArgv.argDebug = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-g", 4) == 0)
    {
      // Glut window geometry
      emuArgcArgv.argGlutGeometry = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-l", 4) == 0)
    {
      // Lcd stub device type
      emuArgcArgv.argLcdType = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-p", 4) == 0)
    {
      // Glut window position
      emuArgcArgv.argGlutPosition = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-t", 4) == 0)
    {
      // Ncurses tty device
      emuArgcArgv.argTty = argCount + 1;
      argCount = argCount + 2;
    }
    else
    {
      // Anything else: force to quit
      emuArgcArgv.argDebug = argc;
      argCount = argc;
    }
  }

  // Check result of command line processing
  if (emuArgcArgv.argLcdType >= argc || emuArgcArgv.argDebug >= argc ||
      emuArgcArgv.argGlutGeometry >= argc || emuArgcArgv.argTty >= argc ||
      emuArgcArgv.argGlutPosition >= argc)
  {
    printf("Use: %s [-l <device>] [-t <tty>] [-g <geometry>] [-p <position>]\n", __progname);
    printf("            [-d <logfile>] [-h]\n");
    printf("  -d <logfile>  - Debug logfile name\n");
    printf("  -g <geometry> - Geometry (x,y) of glut window\n");
    printf("                  Default: \"520x264\"\n");
    printf("                  Examples: \"130x66\" or \"260x132\"\n");
    printf("  -h            - Give usage help\n");
    printf("  -l <device>   - Lcd stub device type\n");
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

  // Validate lcd stub output device
  if (emuArgcArgv.argLcdType > 0)
  {
    if (strcmp(argv[emuArgcArgv.argLcdType], "glut") == 0)
    {
      emuArgcArgv.ctrlDeviceArgs.useGlut = GLCD_TRUE;
      emuArgcArgv.ctrlDeviceArgs.useNcurses = GLCD_FALSE;
    }
    else if (strcmp(argv[emuArgcArgv.argLcdType], "ncurses") == 0)
    {
      emuArgcArgv.ctrlDeviceArgs.useGlut = GLCD_FALSE;
      emuArgcArgv.ctrlDeviceArgs.useNcurses = GLCD_TRUE;
    }
    else if (strcmp(argv[emuArgcArgv.argLcdType], "all") == 0)
    {
      emuArgcArgv.ctrlDeviceArgs.useGlut = GLCD_TRUE;
      emuArgcArgv.ctrlDeviceArgs.useNcurses = GLCD_TRUE;
    }
    else
    {
      printf("%s: -l: Unknown lcd stub device type: %s\n", __progname,
        argv[emuArgcArgv.argLcdType]);
      return CMD_RET_ERROR;
    }
  }

  // Validate glut window geometry
  if (emuArgcArgv.argGlutGeometry > 0)
  {
    char *input;
    char *separator;

    input = malloc(strlen(argv[emuArgcArgv.argGlutGeometry]) + 1);
    strcpy(input, argv[emuArgcArgv.argGlutGeometry]);

    // Must find a 'x' separator
    separator = strchr(input, 'x');
    if (separator == NULL)
    {
      printf("%s: -g: Invalid format glut geometry\n", __progname);
      free(input);
      return CMD_RET_ERROR;
    }

    // Must find non-zero length strings and digit characters only
    *separator = '\0';
    if (emuDigitsCheck(input) == GLCD_FALSE ||
        emuDigitsCheck(separator + 1) == GLCD_FALSE)
    {
      printf("%s: -g: Invalid format glut geometry\n", __progname);
      free(input);
      return CMD_RET_ERROR;
    }
    emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.sizeX = atoi(input);
    emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.sizeY = atoi(separator + 1);
    free(input);
  }

  // Validate glut window position
  if (emuArgcArgv.argGlutPosition > 0)
  {
    char *input;
    char *separator;

    input = malloc(strlen(argv[emuArgcArgv.argGlutPosition]) + 1);
    strcpy(input, argv[emuArgcArgv.argGlutPosition]);

    // Must find a ',' separator
    separator = strchr(input, ',');
    if (separator == NULL)
    {
      printf("%s: -p: Invalid format glut position\n", __progname);
      free(input);
      return CMD_RET_ERROR;
    }

    // Must find non-zero length strings and digit characters only
    *separator = '\0';
    if (emuDigitsCheck(input) == GLCD_FALSE ||
        emuDigitsCheck(separator + 1) == GLCD_FALSE)
    {
      printf("%s: -p: Invalid format glut position\n", __progname);
      free(input);
      return CMD_RET_ERROR;
    }
    emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.posX = atoi(input);
    emuArgcArgv.ctrlDeviceArgs.lcdGlutInitArgs.posY = atoi(separator + 1);
    free(input);
  }

  // Get the ncurses output device
  if (emuArgcArgv.argTty != 0)
  {
    // Got it from the command line
    strcpy(tty, argv[emuArgcArgv.argTty]);
  }
  else if (emuArgcArgv.ctrlDeviceArgs.useNcurses == 1)
  {
    // Get the tty device if not specified on the command line
    char *home;
    char *fullPath;
    int ttyLen = 0;

    // Get the full path to $HOME/.mchron
    home = getenv("HOME");
    if (home == NULL)
    {
      printf("%s: Cannot get $HOME\n", __progname);
      printf("Use switch '-t <tty>' to set lcd output device\n");
      return CMD_RET_ERROR;
    }
    fullPath = malloc(strlen(home) + strlen(NCURSES_TTYFILE) + 1);
    sprintf(fullPath,"%s%s", home, NCURSES_TTYFILE);

    // Open the file with the tty device
    fp = fopen(fullPath, "r");
    free(fullPath);
    if (fp == NULL)
    {
      printf("%s: Cannot open file \"%s%s\".\n", __progname, "$HOME",
        NCURSES_TTYFILE);
      printf("Start a new Monochron ncurses terminal or use switch '-t <tty>' to set\n");
      printf("mchron ncurses terminal tty\n");
      return CMD_RET_ERROR;
    }

    // Read output device in first line. It has a fixed max length.
    fgets(tty, NCURSES_TTYLEN, fp);

    // Kill all \r or \n in the tty string as ncurses doesn't like this.
    // Assume that \r and \n are trailing characters in the string.
    for (ttyLen = strlen(tty); ttyLen > 0; ttyLen--)
    {
      if (tty[ttyLen - 1] == '\n' || tty[ttyLen - 1] == '\r')
        tty[ttyLen - 1] = '\0';
    }

    // Clean up our stuff
    fclose(fp);
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
  mcClockOldTS = mcClockOldTM = mcClockOldTH = 0;
  mcClockOldDD = mcClockOldDM = mcClockOldDY = 0;
  if (mcClockPool[mcMchronClock].clockId != CHRON_NONE &&
      echoCmd == CMD_ECHO_YES)
    printf("released clock\n");
  mcMchronClock = 0;

  // Kill alarm (if sounding anyway) and reset it
  alarmSoundStop();
  alarmSoundReset();
}

//
// Function: emuClockUpdate
//
// Most clocks update their layout in a single clock cycle.
// However, consider the QR clock. This clock requires multiple clock cycles
// to update its layout due to the above average computing power needed to
// do so.
// For specific clocks, like the QR clock, this function generates multiple
// clock cycles, enough to update its layout.
// By default for any other clock, only a single clock cycle is generated,
// which should suffice.
//
void emuClockUpdate(void)
{
  // Nothing to be done when no clock is active
  if (mcClockPool[mcMchronClock].clockId == CHRON_NONE)
    return;

  // We have specific draw requirements for the QR clock
  if (mcClockPool[mcMchronClock].clockId == CHRON_QR_HM ||
      mcClockPool[mcMchronClock].clockId == CHRON_QR_HMS)
  {
    // Generate the clock cycles needed to display a new QR
    int i;

    for (i = 0; i < QR_GEN_CYCLES; i++)
      animClockDraw(DRAW_CYCLE);
  }
  else
  {
    // For a clock by default a single clock cycle is needed
    // to update its layout
    animClockDraw(DRAW_CYCLE);
  }

  // Update clock layout
  ctrlLcdFlush();
  rtcTimeEvent = GLCD_FALSE;
}

//
// Function: emuColorGet
//
// Get the requested color
//
u08 emuColorGet(char colorId)
{
  // Validate color
  if (colorId == 'b')
    return mcBgColor;
  else // colorId == 'f'
    return mcFgColor;
}

//
// Function: emuCoreDump
//
// There's something terribly wrong in Emuchron. It may be caused by bad
// (functional clock) code, a bad mchron command line request or a bug in
// the Emuchron emulator.
// Provide some feedback and generate a coredump file (when enabled).
// Note: A graceful environment shutdown is taken care of by the SIGABRT
// signal handler, invoked by abort().
// Note: In order to get a coredump file it requires to run shell command
// "ulimit -c unlimited" once in the mchron shell.
//
void emuCoreDump(u08 origin, const char *location, int arg1, int arg2,
  int arg3, int arg4)
{
  if (origin == ORIGIN_GLCD)
  {
    // Error in the glcd interface
    // Note: y = vertical lcd byte location (0..7)
    printf("\n*** invalid graphics api request in %s()\n", location);
    printf("api info (controller:x:y:data) = (%d:%d:%d:%d)\n",
      arg1, arg2, arg3, arg4);
  }
  else if (origin == ORIGIN_CTRL)
  {
    // Error in the controller interface
    printf("\n*** invalid controller api request in %s()\n", location);
    printf("api info (method/data)= %d\n", arg1);
  }
  else if (origin == ORIGIN_EEPROM)
  {
    // Error in the eeprom interface
    printf("\n*** invalid eeprom api request in %s()\n", location);
    printf("api info (address)= %d\n", arg1);
  }

  // Dump all Monochron variables. Might be useful.
  printf("*** registered variables\n");
  varPrint("", "*");

  // Stating the obvious
  printf("*** debug by loading coredump file (when created) in a debugger\n");

  // Switch back to regular keyboard input mode and kill audible sound (if any)
  kbModeSet(KB_MODE_LINE);
  alarmSoundStop();

  // Depending on the lcd device(s) used we'll see the latest image or not.
  // In case we're using ncurses, regardless with or without glut, flush the
  // screen. After aborting mchron, the ncurses image will be retained.
  // In case we're only using the glut device, give end-user the means to have
  // a look at the glut device to get a clue what's going on before its display
  // is killed by the application that is being aborted. Note that at this
  // point glut is still running in its own thread and will have its layout
  // constantly refreshed. This allows a glut screendump to be made if needed.
  if (emuArgcArgv.ctrlDeviceArgs.useNcurses == GLCD_TRUE)
  {
    // Flush the ncurses device so we get its contents as-is at the time of
    // the forced coredump
    ctrlLcdFlush();
  }
  else // emuArgcArgv.lcdDeviceParam.useGlut == GLCD_TRUE
  {
    // Have end-user confirm abort, allowing a screendump to be made prior to
    // actual coredump
    waitKeypress(GLCD_FALSE);
  }

  // Cleanup command line read interface, forcing the readline history to be
  // flushed in the history file
  cmdInputCleanup(&cmdInput);

  // Force coredump
  abort();
}

//
// Function: emuDigitsCheck
//
// Verifies whether a string is non-empty and contains only digit characters
//
static int emuDigitsCheck(char *input)
{
  if (*input == '\0')
    return GLCD_FALSE;

  while (*input != '\0')
  {
    if (isdigit(*input) == 0)
      return GLCD_FALSE;
    input++;
  }

  return GLCD_TRUE;
}

//
// Function: emuFontGet
//
// Get the requested font
//
u08 emuFontGet(char *fontName)
{
  // Get font
  if (strcmp(fontName, "5x5p") == 0)
    return FONT_5X5P;
  else // fontName == "5x7n")
    return FONT_5X7N;
}

//
// Function: emuLineExecute
//
// Process a single line input string.
// This is the main command input handler that can be called recursively
// via the 'e' command.
//
int emuLineExecute(cmdLine_t *cmdLine, cmdInput_t *cmdInput)
{
  char *input = cmdLine->input;
  cmdCommand_t *cmdCommand;
  int retVal = CMD_RET_OK;

  // See if we have an empty command line
  if (*input == '\0')
  {
    // Dump newline in the log only when we run at root command level
    if (listExecDepth == 0)
      DEBUGP("");
    return retVal;
  }

  // Prepare command line argument scanner
  cmdArgInit(&input);
  if (*input == '\0')
  {
    // Input contains only white space chars
    return retVal;
  }

  // We have non-whitespace characters so scan the command line with
  // the command profile
  retVal = cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
  if (retVal != CMD_RET_OK)
    return retVal;

  // In case this function is called from a list execution we already have
  // a reference to the command dictionary. If it is called from the command
  // line we need to get it ourselves.
  if (cmdLine->cmdCommand == NULL)
  {
    retVal = cmdDictCmdGet(argWord[0], &cmdCommand);
    if (retVal != CMD_RET_OK)
    {
      printf("%s? unknown: %s\n", argCmd[0].argName, argWord[0]);
      return retVal;
    }
    cmdLine->cmdCommand = cmdCommand;
  }
  else
  {
    cmdCommand = cmdLine->cmdCommand;
  }

  // See what type of command we're dealing with
  if (cmdCommand->cmdPcCtrlType == PC_CONTINUE)
  {
    // Single line command.
    // Scan the command profile for the command using the input string.
    retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount,
      &input, GLCD_FALSE);
    if (retVal != CMD_RET_OK)
      return retVal;

    // Execute the command handler for the command
    retVal = (*cmdCommand->cmdHandler)(cmdLine);
  }
  else if (cmdCommand->cmdPcCtrlType == PC_REPEAT_FOR ||
           cmdCommand->cmdPcCtrlType == PC_IF)
  {
    // The user has entered a program control block start command at
    // command prompt level.
    // Cache all keyboard input commands until this command is matched
    // with a corresponding end command. Then execute the command list.
    cmdLine_t *cmdLineRoot = NULL;
    cmdPcCtrl_t *cmdPcCtrlRoot = NULL;

    // Load keyboard command lines in a linked list.
    // Warning: this will reset the cmd scan global variables.
    retVal = cmdListKeyboardLoad(&cmdLineRoot, &cmdPcCtrlRoot, cmdInput,
      fileExecDepth);
    if (retVal == CMD_RET_OK)
    {
      // Execute the commands in the command list
      int myEchoCmd = echoCmd;
      echoCmd = CMD_ECHO_NO;
      retVal = emuListExecute(cmdLineRoot, (char *)__progname);
      echoCmd = myEchoCmd;
    }

    // We're done. Either all commands in the linked list have been
    // executed successfullly or an error has occured. Do admin stuff
    // by cleaning up the linked lists.
    cmdListCleanup(cmdLineRoot, cmdPcCtrlRoot);
  }
  else
  {
    // All other control block types are invalid on the command line
    // as they need to link to either a repeat-for or if-then command.
    // As such, they can only be entered via multi line keyboard input
    // or a command file.
    printf("%s? cannot match this command: %s\n", argCmd[0].argName,
      argWord[0]);
    retVal = CMD_RET_ERROR;
  }

  return retVal;
}

//
// Function: emuListExecute
//
// Execute the commands in the program list as loaded via a file or
// interactively on the command line.
// We have all command lines and control blocks available in linked lists.
// The control block list has been checked on its integrity, meaning that
// all pointers between command lines and control blocks are present and
// validated so we don't need to worry about that.
// Start at the top of the list and work our way down until we find a
// runtime error or end up at the end of the list.
// We may encounter program control blocks that will influence the program
// counter by creating loops or jumps in the execution plan of the list.
//
int emuListExecute(cmdLine_t *cmdLineRoot, char *source)
{
  char ch = '\0';
  cmdLine_t *cmdProgCounter = NULL;
  int cmdPcCtrlType;
  int i;
  int retVal = CMD_RET_OK;

  // See if we need to switch to keypress mode. We only need to do this
  // at the top of nested list executions. Switching to keypress mode
  // allows the end-user to interrupt the list execution using a 'q'
  // keypress.
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
    if (cmdProgCounter->cmdCommand == NULL)
    {
      // Skip a blank command line
      cmdPcCtrlType = PC_CONTINUE;
      retVal = CMD_RET_OK;
    }
    else if (cmdProgCounter->cmdCommand->cmdPcCtrlType == PC_CONTINUE)
    {
      // Process regular command via the generic input handler
      cmdPcCtrlType = PC_CONTINUE;
      retVal = emuLineExecute(cmdProgCounter, NULL);
    }
    else
    {
      // This is a control block command from a repeat or if construct.
      // Execute the associated program counter control block handler
      // from the command dictionary.
      cmdPcCtrlType = cmdProgCounter->cmdCommand->cmdPcCtrlType;
      retVal = (*cmdProgCounter->cmdCommand->cbHandler)(&cmdProgCounter);
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

    // Move to next command in linked list. Note that after processing
    // a program control type block the program counter already points
    // to the appropriate next line to process.
    if (cmdPcCtrlType == PC_CONTINUE)
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
  if (stubDebugStream != NULL)
    fclose(stubDebugStream);
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
    printf("WARNING: -d <file> ignored as master debugging is Off.\n");
    printf("Assign value 1 to \"#define DEBUGGING\" in monomain.h [firmware] and rebuild\n");
    printf("mchron.\n\n");
  }
  else
  {
    stubDebugStream = fopen(fileName, "a");
    if (stubDebugStream == NULL)
    {
      // Something went wrong opening the logfile
      printf("Cannot open debug output file \"%s\".\n", fileName);
    }
    else
    {
      // Disable buffering so we can properly 'tail -f' the file
      setbuf(stubDebugStream, NULL);
      DEBUGP("**** logging started");
    }
  }
}

//
// Function: emuOrientationGet
//
// Get the requested text orientation
//
u08 emuOrientationGet(char orientationId)
{
  // Get orientation
  if (orientationId == 'b')
    return ORI_VERTICAL_BU;
  else if (orientationId == 'h')
    return ORI_HORIZONTAL;
  else // orientationId == 't'
    return ORI_VERTICAL_TD;
}

//
// Function: emuSigCatch
//
// Signal handler. Mainly used to implement a graceful shutdown so
// we do not need to 'reset' the mchron shell, when it no longer echoes
// input characters due to its keypress mode, or to kill the alarm
// audio PID.
// However, do not close the lcd device (works for ncurses only) since
// we may want to keep the latest screen layout for analytic purposes.
//
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context)
{
  //printf ("Signo     -  %d\n",siginfo->si_signo);
  //printf ("SigCode   -  %d\n",siginfo->si_code);

  // For signals that should make the application quit, switch back to
  // keyboard line mode and kill audio before we actually exit
  if (sig == SIGINT)
  {
    // Keyboard: "^C"
    kbModeSet(KB_MODE_LINE);
    alarmSoundStop();
    invokeExit = GLCD_TRUE;
    printf("\n<ctrl>c - interrupt\n");
    exit(-1);
  }
  else if (sig == SIGTSTP)
  {
    // Keyboard: "^Z"
    kbModeSet(KB_MODE_LINE);
    alarmSoundStop();
    invokeExit = GLCD_TRUE;
    printf("\n<ctrl>z - stop\n");
    exit(-1);
  }
  else if (sig == SIGABRT)
  {
    struct sigaction sigAction;

    kbModeSet(KB_MODE_LINE);
    alarmSoundStop();
    invokeExit = GLCD_TRUE;

    // We must clear the sighandler for SIGABRT or else we'll get an
    // infinite recursive loop due to abort() below that triggers a new
    // SIGABRT that triggers a new (etc)...
    memset(&sigAction, '\0', sizeof(sigaction));
    sigAction.sa_sigaction = NULL;
    sigAction.sa_flags = SA_SIGINFO;
    if (sigaction(SIGABRT, &sigAction, NULL) < 0)
    {
      printf("Cannot clear handler SIGABRT (%d)\n", SIGABRT);
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
    printf("Cannot set handler SIGINT (%d)\n", SIGINT);
  if (sigaction(SIGTSTP, &sigAction, NULL) < 0)
    printf("Cannot set handler SIGTSTP (%d)\n", SIGTSTP);
  if (sigaction(SIGQUIT, &sigAction, NULL) < 0)
    printf("Cannot set handler SIGQUIT (%d)\n", SIGQUIT);
  if (sigaction(SIGABRT, &sigAction, NULL) < 0)
    printf("Cannot set handler SIGABRT (%d)\n", SIGABRT);
  // For SIGWINCH force to restart system calls, mainly meant for fgets()
  // in main loop (that otherwise will end with EOF)
  sigAction.sa_flags = sigAction.sa_flags | SA_RESTART;
  if (sigaction(SIGWINCH, &sigAction, NULL) < 0)
    printf("Cannot set handler SIGWINCH (%d)\n", SIGWINCH);
}

//
// Function: emuStartModeGet
//
// Get the requested start mode
//
int emuStartModeGet(char startId)
{
  if (startId == 'c')
    return CYCLE_REQ_WAIT;
  else // startId == 'n'
    return CYCLE_REQ_NOWAIT;
}

//
// Function: emuTimePrint
//
// Print the time/date/alarm
//
void emuTimePrint(int type)
{
  printf("time   : %02d:%02d:%02d (hh:mm:ss)\n", rtcDateTime.timeHour,
    rtcDateTime.timeMin, rtcDateTime.timeSec);
  printf("date   : %02d/%02d/%04d (dd/mm/yyyy)\n", rtcDateTime.dateDay,
    rtcDateTime.dateMon, rtcDateTime.dateYear + 2000);
  if (type == ALM_EMUCHRON)
    printf("alarm  : %02d:%02d (hh:mm)\n", emuAlarmH, emuAlarmM);
  else
    printf("alarm  : %02d:%02d (hh:mm)\n", mcAlarmH, mcAlarmM);
  alarmSwitchShow();
}

//
// Function: emuTimeSync
//
// Sync functional Emuchron time with internal Emuchron system time
//
void emuTimeSync(void)
{
  rtcDateTimeNext.timeSec = 60;
  DEBUGP("Clear time event");
  rtcTimeEvent = GLCD_FALSE;
  rtcMchronTimeInit();
}

//
// Function: emuWinClose
//
// Callback function for the lcd device when the end user closes its
// window.
// It is primarily used to implement a graceful shutdown in case the glut
// window is closed.
// When that occurs it will kill both the glut thread and the mchron
// application unless a glut handler is setup to exit gracefully.
// We need such a handler, or else our mchron terminal may need to be
// 'reset' when it no longer echoes characters due to an active keypress
// mode or an incorrectly closed readline session. In addition to that,
// audible alarm may keep playing. So, this handler should attempt to exit
// gracefully.
//
void emuWinClose(void)
{
  kbModeSet(KB_MODE_LINE);
  alarmSoundStop();
  cmdInputCleanup(&cmdInput);
  if (invokeExit == GLCD_FALSE)
  {
    if (closeWinMsg == GLCD_FALSE)
    {
      closeWinMsg = GLCD_TRUE;
      printf("\nlcd device closed - exit\n");
    }
  }
  exit(-1);
}

