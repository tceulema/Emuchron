//*****************************************************************************
// Filename : 'mchron.c'
// Title    : Main entry and command line utility for the MONOCHRON emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

// Monochron defines
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "../monomain.h"
#include "../glcd.h"
#include "../anim.h"

// Monochron clocks
#include "../clock/analog.h"
#include "../clock/bigdigit.h"
#include "../clock/cascade.h"
#include "../clock/digital.h"
#include "../clock/nerd.h"
#include "../clock/mosquito.h"
#include "../clock/perftest.h"
#include "../clock/pong.h"
#include "../clock/puzzle.h"
#include "../clock/qr.h"
#include "../clock/slider.h"
#include "../clock/speeddial.h"
#include "../clock/spiderplot.h"
#include "../clock/trafficlight.h"

// Emuchron stubs and utilities
#include "stub.h"
#include "expr.h"
#include "lcd.h"
#include "listvarutil.h"
#include "mchronutil.h"
#include "scanutil.h"
#include "mchron.h"

// Convert a double to other type with decent nearest whole number rounding.
// Use these macros to convert a scanned numeric command argument (that is
// of type double) into a type fit for post-processing in a command handler. 
#define TO_INT(d)	((int)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_U08(d)	((u08)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_UINT8_T(d)	((uint8_t)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_UINT16_T(d)	((uint16_t)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))

// Monochron defined data
extern volatile uint8_t time_event;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern clockDriver_t *mcClockPool;

// The variables that store the processed command line arguments
extern char argChar[];
extern double argDouble[];
extern char *argWord[];
extern char *argString;

// The command profile for an mchron command
extern cmdArg_t argCmd[];

// External data from expression evaluator
extern double exprValue;

// The command list execution depth tells us if we're running at
// command prompt level
extern int listExecDepth;

#ifdef MARIO
// Mario chiptune alarm sanity check data
extern const int marioTonesLen;
extern const int marioBeatsLen;
#endif

// Data resulting from mchron startup command line processing
extern emuArgcArgv_t emuArgcArgv;

// Flag to indicate we're going to exit
extern int invokeExit;

// This is me
extern const char *__progname;

// The command line input stream control structure
cmdInput_t cmdInput;

// The current command echo state
int echoCmd = CMD_ECHO_YES;

// Initial user definable mchron alarm time
uint8_t emuAlarmH = 22;
uint8_t emuAlarmM = 9;

// Current command file execution depth
int fileExecDepth = 0;

// The emulator background/foreground color of the lcd display and backlight
// OFF = 0 = black color (=0x0 bit value in lcd memory)
// ON  = 1 = white color (=0x1 bit value in lcd memory)
static u08 emuBgColor = OFF;
static u08 emuFgColor = ON;
static u08 emuBacklight = 16;

// The clocks supported in the mchron clock test environment.
// Note that Monochron will have its own implemented array of supported
// clocks in anim.c [firmware]. So, we need to switch between the two
// arrays when appropriate.
static clockDriver_t emuMonochron[] =
{
  {CHRON_NONE,        DRAW_INIT_NONE, 0,                  0,                   0},
  {CHRON_ANALOG_HMS,  DRAW_INIT_FULL, analogHmsInit,      analogCycle,         0},
  {CHRON_ANALOG_HM,   DRAW_INIT_FULL, analogHmInit,       analogCycle,         0},
  {CHRON_DIGITAL_HMS, DRAW_INIT_FULL, digitalHmsInit,     digitalCycle,        0},
  {CHRON_DIGITAL_HM,  DRAW_INIT_FULL, digitalHmInit,      digitalCycle,        0},
  {CHRON_MOSQUITO,    DRAW_INIT_FULL, mosquitoInit,       mosquitoCycle,       0},
  {CHRON_NERD,        DRAW_INIT_FULL, nerdInit,           nerdCycle,           0},
  {CHRON_PONG,        DRAW_INIT_FULL, pongInit,           pongCycle,           pongButton},
  {CHRON_PUZZLE,      DRAW_INIT_FULL, puzzleInit,         puzzleCycle,         puzzleButton},
  {CHRON_SLIDER,      DRAW_INIT_FULL, sliderInit,         sliderCycle,         0},
  {CHRON_CASCADE,     DRAW_INIT_FULL, spotCascadeInit,    spotCascadeCycle,    0},
  {CHRON_SPEEDDIAL,   DRAW_INIT_FULL, spotSpeedDialInit,  spotSpeedDialCycle,  0},
  {CHRON_SPIDERPLOT,  DRAW_INIT_FULL, spotSpiderPlotInit, spotSpiderPlotCycle, 0},
  {CHRON_TRAFLIGHT,   DRAW_INIT_FULL, spotTrafLightInit,  spotTrafLightCycle,  0},
  {CHRON_BIGDIG_ONE,  DRAW_INIT_FULL, bigdigInit,         bigdigCycle,         bigdigButton},
  {CHRON_BIGDIG_TWO,  DRAW_INIT_FULL, bigdigInit,         bigdigCycle,         bigdigButton},
  {CHRON_QR_HMS,      DRAW_INIT_FULL, qrInit,             qrCycle,             0},
  {CHRON_QR_HM,       DRAW_INIT_FULL, qrInit,             qrCycle,             0},
  {CHRON_PERFTEST,    DRAW_INIT_FULL, perfInit,           perfCycle,           0}
};
int emuMonochronCount = sizeof(emuMonochron) / sizeof(clockDriver_t);

//
// Function: main
//
// Main program for Emuchron command shell
//
int main(int argc, char *argv[])
{
  cmdLine_t cmdLine;
  char *prompt;
  int retVal = CMD_RET_OK;

  // Setup signal handlers to either recover from signal or to attempt
  // graceful non-standard exit
  emuSigSetup();

  // Do command line processing and setup the lcd device parameters
  retVal = emuArgcArgvGet(argc, argv);
  if (retVal != CMD_RET_OK)
    return CMD_RET_ERROR;

#ifdef MARIO
  // Do sanity check on Mario data
  if (marioTonesLen != marioBeatsLen)
  {
    printf("Error: Mario alarm - Tone and beat array sizes not aligned: %d vs %d\n",
      marioTonesLen, marioBeatsLen);
    return CMD_RET_ERROR;
  }
#endif

  // Welcome in mchron
  printf("\n*** Welcome to Emuchron command line tool (build %s, %s) ***\n",
    __DATE__, __TIME__);

  // Force first time init Monochron eeprom
  stubEepromReset();
  init_eeprom();

  // Init the lcd color modes
  mcBgColor = emuBgColor;
  mcFgColor = emuFgColor;

  // Init initial alarm
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;

  // Init the lcd emulator device(s)
  lcdDeviceInit(emuArgcArgv.lcdDeviceParam);

  // Uncomment this if you want to join with debugger prior to using
  // anything in the glcd library for the lcd device
  //char tmpInput[10];
  //fgets(tmpInput, 10, stdin);

  // Clear and show welcome message on lcd device
  beep(4000, 100);
  lcdDeviceBacklightSet(emuBacklight);
  glcdInit(mcBgColor);
  glcdPutStr2(1, 1, FONT_5X5P, "* Welcome to Emuchron Emulator *", mcFgColor);
  glcdPutStr2(1, 8, FONT_5X5P, "Enter 'h' for help", mcFgColor);
  lcdDeviceFlush(0);

  // Open debug logfile when requested
  if (emuArgcArgv.argDebug != 0)
    emuLogfileOpen(argv[emuArgcArgv.argDebug]);

  // Show our process id and (optional) ncurses output device
  // (handy for attaching a debugger :-)
  printf("\n%s PID = %d\n", __progname, getpid());
  if (emuArgcArgv.lcdDeviceParam.useNcurses == 1)
    printf("ncurses tty = %s\n", emuArgcArgv.lcdDeviceParam.lcdNcurTty);
  printf("\n");
  
  // Init the clock pool supported in mchron command line mode
  mcClockPool = emuMonochron;

  // Init the stubbed alarm switch to 'Off' and clear audible alarm
  alarmSwitchSet(GLCD_FALSE, GLCD_FALSE);
  alarmSoundKill();

  // Init emuchron system clock and report time+date+alarm
  readi2ctime();
  emuTimePrint();

  // Init functional clock plugin time
  mchronTimeInit();

  // Initialize mchron named variable buckets
  varInit();

  // Init the command line input interface
  cmdInput.file = stdin;
  if (emuArgcArgv.lcdDeviceParam.useNcurses == 0)
  {
    // No interference between readline library and ncurses
    cmdInput.readMethod = CMD_INPUT_READLINELIB;
  }
  else
  {
    // When using ncurses we cannot use the readline library
    cmdInput.readMethod = CMD_INPUT_MANUAL;
  }
  cmdInputInit(&cmdInput);

  // All initialization is done!
  printf("\nEnter 'h' for help.\n");

  // We're in business: give prompt and process keyboard commands until
  // the last proton in the universe has desintegrated (or use 'x' or
  // ^D to exit)

  // Initialize a command line for the interpreter
  cmdLine.lineNum = 0;
  cmdLine.input = NULL;
  cmdLine.cmdCommand = NULL;
  cmdLine.cmdPcCtrlParent = NULL;
  cmdLine.cmdPcCtrlChild = NULL;
  cmdLine.next = NULL;

  // Do the first command line read
  prompt = malloc(strlen(__progname) + 3);
  sprintf(prompt, "%s> ", __progname);
  cmdInputRead(prompt, &cmdInput);

  // Keep processing input lines until done
  while (cmdInput.input != NULL)
  {
    // Process input
    cmdLine.lineNum++;
    cmdLine.input = cmdInput.input;
    cmdLine.cmdCommand = NULL;
    retVal = emuLineExecute(&cmdLine, &cmdInput);
    if (retVal == CMD_RET_EXIT)
      break;

    // Get next command
    cmdInputRead(prompt, &cmdInput);
  }

  // Done: caused by 'x' or ^D

  // Cleanup command line read interface
  free(prompt);
  cmdInputCleanup(&cmdInput);

  // Shutdown gracefully by killing audio and stopping the lcd device(s)
  alarmSoundKill();
  lcdDeviceEnd();
  
  // Stop debug output 
  DEBUGP("**** logging stopped");
  emuLogfileClose();

  // Tell user if exit was due to manual EOF
  if (retVal != CMD_RET_EXIT)
    printf("\n<ctrl>d - exit\n");

  // Goodbye
  return CMD_RET_OK;
}

//
// Below are all mchron command and control block handlers functions. Each and
// every mchron command will, when provided with proper arguments, end up in
// one of the functions below.
//
// Upon entering a command handler function for a 'regular' command, all its
// arguments have been successfully scanned and evaluated. All it takes for
// the handler is to pick up and process the evaluated arguments in the
// argChar[], argDouble[], argWord[] and argString variables, based on the
// sequence of command arguments in the command dictionary.
//
// A control block handler however must implement more functionality as command
// arguments are evaluated optionally, depending on the control block type and
// the execution state of the associated block. As such, a control block
// handler is responsible for its own argument scanning and processing.
//

//
// Function: doAlarmPos
//
// Set alarm switch position
//
int doAlarmPos(cmdLine_t *cmdLine)
{
  uint8_t on;
  uint8_t newPosition = TO_UINT8_T(argDouble[0]);

  // Define new alarm switch position
  if (newPosition == 1)
    on = GLCD_TRUE;
  else
    on = GLCD_FALSE;

  // Set alarm switch position
  alarmSwitchSet(on, GLCD_FALSE);

  // Update clock when active
  if (mcClockPool[mcMchronClock].clockId != CHRON_NONE)
  {
    alarmStateSet();
    animClockDraw(DRAW_CYCLE);
    lcdDeviceFlush(0);
  }

  // Report the new alarm settings
  if (echoCmd == CMD_ECHO_YES)
  {
    // Report current time+date+alarm
    readi2ctime();
    emuTimePrint();
  }

  return CMD_RET_OK;
}

//
// Function: doAlarmSet
//
// Set clock alarm time
//
int doAlarmSet(cmdLine_t *cmdLine)
{
  // Set new alarm time
  emuAlarmH = TO_UINT8_T(argDouble[0]);
  emuAlarmM = TO_UINT8_T(argDouble[1]);
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;

  // Update clock when active
  if (mcClockPool[mcMchronClock].clockId != CHRON_NONE)
  {
    if (mcAlarmSwitch == ALARM_SWITCH_ON &&
        (mcClockPool[mcMchronClock].clockId == CHRON_ANALOG_HM ||
         mcClockPool[mcMchronClock].clockId == CHRON_ANALOG_HMS ||
         mcClockPool[mcMchronClock].clockId == CHRON_SLIDER))
    {
      // Normally the alarm can only be set via the config menu, so the new
      // alarm time will be displayed when the clock is initialized after
      // exiting the config menu. We therefore don't care what the old
      // value was. This behavior will not cause a problem for most clocks
      // when we are in command mode and change the alarm: the alarm time
      // will overwrite the old value on the lcd. However, for a clock like
      // Analog that shows the alarm in an analog clock style, changing the
      // alarm in command mode will draw the new alarm while not erasing
      // the old alarm time.
      // We'll use a trick to overwrite the old alarm: toggle the alarm
      // switch twice. Note: This may cause a slight blink in the alarm
      // area when using the glut lcd device.
      alarmSwitchToggle(GLCD_FALSE);
      alarmStateSet();
      animClockDraw(DRAW_CYCLE);
      alarmSwitchToggle(GLCD_FALSE);
      alarmStateSet();
      animClockDraw(DRAW_CYCLE);
    }
    else
    {
      mcAlarmSwitch = ALARM_SWITCH_NONE;
      animClockDraw(DRAW_CYCLE);
    }
    lcdDeviceFlush(0);
  }

  // Report the new alarm settings
  if (echoCmd == CMD_ECHO_YES)
  {
    // Report current time+date+alarm
    readi2ctime();
    emuTimePrint();
  }

  return CMD_RET_OK;
}

//
// Function: doAlarmToggle
//
// Toggle alarm switch position
//
int doAlarmToggle(cmdLine_t *cmdLine)
{
  // Toggle alarm switch position
  alarmSwitchToggle(GLCD_FALSE);

  // Update clock when active
  if (mcClockPool[mcMchronClock].clockId != CHRON_NONE)
  {
    alarmStateSet();
    animClockDraw(DRAW_CYCLE);
    lcdDeviceFlush(0);
  }

  // Report the new alarm settings
  if (echoCmd == CMD_ECHO_YES)
  {
    // Report current time+date+alarm
    readi2ctime();
    emuTimePrint();
  }

  return CMD_RET_OK;
}

//
// Function: doBeep
//
// Give audible beep
//
int doBeep(cmdLine_t *cmdLine)
{
  // Sound beep
  beep(TO_UINT16_T(argDouble[0]), TO_UINT8_T(argDouble[1]));

  return CMD_RET_OK;
}

//
// Function: doClockFeed
//
// Feed clock with time and keyboard events
//
int doClockFeed(cmdLine_t *cmdLine)
{
  char ch = '\0';
  int startMode = CYCLE_NOWAIT;
  int myKbMode = KB_MODE_LINE;

  // Get the start mode
  startMode = emuStartModeGet(argChar[0]);

  // Check clock
  if (mcClockPool[mcMchronClock].clockId == CHRON_NONE)
  {
    printf("no clock is selected\n");
    return CMD_RET_ERROR;
  }

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Init alarm and functional clock time
  mcAlarming = GLCD_FALSE;
  mchronTimeInit();

  // Init stub event handler used in main loop below
  stubEventInit(startMode, stubHelpClockFeed);

  // Run clock until 'q'
  while (ch != 'q' && ch != 'Q')
  {
    // Get timer event and execute clock cycle
    ch = stubEventGet();

    // Process keyboard events
    if (ch == 's' || ch == 'S')
      animClockButton(BTTN_SET);
    if (ch == '+')
      animClockButton(BTTN_PLUS);

    // Execute a clock cycle for the clock
    mcClockTimeEvent = time_event;
    animClockDraw(DRAW_CYCLE);
    lcdDeviceFlush(0);
    if (mcClockTimeEvent == GLCD_TRUE)
    {
      DEBUGP("Clear time event");
      mcClockTimeEvent = GLCD_FALSE;
      time_event = GLCD_FALSE;
    }

    // Just processed another cycle
    mcCycleCounter++;
  }

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  // Kill alarm (if sounding anyway) and reset it
  alarmSoundKill();
  alarmClear();

  return CMD_RET_OK;
}

//
// Function: doClockSelect
//
// Select clock from list of available clocks
//
int doClockSelect(cmdLine_t *cmdLine)
{
  uint8_t clock;

  if (argDouble[0] >= emuMonochronCount - 1 + 0.49L)
  {
    // Requested clock is beyond max value
    printf("%s? invalid: %.0f\n", cmdLine->cmdCommand->cmdArg[0].argName,
      argDouble[0]+0.01);
    return CMD_RET_ERROR;
  }

  clock = TO_UINT8_T(argDouble[0]);
  if (clock == CHRON_NONE)
  {
    // Release clock
    emuClockRelease(echoCmd);
  }
  else
  {
    alarmSoundKill();
    mcClockTimeEvent = GLCD_TRUE;
    mcMchronClock = clock;
    mcAlarmSwitch = ALARM_SWITCH_NONE;
    alarmStateSet();
    animClockDraw(DRAW_INIT_FULL);
    emuClockUpdate();
  }

  return CMD_RET_OK;
}

//
// Function: doComments
//
// Process comments
//
int doComments(cmdLine_t *cmdLine)
{
  // Dump comments in the log only when we run at root command level
  if (listExecDepth == 0)
     DEBUGP(argString);

  return CMD_RET_OK;
}

//
// Function: doDateReset
//
// Reset internal clock date
//
int doDateReset(cmdLine_t *cmdLine)
{
  // Reset date to system date
  stubTimeSet(70, 0, 0, 0, 80, 0, 0);

  // Sync mchron time with new date
  emuTimeSync();

  // Update clock when active
  emuClockUpdate();

  // Report (new) time+date+alarm
  if (echoCmd == CMD_ECHO_YES)
    emuTimePrint();

  return CMD_RET_OK;
}

//
// Function: doDateSet
//
// Set internal clock date
//
int doDateSet(cmdLine_t *cmdLine)
{
  int dateOk = GLCD_TRUE;

  // Set new date
  dateOk = stubTimeSet(70, 0, 0, 0, TO_UINT8_T(argDouble[0]),
    TO_UINT8_T(argDouble[1]), TO_UINT8_T(argDouble[2]));
  if (dateOk == GLCD_FALSE)
    return CMD_RET_ERROR;

  // Sync mchron time with new date
  emuTimeSync();

  // Update clock when active
  emuClockUpdate();

  // Report (new) time+date+alarm
  if (echoCmd == CMD_ECHO_YES)
    emuTimePrint();

  return CMD_RET_OK;
}

//
// Function: doExecute
//
// Execute mchron commands from a file
//
int doExecute(cmdLine_t *cmdLine)
{
  int myEchoCmd;
  char *fileName;
  cmdLine_t *cmdLineRoot = NULL;
  cmdPcCtrl_t *cmdPcCtrlRoot = NULL;
  int retVal = CMD_RET_OK;

  // Verify too deep nested 'e' commands (prevent potential recursive call) 
  if (fileExecDepth >= CMD_FILE_DEPTH_MAX)
  {
    printf("stack level exceeded by last 'e' command (max=%d).\n",
      CMD_FILE_DEPTH_MAX);  
    return CMD_RET_ERROR;
  }

  // Keep current command echo to restore at end of this function
  myEchoCmd = echoCmd;

  // Get new command echo state where 'i' keeps current one
  if (argChar[0] == 'e')
    echoCmd = CMD_ECHO_YES;
  else if (argChar[0] == 's')
    echoCmd = CMD_ECHO_NO;

  // Copy filename
  fileName = malloc(strlen(argString) + 1);
  sprintf(fileName, "%s", argString);

  // Valid command file and stack level: increase stack level
  fileExecDepth++;

  // Load the lines from the command file in a linked list.
  // Warning: this will reset the cmd scan global variables.
  retVal = cmdListFileLoad(&cmdLineRoot, &cmdPcCtrlRoot, fileName,
    fileExecDepth);
  if (retVal == CMD_RET_OK)
  {
    // Execute the commands in the command list
    retVal = emuListExecute(cmdLineRoot, fileName);
  }

  // We're done: decrease stack level
  fileExecDepth--;

  // Either all commands in the linked list have been executed
  // successfullly or an error has occured. Do some admin stuff by
  // cleaning up the linked lists.
  free(fileName);
  cmdListCleanup(cmdLineRoot, cmdPcCtrlRoot);

  // Final stack trace element for error/interrupt that occured at
  // lower level
  if (retVal == CMD_RET_RECOVER && listExecDepth == 0)
    printf("%d:%s:-:%s\n", fileExecDepth, __progname, cmdLine->input);

  // Restore original command echo state
  echoCmd = myEchoCmd;

  return retVal;
}

//
// Function: doExit
//
// Prepare to exit mchron
//
int doExit(cmdLine_t *cmdLine)
{
  int retVal = CMD_RET_OK;

  if (listExecDepth > 0)
  {
    printf("use only at command prompt\n");
    retVal = CMD_RET_ERROR;
  }
  else
  {
    // Indicate we want to exit
    invokeExit = GLCD_TRUE;
    retVal = CMD_RET_EXIT;
  }
  
  return retVal;
}

//
// Function: doHelp
//
// Dump helppage
//
int doHelp(cmdLine_t *cmdLine)
{
  if (listExecDepth > 0)
  {
    printf("use only at command prompt\n");
    return CMD_RET_ERROR;
  }

  // Show help using 'more'
  system("/bin/more ../support/help.txt 2>&1");
  return CMD_RET_OK;
}

//
// Function: doHelpCmd
//
// Print the mchron dictionary content for a command
//
int doHelpCmd(cmdLine_t *cmdLine)
{
  if (listExecDepth > 0)
  {
    printf("use only at command prompt\n");
    return CMD_RET_ERROR;
  }

  // Print dictionary of command or all commands
  if (strcmp(argWord[1], "*") == 0)
  {
    cmdDictCmdPrintAll();
  }
  else
  {
    if (cmdDictCmdPrint(argWord[1]) != CMD_RET_OK)
    {
      printf("%s? invalid: %s\n", cmdLine->cmdCommand->cmdArg[0].argName,
        argWord[1]);
      return CMD_RET_ERROR;
    }
  }

  return CMD_RET_OK;
}

//
// Function: doHelpExpr
//
// Print the result of an expression
//
int doHelpExpr(cmdLine_t *cmdLine)
{
  cmdArgValuePrint(argDouble[0], GLCD_TRUE);
  printf("\n");

  return CMD_RET_OK;
}

//
// Function: doIfElse
//
// The start of an if-else block
//
int doIfElse(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdCommand_t *cmdCommand = cmdLine->cmdCommand;
  char *input = cmdLine->input;
  int retVal;

  if (cmdPcCtrlChild->initialized == GLCD_FALSE)
  {
    // Scan the command line for the if-else parameters
    cmdArgInit(&input);
    cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
    retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount, &input,
      GLCD_FALSE);
    if (retVal != CMD_RET_OK)
      return retVal;
    cmdPcCtrlChild->initialized = GLCD_TRUE;
  }

  // Decide where to go depending on whether the preceding block
  // (if-then or else-if) was active
  if (cmdPcCtrlParent->active == GLCD_TRUE)
  {
    // Deactivate preceding block and jump to end-if
    cmdPcCtrlParent->active = GLCD_FALSE;
    *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
  }
  else
  {
    // Make if-else block active and continue on next line
    cmdPcCtrlChild->active = GLCD_TRUE;
    *cmdProgCounter = (*cmdProgCounter)->next;
  }

  return CMD_RET_OK;
}

//
// Function: doIfElseIf
//
// The start of an if-else-if block
//
int doIfElseIf(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdCommand_t *cmdCommand = cmdLine->cmdCommand;
  char *input = cmdLine->input;
  int retVal;

  if (cmdPcCtrlChild->initialized == GLCD_FALSE)
  {
    // Scan the command line for the if-else-if arguments
    cmdArgInit(&input);
    cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
    retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount, &input,
      GLCD_FALSE);
    if (retVal != CMD_RET_OK)
      return retVal;

    // Copy the condition expression for the if-else-if
    cmdPcCtrlChild->cbArg1 = cmdPcCtrlArgCreate(argWord[1]);
    cmdPcCtrlChild->initialized = GLCD_TRUE;
  }

  // Decide where to go depending on whether the preceding block
  // (if-then or else-if) was active
  if (cmdPcCtrlParent->active == GLCD_TRUE)
  {
    // Deactivate preceding block and jump to end-if
    cmdPcCtrlParent->active = GLCD_FALSE;
    while ((*cmdProgCounter)->cmdCommand->cmdPcCtrlType != PC_IF_END)
      *cmdProgCounter = (*cmdProgCounter)->cmdPcCtrlChild->cmdLineChild;
  }
  else
  {
    // Evaluate the condition expression 
    EXPR_EVALUATE(cmdCommand->cmdArg[0].argName, cmdPcCtrlChild->cbArg1);

    // Decide where to go depending on the condition result
    if (exprValue != 0)
    {
      // The if-else-if block is active and we'll continue on the next line
      cmdPcCtrlChild->active = GLCD_TRUE;
      *cmdProgCounter = (*cmdProgCounter)->next;
    }
    else
    {
      // Jump to next block (if-else-if, if-else or if-end)
      *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
    }
  }

  return CMD_RET_OK;
}

//
// Function: doIfEnd
//
// The closing of an if-then-else block
//
int doIfEnd(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;
  cmdCommand_t *cmdCommand = cmdLine->cmdCommand;
  char *input = cmdLine->input;
  int retVal;

  // Scan the command line for the if-end parameters
  cmdArgInit(&input);
  cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
  retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount, &input,
    GLCD_FALSE);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Deactivate current control block and continue on next line
  cmdPcCtrlParent->active = GLCD_FALSE;
  *cmdProgCounter = (*cmdProgCounter)->next;

  return CMD_RET_OK;
}

//
// Function: doIfThen
//
// Initiate an if-then and determine where to continue
//
int doIfThen(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdCommand_t *cmdCommand = cmdLine->cmdCommand;
  char *input = cmdLine->input;
  int retVal;

  if (cmdPcCtrlChild->initialized == GLCD_FALSE)
  {
    // Scan the command line for the if-then arguments
    cmdArgInit(&input);
    cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
    retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount, &input,
      GLCD_FALSE);
    if (retVal != CMD_RET_OK)
      return retVal;

    // Copy the condition expression for the if-then
    cmdPcCtrlChild->cbArg1 = cmdPcCtrlArgCreate(argWord[1]);
    cmdPcCtrlChild->initialized = GLCD_TRUE;
  }

  // Evaluate the condition expression 
  EXPR_EVALUATE(cmdCommand->cmdArg[0].argName, cmdPcCtrlChild->cbArg1);

  // Decide where to go depending on the condition result
  if (exprValue != 0)
  {
    // The if-then block is active and continue on next line
    cmdPcCtrlChild->active = GLCD_TRUE;
    *cmdProgCounter = (*cmdProgCounter)->next;
  }
  else
  {
    // Jump to if-else-if, if-else or if-end block
    *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
  }

  return CMD_RET_OK;
}

//
// Function: doLcdBacklightSet
//
// Set lcd backlight (0 = almost dark .. 16 = full power).
// Note: Only the glut lcd stub supports backlight.
//
int doLcdBacklightSet(cmdLine_t *cmdLine)
{
  // Process backlight
  emuBacklight = TO_U08(argDouble[0]);
  lcdDeviceBacklightSet(emuBacklight);

  return CMD_RET_OK;
}

//
// Function: doLcdErase
//
// Erase the contents of the lcd screen
//
int doLcdErase(cmdLine_t *cmdLine)
{
  // Erase lcd display
  glcdClearScreen(mcBgColor);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doLcdInverse
//
// Inverse the contents of the lcd screen
//
int doLcdInverse(cmdLine_t *cmdLine)
{
  // Toggle the foreground and background colors
  if (mcBgColor == OFF)
  {
    emuBgColor = mcBgColor = ON;
    emuFgColor = mcFgColor = OFF;
  }
  else
  {
    emuBgColor = mcBgColor = OFF;
    emuFgColor = mcFgColor = ON;
  }

  // Inverse the display
  glcdFillRectangle2(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, ALIGN_TOP,
    FILL_INVERSE, mcFgColor);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doMonochron
//
// Start the stubbed Monochron application
//
int doMonochron(cmdLine_t *cmdLine)
{
  u08 myBacklight = 16;
  int myKbMode = KB_MODE_LINE;
  int startMode = CYCLE_NOWAIT;

  // Get the start mode
  startMode = emuStartModeGet(argChar[0]);

  // Clear active clock (if any)
  emuClockRelease(CMD_ECHO_NO);

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Set essential Monochron startup data
  mcClockTimeEvent = GLCD_FALSE;
  mcAlarmSwitch = ALARM_SWITCH_NONE;

  // Clear the screen so we won't see any flickering upon
  // changing the backlight later on
  glcdClearScreen(OFF);

  // Upon request force the eeprom to init and, based on that, set the
  // backlight of the lcd stub device
  if (argChar[1] == 'r')
  {
    stubEepromReset();
    lcdDeviceBacklightSet(OCR2A_VALUE);
  }
  else
  {
    // No init needed so set the backlight as stored in the eeprom
    myBacklight = eeprom_read_byte((uint8_t *)EE_BRIGHT) >> OCR2B_BITSHIFT;
    lcdDeviceBacklightSet(myBacklight);
  }

  // Init stub event handler used in Monochron
  stubEventInit(startMode, stubHelpMonochron);

  // Start Monochron and witness the magic :-)
  monoMain();

  // We're done.
  // Restore the clock pool that mchron supports (as it was overridden
  // by the Monochron clock pool). By clearing the active clock from that
  // pool also any audible alarm will be stopped and reset.
  mcClockPool = emuMonochron;
  emuClockRelease(CMD_ECHO_NO);

  // Restore alarm, foreground/background color and backlight as they
  // were prior to starting Monochron
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;
  mcBgColor = emuBgColor;
  mcFgColor = emuFgColor;
  lcdDeviceBacklightSet(emuBacklight);

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  return CMD_RET_OK;
}

//
// Function: doPaintAscii
//
// Paint ascii
//
int doPaintAscii(cmdLine_t *cmdLine)
{
  u08 len = 0;
  u08 color;
  u08 orientation;
  u08 font;

  // Get color, orientation and font from command line text values
  color = emuColorGet(argChar[0]);
  orientation = emuOrientationGet(argChar[1]);
  font = emuFontGet(argWord[1]);

  // Paint ascii based on text orientation
  if (orientation == ORI_HORIZONTAL)
  {
    // Horizontal text
    len = glcdPutStr3(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      argString, TO_U08(argDouble[2]), TO_U08(argDouble[3]), color);
    if (echoCmd == CMD_ECHO_YES)
      printf("hor px=%d\n", (int)len);
  }
  else
  {
    // Vertical text
    len = glcdPutStr3v(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      orientation, argString, TO_U08(argDouble[2]), TO_U08(argDouble[3]),
      color);
    if (echoCmd == CMD_ECHO_YES)
      printf("vert px=%d\n", (int)len);
  }
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintCircle
//
// Paint circle
//
int doPaintCircle(cmdLine_t *cmdLine)
{
  u08 color;

  // Get color
  color = emuColorGet(argChar[0]);

  // Draw circle
  glcdCircle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]), TO_U08(argDouble[2]),
    TO_U08(argDouble[3]), color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintCircleFill
//
// Paint circle with fill pattern
//
int doPaintCircleFill(cmdLine_t *cmdLine)
{
  u08 color;
  u08 pattern;

  // Get color
  color = emuColorGet(argChar[0]);

  // All fill patterns are allowed except inverse
  pattern = TO_U08(argDouble[3]);
  if (pattern == FILL_INVERSE)
  {
    printf("%s? invalid: %d\n", cmdLine->cmdCommand->cmdArg[4].argName,
      (int)pattern);
    return CMD_RET_ERROR;
  }

  // Draw filled circle
  glcdFillCircle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), pattern, color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintDot
//
// Paint dot
//
int doPaintDot(cmdLine_t *cmdLine)
{
  u08 color;

  // Get color
  color = emuColorGet(argChar[0]);

  // Draw dot
  glcdDot(TO_U08(argDouble[0]), TO_U08(argDouble[1]), color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintLine
//
// Paint line
//
int doPaintLine(cmdLine_t *cmdLine)
{
  u08 color;

  // Get color
  color = emuColorGet(argChar[0]);

  // Draw line
  glcdLine(TO_U08(argDouble[0]), TO_U08(argDouble[1]), TO_U08(argDouble[2]),
    TO_U08(argDouble[3]), color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintNumber
//
// Paint a number using a c printf format
//
int doPaintNumber(cmdLine_t *cmdLine)
{
  u08 len = 0;
  u08 color;
  u08 orientation;
  u08 font;
  char *valString;

  // Get color, orientation and font from commandline text values
  color = emuColorGet(argChar[0]);
  orientation = emuOrientationGet(argChar[1]);
  font = emuFontGet(argWord[1]);

  // Get output string
  asprintf(&valString, argString, argDouble[4]);

  // Paint ascii based on text orientation
  if (orientation == ORI_HORIZONTAL)
  {
    // Horizontal text
    len = glcdPutStr3(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      valString, TO_U08(argDouble[2]), TO_U08(argDouble[3]), color);
    if (echoCmd == CMD_ECHO_YES)
      printf("hor px=%d\n", (int)len);
  }
  else
  {
    // Vertical text
    len = glcdPutStr3v(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      orientation, valString, TO_U08(argDouble[2]), TO_U08(argDouble[3]),
      color);
    if (echoCmd == CMD_ECHO_YES)
      printf("vert px=%d\n", (int)len);
  }
  lcdDeviceFlush(0);

  // Free the allocated output string
  free(valString);

  return CMD_RET_OK;
}

//
// Function: doPaintRect
//
// Paint rectangle
//
int doPaintRect(cmdLine_t *cmdLine)
{
  u08 color;

  // Get color
  color = emuColorGet(argChar[0]);

  // Draw rectangle
  glcdRectangle(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), TO_U08(argDouble[3]), color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintRectFill
//
// Paint rectangle with fill pattern
//
int doPaintRectFill(cmdLine_t *cmdLine)
{
  u08 color;

  // Get color
  color = emuColorGet(argChar[0]);

  // Draw filled rectangle
  glcdFillRectangle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), TO_U08(argDouble[3]), TO_U08(argDouble[4]),
    TO_U08(argDouble[5]), color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doRepeatFor
//
// Initiate a new repeat loop
//
int doRepeatFor(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdCommand_t *cmdCommand = cmdLine->cmdCommand;
  char *input = cmdLine->input;
  int retVal;

  // Init the control block structure when needed
  if (cmdPcCtrlChild->initialized == GLCD_FALSE)
  {
    // Scan the command line for the repeat arguments
    cmdArgInit(&input);
    cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
    retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount, &input,
      GLCD_FALSE);
    if (retVal != CMD_RET_OK)
      return retVal;

    // Copy the expressions for the init, condition and post arguments
    cmdPcCtrlChild->cbArg1 = cmdPcCtrlArgCreate(argWord[1]);
    cmdPcCtrlChild->cbArg2 = cmdPcCtrlArgCreate(argWord[2]);
    cmdPcCtrlChild->cbArg3 = cmdPcCtrlArgCreate(argWord[3]);
    cmdPcCtrlChild->initialized = GLCD_TRUE;
  }

  // Execute the repeat logic
  if (cmdPcCtrlChild->active == GLCD_FALSE)
  {
    // First entry for this loop. Make the repeat active, then evaluate
    // the repeat init and the repeat condition expressions.
    cmdPcCtrlChild->active = GLCD_TRUE;
    EXPR_EVALUATE(cmdCommand->cmdArg[0].argName, cmdPcCtrlChild->cbArg1);
    EXPR_EVALUATE(cmdCommand->cmdArg[1].argName, cmdPcCtrlChild->cbArg2);
  }
  else
  {
    // For a next repeat loop first evaluate the step expression and
    // then re-evaluate the repeat condition expression
    EXPR_EVALUATE(cmdCommand->cmdArg[2].argName, cmdPcCtrlChild->cbArg3);
    EXPR_EVALUATE(cmdCommand->cmdArg[1].argName, cmdPcCtrlChild->cbArg2);
  }

  // Decide where to go depending on the repeat condition result
  if (exprValue != 0)
  {
    // Continue at next line
    *cmdProgCounter = (*cmdProgCounter)->next;
  }
  else
  {
    // End of loop; make it inactive and jump to repeat next
    cmdPcCtrlChild->active = GLCD_FALSE;
    *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
  }

  return CMD_RET_OK;
}

//
// Function: doRepeatNext
//
// Complete current repeat loop and determine end-of-loop
//
int doRepeatNext(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;
  cmdCommand_t *cmdCommand = cmdLine->cmdCommand;
  char *input = cmdLine->input;
  int retVal;

  // Scan and validate the repeat next command line
  cmdArgInit(&input);
  cmdArgScan(argCmd, 1, &input, GLCD_FALSE);
  retVal = cmdArgScan(cmdCommand->cmdArg, cmdCommand->argCount, &input,
    GLCD_FALSE);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Decide where to go depending on whether the loop is still active
  if (cmdPcCtrlParent->active == GLCD_TRUE)
  {
    // Jump back to top of repeat (and evaluate there whether the repeat
    // loop will continue)
    *cmdProgCounter = cmdPcCtrlParent->cmdLineParent;
  }
  else
  {
    // End of repeat loop; continue at next line
    *cmdProgCounter = (*cmdProgCounter)->next;
  }

  return CMD_RET_OK;
}

//
// Function: doStatsPrint
//
// Print stub, glcd interface and lcd performance statistics
//
int doStatsPrint(cmdLine_t *cmdLine)
{
  printf("statistics:\n");

  // Print stub statistics
  stubStatsPrint();
  
  // Print glcd interface and lcd performance statistics
  lcdStatsPrint();
  
  return CMD_RET_OK;
}

//
// Function: doStatsReset
//
// Reset stub, glcd interface and lcd performance statistics
//
int doStatsReset(cmdLine_t *cmdLine)
{
  // Reset stub statistics
  stubStatsReset();

  // Reset glcd interface and lcd performance statistics
  lcdStatsReset();

  if (echoCmd == CMD_ECHO_YES)
    printf("statistics reset\n");
  
  return CMD_RET_OK;
}

//
// Function: doTimeFlush
//
// Sync with and then report and update clock with date/time/alarm 
//
int doTimeFlush(cmdLine_t *cmdLine)
{
  // Get current time+date
  readi2ctime();

  // Sync mchron time with (new) time
  emuTimeSync();

  // Update clock when active
  emuClockUpdate();

  // Report (new) time+date+alarm
  if (echoCmd == CMD_ECHO_YES)
    emuTimePrint();

  return CMD_RET_OK;
}

//
// Function: doTimePrint
//
// Report current date/time/alarm 
//
int doTimePrint(cmdLine_t *cmdLine)
{
  // Get current time+date
  readi2ctime();

  // Report time+date+alarm
  emuTimePrint();

  return CMD_RET_OK;
}

//
// Function: doTimeReset
//
// Reset internal clock time
//
int doTimeReset(cmdLine_t *cmdLine)
{
  // Reset time to system time
  stubTimeSet(80, 0, 0, 0, 70, 0, 0);

  // Sync mchron time with (new) time
  emuTimeSync();

  // Update clock when active
  emuClockUpdate();

  // Report time+date+alarm
  if (echoCmd == CMD_ECHO_YES)
    emuTimePrint();

  return CMD_RET_OK;
}

//
// Function: doTimeSet
//
// Set internal clock time
//
int doTimeSet(cmdLine_t *cmdLine)
{
  int timeOk = GLCD_TRUE;

  // Overide time
  timeOk = stubTimeSet(TO_UINT8_T(argDouble[2]), TO_UINT8_T(argDouble[1]),
    TO_UINT8_T(argDouble[0]), 0, 70, 0, 0);
  if (timeOk == GLCD_FALSE)
    return CMD_RET_ERROR;

  // Sync mchron time with (new) time
  emuTimeSync();

  // Update clock when active
  emuClockUpdate();

  // Report new time+date+alarm
  if (echoCmd == CMD_ECHO_YES)
    emuTimePrint();

  return CMD_RET_OK;
}

//
// Function: doVarPrint
//
// Print value of one or all used named variables in rows with max 8
// variable values per row
//
int doVarPrint(cmdLine_t *cmdLine)
{
  int retVal;

  // Print all variables or a single one
  retVal = varPrint(cmdLine->cmdCommand->cmdArg[0].argName, argWord[1]);

  return retVal;
}

//
// Function: doVarReset
//
// Clear one or all used named variables
//
int doVarReset(cmdLine_t *cmdLine)
{
  int retVal = CMD_RET_OK;

  // Clear all variables or a single one
  if (strcmp(argWord[1], "*") == 0)
    varReset();
  else
    retVal = varClear(cmdLine->cmdCommand->cmdArg[0].argName, argWord[1]);

  return retVal;
}

//
// Function: doVarSet
//
// Init and set named variable. Note that the actual expression evaluation and
// variable assignment has been performed by the command argument scan module.
// When the expression evaluator failed this function won't be called. Only
// when the expression was evaluated successfully this function is called
// and there's nothing else left for us to do except for returning the
// successful technical result of the expression evaluator.
//
int doVarSet(cmdLine_t *cmdLine)
{
  return CMD_RET_OK;
}

//
// Function: doWait
//
// Wait for keypress or pause in multiple of 1 msec.
//
int doWait(cmdLine_t *cmdLine)
{
  int delay = 0;
  char ch = '\0';

  delay = TO_INT(argDouble[0]);
  if (delay == 0)
  {
    // Wait for keypress
    if (listExecDepth == 0)
      ch = kbWaitKeypress(GLCD_FALSE);
    else
      ch = kbWaitKeypress(GLCD_TRUE);
  }
  else
  {
    // Wait delay*0.001 sec
    ch = kbWaitDelay(delay);
  }

  // If a 'q' was entered, we need to quit the list execution 
  if ((ch == 'q' || ch == 'Q') && listExecDepth > 0)
  {
    printf("quit\n");
    return CMD_RET_INTERRUPT;
  }

  return CMD_RET_OK;
}

