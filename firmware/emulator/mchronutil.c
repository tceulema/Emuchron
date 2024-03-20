//*****************************************************************************
// Filename : 'mchronutil.c'
// Title    : Utility routines for emuchron emulator command line tool
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

// Monochron defines
#include "../global.h"
#include "../glcd.h"
#include "../anim.h"
#include "../ks0108.h"
#include "../monomain.h"

// All Monochron clocks
#include "../clock/analog.h"
#include "../clock/barchart.h"
#include "../clock/bigdigit.h"
#include "../clock/cascade.h"
#include "../clock/crosstable.h"
#include "../clock/dali.h"
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

// Emuchron utilities
#include "dictutil.h"
#include "listutil.h"
#include "scanutil.h"
#include "varutil.h"
#include "mchronutil.h"

// Avoid typos in eeprom item name when printing the eeprom contents
#define EEPNAME(a)	a, #a

// Avoid typos in clock id name when printing the clock dictionary contents
#define CLOCKNAME(a)	a, #a

// The max size of a malloc-ed graphics data buffer.
// Technically the firmware supports a progmem buffer size up to 64KB. The
// Monochron m328 cpu has 32 KB flash available, but 2 KB is reserved for the
// bootloader, leaving 30 KB free for software and progmem data.
#define GRAPH_BUF_BYTES	30720

// The graphics data buffer lcd controller origin (as opposed to filename)
#define GRAPH_ORIGIN_CTRL	"lcd controllers"

// Definition of an eeprom dictionary used to always print in sorted order
typedef struct _eepDict_t
{
  uint8_t eepItemId;		// The eeprom item id
  char *eepItemName;		// The eeprom item id as a string
} eepDict_t;

// Definition of a clock dictionary used to build the mchron clock pool and
// print an overview of available clocks
typedef struct _emuClockDict_t
{
  uint8_t clockId;		// The clock id
  char *clockName;		// The clock id as a string
  void (*init)(u08);		// Clock init method
  void (*cycle)(void);		// Clock loop cycle (=update) method
  void (*button)(u08);		// Clock button event handler method (optional)
  char *clockDesc;		// The short description of the clock
} emuClockDict_t;

// Monochron defined data
extern volatile rtcDateTime_t rtcDateTime;
extern volatile rtcDateTime_t rtcDateTimeNext;
extern volatile uint8_t rtcTimeEvent;
extern volatile uint8_t mcClockOldTS, mcClockOldTM, mcClockOldTH;
extern volatile uint8_t mcClockNewTS, mcClockNewTM, mcClockNewTH;
extern volatile uint8_t mcClockOldDD, mcClockOldDM, mcClockOldDY;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern clockDriver_t *mcClockPool;

// The mchron alarm time
extern uint8_t emuAlarmH;
extern uint8_t emuAlarmM;

// The command line input stream control structure
extern cmdInput_t cmdInput;

// This is me
extern const char *__progname;

// The eeprom dictionary. When printing the eeprom contents using command 'mep'
// the dictionary will be sorted on id value first so it will be printed in
// the proper id order.
static eepDict_t eepDict[] =
{
  { EEPNAME(EE_INIT) }
 ,{ EEPNAME(EE_BRIGHT) }
 ,{ EEPNAME(EE_VOLUME) }
 ,{ EEPNAME(EE_REGION) }
 ,{ EEPNAME(EE_TIME_FORMAT) }
 ,{ EEPNAME(EE_SNOOZE) }
 ,{ EEPNAME(EE_BGCOLOR) }
 ,{ EEPNAME(EE_ALARM_SELECT) }
 ,{ EEPNAME(EE_ALARM_HOUR1) }
 ,{ EEPNAME(EE_ALARM_MIN1) }
 ,{ EEPNAME(EE_ALARM_HOUR2) }
 ,{ EEPNAME(EE_ALARM_MIN2) }
 ,{ EEPNAME(EE_ALARM_HOUR3) }
 ,{ EEPNAME(EE_ALARM_MIN3) }
 ,{ EEPNAME(EE_ALARM_HOUR4) }
 ,{ EEPNAME(EE_ALARM_MIN4) }
};
static int eepDictCount = sizeof(eepDict) / sizeof(eepDict_t);

// The emuchron clock dictionary. It is used to build the mchron clock pool
// and print available mchron clocks. The sequence of the clocks in this
// dictionary impacts commands 'cs' and 'cp'. Add a new clock in here in the
// desired location.
static emuClockDict_t emuClockDict[] =
{
  { CLOCKNAME(CHRON_NONE),		0,                  0,                   0,		"[detach from active clock]" }
 ,{ CLOCKNAME(CHRON_EXAMPLE),		exampleInit,        exampleCycle,        0,		"example" }
 ,{ CLOCKNAME(CHRON_ANALOG_HMS),	analogHmsInit,      analogCycle,         0,		"analog format hms" }
 ,{ CLOCKNAME(CHRON_ANALOG_HM),		analogHmInit,       analogCycle,         0,		"analog format hm" }
 ,{ CLOCKNAME(CHRON_DIGITAL_HMS),	digitalHmsInit,     digitalCycle,        0,		"digital format hms" }
 ,{ CLOCKNAME(CHRON_DIGITAL_HM),	digitalHmInit,      digitalCycle,        0,		"digital format hm" }
 ,{ CLOCKNAME(CHRON_MOSQUITO),		mosquitoInit,       mosquitoCycle,       0,		"mosquito" }
 ,{ CLOCKNAME(CHRON_NERD),		nerdInit,           nerdCycle,           0,		"nerd" }
 ,{ CLOCKNAME(CHRON_PONG),		pongInit,           pongCycle,           pongButton,	"pong" }
 ,{ CLOCKNAME(CHRON_PUZZLE),		puzzleInit,         puzzleCycle,         puzzleButton,	"puzzle" }
 ,{ CLOCKNAME(CHRON_SLIDER),		sliderInit,         sliderCycle,         0,		"slider" }
 ,{ CLOCKNAME(CHRON_CASCADE),		spotCascadeInit,    spotCascadeCycle,    0,		"spotfire cascade" }
 ,{ CLOCKNAME(CHRON_SPEEDDIAL),		spotSpeedDialInit,  spotSpeedDialCycle,  0,		"spotfire speeddial" }
 ,{ CLOCKNAME(CHRON_SPIDERPLOT),	spotSpiderPlotInit, spotSpiderPlotCycle, 0,		"spotfire spider" }
 ,{ CLOCKNAME(CHRON_THERMOMETER),	spotThermInit,      spotThermCycle,      0,		"spotfire thermometer" }
 ,{ CLOCKNAME(CHRON_TRAFLIGHT),		spotTrafLightInit,  spotTrafLightCycle,  0,		"spotfire trafficlight" }
 ,{ CLOCKNAME(CHRON_BARCHART),		spotBarChartInit,   spotBarChartCycle,   0,		"spotfire barchart" }
 ,{ CLOCKNAME(CHRON_CROSSTABLE),	spotCrossTableInit, spotCrossTableCycle, 0,		"spotfire crosstable" }
 ,{ CLOCKNAME(CHRON_LINECHART),		spotLineChartInit,  spotLineChartCycle,  0,		"spotfire linechart" }
 ,{ CLOCKNAME(CHRON_PIECHART),		spotPieChartInit,   spotPieChartCycle,   0,		"spotfire piechart" }
 ,{ CLOCKNAME(CHRON_BIGDIG_ONE),	bigDigInit,         bigDigCycle,         bigDigButton,	"big digit format one" }
 ,{ CLOCKNAME(CHRON_BIGDIG_TWO),	bigDigInit,         bigDigCycle,         bigDigButton,	"big digit format two" }
 ,{ CLOCKNAME(CHRON_QR_HMS),		qrInit,             qrCycle,             0,		"qr format hms" }
 ,{ CLOCKNAME(CHRON_QR_HM),		qrInit,             qrCycle,             0,		"qr format hm" }
 ,{ CLOCKNAME(CHRON_MARIOWORLD),	marioInit,          marioCycle,          0,		"marioworld" }
 ,{ CLOCKNAME(CHRON_WAVE),		waveInit,           waveCycle,           0,		"wave banner" }
 ,{ CLOCKNAME(CHRON_DALI),		daliInit,           daliCycle,           daliButton,	"dali" }
 ,{ CLOCKNAME(CHRON_PERFTEST),		perfInit,           perfCycle,           0,		"performance test" }
};
static int emuClockDictCount = sizeof(emuClockDict) / sizeof(emuClockDict_t);

// Flags indicating active state upon exit
u08 invokeExit = MC_FALSE;
static u08 closeWinMsg = MC_FALSE;

// Local function prototypes
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context);

//
// Function: emuArgcArgvGet
//
// Process mchron startup command line arguments.
// Return: MC_TRUE (success) or MC_FALSE (failure).
//
u08 emuArgcArgvGet(int argc, char *argv[], emuArgcArgv_t *emuArgcArgv)
{
  FILE *fp;
  int argCount = 1;
  char *tty = emuArgcArgv->ctrlDeviceArgs.lcdNcurInitArgs.tty;
  u08 argHelp = MC_FALSE;
  u08 argError = MC_FALSE;

  // Init references to command line argument positions
  emuArgcArgv->argDebug = 0;
  emuArgcArgv->argGlutGeometry = 0;
  emuArgcArgv->argGlutPosition = 0;
  emuArgcArgv->argTty = 0;
  emuArgcArgv->argLcdType = 0;

  // Init the lcd device data
  emuArgcArgv->ctrlDeviceArgs.useNcurses = MC_FALSE;
  emuArgcArgv->ctrlDeviceArgs.useGlut = MC_TRUE;
  emuArgcArgv->ctrlDeviceArgs.lcdNcurInitArgs.tty[0] = '\0';
  emuArgcArgv->ctrlDeviceArgs.lcdNcurInitArgs.winClose = emuShutdown;
  emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.posX = 100;
  emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.posY = 100;
  emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.sizeX = 520;
  emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.sizeY = 264;
  emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.winClose = emuShutdown;

  // Do archaic command line processing to obtain the lcd output device(s),
  // lcd output configs and debug logfile
  while (argCount < argc)
  {
    if (strncmp(argv[argCount], "-d", 4) == 0)
    {
      // Debug output file name
      emuArgcArgv->argDebug = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-g", 4) == 0)
    {
      // Glut window geometry
      emuArgcArgv->argGlutGeometry = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-h", 4) == 0)
    {
      // Request for help: end scan
      argHelp = MC_TRUE;
      argCount = argc;
    }
    else if (strncmp(argv[argCount], "-l", 4) == 0)
    {
      // Lcd stub device type
      emuArgcArgv->argLcdType = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-p", 4) == 0)
    {
      // Glut window position
      emuArgcArgv->argGlutPosition = argCount + 1;
      argCount = argCount + 2;
    }
    else if (strncmp(argv[argCount], "-t", 4) == 0)
    {
      // Ncurses tty device
      emuArgcArgv->argTty = argCount + 1;
      argCount = argCount + 2;
    }
    else
    {
      // Anything else: force to quit
      argCount = argc + 1;
    }

    if (argCount > argc)
      argError = MC_TRUE;
  }

  // Check result of command line processing
  if (argError == MC_TRUE)
    printf("%s: invalid/incomplete command argument\n\n", __progname);
  if (argHelp == MC_TRUE || argError == MC_TRUE)
  {
    system("/usr/bin/head -24 ../support/help.txt | /usr/bin/tail -21 2>&1");
    return MC_FALSE;
  }

  // Validate lcd stub output device
  if (emuArgcArgv->argLcdType > 0)
  {
    if (strcmp(argv[emuArgcArgv->argLcdType], "glut") == 0)
    {
      emuArgcArgv->ctrlDeviceArgs.useGlut = MC_TRUE;
      emuArgcArgv->ctrlDeviceArgs.useNcurses = MC_FALSE;
    }
    else if (strcmp(argv[emuArgcArgv->argLcdType], "ncurses") == 0)
    {
      emuArgcArgv->ctrlDeviceArgs.useGlut = MC_FALSE;
      emuArgcArgv->ctrlDeviceArgs.useNcurses = MC_TRUE;
    }
    else if (strcmp(argv[emuArgcArgv->argLcdType], "all") == 0)
    {
      emuArgcArgv->ctrlDeviceArgs.useGlut = MC_TRUE;
      emuArgcArgv->ctrlDeviceArgs.useNcurses = MC_TRUE;
    }
    else
    {
      printf("%s: -l: invalid lcd stub device type %s\n", __progname,
        argv[emuArgcArgv->argLcdType]);
      return MC_FALSE;
    }
  }

  // Validate glut window geometry
  if (emuArgcArgv->argGlutGeometry > 0)
  {
    regex_t regex;
    int status;
    char *input = argv[emuArgcArgv->argGlutGeometry];
    char *separator;

    // Scan the geometry argument using a regex pattern
    regcomp(&regex, "^[0-9]+x[0-9]+$", REG_EXTENDED | REG_NOSUB);
    status = regexec(&regex, input, (size_t)0, NULL, 0);
    regfree(&regex);
    if (status != 0)
    {
      printf("%s: -g: invalid glut geometry\n", __progname);
      return MC_FALSE;
    }

    // An 'x' separator splits the two numeric geometry arguments
    separator = strchr(input, 'x');
    *separator = '\0';
    emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.sizeX = atoi(input);
    emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.sizeY = atoi(separator + 1);
  }

  // Validate glut window position
  if (emuArgcArgv->argGlutPosition > 0)
  {
    regex_t regex;
    int status;
    char *input = argv[emuArgcArgv->argGlutPosition];
    char *separator;

    // Scan the position argument using a regex pattern
    regcomp(&regex, "^[0-9]+,[0-9]+$", REG_EXTENDED | REG_NOSUB);
    status = regexec(&regex, input, (size_t)0, NULL, 0);
    regfree(&regex);
    if (status != 0)
    {
      printf("%s: -p: invalid glut position\n", __progname);
      return MC_FALSE;
    }

    // A ',' separator splits the two numeric position arguments
    separator = strchr(input, ',');
    *separator = '\0';
    emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.posX = atoi(input);
    emuArgcArgv->ctrlDeviceArgs.lcdGlutInitArgs.posY = atoi(separator + 1);
  }

  // Get the ncurses output device
  if (emuArgcArgv->argTty != 0)
  {
    // Got it from the command line
    if (strlen(argv[emuArgcArgv->argTty]) >= NCURSES_TTYLEN)
    {
      printf("%s: -t: tty too long (max = %d chars)\n", __progname,
        NCURSES_TTYLEN - 1);
      return MC_FALSE;
    }
    strcpy(tty, argv[emuArgcArgv->argTty]);
  }
  else if (emuArgcArgv->ctrlDeviceArgs.useNcurses == 1)
  {
    // Get the tty device if not specified on the command line
    char *home;
    char *fullPath;
    int ttyLen = 0;

    // Get the full path to $HOME/.mchron
    home = getenv("HOME");
    if (home == NULL)
    {
      printf("%s: cannot get $HOME\n", __progname);
      printf("- Use switch \"-t <tty>\" to set lcd output device\n");
      return MC_FALSE;
    }
    fullPath = malloc(strlen(home) + strlen(MCHRON_CONFIG) +
      strlen(NCURSES_TTYFILE) + 1);
    sprintf(fullPath,"%s%s%s", home, MCHRON_CONFIG, NCURSES_TTYFILE);

    // Open the file with the tty device
    fp = fopen(fullPath, "r");
    free(fullPath);
    if (fp == NULL)
    {
      printf("%s: cannot open file \"%s%s%s\".\n", __progname, "~",
        MCHRON_CONFIG, NCURSES_TTYFILE);
      printf("- Manually create folder ~%s\n", MCHRON_CONFIG);
      printf("- Start a new monochron ncurses terminal or use switch \"-t <tty>\" to set\n");
      printf("  mchron ncurses terminal tty\n");
      return MC_FALSE;
    }

    // Read output device in first line. It has a fixed max length.
    fgets(tty, NCURSES_TTYLEN, fp);

    // Kill all \r or \n in the tty string as ncurses doesn't like this.
    // Assume that \r and \n are trailing characters in the string.
    if (strlen(tty) > 0)
    {
      for (ttyLen = strlen(tty) - 1; ttyLen >= 0; ttyLen--)
      {
        if (tty[ttyLen] == '\n' || tty[ttyLen] == '\r')
          tty[ttyLen] = '\0';
        else
          break;
      }
    }

    // Clean up our stuff
    fclose(fp);
  }

  // All seems to be ok
  return MC_TRUE;
}

//
// Function: emuClockPoolInit
//
// Build the mchron clock pool based on emuClockDict[]
//
clockDriver_t *emuClockPoolInit(int *count)
{
  int i;
  clockDriver_t *clockDriver;

  // Get space for the mchron clock driver pool
  clockDriver = malloc(sizeof(clockDriver_t) * emuClockDictCount);
  for (i = 0; i < emuClockDictCount; i++)
  {
    // Copy clock dictionary info to the clock driver pool
    clockDriver[i].clockId = emuClockDict[i].clockId;
    // In the mchron clock pool all clocks must do a full init except the
    // detach (null) clock
    if (emuClockDict[i].clockId == CHRON_NONE)
      clockDriver[i].initType = DRAW_INIT_NONE;
    else
      clockDriver[i].initType = DRAW_INIT_FULL;
    clockDriver[i].init = emuClockDict[i].init;
    clockDriver[i].cycle = emuClockDict[i].cycle;
    clockDriver[i].button = emuClockDict[i].button;
  }

  // Return the pool size and the pool itself
  *count = emuClockDictCount;
  return clockDriver;
}

//
// Function: emuClockPoolReset
//
// Release the emuMonochron clock pool
//
void emuClockPoolReset(clockDriver_t *clockDriver)
{
  free(clockDriver);
}

//
// Function: emuClockPrint
//
// Print an overview of all clocks in the Emuchron clock dictionary
//
void emuClockPrint(void)
{
  int i;
  char active;

  // All Emuchron clocks
  printf("clocks:\n");
  printf("clock clockId              description\n");

  // Do this per entry
  for (i = 0; i < emuClockDictCount; i++)
  {
    // Is this the active clock
    if (i == mcMchronClock)
      active = '*';
    else
      active = ' ';
    printf("%2d%c   %-20s %s\n", i, active, emuClockDict[i].clockName,
        emuClockDict[i].clockDesc);
  }
}

//
// Function: emuClockRelease
//
// Release a selected clock
//
void emuClockRelease(u08 echoCmd)
{
  // Clear clock time and detach from current selected clock
  mcClockOldTS = mcClockOldTM = mcClockOldTH = 0;
  mcClockOldDD = mcClockOldDM = mcClockOldDY = 0;
  if (mcClockPool[mcMchronClock].clockId != CHRON_NONE &&
      echoCmd == CMD_ECHO_YES)
    printf("released clock\n");
  mcMchronClock = 0;

  // Kill alarm (if sounding anyway) and reset it
  alarmSoundReset();
}

//
// Function: emuClockUpdate
//
// Most clocks update their layout in a single clock cycle.
// However, consider the QR clock. This clock requires multiple clock cycles
// to update its layout due to the above average computing power needed to do
// so.
// For specific clocks, like the QR and dali clock, this function generates
// multiple clock cycles, enough to update its layout.
// By default for any other clock, only a single clock cycle is generated,
// which should suffice.
//
void emuClockUpdate(void)
{
  // Nothing to be done when no clock is active
  if (mcClockPool[mcMchronClock].clockId == CHRON_NONE)
    return;

  // We have specific draw requirements for the QR and dali clock
  if (mcClockPool[mcMchronClock].clockId == CHRON_QR_HM ||
      mcClockPool[mcMchronClock].clockId == CHRON_QR_HMS)
  {
    // Generate the clock cycles needed to display a new QR
    u08 i;
    for (i = 0; i < QR_GEN_CYCLES; i++)
      animClockDraw(DRAW_CYCLE);
  }
  else if (mcClockPool[mcMchronClock].clockId == CHRON_DALI)
  {
    // Generate the clock cycles needed to display a dali transition
    u08 i;
    for (i = 0; i <= DALI_GEN_CYCLES; i++)
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
  rtcTimeEvent = MC_FALSE;
}

//
// Function: emuCoreDump
//
// There's something terribly wrong in Emuchron. It may be caused by bad
// (functional clock) code, a bad mchron command line request or a bug in the
// Emuchron emulator.
// Provide some feedback and generate a coredump file (when enabled).
// Note: A graceful environment shutdown is taken care of by the SIGABRT signal
// handler, invoked by abort().
// Note: In order to get a coredump file it requires to run shell command
// "ulimit -c unlimited" once in the mchron shell.
//
void emuCoreDump(u08 origin, const char *location, int arg1, int arg2,
  int arg3, int arg4)
{
  if (origin == CD_GLCD)
  {
    // Error in the glcd interface
    // Note: y = vertical lcd byte location (0..7)
    printf("\n*** invalid graphics api request in %s()\n", location);
    printf("api info (controller:x:y:data) = (%d:%d:%d:%d)\n",
      arg1, arg2, arg3, arg4);
  }
  else if (origin == CD_CTRL)
  {
    // Error in the controller interface
    printf("\n*** invalid controller api request in %s()\n", location);
    printf("api info (method/data) = %d\n", arg1);
  }
  else if (origin == CD_EEPROM)
  {
    // Error in the eeprom interface
    printf("\n*** invalid eeprom api request in %s()\n", location);
    printf("api info (address) = %d\n", arg1);
  }
  else if (origin == CD_VAR)
  {
    // Error in the named variable interface
    printf("\n*** invalid var api request in %s()\n", location);
    printf("api info (bucket, index, count) = (%d:%d:%d)\n", arg1, arg2, arg3);
  }
  else if (origin == CD_CLOCK)
  {
    // Error in the clock interface
    printf("\n*** invalid clock api request in %s()\n", location);
    printf("api info (device, length) = (%d:%d)\n", arg1, arg2);
  }
  else
  {
    // Unknown error origin
    printf("\n*** invalid api request in %s()\n", location);
    printf("api info (arg1, arg1, arg3, arg4) = (%d:%d:%d:%d)\n", arg1, arg2,
      arg3, arg4);
  }

  // Dump all Monochron variables. Might be useful.
  printf("*** registered variables\n");
  varPrint(".", MC_TRUE);

  // Stating the obvious
  printf("*** debug by loading coredump file (when created) in a debugger\n");

  // Switch back to regular keyboard input mode and kill audible sound (if any)
  kbModeSet(KB_MODE_LINE);
  alarmSoundReset();

  // Depending on the lcd device(s) used we'll see the latest image or not.
  // In case we're using ncurses, regardless with or without glut, flush the
  // screen. After aborting mchron, the ncurses image will be retained.
  // In case we're only using the glut device, give end-user the means to have
  // a look at the glut device to get a clue what's going on before its display
  // is killed by aborting mchron. Note that at this point glut is still
  // running in its own thread and will have its layout constantly refreshed.
  // This allows a glut screendump to be made if needed.
  if (ctrlDeviceActive(CTRL_DEVICE_NCURSES) == MC_TRUE)
  {
    // Flush the ncurses device so we get its contents as-is at the time of
    // the forced coredump
    ctrlLcdFlush();
  }
  else // glut device is used
  {
    // Have end-user confirm abort, allowing a screendump to be made prior to
    // actual coredump
    waitKeypress(MC_FALSE);
  }

  // Cleanup command line read interface, forcing the readline history to be
  // flushed in the history file
  cmdInputCleanup(&cmdInput);

  // Force coredump
  abort();
}

//
// Function: emuEchoReqGet
//
// Get the requested list command echo, used for tracing command files
//
u08 emuEchoReqGet(char echo)
{
  // Get requested command list echo type
  if (echo == 'e')
    return LIST_ECHO_ECHO;
  else if (echo == 'i')
    return LIST_ECHO_INHERIT;
  else // 's'
    return LIST_ECHO_SILENT;
}

//
// Function: emuEepromPrint
//
// Prints the eeprom contents using the sorted id's defined in the eeprom
// dictionary.
//
void emuEepromPrint(void)
{
  int i;
  uint8_t value;
  eepDict_t *myDict;
  eepDict_t *dictSort[eepDictCount];
  u08 allSorted = MC_FALSE;
  int eepIdx = eepDictCount;

  printf("eeprom:\n");

  // Memory address offset Monochron settings in eeprom
  printf("monochron eeprom offset = %d (0x%03x)\n", EE_OFFSET, EE_OFFSET);

  // Status Monochron eeprom settings based on value of location EE_INIT
  value = eeprom_read_byte((uint8_t *)EE_INIT);
  printf("monochron eeprom status = ");
  if (value == EE_INITIALIZED)
    printf("initialized\n");
  else if (value == 0xff)
    printf("erased\n");
  else
    printf("invalid\n");

  // All Monochron eeprom settings
  printf("byte address name            value\n");

  // Copy all id's, sort them and then print them
  for (i = 0; i < eepDictCount; i++)
    dictSort[i] = &eepDict[i];
  while (allSorted == MC_FALSE)
  {
    allSorted = MC_TRUE;
    for (i = 0; i < eepIdx - 1; i++)
    {
      if (dictSort[i]->eepItemId > dictSort[i + 1]->eepItemId)
      {
        myDict = dictSort[i];
        dictSort[i] = dictSort[i + 1];
        dictSort[i + 1] = myDict;
        allSorted = MC_FALSE;
      }
    }
    eepIdx--;
  }
  for (i = 0; i < eepDictCount; i++)
  {
    value = eeprom_read_byte((uint8_t *)(size_t)dictSort[i]->eepItemId);
    printf("%2d   0x%03x   %-15s %3d (0x%02x)\n",
      dictSort[i]->eepItemId - EE_OFFSET, dictSort[i]->eepItemId,
      dictSort[i]->eepItemName, value, value);
  }
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
    return FONT_5X7M;
}

//
// Function: emuFormatGet
//
// Get the requested graphics element data format and its metadata
//
u08 emuFormatGet(char formatId, u08 *formatBytes, u08 *formatBits)
{
  // Get data format
  if (formatId == 'b')
  {
    if (formatBytes != NULL)
      *formatBytes = sizeof(uint8_t);
    if (formatBits != NULL)
      *formatBits = 8 * sizeof(uint8_t);
    return ELM_BYTE;
  }
  else if (formatId == 'w')
  {
    if (formatBytes != NULL)
      *formatBytes = sizeof(uint16_t);
    if (formatBits != NULL)
      *formatBits = 8 * sizeof(uint16_t);
    return ELM_WORD;
  }
  else // formatId == 'd'
  {
    if (formatBytes != NULL)
      *formatBytes = sizeof(uint32_t);
    if (formatBits != NULL)
      *formatBits = 8 * sizeof(uint32_t);
    return ELM_DWORD;
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
// Function: emuSearchTypeGet
//
// Get the requested command dictionary search type
//
u08 emuSearchTypeGet(char searchType)
{
  // Get dictionary search type
  if (searchType == 'a')
    return CMD_SEARCH_ARG;
  else if (searchType == 'd')
    return CMD_SEARCH_DESCR;
  else if (searchType == 'n')
    return CMD_SEARCH_NAME;
  else // '.'
    return CMD_SEARCH_ALL;
}

//
// Function: emuShutdown
//
// This function is used in two ways. First, it is used to implement a graceful
// shutdown when an lcd device window is closed, by using it as a callback
// function. Second, it is used in non-standard shutdown circumstances, created
// by system signal handlers such as ctrl-c.
//
void emuShutdown(void)
{
  kbModeSet(KB_MODE_LINE);
  alarmSoundReset();
  cmdInputCleanup(&cmdInput);
  ctrlCleanup();
  if (invokeExit == MC_FALSE && closeWinMsg == MC_FALSE)
  {
    closeWinMsg = MC_TRUE;
    printf("\nlcd device closed - exit\n");
  }
  exit(-1);
}

//
// Function: emuSigCatch
//
// Main signal handler wrapper. Used for system timers and signals to implement
// a graceful shutdown (preventing a screwed up bash terminal and killing alarm
// audio).
// For signals that should make the application quit, switch back to keyboard
// line mode and kill audio before we actually exit.
//
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context)
{
  //printf("signo=%d\n", siginfo->si_signo);

  if (sig == SIGVTALRM)
  {
    // Recurring system timer expiry.
    // Execute the handler as requested in emuSysTimerStart().
    ((void (*)(void))siginfo->si_value.sival_ptr)();
  }
  else if (sig == SIGINT)
  {
    // Keyboard: "^C"
    printf("\n<ctrl>c - interrupt\n");
    invokeExit = MC_TRUE;
    emuShutdown();
  }
  else if (sig == SIGTSTP)
  {
    // Keyboard: "^Z"
    printf("\n<ctrl>z - stop\n");
    invokeExit = MC_TRUE;
    emuShutdown();
  }
  else if (sig == SIGABRT)
  {
    struct sigaction sigAction;

    // We must clear the sighandler for SIGABRT or else we'll get an infinite
    // recursive loop due to abort() below that triggers a new SIGABRT that
    // triggers a new (etc)...
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
    // coredump it requires to run shell command "ulimit -c unlimited" once in
    // the mchron shell prior to starting mchron.
    abort();
  }
  else if (sig == SIGQUIT)
  {
    // Keyboard: "^\"
    // Note that abort() below will trigger a SIGABRT that will be handled
    // separately, eventually causing the program to coredump
    kbModeSet(KB_MODE_LINE);
    alarmSoundReset();
    invokeExit = MC_TRUE;
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

  if (sigaction(SIGVTALRM, &sigAction, NULL) < 0)
    printf("Cannot set handler SIGVTALRM (%d)\n", SIGVTALRM);
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
u08 emuStartModeGet(char startId)
{
  if (startId == 'c')
    return MC_TRUE;
  else // startId == 'r'
    return MC_FALSE;
}

//
// Function: emuTimePrint
//
// Print the time/date and optional alarm
//
void emuTimePrint(u08 type)
{
  printf("time   : %02d:%02d:%02d (hh:mm:ss)\n", rtcDateTime.timeHour,
    rtcDateTime.timeMin, rtcDateTime.timeSec);
  printf("date   : %02d/%02d/%04d (dd/mm/yyyy)\n", rtcDateTime.dateDay,
    rtcDateTime.dateMon, rtcDateTime.dateYear + 2000);
  if (type == ALM_EMUCHRON)
    printf("alarm  : %02d:%02d (hh:mm)\n", emuAlarmH, emuAlarmM);
  else if (type == ALM_MONOCHRON)
    printf("alarm  : %02d:%02d (hh:mm)\n", mcAlarmH, mcAlarmM);
  if (type != ALM_NONE)
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
  DEBUGTP("Clear time event");
  rtcTimeEvent = MC_FALSE;
  rtcMchronTimeInit();
}

//
// Function: grBufCopy
//
// Copy non-empty graphics buffer
//
u08 grBufCopy(emuGrBuf_t *emuGrBufFrom, emuGrBuf_t *emuGrBufTo)
{
  // Omit copy if we copy into oneself
  if (emuGrBufFrom == emuGrBufTo)
    return CMD_RET_OK;

  // Reset target buffer
  grBufReset(emuGrBufTo);

  // Copy buffer but make fresh copy of origin string and buffer data
  *emuGrBufTo = *emuGrBufFrom;
  if (emuGrBufFrom->bufOrigin != NULL)
  {
     emuGrBufTo->bufOrigin = malloc(strlen(emuGrBufFrom->bufOrigin) + 1);
     strcpy(emuGrBufTo->bufOrigin, emuGrBufFrom->bufOrigin);
  }
  if (emuGrBufFrom->bufData != NULL)
  {
     emuGrBufTo->bufData =
       malloc(emuGrBufFrom->bufElmCount * emuGrBufFrom->bufElmByteSize);
     memcpy(emuGrBufTo->bufData, emuGrBufFrom->bufData,
       emuGrBufFrom->bufElmCount * emuGrBufFrom->bufElmByteSize);
  }

  return CMD_RET_OK;
}

//
// Function: grBufInfoPrint
//
// Print metadata info on sprite
//
void grBufInfoPrint(emuGrBuf_t *emuGrBuf)
{
  char tmString[25];
  struct tm* tmInfo;

  if (emuGrBuf->bufElmFormat == ELM_NULL)
  {
    printf("buffer is empty\n");
    return;
  }

  // Buffer origin and time, data type and byte size per element
  printf("data origin     : %s\n", emuGrBuf->bufOrigin);
  tmInfo = localtime(&emuGrBuf->bufCreate);
  strftime(tmString, 25, "%Y-%m-%d %H:%M:%S", tmInfo);
  printf("data loaded at  : %s\n", tmString);
  printf("data format     : ");
  if (emuGrBuf->bufElmFormat == ELM_BYTE)
    printf("byte ");
  else if (emuGrBuf->bufElmFormat == ELM_WORD)
    printf("word ");
  else
    printf("double word ");
  if (emuGrBuf->bufElmByteSize == 1)
    printf("(%d byte per element)\n", emuGrBuf->bufElmByteSize);
  else
    printf("(%d bytes per element)\n", emuGrBuf->bufElmByteSize);

  // Data elements and size in bytes
  printf("data elements   : %d (%d bytes)\n", emuGrBuf->bufElmCount,
    emuGrBuf->bufElmCount * emuGrBuf->bufElmByteSize);

  // We either have raw data, image data or sprite data
  printf("data contents   : ");
  if (emuGrBuf->bufType == GRAPH_RAW)
    printf("raw (free format data)\n");
  else if (emuGrBuf->bufType == GRAPH_IMAGE)
    printf("image (single fixed size image)\n");
  else // GRAPH_SPRITE
    printf("sprite (multiple fixed size image frames)\n");

  // Provide content details
  printf("content details : ");
  if (emuGrBuf->bufType == GRAPH_RAW)
    printf("none\n");
  else if (emuGrBuf->bufType == GRAPH_IMAGE)
    printf("image size %dx%d pixels requiring %d frame(s)\n",
      emuGrBuf->bufImgWidth, emuGrBuf->bufImgHeight, emuGrBuf->bufImgFrames);
  else // GRAPH_SPRITE
    printf("sprite size %dx%d pixels, %d frame(s)\n",
      emuGrBuf->bufSprWidth, emuGrBuf->bufSprHeight, emuGrBuf->bufSprFrames);
}

//
// Function: grBufInit
//
// Initialize a graphics buffer
//
void grBufInit(emuGrBuf_t *emuGrBuf)
{
  // Clear and init buffer contents
  memset(emuGrBuf, 0, sizeof(emuGrBuf_t));
  emuGrBuf->bufType = GRAPH_NULL;
  emuGrBuf->bufOrigin = NULL;
  emuGrBuf->bufElmFormat = ELM_NULL;
  emuGrBuf->bufData = NULL;
}

//
// Function: grBufLoadCtrl
//
// Load graphics buffer with data from the lcd controllers
//
void grBufLoadCtrl(u08 x, u08 y, u08 width, u08 height, char formatName,
  emuGrBuf_t *emuGrBuf)
{
  u08 frame;
  u08 i;
  u08 j;
  u16 count = 0;
  uint32_t bufVal;
  uint32_t lcdByte;
  u08 yOffset = y % 8;
  u08 yStart = y / 8;
  u08 yFrameBytes;
  u08 yFrameStart;
  u08 bitsToDo;
  u08 frames;
  u08 format;
  u08 formatBytes;
  u08 formatBits;

  // Setup for loading graphics data
  grBufReset(emuGrBuf);
  format = emuFormatGet(formatName, &formatBytes, &formatBits);

  // Split up requested image in frames and reserve buffer space
  frames = ((height - 1) / 8) / formatBytes + 1;
  emuGrBuf->bufData = malloc(width * formatBytes * frames);

  // Retrieve the data from the lcd controllers and store it in the buffer
  for (frame = 0; frame < frames; frame++)
  {
    // Get lcd y cursor position to start reading from
    yFrameStart = frame * formatBytes + yStart;

    // For this frame move x from left to right
    for (i = x; i < x + width; i++)
    {
      bufVal = 0;

      // Read as many y bytes to fill a target format element but be aware that
      // the last frame may require less y bytes
      yFrameBytes = formatBytes;
      if (yOffset > 0)
         yFrameBytes++;

      if (frame < frames - 1)
        bitsToDo = formatBits;
      else
        bitsToDo = (height - 1) % formatBits + 1;

      // Get a vertical frame byte of the proper height
      for (j = 0; j < yFrameBytes; j++)
      {
        // Set cursor and read twice to read the lcd byte
        glcdSetAddress(i, yFrameStart + j);
        glcdDataRead();
        lcdByte = (uint32_t)glcdDataRead();

        // Clip data from first and last y byte if needed and keep track of
        // remaining bits
        if (j == 0)
        {
          if (bitsToDo <= 8 - yOffset)
          {
            lcdByte = (lcdByte & (0xff >> (8 - bitsToDo - yOffset)));
            bitsToDo = 0;
          }
          else
          {
            bitsToDo = bitsToDo - 8 + yOffset;
          }
        }
        else if (bitsToDo < 8)
        {
          lcdByte = (lcdByte & (0xff >> (8 - bitsToDo)));
          bitsToDo = 0;
        }
        else
        {
          bitsToDo = bitsToDo - 8;
        }

        // Merge this lcd byte into buffer element value
        lcdByte = (lcdByte << j * 8) >> yOffset;
        bufVal = (bufVal | lcdByte);
      }

      // Put buffer value in right format and save it in graphics buffer
      if (format == ELM_BYTE)
        ((uint8_t *)(emuGrBuf->bufData))[count] = (uint8_t)bufVal;
      else if (format == ELM_WORD)
        ((uint16_t *)(emuGrBuf->bufData))[count] = (uint16_t)bufVal;
      else
        ((uint32_t *)(emuGrBuf->bufData))[count] = (uint32_t)bufVal;
      count++;
    }
  }

  // Administer metadata for raw data
  emuGrBuf->bufType = GRAPH_RAW;
  emuGrBuf->bufOrigin = malloc(strlen(GRAPH_ORIGIN_CTRL) + 1);
  strcpy(emuGrBuf->bufOrigin, GRAPH_ORIGIN_CTRL);
  emuGrBuf->bufElmFormat = format;
  emuGrBuf->bufElmByteSize = formatBytes;
  emuGrBuf->bufElmBitSize = formatBits;
  emuGrBuf->bufElmCount = count;
  emuGrBuf->bufCreate = time(NULL);
}

//
// Function: grBufLoadFile
//
// Load graphics buffer with data from a file
//
u08 grBufLoadFile(char *argName, char formatName, u16 maxElements,
  char *fileName, emuGrBuf_t *emuGrBuf)
{
  FILE *fp;
  u16 count = 0;
  u16 readCount = 0;
  unsigned int bufVal;
  int scanRetVal;
  u08 format;
  u08 formatBytes;
  u08 formatBits;

  // Setup for loading graphics data
  grBufReset(emuGrBuf);
  format = emuFormatGet(formatName, &formatBytes, &formatBits);

  // Open graphics data file
  fp = fopen(fileName, "r");
  if (fp == NULL)
  {
    printf("cannot open data file \"%s\"\n", fileName);
    return CMD_RET_ERROR;
  }

  // Do preliminary scan of file contents element by element
  scanRetVal = fscanf(fp, "%i,", &bufVal);
  while (scanRetVal == 1)
  {
    // Check on buffer overflow
    if (count * formatBytes >= GRAPH_BUF_BYTES)
    {
      printf("buffer overflow at element %d\n", count);
      fclose(fp);
      return CMD_RET_ERROR;
    }

    // Check on value overflow based on data format
    if ((format == ELM_BYTE && bufVal > 0xff) ||
        (format == ELM_WORD && bufVal > 0xffff) ||
        (format == ELM_DWORD && bufVal > 0xffffffff))
    {
      printf("%s? data value overflow at element %d\n", argName, count + 1);
      fclose(fp);
      return CMD_RET_ERROR;
    }

    // Stop when enough elements are loaded
    count++;
    if (maxElements > 0 && count >= maxElements)
      break;

    // Scan next element in file
    scanRetVal = fscanf(fp, "%i,", &bufVal);
  }

  // See if we encountered a scan error
  if (scanRetVal == 0)
  {
    printf("%s? data scan error at element %i\n", argName, count + 1);
    fclose(fp);
    return CMD_RET_ERROR;
  }

  // Reserve a buffer and re-scan file contents element by element
  emuGrBuf->bufData = malloc(count * formatBytes);
  rewind(fp);
  readCount = count;
  for (count = 0; count < readCount; count++)
  {
    // Scan and store scanned element in buffer based on data format
    fscanf(fp, "%i,", &bufVal);
    if (format == ELM_BYTE)
      ((uint8_t *)(emuGrBuf->bufData))[count] = (uint8_t)bufVal;
    else if (format == ELM_WORD)
      ((uint16_t *)(emuGrBuf->bufData))[count] = (uint16_t)bufVal;
    else
      ((uint32_t *)(emuGrBuf->bufData))[count] = (uint32_t)bufVal;
  }
  fclose(fp);

  // Administer (initial) metadata
  emuGrBuf->bufType = GRAPH_RAW;
  emuGrBuf->bufOrigin = malloc(strlen(fileName) + 1);
  strcpy(emuGrBuf->bufOrigin, fileName);
  emuGrBuf->bufElmFormat = format;
  emuGrBuf->bufElmByteSize = formatBytes;
  emuGrBuf->bufElmBitSize = formatBits;
  emuGrBuf->bufElmCount = readCount;
  emuGrBuf->bufCreate = time(NULL);

  return CMD_RET_OK;
}

//
// Function: grBufReset
//
// Reset a graphics buffer
//
void grBufReset(emuGrBuf_t *emuGrBuf)
{
  // Clear malloc-ed buffer and origin info and initialize the buffer itself
  if (emuGrBuf->bufOrigin != NULL)
    free(emuGrBuf->bufOrigin);
  if (emuGrBuf->bufData != NULL)
    free(emuGrBuf->bufData);
  grBufInit(emuGrBuf);
}

//
// Function: grBufSaveFile
//
// Save graphics buffer data in file
//
u08 grBufSaveFile(char *argName, u08 lineElements, char *fileName,
  emuGrBuf_t *emuGrBuf)
{
  FILE *fp;
  int i;
  int element;
  int elements;
  char *printFmt;
  int success;

  // Setup for loading sprite and printed elements per line
  if (emuGrBuf->bufElmFormat == ELM_BYTE)
  {
    printFmt = "0x%02x";
    if (lineElements == 0)
      lineElements = 78 / 5;
  }
  else if (emuGrBuf->bufElmFormat == ELM_WORD)
  {
    printFmt = "0x%04x";
    if (lineElements == 0)
      lineElements = 78 / 7;
  }
  else // ELM_DWORD
  {
    printFmt = "0x%08x";
    if (lineElements == 0)
      lineElements = 78 / 11;
  }

  // Rewrite graphics data file
  fp = fopen(fileName, "w");
  if (fp == NULL)
  {
    printf("%s? cannot open data file \"%s\"\n", argName, fileName);
    return CMD_RET_ERROR;
  }

  // Print all buffer elements, split over multiple output lines
  elements = 0;
  for (i = 0; i < emuGrBuf->bufElmCount; i++)
  {
    // Indent in case of new line
    if (elements == 0)
      fprintf(fp, "  ");

    // Get buffer element value
    if (emuGrBuf->bufElmFormat == ELM_BYTE)
      element = (int)((uint8_t *)(emuGrBuf->bufData))[i];
    else if (emuGrBuf->bufElmFormat == ELM_WORD)
      element = (int)((uint16_t *)(emuGrBuf->bufData))[i];
    else
      element = (int)((uint32_t *)(emuGrBuf->bufData))[i];

    // Write element and trailing ','
    success = fprintf(fp, printFmt, element);
    if (success < 0)
    {
      printf("%s? write error at buffer element %d\n", argName, i);
      fclose(fp);
      return CMD_RET_ERROR;
    }
    if (i != emuGrBuf->bufElmCount - 1)
      fprintf(fp, "%c", ',');

    // Enter new line when needed
    elements++;
    if (elements == lineElements)
    {
      fprintf(fp, "\n");
      elements = 0;
    }
  }

  // Close file
  fclose(fp);

  return CMD_RET_OK;
}

//
// Function: waitDelay
//
// Wait amount of time (in msec) while allowing a 'q' keypress interrupt
//
char waitDelay(int delay)
{
  char ch = '\0';
  struct timeval tvNow;
  struct timeval tvEnd;
  suseconds_t timeDiff;
  struct timespec timeSleep = { 0, 250000000 };
  struct timespec timeRem = { 0, 0 };
  u08 myKbMode = KB_MODE_LINE;

  // Set offset for wait period
  gettimeofday(&tvNow, NULL);
  gettimeofday(&tvEnd, NULL);

  // Set end timestamp based current time plus delay and get diff in time
  tvEnd.tv_usec = tvEnd.tv_usec + delay * 1000;
  timeDiff = TIMEDIFF_USEC(tvEnd, tvNow);

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Wait till end of delay or a 'q' keypress and ignore a remaining wait time
  // that is less than 0.5 msec
  while (ch != 'q' && timeDiff > 500)
  {
    // Split time to delay up in parts of max 250 msec
    if (timeDiff < 250000)
      timeSleep.tv_nsec = timeDiff * 1000;
    nanosleep(&timeSleep, &timeRem);

    // Scan keyboard
    ch = kbKeypressScan(MC_TRUE);
    if (ch == 'q')
      break;

    // Based on last wait and keypress delays get time left to wait
    gettimeofday(&tvNow, NULL);
    timeDiff = TIMEDIFF_USEC(tvEnd, tvNow);
  }

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  // Clear return character for consistent interface
  if (ch != 'q')
    ch = '\0';

  return ch;
}

//
// Function: waitKeypress
//
// Wait for keyboard keypress.
// The keyboard buffer is cleared first to enforce a wait cycle.
//
char waitKeypress(u08 allowQuit)
{
  char ch = '\0';
  u08 myKbMode = KB_MODE_LINE;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Clear keyboard buffer
  kbKeypressScan(MC_FALSE);

  // Wait for single keypress
  if (allowQuit == MC_FALSE)
    printf("<wait: press key to continue> ");
  else
    printf("<wait: q = quit, other key = continue> ");
  fflush(stdout);
  while (ch == '\0')
  {
    // Sleep 150 msec and scan keyboard
    waitSleep(150);
    ch = kbKeypressScan(MC_TRUE);
  }

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  printf("\n");

  return ch;
}

//
// Function: waitSleep
//
// Sleep amount of time (in msec) without keyboard interaction with a max value
// of 999 msec
//
void waitSleep(int sleep)
{
  struct timespec timeSleep;

  timeSleep.tv_sec = 0;
  timeSleep.tv_nsec = sleep * 1000000;
  nanosleep(&timeSleep, NULL);
}

//
// Function: waitTimerExpiry
//
// Wait amount of time (in msec) after a timer (re)start while optionally
// allowing a 'q' keypress interrupt. Restart the timer when timer has already
// expired upon entering this function or has expired after the remaining timer
// period. When pressing the 'q' key the timer will not be restarted.
// (Optional) return parameter remaining will indicate the remaining timer time
// in usec upon entering this function, or -1 in case the timer had already
// expired.
//
char waitTimerExpiry(struct timeval *tvTimer, int expiry, u08 allowQuit,
  suseconds_t *remaining)
{
  char ch = '\0';
  struct timeval tvNow;
  struct timespec timeSleep;
  suseconds_t timeDiff;

  // Get the total time to wait based on timer expiry
  gettimeofday(&tvNow, NULL);
  timeDiff = TIMEDIFF_USEC(*tvTimer, tvNow) + expiry * 1000;

  // See if timer has already expired
  if (timeDiff < 0)
  {
    // Get next timer offset using current time as reference, so do not even
    // attempt to compensate
    if (remaining != NULL)
      *remaining = -1;
    *tvTimer = tvNow;
  }
  else
  {
    // Wait the remaining time of the timer, defaulting to at least 1 msec
    if (remaining != NULL)
      *remaining = timeDiff;
    if (allowQuit == MC_TRUE)
    {
      if ((int)(timeDiff / 1000) == 0)
        ch = waitDelay(1);
      else
        ch = waitDelay((int)(timeDiff / 1000 + (float)0.5));
    }
    else
    {
      timeSleep.tv_sec = timeDiff / 1E6;
      timeSleep.tv_nsec = (timeDiff % (int)1E6) * 1000;
      nanosleep(&timeSleep, NULL);
    }

    // Get next timer offset by adding expiry to current timer offset
    if (ch != 'q')
    {
      // Add expiry to current timer offset
      tvTimer->tv_sec = tvTimer->tv_sec + expiry / 1000;
      tvTimer->tv_usec = tvTimer->tv_usec + (expiry % 1000) * 1000;
      if (tvTimer->tv_usec > 1E6)
      {
        tvTimer->tv_sec++;
        tvTimer->tv_usec = tvTimer->tv_usec - 1E6;
      }
    }
  }

  return ch;
}

//
// Function: waitTimerStart
//
// (Re)set wait timer to current time
//
void waitTimerStart(struct timeval *tvTimer)
{
  // Set timer to current timestamp
  gettimeofday(tvTimer, NULL);
}
