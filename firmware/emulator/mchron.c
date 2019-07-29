//*****************************************************************************
// Filename : 'mchron.c'
// Title    : Main entry and command line utility for the emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Monochron defines
#include "../monomain.h"
#include "../buttons.h"
#include "../ks0108.h"
#include "../glcd.h"
#include "../anim.h"
#include "../config.h"

// Monochron clocks
#include "../clock/analog.h"
#include "../clock/barchart.h"
#include "../clock/bigdigit.h"
#include "../clock/cascade.h"
#include "../clock/crosstable.h"
#include "../clock/digital.h"
#include "../clock/example.h"
#include "../clock/linechart.h"
#include "../clock/mosquito.h"
#include "../clock/nerd.h"
#include "../clock/perftest.h"
#include "../clock/piechart.h"
#include "../clock/pong.h"
#include "../clock/puzzle.h"
#include "../clock/qr.h"
#include "../clock/slider.h"
#include "../clock/speeddial.h"
#include "../clock/spiderplot.h"
#include "../clock/thermometer.h"
#include "../clock/trafficlight.h"

// Emuchron stubs and utilities
#include "stub.h"
#include "expr.h"
#include "dictutil.h"
#include "listutil.h"
#include "mchronutil.h"
#include "scanutil.h"
#include "varutil.h"
#include "mchron.h"

// Convert a double to other type with decent nearest whole number rounding.
// Use these macros to convert a scanned numeric command argument (that is
// of type double) into a type fit for post-processing in a command handler.
#define TO_INT(d)	((int)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_U08(d)	((u08)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_UINT8_T(d)	((uint8_t)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_UINT16_T(d)	((uint16_t)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))

// Monochron defined data
extern volatile uint8_t animDisplayMode;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockNewDD, mcClockNewDM, mcClockNewDY;
extern volatile uint8_t mcClockTimeEvent, mcClockDateEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern clockDriver_t *mcClockPool;
extern clockDriver_t monochron[];
extern volatile uint8_t almSwitchOn;
extern volatile uint8_t almAlarming;
extern int16_t almTickerAlarm;
extern uint16_t almTickerSnooze;

// The variables that store the published command line arguments
extern char argChar[];
extern double argDouble[];
extern char *argWord[];
extern char *argString;

// The expression evaluator expression result value
extern double exprValue;

// The command list execution depth tells us if we're running at command prompt
// level
extern int listExecDepth;

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

// The timer used for the 'wte' and 'wts' commands
static struct timeval tvTimer;

// The emulator background/foreground color of the lcd display and backlight
// GLCD_OFF = 0 = black color (=0x0 bit value in lcd memory)
// GLCD_ON  = 1 = white color (=0x1 bit value in lcd memory)
static u08 emuBgColor = GLCD_OFF;
static u08 emuFgColor = GLCD_ON;
static u08 emuBacklight = 16;

// The clocks supported in the mchron clock test environment.
// Note that Monochron will have its own implemented array of supported clocks
// in anim.c [firmware]. So, we need to switch between the two arrays when
// appropriate.
static clockDriver_t emuMonochron[] =
{
  {CHRON_NONE,        DRAW_INIT_NONE, 0,                  0,                   0},
  {CHRON_EXAMPLE,     DRAW_INIT_FULL, exampleInit,        exampleCycle,        0},
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
  {CHRON_THERMOMETER, DRAW_INIT_FULL, spotThermInit,      spotThermCycle,      0},
  {CHRON_TRAFLIGHT,   DRAW_INIT_FULL, spotTrafLightInit,  spotTrafLightCycle,  0},
  {CHRON_BARCHART,    DRAW_INIT_FULL, spotBarChartInit,   spotBarChartCycle,   0},
  {CHRON_CROSSTABLE,  DRAW_INIT_FULL, spotCrossTableInit, spotCrossTableCycle, 0},
  {CHRON_LINECHART,   DRAW_INIT_FULL, spotLineChartInit,  spotLineChartCycle,  0},
  {CHRON_PIECHART,    DRAW_INIT_FULL, spotPieChartInit,   spotPieChartCycle,   0},
  {CHRON_BIGDIG_ONE,  DRAW_INIT_FULL, bigDigInit,         bigDigCycle,         bigDigButton},
  {CHRON_BIGDIG_TWO,  DRAW_INIT_FULL, bigDigInit,         bigDigCycle,         bigDigButton},
  {CHRON_QR_HMS,      DRAW_INIT_FULL, qrInit,             qrCycle,             0},
  {CHRON_QR_HM,       DRAW_INIT_FULL, qrInit,             qrCycle,             0},
  {CHRON_PERFTEST,    DRAW_INIT_FULL, perfInit,           perfCycle,           0}
};
static int emuMonochronCount = sizeof(emuMonochron) / sizeof(clockDriver_t);

//
// Function: main
//
// Main program for Emuchron command shell
//
int main(int argc, char *argv[])
{
  cmdLine_t cmdLine;
  char *prompt;
  emuArgcArgv_t emuArgcArgv;
  int retVal = CMD_RET_OK;

  // Setup signal handlers to either recover from signal or to attempt graceful
  // non-standard exit
  emuSigSetup();

  // Do command line processing
  retVal = emuArgcArgvGet(argc, argv, &emuArgcArgv);
  if (retVal != CMD_RET_OK)
    return CMD_RET_ERROR;

  // Init the lcd color modes
  mcBgColor = emuBgColor;
  mcFgColor = emuFgColor;

  // Init initial alarm
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;

  // Open debug logfile when requested
  if (emuArgcArgv.argDebug != 0)
  {
    retVal = stubLogfileOpen(argv[emuArgcArgv.argDebug]);
    if (retVal != CMD_RET_OK)
      return retVal;
  }

  // Init the lcd controllers and display stub device(s)
  retVal = ctrlInit(&emuArgcArgv.ctrlDeviceArgs);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Uncomment this if you want to join with debugger prior to using anything
  // in the glcd library for the lcd device
  //char tmpInput[10];
  //fgets(tmpInput, 10, stdin);

  // Welcome in mchron
  printf("\n*** Welcome to Emuchron emulator command line tool %s %s ***\n\n",
    __progname, EMUCHRON_VERSION);

  // Clear and show welcome message on lcd device
  beep(4000, 100);
  ctrlLcdBacklightSet(emuBacklight);
  glcdInit(mcBgColor);
  glcdPutStr2(1, 1, FONT_5X5P, "* Welcome to Emuchron Emulator *", mcFgColor);
  glcdPutStr2(1, 8, FONT_5X5P, "Enter 'h' for help", mcFgColor);
  ctrlLcdFlush();

  // Show process id and (optional) ncurses output device
  printf("process id  : %d\n", getpid());
  if (emuArgcArgv.ctrlDeviceArgs.useNcurses == 1)
    printf("ncurses tty : %s\n",
      emuArgcArgv.ctrlDeviceArgs.lcdNcurInitArgs.tty);
  printf("\n");

  // Init the clock pool supported in mchron command line mode
  mcClockPool = emuMonochron;

  // Init the stubbed alarm switch to 'Off' and clear audible alarm
  alarmSwitchSet(GLCD_FALSE, GLCD_FALSE);
  alarmSoundStop();

  // Init emuchron system clock + clock plugin time, then print it
  rtcTimeInit();
  rtcMchronTimeInit();
  emuTimePrint(ALM_EMUCHRON);

  // Init mchron named variable buckets
  varInit();

  // Init Monochron eeprom
  stubEepReset();
  eepInit();

  // Init mchron wait expiry timer
  waitTimerStart(&tvTimer);

  // Init the command line input interface
  cmdInput.file = stdin;
  cmdInput.readMethod = CMD_INPUT_READLINELIB;
  cmdInputInit(&cmdInput);

  // All initialization is done!
  printf("\nenter 'h' for help\n");

  // We're in business: give prompt and process keyboard commands until the
  // last proton in the universe has desintegrated (or use 'x' or ^D to exit)

  // Initialize a command line for the interpreter
  cmdLine.lineNum = 0;
  cmdLine.input = NULL;
  cmdLine.args = NULL;
  cmdLine.initialized = GLCD_FALSE;
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
    retVal = cmdLineExecute(&cmdLine, &cmdInput);
    cmdArgCleanup(&cmdLine);
    if (retVal == CMD_RET_EXIT)
      break;

    // Get next command
    cmdInputRead(prompt, &cmdInput);
  }

  // Done: caused by 'x' or ^D
  if (retVal != CMD_RET_EXIT)
    printf("\n<ctrl>d - exit\n");

  // Cleanup command line read interface
  free(prompt);
  cmdInputCleanup(&cmdInput);

  // Shutdown gracefully by killing audio and stopping the controller and lcd
  // device(s)
  alarmSoundStop();
  ctrlCleanup();

  // Stop debug output
  DEBUGP("**** logging stopped");
  stubLogfileClose();

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
// A control block handler (for if-logic and repeat commands) however must
// decide which command arguments are evaluated, depending on the state of its
// control block. In other words, command arguments are evaluated optionally,
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
    almStateSet();
    animClockDraw(DRAW_CYCLE);
    ctrlLcdFlush();
  }

  // Report the new alarm settings
  if (echoCmd == CMD_ECHO_YES)
  {
    // Report current time+date+alarm
    rtcTimeRead();
    emuTimePrint(ALM_EMUCHRON);
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
      // exiting the config menu. We therefore don't care what the old value
      // was. This behavior will not cause a problem for most clocks when we
      // are in command mode and change the alarm: the alarm time will
      // overwrite the old value on the lcd. However, for a clock like Analog
      // that shows the alarm in an analog clock style, changing the alarm in
      // command mode will draw the new alarm while not erasing the old alarm
      // time.
      // We'll use a trick to overwrite the old alarm: toggle the alarm switch
      // twice. Note: This may cause a slight blink in the alarm area when
      // using the glut lcd device.
      alarmSwitchToggle(GLCD_FALSE);
      almStateSet();
      animClockDraw(DRAW_CYCLE);
      alarmSwitchToggle(GLCD_FALSE);
      almStateSet();
      animClockDraw(DRAW_CYCLE);
    }
    else
    {
      mcAlarmSwitch = ALARM_SWITCH_NONE;
      animClockDraw(DRAW_CYCLE);
    }
    ctrlLcdFlush();
  }

  // Report the new alarm settings
  if (echoCmd == CMD_ECHO_YES)
  {
    // Report current time+date+alarm
    rtcTimeRead();
    emuTimePrint(ALM_EMUCHRON);
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
    almStateSet();
    animClockDraw(DRAW_CYCLE);
    ctrlLcdFlush();
  }

  // Report the new alarm settings
  if (echoCmd == CMD_ECHO_YES)
  {
    // Report current time+date+alarm
    rtcTimeRead();
    emuTimePrint(ALM_EMUCHRON);
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
  int startWait = GLCD_FALSE;
  u08 myKbMode = KB_MODE_LINE;

  // Get the start mode
  startWait = emuStartModeGet(argChar[0]);

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
  rtcMchronTimeInit();

  // Init stub event handler used in main loop below and get first event
  stubEventInit(startWait, stubHelpClockFeed);
  ch = stubEventGet();

  // Run clock until 'q'
  while (ch != 'q')
  {
    // Process keyboard events
    if (ch == 's')
      animClockButton(BTN_SET);
    if (ch == '+')
      animClockButton(BTN_PLUS);

    // Execute a clock cycle for the clock
    animClockDraw(DRAW_CYCLE);

    // Just processed another cycle
    mcCycleCounter++;

    // Get next timer event
    ch = stubEventGet();
  }

  // Flush any pending updates in the lcd device
  ctrlLcdFlush();

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  // Kill alarm (if sounding anyway) and reset it
  alarmSoundStop();
  alarmSoundReset();

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
      argDouble[0] + 0.01);
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
    // Switch to new clock: init and do first clock cycle
    alarmSoundStop();
    mcMchronClock = clock;
    almStateSet();
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
    emuTimePrint(ALM_EMUCHRON);

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
    emuTimePrint(ALM_EMUCHRON);

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
    printf("stack level exceeded by last '%s' command (max=%d)\n",
      cmdLine->cmdCommand->cmdName, CMD_FILE_DEPTH_MAX);
    return CMD_RET_ERROR;
  }

  // Keep current command echo to restore at end of this function
  myEchoCmd = echoCmd;

  // Get new command echo state where 'i' keeps current one
  if (argChar[0] == 'e')
    echoCmd = CMD_ECHO_YES;
  else if (argChar[0] == 's')
    echoCmd = CMD_ECHO_NO;

  // Let's start: increase stack level
  fileExecDepth++;

  // Load the lines from the command file in a linked list.
  // Warning: this will reset the cmd scan global variables.
  // When ok execute the list. If an error occured prepare for completing a
  // stack trace.
  fileName = malloc(strlen(argString) + 1);
  sprintf(fileName, "%s", argString);
  retVal = cmdListFileLoad(&cmdLineRoot, &cmdPcCtrlRoot, fileName,
    fileExecDepth);
  if (retVal == CMD_RET_OK)
  {
    // Start command list statistics when at root level and execute the command
    // list
    if (listExecDepth == 0)
      cmdListStatsInit();
    retVal = cmdListExecute(cmdLineRoot, fileName);
  }
  else if (retVal == CMD_RET_ERROR)
  {
    retVal = CMD_RET_RECOVER;
  }

  // We're done: decrease stack level
  fileExecDepth--;

  // Either all commands in the linked list have been executed successfully or
  // an error has occured. Do some admin stuff by cleaning up the linked list.
  free(fileName);
  cmdListCleanup(cmdLineRoot, cmdPcCtrlRoot);

  // Final stack trace element for error/interrupt that occured at lower level
  if (retVal == CMD_RET_RECOVER && listExecDepth == 0)
    printf(CMD_STACK_ROOT_FMT, fileExecDepth, __progname, cmdLine->input);

  // Print command list statistics when back at root level
  if (listExecDepth == 0)
    cmdListStatsPrint();

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
  int retVal;

  if (listExecDepth > 0)
  {
    printf("use only at command prompt\n");
    return CMD_RET_ERROR;
  }

  // Print dictionary of command(s) where '.' is all
  retVal = dictPrint(argWord[1]);
  if (retVal != CMD_RET_OK)
    printf("%s? invalid\n", cmdLine->cmdCommand->cmdArg[0].argName);

  return retVal;
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
// Function: doHelpMsg
//
// Show help message
//
int doHelpMsg(cmdLine_t *cmdLine)
{
  // Show message in the command shell
  printf("%s\n", argString);

  return CMD_RET_OK;
}

//
// Function: doIf
//
// Initiate an if and determine where to continue
//
int doIf(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;

  // Evaluate the condition expression
  if (exprEvaluate(cmdArg[0].argName, cmdLine->args[0]) != CMD_RET_OK)
    return CMD_RET_ERROR;

  // Decide where to go depending on the condition result
  if (exprValue != 0)
  {
    // Make the if-then block active and continue on next line
    cmdPcCtrlChild->active = GLCD_TRUE;
    *cmdProgCounter = (*cmdProgCounter)->next;
  }
  else
  {
    // Jump to next if-else-if, if-else or if-end block
    *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
  }

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

  // Decide where to go depending on whether the preceding block (if-then or
  // else-if) was active
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
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;

  // Decide where to go depending on whether the preceding block (if-then or
  // else-if) was active
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
    if (exprEvaluate(cmdArg[0].argName, cmdLine->args[0]) != CMD_RET_OK)
      return CMD_RET_ERROR;

    // Decide where to go depending on the condition result
    if (exprValue != 0)
    {
      // Make the if-else-if block active and continue on the next line
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

  // Deactivate preceding control block (if anyway) and continue on next line
  cmdPcCtrlParent->active = GLCD_FALSE;
  *cmdProgCounter = (*cmdProgCounter)->next;

  return CMD_RET_OK;
}

//
// Function: doLcdBacklightSet
//
// Set lcd backlight (0 = almost dark .. 16 = full power).
//
int doLcdBacklightSet(cmdLine_t *cmdLine)
{
  // Process backlight
  emuBacklight = TO_U08(argDouble[0]);
  ctrlLcdBacklightSet(emuBacklight);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdCursorReset
//
// Reset controller lcd cursors and sync with software cursor to (0,0)
//
int doLcdCursorReset(cmdLine_t *cmdLine)
{
  // Reset the controller hardware cursors to (0,0) and by doing so sync the y
  // location with the glcd administered cursor y location
  glcdSetAddress(64, 7);
  glcdSetAddress(64, 0);
  glcdSetAddress(0, 7);
  glcdSetAddress(0, 0);

  return CMD_RET_OK;
}

//
// Function: doLcdCursorSet
//
// Send x and y cursor position to controller
//
int doLcdCursorSet(cmdLine_t *cmdLine)
{
  u08 payload;
  u08 controller = TO_U08(argDouble[0]);

  // Send x position and y page to lcd controller
  payload = TO_U08(argDouble[1]);
  ctrlExecute(CTRL_METHOD_COMMAND, controller, GLCD_SET_Y_ADDR | payload);
  payload = TO_U08(argDouble[2]);
  ctrlExecute(CTRL_METHOD_COMMAND, controller, GLCD_SET_PAGE | payload);

  return CMD_RET_OK;
}

//
// Function: doLcdDisplaySet
//
// Switch controller displays on/off
//
int doLcdDisplaySet(cmdLine_t *cmdLine)
{
  u08 payload;

  // Send display on/off to controller 0 and 1
  payload = TO_U08(argDouble[0]);
  ctrlExecute(CTRL_METHOD_COMMAND, 0, GLCD_ON_CTRL | payload);
  payload = TO_U08(argDouble[1]);
  ctrlExecute(CTRL_METHOD_COMMAND, 1, GLCD_ON_CTRL | payload);
  ctrlLcdFlush();

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
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdGlutGrSet
//
// Set glut graphics options
//
int doLcdGlutGrSet(cmdLine_t *cmdLine)
{
  // Ignore if glut device is not used
  if (ctrlDeviceActive(CTRL_DEVICE_GLUT) == GLCD_FALSE)
    return CMD_RET_OK;

  // Set glut pixel bezel and gridline options on/off
  ctrlLcdGlutGrSet(TO_UINT8_T(argDouble[0]), TO_UINT8_T(argDouble[1]));

  return CMD_RET_OK;
}

//
// Function: doLcdHlReset
//
// Reset (clear) glcd pixel highlight (glut only)
//
int doLcdHlReset(cmdLine_t *cmdLine)
{
  // Disable glcd pixel highlight
  ctrlLcdHighlight(GLCD_FALSE, 0, 0);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdHlSet
//
// Enable glcd pixel highlight (glut only)
//
int doLcdHlSet(cmdLine_t *cmdLine)
{
  // Enable glcd pixel highlight
  ctrlLcdHighlight(GLCD_TRUE, TO_U08(argDouble[0]), TO_U08(argDouble[1]));
  ctrlLcdFlush();

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
  if (mcBgColor == GLCD_OFF)
  {
    emuBgColor = mcBgColor = GLCD_ON;
    emuFgColor = mcFgColor = GLCD_OFF;
  }
  else
  {
    emuBgColor = mcBgColor = GLCD_OFF;
    emuFgColor = mcFgColor = GLCD_ON;
  }

  // Inverse the display
  glcdFillRectangle2(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, ALIGN_TOP, FILL_INVERSE,
    mcFgColor);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdNcurGrSet
//
// Set ncurses graphics options
//
int doLcdNcurGrSet(cmdLine_t *cmdLine)
{
  // Ignore if ncurses device is not used
  if (ctrlDeviceActive(CTRL_DEVICE_NCURSES) == GLCD_FALSE)
    return CMD_RET_OK;

  // Set ncurses backlight option on/off
  ctrlLcdNcurGrSet(TO_UINT8_T(argDouble[0]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdPrint
//
// Print controller state and registers
//
int doLcdPrint(cmdLine_t *cmdLine)
{
  // Print controller state and registers
  ctrlRegPrint();

  return CMD_RET_OK;
}

//
// Function: doLcdRead
//
// Read data from controller lcd using the internal controller cursor
//
int doLcdRead(cmdLine_t *cmdLine)
{
  char *varName;
  int varId;
  u08 lcdByte;

  // Read data from controller lcd buffer
  lcdByte = ctrlExecute(CTRL_METHOD_READ, TO_U08(argDouble[0]), 0);

  // Assign the lcd byte value to the variable
  varId = varIdGet(argWord[1], GLCD_TRUE);
  if (varId < 0)
  {
    printf("%s? internal error\n", cmdLine->cmdCommand->cmdArg[1].argName);
    return CMD_RET_ERROR;
  }
  varValSet(varId, (double)lcdByte);

  // Print the variable holding the lcd byte
  if (echoCmd == CMD_ECHO_YES)
  {
    varName = malloc(strlen(argWord[1]) + 3);
    sprintf(varName, "^%s$", argWord[1]);
    varPrint(varName, GLCD_FALSE);
    free(varName);
  }

  return CMD_RET_OK;
}

//
// Function: doLcdStartLineSet
//
// Set display startline in controllers
//
int doLcdStartLineSet(cmdLine_t *cmdLine)
{
  u08 payload;

  // Send display startline to controller 0 and 1
  payload = TO_U08(argDouble[0]);
  ctrlExecute(CTRL_METHOD_COMMAND, 0, GLCD_START_LINE | payload);
  payload = TO_U08(argDouble[1]);
  ctrlExecute(CTRL_METHOD_COMMAND, 1, GLCD_START_LINE | payload);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdWrite
//
// Write data to controller lcd using the internal controller cursor
//
int doLcdWrite(cmdLine_t *cmdLine)
{
  // Write data to controller lcd
  ctrlExecute(CTRL_METHOD_WRITE, TO_U08(argDouble[0]), TO_U08(argDouble[1]));
  ctrlLcdFlush();

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
  u08 startWait = GLCD_FALSE;
  u08 myKbMode = KB_MODE_LINE;

  // Get the start mode
  startWait = emuStartModeGet(argChar[0]);

  // Clear active clock (if any)
  emuClockRelease(CMD_ECHO_NO);

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Set essential Monochron startup data
  mcClockOldTS = mcClockOldTM = mcClockOldTH = 0;
  mcClockNewTS = mcClockNewTM = mcClockNewTH = 0;
  mcClockOldDD = mcClockOldDM = mcClockOldDY = 0;
  mcClockNewDD = mcClockNewDM = mcClockNewDY = 0;
  mcClockPool = monochron;
  mcMchronClock = 0;
  mcClockTimeEvent = GLCD_FALSE;
  mcClockDateEvent = GLCD_FALSE;
  animDisplayMode = SHOW_TIME;
  almSwitchOn = GLCD_FALSE;
  almAlarming = GLCD_FALSE;
  almTickerAlarm = 0;
  almTickerSnooze = 0;

  // Clear the screen so we won't see any flickering upon changing the
  // backlight later on
  glcdClearScreen(GLCD_OFF);

  // Upon request force the eeprom to init
  if (argChar[1] == 'r')
    stubEepReset();

  // Set the backlight as stored in the eeprom
  myBacklight = eeprom_read_byte((uint8_t *)EE_BRIGHT) >> OCR2B_BITSHIFT;
  ctrlLcdBacklightSet(myBacklight);

  // Init stub event handler used in Monochron
  stubEventInit(startWait, stubHelpMonochron);

  // Start Monochron and witness the magic :-)
  monoMain();

  // We're done.
  // Restore the clock pool that mchron supports (as it was overridden by the
  // Monochron clock pool). By clearing the active clock from that pool also
  // any audible alarm will be stopped and reset.
  mcClockPool = emuMonochron;
  emuClockRelease(CMD_ECHO_NO);

  // Restore alarm, foreground/background color and backlight as they were
  // prior to starting Monochron
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;
  mcBgColor = emuBgColor;
  mcFgColor = emuFgColor;
  ctrlLcdBacklightSet(emuBacklight);
  ctrlLcdFlush();

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
  ctrlLcdFlush();

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
  ctrlLcdFlush();

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

  // Get color
  color = emuColorGet(argChar[0]);

  // Draw filled circle
  glcdFillCircle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), TO_U08(argDouble[3]), color);
  ctrlLcdFlush();

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
  ctrlLcdFlush();

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
  ctrlLcdFlush();

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
  ctrlLcdFlush();

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
  ctrlLcdFlush();

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
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doRepeatFor
//
// Initiate a new or continue a repeat loop
//
int doRepeatFor(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;

  // Execute the repeat logic
  if (cmdPcCtrlChild->active == GLCD_FALSE)
  {
    // First entry for this loop. Make the repeat active, then evaluate the
    // repeat init and the repeat condition expressions.
    cmdPcCtrlChild->active = GLCD_TRUE;
    if (exprEvaluate(cmdArg[0].argName, cmdLine->args[0]) != CMD_RET_OK)
      return CMD_RET_ERROR;
    if (exprEvaluate(cmdArg[1].argName, cmdLine->args[1]) != CMD_RET_OK)
      return CMD_RET_ERROR;
  }
  else
  {
    // For a next repeat loop first evaluate the step expression and then
    // re-evaluate the repeat condition expression
    if (exprEvaluate(cmdArg[2].argName, cmdLine->args[2]) != CMD_RET_OK)
      return CMD_RET_ERROR;
    if (exprEvaluate(cmdArg[1].argName, cmdLine->args[1]) != CMD_RET_OK)
      return CMD_RET_ERROR;
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

  // Decide where to go depending on whether the loop is still active
  if (cmdPcCtrlParent->active == GLCD_TRUE)
  {
    // Jump back to top of repeat (and evaluate there whether the repeat loop
    // will continue)
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
  ctrlStatsPrint(CTRL_STATS_ALL);

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
  ctrlStatsReset(CTRL_STATS_ALL);

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
  rtcTimeRead();

  // Sync mchron time with (new) time
  emuTimeSync();

  // Update clock when active
  emuClockUpdate();

  // Report (new) time+date+alarm
  if (echoCmd == CMD_ECHO_YES)
    emuTimePrint(ALM_EMUCHRON);

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
  rtcTimeRead();

  // Report time+date+alarm
  emuTimePrint(ALM_EMUCHRON);

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
    emuTimePrint(ALM_EMUCHRON);

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
    emuTimePrint(ALM_EMUCHRON);

  return CMD_RET_OK;
}

//
// Function: doVarPrint
//
// Print value of variables in rows with max 8 variable values per row
//
int doVarPrint(cmdLine_t *cmdLine)
{
  int retVal;

  // Print all variables matching a regexp pattern where '.' is all
  retVal = varPrint(argWord[1], GLCD_TRUE);
  if (retVal != CMD_RET_OK)
    printf("%s? invalid\n", cmdLine->cmdCommand->cmdArg[0].argName);

  return retVal;
}

//
// Function: doVarReset
//
// Clear one or all used named variables
//
int doVarReset(cmdLine_t *cmdLine)
{
  int varId;
  int varInUse;
  int retVal;

  // Clear all variables
  if (strcmp(argWord[1], ".") == 0)
  {
    varInUse = varReset();
    if (echoCmd == CMD_ECHO_YES)
      printf("reset variables: %d\n", varInUse);
    return CMD_RET_OK;
  }

  // Clear the single variable
  varId = varIdGet(argWord[1], GLCD_FALSE);
  if (varId < 0)
  {
    printf("%s? not in use\n", cmdLine->cmdCommand->cmdArg[0].argName);
    return CMD_RET_ERROR;
  }
  retVal = varClear(varId);
  if (retVal != CMD_RET_OK)
    printf("%s? not in use\n", cmdLine->cmdCommand->cmdArg[0].argName);

  return retVal;
}

//
// Function: doVarSet
//
// Init and set named variable. Note that the actual expression evaluation and
// variable assignment has been performed by the command argument scan module.
// When the expression evaluator failed this function won't be called.
// Only when the expression was evaluated successfully this function is called
// but there's nothing else left for us to do except for returning the
// successful technical result of the expression evaluator.
//
int doVarSet(cmdLine_t *cmdLine)
{
  return CMD_RET_OK;
}

//
// Function: doWait
//
// Wait for keypress or pause in multiples of 1 msec.
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
      ch = waitKeypress(GLCD_FALSE);
    else
      ch = waitKeypress(GLCD_TRUE);
  }
  else
  {
    // Wait delay*0.001 sec
    ch = waitDelay(delay);
  }

  // A 'q' will interrupt any command execution
  if (ch == 'q' && listExecDepth > 0)
  {
    printf("quit\n");
    return CMD_RET_INTERRUPT;
  }

  return CMD_RET_OK;
}

//
// Function: doWaitTimerExpiry
//
// Wait for timer expiry in multiples of 1 msec.
//
int doWaitTimerExpiry(cmdLine_t *cmdLine)
{
  int delay = 0;
  suseconds_t remaining;
  char ch = '\0';

  // Wait for timer expiry (if not already expired)
  delay = TO_INT(argDouble[0]);
  ch = waitTimerExpiry(&tvTimer, delay, GLCD_TRUE, &remaining);

  // A 'q' will interrupt any command execution
  if (ch == 'q' && listExecDepth > 0)
  {
    printf("quit\n");
    return CMD_RET_INTERRUPT;
  }

  return CMD_RET_OK;
}

//
// Function: doWaitTimerStart
//
// Start a new wait timer
//
int doWaitTimerStart(cmdLine_t *cmdLine)
{
  waitTimerStart(&tvTimer);
  return CMD_RET_OK;
}
