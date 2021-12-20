//*****************************************************************************
// Filename : 'mchron.c'
// Title    : Main entry and command line utility for the emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Monochron defines
#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../monomain.h"
#include "../ks0108.h"
#include "../ks0108conf.h"
#include "../config.h"
#include "../buttons.h"

// Monochron clocks
#include "../clock/analog.h"
#include "../clock/barchart.h"
#include "../clock/bigdigit.h"
#include "../clock/cascade.h"
#include "../clock/crosstable.h"
#include "../clock/digital.h"
#include "../clock/example.h"
#include "../clock/linechart.h"
#include "../clock/marioworld.h"
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
#include "../clock/wave.h"

// Emuchron defines and utilities
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
#define TO_S08(d)	((s08)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_U08(d)	((u08)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_U16(d)	((u16)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_UINT8_T(d)	((uint8_t)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
#define TO_UINT16_T(d)	((uint16_t)((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))

// Monochron defined data
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
extern volatile uint8_t almAlarmSelect;
extern int16_t almTickerAlarm;
extern uint16_t almTickerSnooze;

// The variables that store the published command line arguments
extern char argChar[];
extern double argDouble[];
extern char *argString[];

// The expression evaluator expression result value
extern double exprValue;

// The command list execution depth tells us if we're running at command prompt
// level
extern int listExecDepth;

// Flag to indicate we're going to exit
extern u08 invokeExit;

// This is me
extern const char *__progname;

// The command line input stream control structure
cmdInput_t cmdInput;

// The current command echo state
u08 echoCmd = CMD_ECHO_YES;

// Initial user definable mchron alarm time
uint8_t emuAlarmH = 22;
uint8_t emuAlarmM = 9;

// Current command file execution depth
int fileExecDepth = 0;

// The timer used for the 'wte' and 'wts' commands
static struct timeval tvTimer;

// Graphics data buffers for use with graphics data and paint commands
emuGrBuf_t emuGrBufs[GRAPHICS_BUFFERS];

// The emulator background/foreground color of the lcd display and backlight
// GLCD_OFF = 0 = black color (=0x0 bit value in lcd memory)
// GLCD_ON  = 1 = white color (=0x1 bit value in lcd memory)
static uint8_t emuBgColor = GLCD_OFF;
static uint8_t emuFgColor = GLCD_ON;
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
  {CHRON_MARIOWORLD,  DRAW_INIT_FULL, marioInit,          marioCycle,          0},
  {CHRON_WAVE,        DRAW_INIT_FULL, waveInit,           waveCycle,           0},
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
  cmdLine_t *cmdLine;
  char *prompt;
  emuArgcArgv_t emuArgcArgv;
  int i;
  u08 retVal;
  u08 success;

  // Verify integrity of the command dictionary
  success = dictVerify();
  if (success == MC_FALSE)
    return CMD_RET_ERROR;

  // Setup signal handlers to either recover from signal or to attempt graceful
  // non-standard exit
  emuSigSetup();

  // Do command line processing
  success = emuArgcArgvGet(argc, argv, &emuArgcArgv);
  if (success == MC_FALSE)
    return CMD_RET_ERROR;

  // Init the lcd color modes
  mcBgColor = emuBgColor;
  mcFgColor = emuFgColor;

  // Init initial alarm
  mcAlarmH = emuAlarmH;
  mcAlarmM = emuAlarmM;

  // Init graphics data buffers
  for (i = 0; i < GRAPHICS_BUFFERS; i++)
    grBufInit(&emuGrBufs[i], MC_FALSE);

  // Open debug logfile when requested
  if (emuArgcArgv.argDebug != 0)
  {
    success = stubLogfileOpen(argv[emuArgcArgv.argDebug]);
    if (success == MC_FALSE)
      return CMD_RET_ERROR;
  }

  // Init the lcd controllers and display stub device(s)
  success = ctrlInit(&emuArgcArgv.ctrlDeviceArgs);
  if (success == MC_FALSE)
    return CMD_RET_ERROR;

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
  glcdInit();
  glcdClearScreen();
  glcdColorSetFg();
  glcdPutStr2(1, 1, FONT_5X5P, "* Welcome to Emuchron Emulator *");
  glcdPutStr2(1, 8, FONT_5X5P, "Enter 'h' for help");
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
  alarmSwitchSet(MC_FALSE, MC_FALSE);
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
  cmdInputInit(&cmdInput, stdin, CMD_INPUT_READLINELIB);

  // All initialization is done!
  printf("\nenter 'h' for help\n");

  // We're in business: give prompt and process keyboard commands until the
  // last proton in the universe has desintegrated (or use 'x' or ^D to exit)

  // Do the first command line read
  prompt = malloc(strlen(__progname) + 3);
  sprintf(prompt, "%s> ", __progname);
  cmdInputRead(prompt, &cmdInput);

  // Create a command line and keep processing input lines until done
  cmdLine = cmdLineCreate(NULL, NULL);
  while (cmdInput.input != NULL)
  {
    // Process input
    cmdLine->lineNum++;
    cmdLine->input = cmdInput.input;
    cmdLine->cmdCommand = NULL;
    retVal = cmdLineExecute(cmdLine, &cmdInput);
    cmdArgCleanup(cmdLine);
    if (retVal == CMD_RET_EXIT)
      break;

    // Get next command
    cmdInputRead(prompt, &cmdInput);
  }

  // Done: caused by 'x' or ^D
  if (retVal != CMD_RET_EXIT)
    printf("\n<ctrl>d - exit\n");

  // Cleanup command line and command line read interface
  free(prompt);
  cmdLine->input = NULL;
  cmdListCleanup(cmdLine, NULL);
  cmdInputCleanup(&cmdInput);

  // Shutdown gracefully by killing audio, stopping the controller and lcd
  // device(s), and cleaning up the named variables and graphics buffers
  alarmSoundStop();
  ctrlCleanup();
  varReset();
  for (i = 0; i < GRAPHICS_BUFFERS; i++)
    grBufInit(&emuGrBufs[i], MC_TRUE);

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
// argChar[], argDouble[] and argString[] arrays, based on the sequence of
// command arguments in the command dictionary.
//
// A control block handler (for if-logic and repeat commands) however must
// decide which command arguments are evaluated, depending on the state of its
// control block. In other words, command arguments are evaluated optionally.
//

//
// Function: doBeep
//
// Give audible beep
//
u08 doBeep(cmdLine_t *cmdLine)
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
u08 doClockFeed(cmdLine_t *cmdLine)
{
  char ch = '\0';
  u08 startWait = MC_FALSE;
  u08 myKbMode = KB_MODE_LINE;

  // Get the start mode
  startWait = emuStartModeGet(argChar[0]);

  // Check clock
  if (mcClockPool[mcMchronClock].clockId == CHRON_NONE)
  {
    printf("%s: no clock is selected\n", cmdLine->cmdCommand->cmdName);
    return CMD_RET_ERROR;
  }

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Init alarm and functional clock time
  mcAlarming = MC_FALSE;
  rtcMchronTimeInit();

  // Init stub event handler used in main loop below and get first event
  stubEventInit(startWait, MC_TRUE, stubHelpClockFeed);
  ch = stubEventGet(MC_TRUE);

  // Run clock until 'q'
  while (ch != 'q')
  {
    // Process keyboard events
    if (ch == 's')
      animClockButton(BTN_SET);
    if (ch == '+')
      animClockButton(BTN_PLUS);

    // Execute a clock cycle for the clock and get next timer event
    animClockDraw(DRAW_CYCLE);
    mcCycleCounter++;
    ch = stubEventGet(MC_TRUE);
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
u08 doClockSelect(cmdLine_t *cmdLine)
{
  uint8_t clock;

  clock = TO_UINT8_T(argDouble[0]);
  if (clock > emuMonochronCount - 1)
  {
    // Requested clock is beyond max value
    printf("%s? invalid: %d\n", cmdLine->cmdCommand->cmdArg[0].argName, clock);
    return CMD_RET_ERROR;
  }

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
u08 doComments(cmdLine_t *cmdLine)
{
  // Dump comments command in the log only when we run at root command level
  if (listExecDepth == 0)
    DEBUGP(cmdLine->input);

  return CMD_RET_OK;
}

//
// Function: doEepromPrint
//
// Print eeprom contents
//
u08 doEepromPrint(cmdLine_t *cmdLine)
{
  emuEepromPrint();

  return CMD_RET_OK;
}

//
// Function: doEepromReset
//
// Reset eeprom contents and init with Monochron defaults
//
u08 doEepromReset(cmdLine_t *cmdLine)
{
  stubEepReset();
  eepInit();

  if (echoCmd == CMD_ECHO_YES)
    printf("eeprom reset\n");

  return CMD_RET_OK;
}

//
// Function: doEepromWrite
//
// Write data to eeprom
//
u08 doEepromWrite(cmdLine_t *cmdLine)
{
  size_t address;

  address = (size_t)(TO_UINT16_T(argDouble[0]));
  eeprom_write_byte((uint8_t *)address, TO_UINT8_T(argDouble[1]));

  return CMD_RET_OK;
}

//
// Function: doExecute
//
// Execute mchron commands from a file
//
u08 doExecute(cmdLine_t *cmdLine)
{
  int myEchoCmd;
  char *fileName;
  cmdLine_t *cmdLineRoot = NULL;
  cmdPcCtrl_t *cmdPcCtrlRoot = NULL;
  u08 retVal = CMD_RET_OK;

  // Verify too deep nested 'e' commands (prevent potential recursive call)
  if (fileExecDepth >= CMD_FILE_DEPTH_MAX)
  {
    printf("%s: max stack level exceeded (max=%d)\n",
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
  // Warning: this will overwrite the cmd scan global variables so we need to
  // copy the filename first for future reference.
  // When ok execute the list. If an error occured prepare for completing a
  // stack trace.
  fileName = malloc(strlen(argString[1]) + 1);
  sprintf(fileName, "%s", argString[1]);
  retVal = cmdListFileLoad(&cmdLineRoot, &cmdPcCtrlRoot,
    cmdLine->cmdCommand->cmdArg[1].argName, fileName, fileExecDepth);
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
u08 doExit(cmdLine_t *cmdLine)
{
  u08 retVal = CMD_RET_OK;

  if (listExecDepth > 0)
  {
    printf("%s: use only at command prompt\n", cmdLine->cmdCommand->cmdName);
    retVal = CMD_RET_ERROR;
  }
  else
  {
    // Indicate we want to exit
    invokeExit = MC_TRUE;
    retVal = CMD_RET_EXIT;
  }

  return retVal;
}

//
// Function: doGrCopy
//
// Copy graphics buffer
//
u08 doGrCopy(cmdLine_t *cmdLine)
{
  u08 from = TO_U08(argDouble[0]);
  u08 to = TO_U08(argDouble[1]);

  // Copy graphics buffer
  grBufCopy(&emuGrBufs[from], &emuGrBufs[to]);

  return CMD_RET_OK;
}

//
// Function: doGrInfo
//
// Print graphics buffer info
//
u08 doGrInfo(cmdLine_t *cmdLine)
{
  u08 i;
  s08 bufferId = TO_S08(argDouble[0]);

  // Show graphics buffer info for a single or all buffers
  if (bufferId >= 0)
  {
    grBufInfoPrint(&emuGrBufs[bufferId]);
  }
  else
  {
    for (i = 0; i < GRAPHICS_BUFFERS; i++)
    {
      printf("- buffer %d", i);
      if (emuGrBufs[i].bufType == GRAPH_NULL)
      {
        printf(" (empty)\n");
      }
      else
      {
        printf("\n");
        grBufInfoPrint(&emuGrBufs[i]);
      }
    }
  }

  return CMD_RET_OK;
}

//
// Function: doGrLoadCtrImg
//
// Load image graphics data in buffer from lcd controllers
//
u08 doGrLoadCtrImg(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 width = TO_U08(argDouble[3]);
  u08 height = TO_U08(argDouble[4]);

  // Load the image data from the lcd controllers
  grBufLoadCtrl(TO_U08(argDouble[1]), TO_U08(argDouble[2]), width, height,
    argChar[0], emuGrBuf);

  // Change buffer type from raw to image
  emuGrBuf->bufType = GRAPH_IMAGE;
  emuGrBuf->bufImgWidth = width;
  emuGrBuf->bufImgHeight = height;
  emuGrBuf->bufImgFrames = emuGrBuf->bufElmCount / width;

  // Show image buffer info
  if (echoCmd == CMD_ECHO_YES)
    grBufInfoPrint(emuGrBuf);

  return CMD_RET_OK;
}

//
// Function: doGrLoadFile
//
// Load raw graphics data in buffer from file
//
u08 doGrLoadFile(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 retVal;

  // Load the raw data
  retVal = grBufLoadFile(cmdLine->cmdCommand->cmdArg[2].argName, argChar[0], 0,
    argString[1], emuGrBuf);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Show raw data buffer info
  if (echoCmd == CMD_ECHO_YES)
    grBufInfoPrint(emuGrBuf);

  return CMD_RET_OK;
}

//
// Function: doGrLoadFileImg
//
// Load image graphics data in buffer from file
//
u08 doGrLoadFileImg(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 width = TO_U08(argDouble[1]);
  u08 height = TO_U08(argDouble[2]);
  u08 formatBits;
  u16 elmExpected;
  u08 retVal;

  // Get graphics data format and expected number of elements
  emuFormatGet(argChar[0], NULL, &formatBits);
  elmExpected = width * ((height - 1) / formatBits + 1);

  // Load the image data
  retVal = grBufLoadFile(cmdLine->cmdCommand->cmdArg[1].argName, argChar[0],
    elmExpected, argString[1], emuGrBuf);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Validate data volume read from file
  if (elmExpected != emuGrBuf->bufElmCount)
  {
    printf("file data incomplete: elements read = %d, elements expected = %d\n",
      emuGrBuf->bufElmCount, elmExpected);
    grBufInit(emuGrBuf, MC_TRUE);
    return CMD_RET_ERROR;
  }

  // Change buffer type from raw to image
  emuGrBuf->bufType = GRAPH_IMAGE;
  emuGrBuf->bufImgWidth = width;
  emuGrBuf->bufImgHeight = height;
  emuGrBuf->bufImgFrames = emuGrBuf->bufElmCount / width;

  // Show image buffer info
  if (echoCmd == CMD_ECHO_YES)
    grBufInfoPrint(emuGrBuf);

  return CMD_RET_OK;
}

//
// Function: doGrLoadFileSpr
//
// Load sprite frame graphics data in buffer from file
//
u08 doGrLoadFileSpr(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 width = TO_U08(argDouble[1]);
  u08 height = TO_U08(argDouble[2]);
  char formatName;
  u16 frames;
  u08 retVal;

  // The graphics data format depends on the sprite height
  if (height <= 8)
    formatName = 'b';
  else if (height <= 16)
    formatName = 'w';
  else
    formatName = 'd';

  // Load the sprite data
  retVal = grBufLoadFile(cmdLine->cmdCommand->cmdArg[1].argName, formatName,
    0, argString[1], emuGrBuf);
  if (retVal != CMD_RET_OK)
    return retVal;

  // Validate data volume read from file
  frames = emuGrBuf->bufElmCount / width;
  if (emuGrBuf->bufElmCount % width != 0)
  {
    printf("file data incomplete: elements read = %d, elements expected = %d\n",
      emuGrBuf->bufElmCount, width * ((int)frames + 1));
    grBufInit(emuGrBuf, MC_TRUE);
    return CMD_RET_ERROR;
  }

  // Change buffer type from raw to sprite
  emuGrBuf->bufType = GRAPH_SPRITE;
  emuGrBuf->bufSprWidth = width;
  emuGrBuf->bufSprHeight = height;
  emuGrBuf->bufSprFrames = frames;

  // Show sprite buffer info
  if (echoCmd == CMD_ECHO_YES)
    grBufInfoPrint(emuGrBuf);

  return CMD_RET_OK;
}

//
// Function: doGrReset
//
// Reset graphics buffer or all buffers
//
u08 doGrReset(cmdLine_t *cmdLine)
{
  u08 i;
  s08 bufferId = TO_S08(argDouble[0]);

  if (bufferId >= 0)
  {
    grBufInit(&emuGrBufs[bufferId], MC_TRUE);
  }
  else
  {
    for (i = 0; i < GRAPHICS_BUFFERS; i++)
      grBufInit(&emuGrBufs[i], MC_TRUE);
    if (echoCmd == CMD_ECHO_YES)
      printf("buffers reset\n");
  }

  return CMD_RET_OK;
}

//
// Function: doGrSaveFile
//
// Save graphics data from buffer in file
//
u08 doGrSaveFile(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 retVal;

  // Check if data was loaded
  if (emuGrBuf->bufType == GRAPH_NULL)
  {
    printf("%s? %d: buffer is empty\n", cmdLine->cmdCommand->cmdArg[0].argName,
      (int)bufferId);
    return CMD_RET_ERROR;
  }

  // Save the buffer data
  retVal = grBufSaveFile(cmdLine->cmdCommand->cmdArg[1].argName,
    TO_U08(argDouble[1]), argString[1], emuGrBuf);
  if (retVal != CMD_RET_OK)
    return retVal;

  return CMD_RET_OK;
}

//
// Function: doHelp
//
// Dump helppage
//
u08 doHelp(cmdLine_t *cmdLine)
{
  if (listExecDepth > 0)
  {
    printf("%s: use only at command prompt\n", cmdLine->cmdCommand->cmdName);
    return CMD_RET_ERROR;
  }

  // Show help using 'more'
  system("/bin/more ../support/help.txt 2>&1");
  return CMD_RET_OK;
}

//
// Function: doHelpCmd
//
// Print the mchron dictionary content
//
u08 doHelpCmd(cmdLine_t *cmdLine)
{
  u08 searchType;
  u08 retVal;

  if (listExecDepth > 0)
  {
    printf("%s: use only at command prompt\n", cmdLine->cmdCommand->cmdName);
    return CMD_RET_ERROR;
  }

  // Print dictionary of command(s) where '.' is all
  searchType = emuSearchTypeGet(argChar[0]);
  retVal = dictPrint(argString[1], searchType);
  if (retVal != CMD_RET_OK)
    printf("%s? invalid: %s\n", cmdLine->cmdCommand->cmdArg[0].argName,
      argString[1]);

  return retVal;
}

//
// Function: doHelpExpr
//
// Print the result of an expression
//
u08 doHelpExpr(cmdLine_t *cmdLine)
{
  // Print expression result in the command shell
  cmdArgValuePrint(argDouble[0], MC_TRUE, MC_TRUE);

  return CMD_RET_OK;
}

//
// Function: doHelpMsg
//
// Show help message
//
u08 doHelpMsg(cmdLine_t *cmdLine)
{
  // Show message in the command shell
  printf("%s\n", argString[1]);

  return CMD_RET_OK;
}

//
// Function: doIf
//
// Initiate an if and determine where to continue
//
u08 doIf(cmdLine_t **cmdProgCounter)
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
    cmdPcCtrlChild->active = MC_TRUE;
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
u08 doIfElse(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;

  // Decide where to go depending on whether the preceding block (if-then or
  // else-if) was active
  if (cmdPcCtrlParent->active == MC_TRUE)
  {
    // Deactivate preceding block and jump to end-if
    cmdPcCtrlParent->active = MC_FALSE;
    *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
  }
  else
  {
    // Make if-else block active and continue on next line
    cmdPcCtrlChild->active = MC_TRUE;
    *cmdProgCounter = (*cmdProgCounter)->next;
  }

  return CMD_RET_OK;
}

//
// Function: doIfElseIf
//
// The start of an if-else-if block
//
u08 doIfElseIf(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;

  // Decide where to go depending on whether the preceding block (if-then or
  // else-if) was active
  if (cmdPcCtrlParent->active == MC_TRUE)
  {
    // Deactivate preceding block and jump to end-if
    cmdPcCtrlParent->active = MC_FALSE;
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
      cmdPcCtrlChild->active = MC_TRUE;
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
u08 doIfEnd(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;

  // Deactivate preceding control block (if anyway) and continue on next line
  cmdPcCtrlParent->active = MC_FALSE;
  *cmdProgCounter = (*cmdProgCounter)->next;

  return CMD_RET_OK;
}


//
// Function: doLcdActCtrlSet
//
// Select active lcd controller
//
u08 doLcdActCtrlSet(cmdLine_t *cmdLine)
{
  ctrlControlSelect(TO_U08(argDouble[0]));

  return CMD_RET_OK;
}

//
// Function: doLcdBacklightSet
//
// Set lcd backlight (0 = almost dark .. 16 = full power).
//
u08 doLcdBacklightSet(cmdLine_t *cmdLine)
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
// Reset active controller and controller lcd cursors, and sync with software
// controller and cursor to (0,0)
//
u08 doLcdCursorReset(cmdLine_t *cmdLine)
{
  // For resetting and syncing first force hardware to use controller 1
  ctrlControlSelect(1);

  // Then reset both controller hardware cursors to (0,0). This will sync the y
  // location with the glcd administered cursor y location, and also sync the
  // active hardware controller with the glcd administered controller.
  glcdSetAddress(64, 7);
  glcdSetAddress(64, 0);
  glcdSetAddress(0, 7);
  glcdSetAddress(0, 0);

  return CMD_RET_OK;
}

//
// Function: doLcdDisplaySet
//
// Switch controller displays on/off
//
u08 doLcdDisplaySet(cmdLine_t *cmdLine)
{
  u08 payload;

  // Send display on/off to controller 0 and 1
  payload = TO_U08(argDouble[0]);
  glcdControlWrite(0, GLCD_ON_CTRL | payload);
  payload = TO_U08(argDouble[1]);
  glcdControlWrite(1, GLCD_ON_CTRL | payload);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdErase
//
// Erase the contents of the lcd screen
//
u08 doLcdErase(cmdLine_t *cmdLine)
{
  // Erase lcd display
  glcdClearScreen();
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdGlutEdit
//
// Edit lcd contents in glut lcd display using a left mouse double-click to
// toggle a pixel
//
u08 doLcdGlutEdit(cmdLine_t *cmdLine)
{
  static struct timeval tvTimer;
  char ch = '\0';
  u08 x;
  u08 y;
  u08 event;

  if (listExecDepth > 0)
  {
    printf("%s: use only at command prompt\n", cmdLine->cmdCommand->cmdName);
    return CMD_RET_ERROR;
  }

  // Ignore if glut device is not used
  if (ctrlDeviceActive(CTRL_DEVICE_GLUT) == MC_FALSE)
    return CMD_RET_OK;

  // Give instructions
  printf("<glut double-click left button = toggle pixel, q = quit> ");
  fflush(stdout);

  // Prepare for keyboard scan and timer loop, and enable double-click events
  kbModeSet(KB_MODE_SCAN);
  waitTimerStart(&tvTimer);
  ctrlGlcdPixEnable();

  // Process events in a timer loop until a 'q' has been pressed
  while (ch != 'q')
  {
    // Wait remaining time for cycle while detecting 'q' keypress
    ch = waitTimerExpiry(&tvTimer, ANIM_TICK_CYCLE_MS, MC_TRUE, NULL);
    if (ch == 'q')
      break;

    // Process double-click event
    event = ctrlGlcdPixGet(&x, &y);
    if (event == MC_TRUE)
    {
      // Toggle the pixel and confirm double-click event so we can get new one
      glcdFillRectangle2(x, y, 1, 1, ALIGN_AUTO, FILL_INVERSE);
      ctrlLcdFlush();
      ctrlGlcdPixConfirm();
    }
  }

  // Disable double-click events and return to keyboard line mode
  ctrlGlcdPixDisable();
  kbModeSet(KB_MODE_LINE);
  printf("\n");

  return CMD_RET_OK;
}

//
// Function: doLcdGlutGrSet
//
// Set glut graphics options
//
u08 doLcdGlutGrSet(cmdLine_t *cmdLine)
{
  // Ignore if glut device is not used
  if (ctrlDeviceActive(CTRL_DEVICE_GLUT) == MC_FALSE)
    return CMD_RET_OK;

  // Set glut pixel bezel and gridline options on/off
  ctrlLcdGlutGrSet(TO_U08(argDouble[0]), TO_U08(argDouble[1]));

  return CMD_RET_OK;
}

//
// Function: doLcdHlReset
//
// Reset (clear) glcd pixel highlight (glut only)
//
u08 doLcdHlReset(cmdLine_t *cmdLine)
{
  // Disable glcd pixel highlight
  ctrlLcdHighlight(MC_FALSE, 0, 0);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdHlSet
//
// Enable glcd pixel highlight (glut only)
//
u08 doLcdHlSet(cmdLine_t *cmdLine)
{
  // Enable glcd pixel highlight
  ctrlLcdHighlight(MC_TRUE, TO_U08(argDouble[0]), TO_U08(argDouble[1]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdInverse
//
// Inverse the contents of the lcd screen and foreground/background/draw colors
//
u08 doLcdInverse(cmdLine_t *cmdLine)
{
  // Toggle the foreground and background colors, and the draw color
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
  if (glcdColorGet() == GLCD_OFF)
    glcdColorSet(GLCD_ON);
  else
    glcdColorSet(GLCD_OFF);

  // Inverse the display
  glcdFillRectangle2(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, ALIGN_AUTO,
    FILL_INVERSE);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdNcurGrSet
//
// Set ncurses graphics options
//
u08 doLcdNcurGrSet(cmdLine_t *cmdLine)
{
  // Ignore if ncurses device is not used
  if (ctrlDeviceActive(CTRL_DEVICE_NCURSES) == MC_FALSE)
    return CMD_RET_OK;

  // Set ncurses backlight option on/off
  ctrlLcdNcurGrSet(TO_U08(argDouble[0]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdPrint
//
// Print controller state and registers
//
u08 doLcdPrint(cmdLine_t *cmdLine)
{
  // Print controller state and registers
  ctrlRegPrint();

  return CMD_RET_OK;
}

//
// Function: doLcdRead
//
// Read data from active lcd controller using the hardware controller cursor
//
u08 doLcdRead(cmdLine_t *cmdLine)
{
  char *varName;
  int varId;
  u08 lcdByte;

  // Read data from controller lcd buffer
  ctrlExecute(CTRL_METHOD_READ);
  lcdByte = (GLCD_DATAH_PIN & 0xf0) | (GLCD_DATAL_PIN & 0x0f);

  // Assign the lcd byte value to the variable
  varId = varIdGet(argString[1], MC_TRUE);
  if (varId < 0)
  {
    printf("%s? internal error\n", cmdLine->cmdCommand->cmdArg[1].argName);
    return CMD_RET_ERROR;
  }
  varValSet(varId, (double)lcdByte);

  // Print the variable holding the lcd byte
  if (echoCmd == CMD_ECHO_YES)
  {
    varName = malloc(strlen(argString[1]) + 3);
    sprintf(varName, "^%s$", argString[1]);
    varPrint(varName, MC_FALSE);
    free(varName);
  }

  return CMD_RET_OK;
}

//
// Function: doLcdStartLineSet
//
// Set display startline in controllers
//
u08 doLcdStartLineSet(cmdLine_t *cmdLine)
{
  u08 payload;

  // Send display startline to controller 0 and 1
  payload = TO_U08(argDouble[0]);
  glcdControlWrite(0, GLCD_START_LINE | payload);
  payload = TO_U08(argDouble[1]);
  glcdControlWrite(1, GLCD_START_LINE | payload);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdWrite
//
// Write data to active lcd controller using the hardware controller cursor
//
u08 doLcdWrite(cmdLine_t *cmdLine)
{
  // Write data to controller lcd
  ctrlPortDataSet(TO_U08(argDouble[0]));
  ctrlExecute(CTRL_METHOD_WRITE);
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doLcdXCursorSet
//
// Send x cursor position to active lcd controller
//
u08 doLcdXCursorSet(cmdLine_t *cmdLine)
{
  u08 payload;

  // Send x position to lcd controller
  payload = TO_U08(argDouble[0]);
  ctrlPortDataSet(GLCD_SET_Y_ADDR | payload);
  ctrlExecute(CTRL_METHOD_CTRL_W);

  return CMD_RET_OK;
}

//
// Function: doLcdCursorYSet
//
// Send y cursor position to active lcd controller
//
u08 doLcdYCursorSet(cmdLine_t *cmdLine)
{
  u08 payload;

  // Send y page to lcd controller
  payload = TO_U08(argDouble[0]);
  ctrlPortDataSet(GLCD_SET_PAGE | payload);
  ctrlExecute(CTRL_METHOD_CTRL_W);

  return CMD_RET_OK;
}

//
// Function: doMonochron
//
// Start the stubbed Monochron application
//
u08 doMonochron(cmdLine_t *cmdLine)
{
  u08 myBacklight = 16;
  u08 startWait = MC_FALSE;
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
  mcClockTimeEvent = MC_FALSE;
  mcClockDateEvent = MC_FALSE;
  almSwitchOn = MC_FALSE;
  almAlarming = MC_FALSE;
  almTickerAlarm = 0;
  almTickerSnooze = 0;

  // Clear the screen so we won't see any flickering upon changing the
  // backlight later on
  glcdClearScreen();

  // Set the backlight as stored in the eeprom
  eepInit();
  myBacklight = (eeprom_read_byte((uint8_t *)EE_BRIGHT) % 17) >>
    OCR2B_BITSHIFT;
  ctrlLcdBacklightSet(myBacklight);

  // Init stub event handler used in Monochron
  stubEventInit(startWait, MC_TRUE, stubHelpMonochron);

  // Start Monochron and witness the magic :-)
  monoMain();

  // We're done.
  // Restore the clock pool that mchron supports (as it was overridden by the
  // Monochron clock pool). By clearing any active clock from that pool also
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
// Function: doMonoConfig
//
// Start the stubbed Monochron configuration pages
//
u08 doMonoConfig(cmdLine_t *cmdLine)
{
  u08 myBacklight = 16;
  u08 startWait = MC_FALSE;
  u08 restart = MC_FALSE;
  u08 myKbMode = KB_MODE_LINE;

  // Get the start mode and the option to restart upon exiting the menu
  startWait = emuStartModeGet(argChar[0]);
  restart = TO_U08(argDouble[1]);

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Set essential Monochron startup data
  mcClockTimeEvent = MC_FALSE;
  mcClockDateEvent = MC_FALSE;
  almSwitchOn = MC_FALSE;
  almAlarming = MC_FALSE;
  almTickerAlarm = 0;
  almTickerSnooze = 0;

  // Clear the screen so we won't see any flickering upon changing the
  // backlight later on
  glcdClearScreen();

  // Misc eeprom-based initialization
  eepInit();
  myBacklight = (eeprom_read_byte((uint8_t *)EE_BRIGHT) % 17) >>
    OCR2B_BITSHIFT;
  ctrlLcdBacklightSet(myBacklight);
  mcBgColor = eeprom_read_byte((uint8_t *)EE_BGCOLOR) % 2;
  mcFgColor = (mcBgColor == GLCD_OFF ? GLCD_ON : GLCD_OFF);
  almAlarmSelect = eeprom_read_byte((uint8_t *)EE_ALARM_SELECT) % 4;
  almTimeGet(almAlarmSelect, &mcAlarmH, &mcAlarmM);

  // Init stub event handler used in Monochron
  stubEventInit(startWait, TO_U08(argDouble[0]), stubHelpMonochron);

  // (Re)start Monochron configuration menu pages until a quit keypress
  // occurred or regular menu exit is considered as final
  do
    cfgMenuMain();
  while (stubEventQuitGet() == MC_FALSE && restart == MC_TRUE);

  // We're done.
  // Restore the clock pool that mchron supports. By clearing any active clock
  // from that pool also any audible alarm will be stopped and reset.
  mcClockPool = emuMonochron;
  emuClockRelease(CMD_ECHO_NO);

  // Restore alarm, foreground/background color and backlight as they were
  // prior to starting Monochron config.
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
u08 doPaintAscii(cmdLine_t *cmdLine)
{
  u08 len = 0;
  u08 orientation;
  u08 font;

  // Get color, orientation and font from command line text values
  orientation = emuOrientationGet(argChar[0]);
  font = emuFontGet(argString[1]);

  // Paint ascii based on text orientation
  if (orientation == ORI_HORIZONTAL)
  {
    // Horizontal text
    len = glcdPutStr3(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      argString[2], TO_U08(argDouble[2]), TO_U08(argDouble[3]));
    if (echoCmd == CMD_ECHO_YES)
      printf("hor px=%d\n", (int)len);
  }
  else
  {
    // Vertical text
    len = glcdPutStr3v(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      orientation, argString[2], TO_U08(argDouble[2]), TO_U08(argDouble[3]));
    if (echoCmd == CMD_ECHO_YES)
      printf("vert px=%d\n", (int)len);
  }
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintBuffer
//
// Paint free format section of buffer
//
u08 doPaintBuffer(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];

  // Check if data was loaded
  if (emuGrBuf->bufType == GRAPH_NULL)
  {
    printf("%s? %d: buffer is empty\n",
      cmdLine->cmdCommand->cmdArg[1].argName, (int)bufferId);
    return CMD_RET_ERROR;
  }

  // Draw section from buffer
  glcdBitmap(TO_U08(argDouble[1]), TO_U08(argDouble[2]), TO_U16(argDouble[3]),
    TO_U08(argDouble[4]), TO_U08(argDouble[5]), TO_U08(argDouble[6]),
    emuGrBuf->bufElmFormat, DATA_RAM, (void *)(emuGrBuf->bufData));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintBufferImg
//
// Paint image using buffer data
//
u08 doPaintBufferImg(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 i;
  u08 height;
  u08 frame = 0;

  // Do we refer to an image buffer
  if (emuGrBuf->bufType != GRAPH_IMAGE)
  {
    printf("%s? %d: buffer does not contain image data\n",
      cmdLine->cmdCommand->cmdArg[1].argName, (int)bufferId);
    return CMD_RET_ERROR;
  }

  // Draw image frames from buffer
  for (i = 0; i < emuGrBuf->bufImgHeight; i = i + height)
  {
    if (i + emuGrBuf->bufElmBitSize > emuGrBuf->bufImgHeight)
      height = emuGrBuf->bufImgHeight - i;
    else
      height = emuGrBuf->bufElmBitSize;

    // Draw image frame
    glcdBitmap(TO_U08(argDouble[1]), TO_U08(argDouble[2]) + i,
      frame * emuGrBuf->bufImgWidth, 0, emuGrBuf->bufImgWidth, height,
      emuGrBuf->bufElmFormat, DATA_RAM, (void *)(emuGrBuf->bufData));
    frame++;
  }
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintBufferSpr
//
// Paint sprite frame data from buffer
//
u08 doPaintBufferSpr(cmdLine_t *cmdLine)
{
  u08 bufferId = TO_U08(argDouble[0]);
  emuGrBuf_t *emuGrBuf = &emuGrBufs[bufferId];
  u08 frame = TO_U08(argDouble[3]);

  // Do we refer to a sprite buffer
  if (emuGrBuf->bufType != GRAPH_SPRITE)
  {
    printf("%s? %d: buffer does not contain sprite data\n",
      cmdLine->cmdCommand->cmdArg[1].argName, (int)bufferId);
    return CMD_RET_ERROR;
  }

  // Do we exceed the max sprite in buffer
  if (frame >= emuGrBuf->bufSprFrames)
  {
    printf("%s? %d: exceeds buffer data (max = %d)\n",
      cmdLine->cmdCommand->cmdArg[4].argName, (int)frame,
      emuGrBuf->bufSprFrames - 1);
    return CMD_RET_ERROR;
  }

  // Draw sprite
  glcdBitmap(TO_U08(argDouble[1]), TO_U08(argDouble[2]),
    frame * emuGrBuf->bufSprWidth, 0, emuGrBuf->bufSprWidth,
    emuGrBuf->bufSprHeight, emuGrBuf->bufElmFormat, DATA_RAM,
    (void *)(emuGrBuf->bufData));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintCircle
//
// Paint circle
//
u08 doPaintCircle(cmdLine_t *cmdLine)
{
  // Draw circle
  glcdCircle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]), TO_U08(argDouble[2]),
    TO_U08(argDouble[3]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintCircleFill
//
// Paint circle with fill pattern
//
u08 doPaintCircleFill(cmdLine_t *cmdLine)
{
  // Draw filled circle
  glcdFillCircle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), TO_U08(argDouble[3]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintDot
//
// Paint dot
//
u08 doPaintDot(cmdLine_t *cmdLine)
{
  // Draw dot
  glcdDot(TO_U08(argDouble[0]), TO_U08(argDouble[1]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintLine
//
// Paint line
//
u08 doPaintLine(cmdLine_t *cmdLine)
{
  // Draw line
  glcdLine(TO_U08(argDouble[0]), TO_U08(argDouble[1]), TO_U08(argDouble[2]),
    TO_U08(argDouble[3]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintNumber
//
// Paint a number using a c printf format
//
u08 doPaintNumber(cmdLine_t *cmdLine)
{
  u08 len = 0;
  u08 orientation;
  u08 font;
  char *valString;

  // Get color, orientation and font from commandline text values
  orientation = emuOrientationGet(argChar[0]);
  font = emuFontGet(argString[1]);

  // Get output string
  asprintf(&valString, argString[2], argDouble[4]);

  // Paint ascii based on text orientation
  if (orientation == ORI_HORIZONTAL)
  {
    // Horizontal text
    len = glcdPutStr3(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      valString, TO_U08(argDouble[2]), TO_U08(argDouble[3]));
    if (echoCmd == CMD_ECHO_YES)
      printf("hor px=%d\n", (int)len);
  }
  else
  {
    // Vertical text
    len = glcdPutStr3v(TO_U08(argDouble[0]), TO_U08(argDouble[1]), font,
      orientation, valString, TO_U08(argDouble[2]), TO_U08(argDouble[3]));
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
u08 doPaintRect(cmdLine_t *cmdLine)
{
  // Draw rectangle
  glcdRectangle(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), TO_U08(argDouble[3]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintRectFill
//
// Paint rectangle with fill pattern
//
u08 doPaintRectFill(cmdLine_t *cmdLine)
{
  // Draw filled rectangle
  glcdFillRectangle2(TO_U08(argDouble[0]), TO_U08(argDouble[1]),
    TO_U08(argDouble[2]), TO_U08(argDouble[3]), TO_U08(argDouble[4]),
    TO_U08(argDouble[5]));
  ctrlLcdFlush();

  return CMD_RET_OK;
}

//
// Function: doPaintSetBg
//
// Set glcd draw color to background color
//
u08 doPaintSetBg(cmdLine_t *cmdLine)
{
  glcdColorSetBg();

  return CMD_RET_OK;
}

//
// Function: doPaintSetColor
//
// Set glcd draw color
//
u08 doPaintSetColor(cmdLine_t *cmdLine)
{
  u08 color = TO_U08(argDouble[0]);

  if (color == 0)
    glcdColorSet(GLCD_OFF);
  else
    glcdColorSet(GLCD_ON);

  return CMD_RET_OK;
}

//
// Function: doPaintSetFg
//
// Set glcd draw color to foreground color
//
u08 doPaintSetFg(cmdLine_t *cmdLine)
{
  glcdColorSetFg();

  return CMD_RET_OK;
}

//
// Function: doRepeatFor
//
// Initiate a new or continue a repeat loop
//
u08 doRepeatFor(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlChild = cmdLine->cmdPcCtrlChild;
  cmdArg_t *cmdArg = cmdLine->cmdCommand->cmdArg;

  // Execute the repeat logic
  if (cmdPcCtrlChild->active == MC_FALSE)
  {
    // First entry for this loop. Make the repeat active, then evaluate the
    // repeat init and the repeat condition expressions.
    cmdPcCtrlChild->active = MC_TRUE;
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
    cmdPcCtrlChild->active = MC_FALSE;
    *cmdProgCounter = cmdPcCtrlChild->cmdLineChild;
  }

  return CMD_RET_OK;
}

//
// Function: doRepeatNext
//
// Complete current repeat loop and determine end-of-loop
//
u08 doRepeatNext(cmdLine_t **cmdProgCounter)
{
  cmdLine_t *cmdLine = *cmdProgCounter;
  cmdPcCtrl_t *cmdPcCtrlParent = cmdLine->cmdPcCtrlParent;

  // Decide where to go depending on whether the loop is still active
  if (cmdPcCtrlParent->active == MC_TRUE)
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
u08 doStatsPrint(cmdLine_t *cmdLine)
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
u08 doStatsReset(cmdLine_t *cmdLine)
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
// Function: doTimeAlarmPos
//
// Set alarm switch position
//
u08 doTimeAlarmPos(cmdLine_t *cmdLine)
{
  u08 on;
  u08 newPosition = TO_U08(argDouble[0]);

  // Define new alarm switch position
  if (newPosition == 1)
    on = MC_TRUE;
  else
    on = MC_FALSE;

  // Set alarm switch position
  alarmSwitchSet(on, MC_FALSE);

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
// Function: doTimeAlarmSet
//
// Set clock alarm time
//
u08 doTimeAlarmSet(cmdLine_t *cmdLine)
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
         mcClockPool[mcMchronClock].clockId == CHRON_SLIDER ||
         mcClockPool[mcMchronClock].clockId == CHRON_MARIOWORLD))
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
      alarmSwitchToggle(MC_FALSE);
      almStateSet();
      animClockDraw(DRAW_CYCLE);
      alarmSwitchToggle(MC_FALSE);
      almStateSet();
      animClockDraw(DRAW_CYCLE);
    }
    else if (mcAlarmSwitch == ALARM_SWITCH_ON)
    {
      // Clear alarm switch status forcing clock to paint alarm info
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
// Function: doTimeAlarmToggle
//
// Toggle alarm switch position
//
u08 doTimeAlarmToggle(cmdLine_t *cmdLine)
{
  // Toggle alarm switch position
  alarmSwitchToggle(MC_FALSE);

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
// Function: doTimeDateReset
//
// Reset internal clock date
//
u08 doTimeDateReset(cmdLine_t *cmdLine)
{
  // Reset date to system date
  stubTimeSet(DT_TIME_KEEP, 0, 0, DT_DATE_RESET, 0, 0);

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
// Function: doTimeDateSet
//
// Set internal clock date
//
u08 doTimeDateSet(cmdLine_t *cmdLine)
{
  u08 dateOk = MC_TRUE;

  // Set new date
  dateOk = stubTimeSet(DT_TIME_KEEP, 0, 0, TO_UINT8_T(argDouble[0]),
    TO_UINT8_T(argDouble[1]), TO_UINT8_T(argDouble[2]));
  if (dateOk == MC_FALSE)
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
// Function: doTimeFlush
//
// Sync with and then report and update clock with date/time/alarm
//
u08 doTimeFlush(cmdLine_t *cmdLine)
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
u08 doTimePrint(cmdLine_t *cmdLine)
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
u08 doTimeReset(cmdLine_t *cmdLine)
{
  // Reset time to system time
  stubTimeSet(DT_TIME_RESET, 0, 0, DT_DATE_KEEP, 0, 0);

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
u08 doTimeSet(cmdLine_t *cmdLine)
{
  u08 timeOk = MC_TRUE;

  // Overide time
  timeOk = stubTimeSet(TO_UINT8_T(argDouble[2]), TO_UINT8_T(argDouble[1]),
    TO_UINT8_T(argDouble[0]), DT_DATE_KEEP, 0, 0);
  if (timeOk == MC_FALSE)
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
u08 doVarPrint(cmdLine_t *cmdLine)
{
  u08 retVal;

  // Print all variables matching a regex pattern where '.' is all
  retVal = varPrint(argString[1], MC_TRUE);
  if (retVal != CMD_RET_OK)
    printf("%s? invalid: %s\n", cmdLine->cmdCommand->cmdArg[0].argName,
      argString[1]);

  return retVal;
}

//
// Function: doVarReset
//
// Clear one or all used named variables
//
u08 doVarReset(cmdLine_t *cmdLine)
{
  int varId;
  int varInUse;
  u08 retVal;

  // Clear all variables
  if (strcmp(argString[1], ".") == 0)
  {
    varInUse = varReset();
    if (echoCmd == CMD_ECHO_YES)
      printf("reset variables: %d\n", varInUse);
    return CMD_RET_OK;
  }

  // Clear the single variable
  varId = varIdGet(argString[1], MC_FALSE);
  if (varId < 0)
  {
    printf("%s? not in use: %s\n", cmdLine->cmdCommand->cmdArg[0].argName,
      argString[1]);
    return CMD_RET_ERROR;
  }
  retVal = varClear(varId);
  if (retVal != CMD_RET_OK)
    printf("%s? not in use: %s\n", cmdLine->cmdCommand->cmdArg[0].argName,
      argString[1]);

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
u08 doVarSet(cmdLine_t *cmdLine)
{
  return CMD_RET_OK;
}

//
// Function: doWait
//
// Wait for keypress or pause in multiples of 1 msec.
//
u08 doWait(cmdLine_t *cmdLine)
{
  int delay = 0;
  char ch = '\0';

  delay = TO_INT(argDouble[0]);
  if (delay == 0)
  {
    // Wait for keypress
    if (listExecDepth == 0)
      ch = waitKeypress(MC_FALSE);
    else
      ch = waitKeypress(MC_TRUE);
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
u08 doWaitTimerExpiry(cmdLine_t *cmdLine)
{
  int delay = 0;
  char ch = '\0';

  // Wait for timer expiry (if not already expired)
  delay = TO_INT(argDouble[0]);
  ch = waitTimerExpiry(&tvTimer, delay, MC_TRUE, NULL);

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
u08 doWaitTimerStart(cmdLine_t *cmdLine)
{
  waitTimerStart(&tvTimer);

  return CMD_RET_OK;
}
