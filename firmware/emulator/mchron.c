//*****************************************************************************
// Filename : 'mchron.c'
// Title    : Main entry and command line utility for the MONOCHRON emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <readline/readline.h>
#include <readline/history.h>

// Monochron defines
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "../ratt.h"
#include "../glcd.h"
#include "../anim.h"
#include "../config.h"

// Monochron clocks
#include "../clock/analog.h"
#include "../clock/digital.h"
#include "../clock/nerd.h"
#include "../clock/mosquito.h"
#include "../clock/pong.h"
#include "../clock/puzzle.h"
#include "../clock/slider.h"
#include "../clock/cascade.h"
#include "../clock/speeddial.h"
#include "../clock/spiderplot.h"
#include "../clock/trafficlight.h"
#include "../clock/bigdigit.h"
#include "../clock/qr.h"

// Mchron stubs, utilities and command profile defines
#include "stub.h"
#include "stubrefs.h"
#include "lcd.h"
#include "scanutil.h"
#include "scanprofile.h"
#include "mchronutil.h"

// Monochron defined data
extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t date_d, date_m, date_y;
extern volatile uint8_t new_ts;
extern volatile uint8_t time_event;
extern volatile uint8_t just_pressed;
extern volatile uint8_t alarmOn;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcBgColor, mcFgColor;
extern volatile uint8_t mcAlarming, mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern volatile uint8_t mcAlarmSwitch;
extern volatile uint8_t mcUpdAlarmSwitch;
extern volatile uint8_t mcCycleCounter;
extern clockDriver_t *mcClockPool;

// The variables that store the processed command line arguments
extern char argChar[];
extern int argInt[];
extern char *argWord[];
extern char *argString;

// Help function when running the clock or Monochron emulator
extern void (*stubHelp)(void);

// External data from expression evaluator
extern int exprValue;
extern int varError;

// The command list execution depth tells us if we're running at
// command prompt level
extern int listExecDepth;

// Event handler stub interface
extern int eventInit;
extern int eventCycleState;

#ifdef MARIO
// Mario chiptune alarm sanity check data
extern const int marioTonesLen;
extern const int marioBeatsLen;
#endif

// This is me
extern const char *__progname;

// The emulator background/foreground color of the LCD display and backlight
// OFF = 0 = black color (=0x0 bit value in LCD memory)
// ON  = 1 = white color (=0x1 bit value in LCD memory)
u08 emuBgColor = OFF;
u08 emuFgColor = ON;
u08 emuBacklight = 16;

// Current command file execution depth
int fileExecDepth = 0;

// Initial user definable mchron alarm time
uint8_t emuAlarmH = 22;
uint8_t emuAlarmM = 9;

// Data resulting from mchron startup command line processing
argcArgv_t argcArgv;

// The command line input stream control structure
cmdInput_t cmdInput;

// The clocks supported in the mchron clock test environment.
// Note that Monochron will have its own implemented array of supported
// clocks in anim.c [firmware]. So, we need to switch between the two
// arrays when appropriate.
clockDriver_t emuMonochron[] =
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
};

// Main command handler function prototypes
int doAlarmPosition(char *input, int echoCmd);
int doAlarmSet(char *input, int echoCmd);
int doBeep(char *input, int echoCmd);
int doClockFeed(char *input, int echoCmd);
int doClockSet(char *input, int echoCmd);
int doComments(char *input, int echoCmd);
int doDate(char *input, int echoCmd, int reset);
int doExecute(char *input, int echoCmd);
int doExit(char *input, int echoCmd);
int doHelp(char *input, int echoCmd);
int doInput(cmdInput_t *cmdInput, int echoCmd);
int doLcdBacklightSet(char *input, int echoCmd);
int doLcdErase(char *input, int echoCmd);
int doLcdInverse(char *input, int echoCmd);
int doMonochron(char *input, int echoCmd);
int doPaintAscii(char *input, int echoCmd);
int doPaintCircle(char *input, int echoCmd, int fill);
int doPaintDot(char *input, int echoCmd);
int doPaintLine(char *input, int echoCmd);
int doPaintRectangle(char *input, int echoCmd, int fill);
int doRepeatNext(char *input, int echoCmd);
int doRepeatWhile(cmdInput_t *cmdInput, int echoCmd);
int doStats(char *input, int echoCmd, int reset);
int doTime(char *input, int echoCmd, char type);
int doVarPrint(char *input, int echoCmd);
int doVarReset(char *input, int echoCmd);
int doVarSet(char *input, int echoCmd);
int doWait(char *input, int echoCmd);

//
// Function: main
//
// Main program for Emuchron command shell
//
int main(int argc, char *argv[])
{
  char prompt[50];
  int retVal = CMD_RET_OK;

  // Setup signal handlers to either recover from signal or to attempt
  // graceful non-standard exit
  emuSigSetup();

  // Do command line processing and setup the LCD device parameters
  retVal = emuArgcArgv(argc, argv, &argcArgv);
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

  // Init the LCD color modes
  mcBgColor = emuBgColor;
  mcFgColor = emuFgColor;

  // Init initial alarm
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;

  // Init the LCD emulator device(s)
  lcdDeviceInit(argcArgv.lcdDeviceParam);

  // Uncomment this if you want to join with debugger prior to using
  // anything in the glcd library for the LCD device
  //char tmpInput[10];
  //fgets(tmpInput, 10, stdin);

  // Clear and show welcome message on LCD device
  beep(4000, 100);
  lcdDeviceBacklightSet(emuBacklight);
  glcdInit(mcBgColor);
  glcdPutStr2(1, 1, FONT_5X5P, "* Welcome to Emuchron Emulator *", mcFgColor);
  glcdPutStr2(1, 8, FONT_5X5P, "Enter 'h' for help", mcFgColor);
  lcdDeviceFlush(0);

  // Open debug logfile when requested
  if (argcArgv.argDebug != 0)
  {
    emuLogfileOpen(argv[argcArgv.argDebug]);
  }

  // Show our process id and (optional) ncurses output device
  // (handy for attaching a debugger :-)
  printf("\n%s PID = %d\n", __progname, getpid());
  if (argcArgv.lcdDeviceParam.useNcurses == 1)
    printf("ncurses tty = %s\n", argcArgv.lcdDeviceParam.lcdNcurTty);
  printf("\n");
  
  // Init the clock pool supported in mchron command line mode
  mcClockPool = (clockDriver_t *)emuMonochron;

  // Init the stubbed alarm switch to 'Off' and clear audible alarm
  alarmSwitchSet(GLCD_FALSE, GLCD_FALSE);
  alarmSoundKill();

  // Init emuchron system clock and report time/date/alarm
  readi2ctime();
  emuTimePrint();

  // Init functional clock plugin time
  mchronTimeInit();

  // Reset named signed int variables a..z, aa..zz
  varReset();

  // Init the command line input interface
  cmdInput.file = stdin;
  if (argcArgv.lcdDeviceParam.useNcurses == 0)
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

  // Do the first command line read
  sprintf(prompt, "%s> ", __progname);
  cmdInputRead(prompt, &cmdInput);

  // Keep processing input lines until done
  while (cmdInput.input != NULL)
  {
    // Process input
    retVal = doInput(&cmdInput, CMD_ECHO_YES);
    if (retVal == CMD_RET_EXIT)
      break;

    // Get next command
    cmdInputRead(prompt, &cmdInput);
  }

  // Done: caused by 'x' or ^D

  // Cleanup command line read interface
  cmdInputCleanup(&cmdInput);

  // Shutdown gracefully by killing audio and stopping the LCD device(s)
  alarmSoundKill();
  lcdDeviceEnd();
  
  // Stop debug output 
  DEBUGP("**** logging stopped");
  emuLogfileClose();

  // Tell user if exit was due to manual EOF
  if (retVal != CMD_RET_EXIT)
  {
    printf("\n<ctrl>d - exit\n");
  }

  // Goodbye
  return CMD_RET_OK;
}

//
// Function: doAlarmPosition
//
// Set alarm switch position
//
int doAlarmPosition(char *input, int echoCmd)
{
  uint8_t on = GLCD_TRUE;
  
  // Scan command line with alarm switch position profile
  ARGSCAN(argAlarmPosition, &input);

  // Define new alarm switch position
  if (argInt[0] == 0)
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
    // Get current time+date
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
int doAlarmSet(char *input, int echoCmd)
{
  // Overide alarm
  // Scan command line with alarm set profile
  ARGSCAN(argAlarmSet, &input);

  // Set new alarm time
  emuAlarmH = ((uint8_t)argInt[0]) % 24;
  emuAlarmM = ((uint8_t)argInt[1]) % 60;
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
      // will overwrite the old value on the LCD. However, for a clock like
      // Analog that shows the alarm in an analog clock style, changing the
      // alarm in command mode will draw the new alarm while not erasing
      // the old alarm time.
      // We'll use a trick to overwrite the old alarm: toggle the alarm
      // switch twice. Note: This may cause a slight blink in the alarm
      // area when using the glut LCD device.
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
    // Get current time+date
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
int doBeep(char *input, int echoCmd)
{
  // Scan command line with beep profile
  ARGSCAN(argBeep, &input);

  // Sound beep
  beep(argInt[0], (uint8_t)argInt[1]);

  return CMD_RET_OK;
}

//
// Function: doClockFeed
//
// Feed clock with time and keyboard events
//
int doClockFeed(char *input, int echoCmd)
{
  char ch = '\0';
  int startMode = CYCLE_NOWAIT;
  int myKbMode = KB_MODE_LINE;

  // Scan command line with clock feed profile
  ARGSCAN(argClockFeed, &input);

  // Get the start mode
  emuStartModeGet(argChar[0], &startMode);

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

  // Setup and provide emulator end-user help
  stubHelp = stubHelpClockFeed;
  stubHelp();

  // Init alarm and functional clock time
  mcAlarming = GLCD_FALSE;
  mchronTimeInit();

  // Init stub event handler
  eventInit = GLCD_TRUE;
  eventCycleState = startMode;

  // Run clock until 'q'
  while (ch != 'q' && ch != 'Q')
  {
    // Get timer event and execute clock cycle
    ch = stubGetEvent();

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
// Function: doClockSet
//
// Select clock from list of available clocks
//
int doClockSet(char *input, int echoCmd)
{
  // Scan command line with clock set profile
  ARGSCAN(argClockSelect, &input);

  if (argInt[0] == CHRON_NONE)
  {
    // Release clock
    emuClockRelease(echoCmd);
  }
  else
  {
    // Init clock layout for selected clock
    if (argInt[0] > sizeof(emuMonochron) / sizeof(clockDriver_t) - 1)
    {
      // Requested clock is beyond max value
      printf("%s? invalid value: %d\n", argClockSelect[0].argName, argInt[0]);
      return CMD_RET_ERROR;
    }
    alarmSoundKill();
    mcClockTimeEvent = GLCD_TRUE;
    mcMchronClock = (uint8_t)argInt[0];
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
int doComments(char *input, int echoCmd)
{
  // Dump comments in the log only when we run at root command level
  if (listExecDepth == 0)
  {
     DEBUGP(input);
  }

  return CMD_RET_OK;
}

//
// Function: doDate
//
// Set/reset internal clock date
//
int doDate(char *input, int echoCmd, int reset)
{
  int dateOk = GLCD_TRUE;

  if (reset == GLCD_TRUE)
  {
    // Reset date to system date
    // The command line may not contain additional arguments
    ARGSCAN(argEnd, &input);

    stubTimeSet(70, 0, 0, 0, 80, 0, 0);
  }
  else
  {
    // Set date
    // Scan command line with date set profile
    ARGSCAN(argDateSet, &input);

    dateOk = stubTimeSet(70, 0, 0, 0, (uint8_t)argInt[0], (uint8_t)argInt[1],
      (uint8_t)argInt[2]);
    if (dateOk == GLCD_FALSE)
      return CMD_RET_ERROR;
  }

  // Sync mchron time with new date
  new_ts = 60;
  DEBUGP("Clear time event");
  time_event = GLCD_FALSE;
  mchronTimeInit();

  // Update clock when active
  emuClockUpdate();

  // Report the new date settings
  if (echoCmd == CMD_ECHO_YES)
  {
    emuTimePrint();
  }

  return CMD_RET_OK;
}

//
// Function: doExecute
//
// Execute mchron commands from a file
//
int doExecute(char *input, int echoCmd)
{
  int echo = 0;
  char fileName[250];
  int i = 0;
  cmdLine_t *cmdLineRoot = NULL;
  cmdRepeat_t *cmdRepeatRoot = NULL;
  int retVal = CMD_RET_OK;

  // Verify too deep nested 'e' commands (prevent potential recursive call) 
  if (fileExecDepth >= CMD_FILE_DEPTH_MAX)
  {
    printf("stack level exceeded by last 'e' command (max=%d).\n",
      CMD_FILE_DEPTH_MAX);  
    return CMD_RET_ERROR;
  }

  // Scan command line with execute profile
  ARGSCAN(argExecute, &input);

  // Get echo
  if (argChar[0] == 'e')
    echo = CMD_ECHO_YES;
  else if (argChar[0] == 'i')
    echo = echoCmd;
  else // argChar[0] == 's'
    echo = CMD_ECHO_NO;

  // Copy filename
  while (argString[i] != '\0')
  {
    fileName[i] = argString[i];
    i++;
  }
  fileName[i] = '\0';

  // Load the lines from the command file in a linked list.
  // Warning: this will reset the cmd scan global variables.
  retVal = cmdFileLoad(&cmdLineRoot, &cmdRepeatRoot, fileName);
  if (retVal == CMD_RET_OK)
  {
    // Valid command file: increase stack level
    fileExecDepth++;

    // Execute the commands in the command list
    retVal = emuListExecute(cmdLineRoot, fileName, echo, doInput);

    // We're done: decrease stack level
    fileExecDepth--;
  }

  // We're done. Either all commands in the linked list have been executed
  // successfullly or an error has occured. Do some admin stuff by
  // cleaning up the linked lists.
  cmdListCleanup(cmdLineRoot, cmdRepeatRoot);

  return retVal;
}

//
// Function: doExit
//
// Prepare to exit mchron
//
int doExit(char *input, int echoCmd)
{
  int retVal = CMD_RET_OK;

  // The command line may not contain additional arguments
  ARGSCAN(argEnd, &input);

  if (listExecDepth > 0)
  {
    printf("use only at command prompt\n");
    retVal = CMD_RET_ERROR;
  }
  else
  {
    // Indicate we want to exit 
    retVal = CMD_RET_EXIT;
  }
  
  return retVal;
}

//
// Function: doHelp
//
// Dump helppage
//
int doHelp(char *input, int echoCmd)
{
  // The command line may not contain additional arguments
  ARGSCAN(argEnd, &input);

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
// Function: doInput
//
// Process input string.
// This is the main command input handler that can be called recursively
// via the 'e' command.
//
int doInput(cmdInput_t *cmdInput, int echoCmd)
{
  char *input = cmdInput->input;
  int retVal = CMD_RET_OK;

  // See if we have an empty command line
  if (*input == '\0')
  {
    // Do nothing (but optionally dump empty line in logfile)
    retVal = doComments(cmdInput->input, echoCmd);
    return retVal;
  }

  // Prepare command line argument scanner
  argInit(&input);
  if (*input == '\0')
  {
    // Input only contains white space chars
    return retVal;
  }

  // We have non-whitespace characters so scan the command line with
  // the command profile
  ARGSCAN(argCmd, &input);

  // Process the main command
  if (strcmp(argWord[0], "#") == 0)
  {
    // Do comments
    retVal = doComments(cmdInput->input, echoCmd);
  }
  else if (strcmp(argWord[0], "ap") == 0)
  {
    // Set alarm switch position
    retVal = doAlarmPosition(input, echoCmd);
  }
  else if (strcmp(argWord[0], "as") == 0)
  {
    // Set alarm time
    retVal = doAlarmSet(input, echoCmd);
  }
  else if (strcmp(argWord[0], "b") == 0)
  {
    // Sound beep
    retVal = doBeep(input, echoCmd);
  }
  else if (strcmp(argWord[0], "cf") == 0)
  {
    // Feed clock in emulator with time and keyboard events
    retVal = doClockFeed(input, echoCmd);
  }
  else if (strcmp(argWord[0], "cs") == 0)
  {
    // Set clock for emulator
    retVal = doClockSet(input, echoCmd);
  }
  else if (strcmp(argWord[0], "dr") == 0)
  {
    // Reset date to system date
    retVal = doDate(input, echoCmd, GLCD_TRUE);
  }
  else if (strcmp(argWord[0], "ds") == 0)
  {
    // Set date
    retVal = doDate(input, echoCmd, GLCD_FALSE);
  }
  else if (strcmp(argWord[0], "e") == 0)
  {
    // Execute mchron commands from a file
    retVal = doExecute(input, echoCmd);
    if (retVal < CMD_RET_OK && listExecDepth == 0)
    {
      // Final stack trace element for error that occured at lower level
      if (retVal == CMD_RET_ERROR_RECOVER || retVal == CMD_RET_INTERRUPT)
        printf("%d:%s:-:%s\n", fileExecDepth, __progname, cmdInput->input);
    }
  }
  else if (strcmp(argWord[0], "h") == 0)
  {
    // Dump help page
    retVal = doHelp(input, echoCmd);
  }
  else if (strcmp(argWord[0], "li") == 0)
  {
    // Inverse screen
    retVal = doLcdInverse(input, echoCmd);
  }
  else if (strcmp(argWord[0], "lbs") == 0)
  {
    // Set LCD backlight
    retVal = doLcdBacklightSet(input, echoCmd);
  }
  else if (strcmp(argWord[0], "le") == 0)
  {
    // Erase LCD display
    retVal = doLcdErase(input, echoCmd);
  }
  else if (strcmp(argWord[0], "m") == 0)
  {
    // Monochron emulator
    retVal = doMonochron(input, echoCmd);
  }
  else if (strcmp(argWord[0], "pa") == 0)
  {
    // Paint ascii
    retVal = doPaintAscii(input, echoCmd);
  }
  else if (strcmp(argWord[0], "pc") == 0)
  {
    // Paint circle
    retVal = doPaintCircle(input, echoCmd, GLCD_FALSE);
  }
  else if (strcmp(argWord[0], "pcf") == 0)
  {
    // Paint filled circle
    retVal = doPaintCircle(input, echoCmd, GLCD_TRUE);
  }
  else if (strcmp(argWord[0], "pd") == 0)
  {
    // Paint dot
    retVal = doPaintDot(input, echoCmd);
  }
  else if (strcmp(argWord[0], "pl") == 0)
  {
    // Paint line
    retVal = doPaintLine(input, echoCmd);
  }
  else if (strcmp(argWord[0], "pr") == 0)
  {
    // Paint rectangle
    retVal = doPaintRectangle(input, echoCmd, GLCD_FALSE);
  }
  else if (strcmp(argWord[0], "prf") == 0)
  {
    // Paint filled rectangle
    retVal = doPaintRectangle(input, echoCmd, GLCD_TRUE);
  }
  else if (strcmp(argWord[0], "q") == 0)
  {
    // The <quit> key is not really a command...
    printf("use only as command interrupt keypress\n");
    retVal = CMD_RET_ERROR;
  }
  else if (strcmp(argWord[0], "rn") == 0)
  {
    // The user has entered a 'rn' command at command prompt level.
    // This is either a syntax or a parse error. Handle it though
    // like any other command.
    // A repeat while/next combination is handled in doListExecute().
    retVal = doRepeatNext(input, echoCmd);
  }
  else if (strcmp(argWord[0], "rw") == 0)
  {
    // The user has entered a 'rw' command at command prompt level.
    // Cache all keyboard input commands until this 'rw' is matched
    // with a corresponding 'rn' command. Then execute the command
    // list.
    retVal = doRepeatWhile(cmdInput, echoCmd);
  }
  else if (strcmp(argWord[0], "sp") == 0)
  {
    // Statistics print
    retVal = doStats(input, echoCmd, GLCD_FALSE);
  }
  else if (strcmp(argWord[0], "sr") == 0)
  {
    // Statistics reset
    retVal = doStats(input, echoCmd, GLCD_TRUE);
  }
  else if (strcmp(argWord[0], "tf") == 0)
  {
    // Flush time to active clock
    retVal = doTime(input, echoCmd, 'f');
  }
  else if (strcmp(argWord[0], "tp") == 0)
  {
    // Print time
    retVal = doTime(input, echoCmd, 'p');
  }
  else if (strcmp(argWord[0], "tr") == 0)
  {
    // Reset time to system time
    retVal = doTime(input, echoCmd, 'r');
  }
  else if (strcmp(argWord[0], "ts") == 0)
  {
    // Set time
    retVal = doTime(input, echoCmd, 's');
  }
  else if (strcmp(argWord[0], "vp") == 0)
  {
    // Print value of one or all variable
    retVal = doVarPrint(input, echoCmd);
  }
  else if (strcmp(argWord[0], "vr") == 0)
  {
    // Reset one or all all variables
    retVal = doVarReset(input, echoCmd);
  }
  else if (strcmp(argWord[0], "vs") == 0)
  {
    // Set value of variable
    retVal = doVarSet(input, echoCmd);
  }
  else if (strcmp(argWord[0], "w") == 0)
  {
    // Wait delay time or keypress
    retVal = doWait(input, echoCmd);
  }
  else if (strcmp(argWord[0], "x") == 0)
  {
    // Exit mchron
    retVal = doExit(input, echoCmd);
  }
  else
  {
    printf("%s? invalid value: %s\n", argCmd[0].argName, argWord[0]);
    retVal = CMD_RET_ERROR;
  }

  return retVal;
}

//
// Function: doLcdBacklightSet
//
// Set LCD backlight (0 = almost dark .. 16 = full power).
// Note: Only the glut LCD stub supports backlight .
//
int doLcdBacklightSet(char *input, int echoCmd)
{
  // Scan command line with backlight profile
  ARGSCAN(argBacklight, &input);

  // Process backlight
  emuBacklight = argInt[0];
  lcdDeviceBacklightSet(emuBacklight);

  return CMD_RET_OK;
}

//
// Function: doLcdErase
//
// Erase the contents of the LCD screen
//
int doLcdErase(char *input, int echoCmd)
{
  // The command line may not contain additional arguments
  ARGSCAN(argEnd, &input);

  // Erase LCD display and flush the display
  glcdClearScreen(mcBgColor);
  lcdDeviceFlush(1);

  return CMD_RET_OK;
}

//
// Function: doLcdInverse
//
// Inverse the contents of the LCD screen
//
int doLcdInverse(char *input, int echoCmd)
{
  // The command line may not contain additional arguments
  ARGSCAN(argEnd, &input);

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

  // Inverse and flush the display
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
int doMonochron(char *input, int echoCmd)
{
  u08 myBacklight = 16;
  int startMode = CYCLE_NOWAIT;
  int myKbMode = KB_MODE_LINE;

  // Scan command line with monochron profile
  ARGSCAN(argMchronStart, &input);

  // Get the start mode
  emuStartModeGet(argChar[0], &startMode);

  // Clear active clock (if any)
  emuClockRelease(CMD_ECHO_NO);

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Setup and provide emulator end-user help
  stubHelp = stubHelpMonochron;
  stubHelp();

  // Init stub event handler
  eventInit = GLCD_TRUE;
  eventCycleState = startMode;

  // Set essential Monochron startup data
  mcClockTimeEvent = GLCD_FALSE;
  mcAlarmSwitch = ALARM_SWITCH_NONE;
  just_pressed = 0;

  // Clear the screen so we won't see any flickering upon
  // changing the backlight later on
  glcdClearScreen(OFF);

  // Upon request force the eeprom to init and, based on that,
  // set the backlight of the LCD stub device
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

  // Start Monochron and witness the magic :-)
  stubMain();

  // We're done.
  // Restore the clock pool that mchron supports (as it was overridden
  // by the Monochron clock pool). By clearing the active clock from that
  // pool also any audible alarm will be stopped and reset.
  mcClockPool = (clockDriver_t *)emuMonochron;
  emuClockRelease(CMD_ECHO_NO);

  // Restore alarm and foreground/background color and backlight as they
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
int doPaintAscii(char *input, int echoCmd)
{
  u08 len = 0;
  u08 orientation = ORI_HORIZONTAL;
  u08 font = FONT_5X5P;
  int color;

  // Scan paint ascii text with x/y font scaling profile
  ARGSCAN(argPaintAscii, &input);

  // Get color
  emuColorGet(argChar[0], &color);

  // Get font
  if (strcmp(argWord[1], "5x5p") == 0)
    font = FONT_5X5P;
  else // argWord[1] == "5x7n")
    font = FONT_5X7N;

  // Get orientation
  if (argChar[1] == 'b')
    orientation = ORI_VERTICAL_BU;
  else if (argChar[1] == 'h')
    orientation = ORI_HORIZONTAL;
  else // argChar[1] == 't'
    orientation = ORI_VERTICAL_TD;

  // Paint ascii based on text orientation
  if (orientation == ORI_HORIZONTAL)
  {
    // Horizontal text
    len = glcdPutStr3((u08)argInt[0], (u08)argInt[1], font, argString,
      (u08)argInt[2], (u08)argInt[3], (u08)color);
    if (echoCmd == CMD_ECHO_YES)
      printf("hor px=%d\n", (int)len);
  }
  else
  {
    // Vertical text
    len = glcdPutStr3v((u08)argInt[0], (u08)argInt[1], font, orientation,
      argString, (u08)argInt[2], (u08)argInt[3], (u08)color);
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
int doPaintCircle(char *input, int echoCmd, int fill)
{
  int color;

  if (fill == GLCD_FALSE)
  {
    // Scan circle profile
    ARGSCAN(argPaintCircle, &input);

    // Get color
    emuColorGet(argChar[0], &color);

    // Draw circle
    glcdCircle2((u08)argInt[0], (u08)argInt[1], (u08)argInt[2], (u08)argInt[3],
      (u08)color);
  }
  else
  {
    // Scan fill circle profile
    ARGSCAN(argPaintCircleFill, &input);

    // Get color
    emuColorGet(argChar[0], &color);

    // All fill patterns are allowed except inverse
    if (argInt[3] == FILL_INVERSE)
    {
      printf("%s? invalid value: %d\n", argPaintCircleFill[4].argName, argInt[3]);
      return CMD_RET_ERROR;
    }

    // Draw filled circle
    glcdFillCircle2((u08)argInt[0], (u08)argInt[1], (u08)argInt[2],
      (u08)argInt[3], (u08)color);
  }
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintDot
//
// Paint dot
//
int doPaintDot(char *input, int echoCmd)
{
  int color;

  // Scan dot profile
  ARGSCAN(argPaintDot, &input);

  // Get color
  emuColorGet(argChar[0], &color);

  // Draw dot
  glcdDot((u08)argInt[0], (u08)argInt[1], (u08)color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintLine
//
// Paint line
//
int doPaintLine(char *input, int echoCmd)
{
  int color;

  // Scan line profile
  ARGSCAN(argPaintLine, &input);

  // Get color
  emuColorGet(argChar[0], &color);

  // Draw line
  glcdLine((u08)argInt[0], (u08)argInt[1], (u08)argInt[2], (u08)argInt[3],
    (u08)color);
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doPaintRectangle
//
// Paint rectangle
//
int doPaintRectangle(char *input, int echoCmd, int fill)
{
  int color;

  if (fill == GLCD_FALSE)
  {
    // Scan paint rectangle profile
    ARGSCAN(argPaintRect, &input);

    // Get color
    emuColorGet(argChar[0], &color);

    // Draw rectangle
    glcdRectangle((u08)argInt[0], (u08)argInt[1], (u08)argInt[2], (u08)argInt[3],
      (u08)color);
  }
  else
  {
    // Scan fill reactangle profile
    ARGSCAN(argPaintRectFill, &input);

    // Get color
    emuColorGet(argChar[0], &color);

    // Draw filled rectangle
    glcdFillRectangle2((u08)argInt[0], (u08)argInt[1], (u08)argInt[2],
      (u08)argInt[3], (u08)argInt[4], (u08)argInt[5], (u08)color);
  }
  lcdDeviceFlush(0);

  return CMD_RET_OK;
}

//
// Function: doRepeatNext
//
// Invalid repeat next at command prompt level
//
int doRepeatNext(char *input, int echoCmd)
{
  // This 'rn' command is entered at command prompt level so it can
  // never be a valid one. If it is syntactically correct then it is
  // semantically incorrect as it cannot be matched with a 'rw' command.
  // A repeat while/next combination is handled in doListExecute().

  // Scan end of command
  ARGSCAN(argEnd, &input);

  // Parse error
  printf("%s? parse error: no matching rw command\n", argCmd[0].argName);
  return CMD_RET_ERROR;
}

//
// Function: doRepeatWhile
//
// Load repeat while loop commands interactively via keyboard and
// execute when command entry is complete.
//
int doRepeatWhile(cmdInput_t *cmdInput, int echoCmd)
{
  cmdLine_t *cmdLineRoot = NULL;
  cmdRepeat_t *cmdRepeatRoot = NULL;
  int retVal = CMD_RET_OK;

  // Load keyboard command lines in a linked list.
  // Warning: this will reset the cmd scan global variables.
  retVal = cmdKeyboardLoad(&cmdLineRoot, &cmdRepeatRoot, cmdInput);
  if (retVal == CMD_RET_OK)
  {
    // Execute the commands in the command list
    retVal = emuListExecute(cmdLineRoot, (char *)__progname, CMD_ECHO_NO,
      doInput);
  }

  // We're done. Either all commands in the linked list have been
  // executed successfullly or an error has occured. Do admin stuff
  // by cleaning up the linked lists.
  cmdListCleanup(cmdLineRoot, cmdRepeatRoot);

  return retVal;
}

//
// Function: doStats
//
// Print/reset stub and LCD performance statistics
//
int doStats(char *input, int echoCmd, int reset)
{
  // The command line may not contain additional arguments
  ARGSCAN(argEnd, &input);

  if (reset == GLCD_FALSE)
  {
    // Print statistics
    statsPrint();
  }
  else
  {
    // Reset statistics
    statsReset();
    if (echoCmd == CMD_ECHO_YES)
    {
      printf("statistics reset\n");
    }
  }
  
  return CMD_RET_OK;
}

//
// Function: doTime
//
// Set internal clock time or report time/date/alarm
//
int doTime(char *input, int echoCmd, char type)
{
  int timeOk = GLCD_TRUE;

  if (type == 'p' || type == 'f')
  {
    // The command line may not contain additional arguments
    ARGSCAN(argEnd, &input);

    // Get current time+date
    readi2ctime();
  }
  else if (type == 'r')
  {
    // The command line may not contain additional arguments
    ARGSCAN(argEnd, &input);

    // Reset time to system time
    stubTimeSet(80, 0, 0, 0, 70, 0, 0);
  }
  else // type = 's'
  {
    // Scan command line with time set profile
    ARGSCAN(argTimeSet, &input);

    // Overide time
    timeOk = stubTimeSet((uint8_t)argInt[2], (uint8_t)argInt[1],
      (uint8_t)argInt[0], 0, 70, 0, 0);
    if (timeOk == GLCD_FALSE)
      return CMD_RET_ERROR;
  }

  // In case time was changed or a flush request is made update running clock
  if (type == 'r' || type == 's' || type == 'f')
  {
    // Sync mchron time with (new) time
    new_ts = 60;
    DEBUGP("Clear time event");
    time_event = GLCD_FALSE;
    mchronTimeInit();

    // Update clock when active
    emuClockUpdate();
  }

  // Report (new) time+date+alarm
  if (echoCmd == CMD_ECHO_YES || type == 'p')
  {
    emuTimePrint();
  }

  return CMD_RET_OK;
}

//
// Function: doVarPrint
//
// Print value of one or all used named variables in rows with max 8
// variable values per row
//
int doVarPrint(char *input, int echoCmd)
{
  int retVal = CMD_RET_OK;

  // Scan command line with var print profile
  ARGSCAN(argVarPrint, &input);

  // Print the value of variable(s)
  retVal = varValPrint(argWord[1], argVarSet[0].argName);

  return retVal;
}

//
// Function: doVarReset
//
// Clear one or all used named variables
//
int doVarReset(char *input, int echoCmd)
{
  int retVal = CMD_RET_OK;

  // Scan command line with var reset profile
  ARGSCAN(argVarReset, &input);

  // Clear the variable
  if (strcmp(argWord[1], "*") == 0)
  {
    varReset();
  }
  else
  {
    retVal = varClear(argWord[1]);
  }

  return retVal;
}

//
// Function: doVarSet
//
// Init and set named variable
//
int doVarSet(char *input, int echoCmd)
{
  int retVal = CMD_RET_OK;

  // Scan command line with var set profile
  ARGSCAN(argVarSet, &input);

  // Make the variable active (when not in use) and set its value
  retVal = varValSet(argWord[1], argInt[0]);
  if (retVal != CMD_RET_OK)
  {
    printf("%s? invalid value: %s\n", argVarSet[0].argName, argWord[1]);
  }
  
  return retVal;
}

//
// Function: doWait
//
// Wait for keypress or pause (in 0.01 sec)
//
int doWait(char *input, int echoCmd)
{
  int delay = 0;
  char ch = '\0';

  // Scan command line with delay profile
  ARGSCAN(argWait, &input);

  delay = argInt[0];
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
    ch = kbWaitDelay(delay * 10);
  }

  // If a 'q' was entered, we need to quit the list execution 
  if ((ch == 'q' || ch == 'Q') && listExecDepth > 0)
  {
    printf("quit\n");
    return CMD_RET_INTERRUPT;
  }

  return CMD_RET_OK;
}

