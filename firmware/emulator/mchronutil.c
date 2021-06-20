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

// Avoid typos in eeprom item name when printing the eeprom contents
#define EEPNAME(a)	a, #a

// The graphics data buffer lcd controller origin (as opposed to filename)
#define GRAPH_ORIGIN_CTRL	"lcd controllers"

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

// The mchron alarm time
extern uint8_t emuAlarmH;
extern uint8_t emuAlarmM;

// The command line input stream control structure
extern cmdInput_t cmdInput;

// This is me
extern const char *__progname;

// Flags indicating active state upon exit
u08 invokeExit = GLCD_FALSE;
static u08 closeWinMsg = GLCD_FALSE;

// Local function prototypes
static void emuEepromPrintItem(uint16_t item, char *name);
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context);

//
// Function: emuArgcArgvGet
//
// Process mchron startup command line arguments
//
u08 emuArgcArgvGet(int argc, char *argv[], emuArgcArgv_t *emuArgcArgv)
{
  FILE *fp;
  int argCount = 1;
  char *tty = emuArgcArgv->ctrlDeviceArgs.lcdNcurInitArgs.tty;
  u08 argHelp = GLCD_FALSE;
  u08 argError = GLCD_FALSE;

  // Init references to command line argument positions
  emuArgcArgv->argDebug = 0;
  emuArgcArgv->argGlutGeometry = 0;
  emuArgcArgv->argGlutPosition = 0;
  emuArgcArgv->argTty = 0;
  emuArgcArgv->argLcdType = 0;

  // Init the lcd device data
  emuArgcArgv->ctrlDeviceArgs.useNcurses = GLCD_FALSE;
  emuArgcArgv->ctrlDeviceArgs.useGlut = GLCD_TRUE;
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
      argHelp = GLCD_TRUE;
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
      argError = GLCD_TRUE;
  }

  // Check result of command line processing
  if (argError == GLCD_TRUE)
    printf("%s: invalid/incomplete command argument\n\n", __progname);
  if (argHelp == GLCD_TRUE || argError == GLCD_TRUE)
  {
    system("/usr/bin/head -24 ../support/help.txt | /usr/bin/tail -21 2>&1");
    return CMD_RET_ERROR;
  }

  // Validate lcd stub output device
  if (emuArgcArgv->argLcdType > 0)
  {
    if (strcmp(argv[emuArgcArgv->argLcdType], "glut") == 0)
    {
      emuArgcArgv->ctrlDeviceArgs.useGlut = GLCD_TRUE;
      emuArgcArgv->ctrlDeviceArgs.useNcurses = GLCD_FALSE;
    }
    else if (strcmp(argv[emuArgcArgv->argLcdType], "ncurses") == 0)
    {
      emuArgcArgv->ctrlDeviceArgs.useGlut = GLCD_FALSE;
      emuArgcArgv->ctrlDeviceArgs.useNcurses = GLCD_TRUE;
    }
    else if (strcmp(argv[emuArgcArgv->argLcdType], "all") == 0)
    {
      emuArgcArgv->ctrlDeviceArgs.useGlut = GLCD_TRUE;
      emuArgcArgv->ctrlDeviceArgs.useNcurses = GLCD_TRUE;
    }
    else
    {
      printf("%s: -l: invalid lcd stub device type %s\n", __progname,
        argv[emuArgcArgv->argLcdType]);
      return CMD_RET_ERROR;
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
      return CMD_RET_ERROR;
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
      return CMD_RET_ERROR;
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
      return CMD_RET_ERROR;
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
      printf("- use switch \"-t <tty>\" to set lcd output device\n");
      return CMD_RET_ERROR;
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
      printf("- manually create folder ~%s\n", MCHRON_CONFIG);
      printf("- start a new monochron ncurses terminal or use switch \"-t <tty>\" to set\n");
      printf("  mchron ncurses terminal tty\n");
      return CMD_RET_ERROR;
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
  return CMD_RET_OK;
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
  alarmSoundStop();
  alarmSoundReset();
}

//
// Function: emuClockUpdate
//
// Most clocks update their layout in a single clock cycle.
// However, consider the QR clock. This clock requires multiple clock cycles
// to update its layout due to the above average computing power needed to do
// so.
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
    u08 i;
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
    printf("api info (method/data)= %d\n", arg1);
  }
  else if (origin == CD_EEPROM)
  {
    // Error in the eeprom interface
    printf("\n*** invalid eeprom api request in %s()\n", location);
    printf("api info (address)= %d\n", arg1);
  }
  else if (origin == CD_VAR)
  {
    // Error in the named variable interface
    printf("\n*** invalid var api request in %s()\n", location);
    printf("api info (bucket, index, count) = (%d:%d:%d)\n", arg1, arg2, arg3);
  }

  // Dump all Monochron variables. Might be useful.
  printf("*** registered variables\n");
  varPrint(".", GLCD_TRUE);

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
  // is killed by aborting mchron. Note that at this point glut is still
  // running in its own thread and will have its layout constantly refreshed.
  // This allows a glut screendump to be made if needed.
  if (ctrlDeviceActive(CTRL_DEVICE_NCURSES) == GLCD_TRUE)
  {
    // Flush the ncurses device so we get its contents as-is at the time of
    // the forced coredump
    ctrlLcdFlush();
  }
  else // glut device is used
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
// Function: emuEepromPrint
//
// Warning: This function assumes the contents of the eeprom is according the
// sequence of EE_* defines in monomain.h [firmware]. Keep this function in
// line with these #define's.
//
void emuEepromPrint(void)
{
  uint8_t value;

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
  printf("byte address name             value\n");
  emuEepromPrintItem(EEPNAME(EE_INIT));
  emuEepromPrintItem(EEPNAME(EE_BRIGHT));
  emuEepromPrintItem(EEPNAME(EE_VOLUME));
  emuEepromPrintItem(EEPNAME(EE_REGION));
  emuEepromPrintItem(EEPNAME(EE_TIME_FORMAT));
  emuEepromPrintItem(EEPNAME(EE_SNOOZE));
  emuEepromPrintItem(EEPNAME(EE_BGCOLOR));
  emuEepromPrintItem(EEPNAME(EE_ALARM_SELECT));
  emuEepromPrintItem(EEPNAME(EE_ALARM_HOUR1));
  emuEepromPrintItem(EEPNAME(EE_ALARM_MIN1));
  emuEepromPrintItem(EEPNAME(EE_ALARM_HOUR2));
  emuEepromPrintItem(EEPNAME(EE_ALARM_MIN2));
  emuEepromPrintItem(EEPNAME(EE_ALARM_HOUR3));
  emuEepromPrintItem(EEPNAME(EE_ALARM_MIN3));
  emuEepromPrintItem(EEPNAME(EE_ALARM_HOUR4));
  emuEepromPrintItem(EEPNAME(EE_ALARM_MIN4));
}

//
// Function: emuEepromPrintItem
//
// Print eeprom info for a single Monochron eeprom settings item
//
static void emuEepromPrintItem(uint16_t item, char *name)
{
  uint8_t value;

  value = eeprom_read_byte((uint8_t *)(size_t)item);
  printf("%2d   0x%03x   %-15s %3d (0x%02x)\n", item - EE_OFFSET, item, name,
    value, value);
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
  alarmSoundStop();
  cmdInputCleanup(&cmdInput);
  ctrlCleanup();
  if (invokeExit == GLCD_FALSE && closeWinMsg == GLCD_FALSE)
  {
    closeWinMsg = GLCD_TRUE;
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
//
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context)
{
  //printf("signo=%d\n", siginfo->si_signo);
  //printf("sigcode=%d\n", siginfo->si_code);

  // For signals that should make the application quit, switch back to
  // keyboard line mode and kill audio before we actually exit
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
    invokeExit = GLCD_TRUE;
    emuShutdown();
  }
  else if (sig == SIGTSTP)
  {
    // Keyboard: "^Z"
    printf("\n<ctrl>z - stop\n");
    invokeExit = GLCD_TRUE;
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
    // coredump it requires to run shell command "ulimit -c unlimited"
    // once in the mchron shell prior to starting mchron.
    abort();
  }
  else if (sig == SIGQUIT)
  {
    // Keyboard: "^\"
    // Note that abort() below will trigger a SIGABRT that will be handled
    // separately, eventually causing the program to coredump
    kbModeSet(KB_MODE_LINE);
    alarmSoundStop();
    invokeExit = GLCD_TRUE;
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
    return GLCD_TRUE;
  else // startId == 'n'
    return GLCD_FALSE;
}

//
// Function: emuSysTimerStart
//
// Start a repeating msec realtime interval timer
//
void emuSysTimerStart(timer_t *timer, int interval, void (*handler)(void))
{
  struct itimerspec iTimer;
  struct sigevent sEvent;

  // Setup repeating timer generating signal SIGVTALRM
  iTimer.it_value.tv_sec = interval / 1000;
  iTimer.it_value.tv_nsec = (interval % 1000) * 1E6;
  iTimer.it_interval.tv_sec = iTimer.it_value.tv_sec;
  iTimer.it_interval.tv_nsec = iTimer.it_value.tv_nsec;
  sEvent.sigev_notify = SIGEV_SIGNAL;
  sEvent.sigev_signo = SIGVTALRM;
  sEvent.sigev_value.sival_ptr = handler;

  // Make timer timeout on realtime and start it
  timer_create(CLOCK_REALTIME, &sEvent, timer);
  timer_settime(*timer, 0, &iTimer, NULL);
}

//
// Function: emuSysTimerStop
//
// Stop (disarm) repeating msec realtime interval timer
//
void emuSysTimerStop(timer_t *timer)
{
  struct itimerspec iTimer;

  iTimer.it_value.tv_sec = 0;
  iTimer.it_value.tv_nsec = 0;
  iTimer.it_interval.tv_sec = 0;
  iTimer.it_interval.tv_nsec = 0;
  timer_settime(*timer, 0, &iTimer, NULL);
}

//
// Function: emuTimePrint
//
// Print the time/date/alarm
//
void emuTimePrint(u08 type)
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
  DEBUGTP("Clear time event");
  rtcTimeEvent = GLCD_FALSE;
  rtcMchronTimeInit();
}

//
// Function: grBufCopy
//
// Copy non-empty graphics buffer
//
u08 grBufCopy(emuGrBuf_t *emuGrBufFrom, emuGrBuf_t *emuGrBufTo)
{
  // Reset target buffer
  grBufInit(emuGrBufTo, GLCD_TRUE);

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
  if (emuGrBuf->bufElmFormat == ELM_NULL)
  {
    printf("buffer is empty\n");
    return;
  }

  // Buffer origin, data type and byte size per element
  printf("data origin     : %s\n", emuGrBuf->bufOrigin);
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
void grBufInit(emuGrBuf_t *emuGrBuf, u08 reset)
{
  // Clear malloc-ed buffer origin info, when requested
  if (reset == GLCD_TRUE)
  {
    if (emuGrBuf->bufOrigin != NULL)
      free(emuGrBuf->bufOrigin);
    if (emuGrBuf->bufData != NULL)
      free(emuGrBuf->bufData);
  }

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
  grBufInit(emuGrBuf, GLCD_TRUE);
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
  grBufInit(emuGrBuf, GLCD_TRUE);
  format = emuFormatGet(formatName, &formatBytes, &formatBits);

  // Open graphics data file
  fp = fopen(fileName, "r");
  if (fp == NULL)
  {
    printf("%s? cannot open data file \"%s\"\n", argName, fileName);
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
      grBufInit(emuGrBuf, GLCD_TRUE);
      fclose(fp);
      return CMD_RET_ERROR;
    }

    // Check on value overflow based on data format
    if ((format == ELM_BYTE && bufVal > 0xff) ||
        (format == ELM_WORD && bufVal > 0xffff) ||
        (format == ELM_DWORD && bufVal > 0xffffffff))
    {
      printf("%s? data value overflow at element %d\n", argName, count + 1);
      grBufInit(emuGrBuf, GLCD_TRUE);
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
    grBufInit(emuGrBuf, GLCD_TRUE);
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

  return CMD_RET_OK;
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
  struct timeval tvWait;
  struct timeval tvNow;
  struct timeval tvEnd;
  suseconds_t timeDiff;
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
    tvWait.tv_sec = 0;
    if (timeDiff >= 250000)
      tvWait.tv_usec = 250000;
    else
      tvWait.tv_usec = timeDiff;
    select(0, NULL, NULL, NULL, &tvWait);

    // Scan keyboard
    ch = kbKeypressScan(GLCD_TRUE);
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
  kbKeypressScan(GLCD_FALSE);

  // Wait for single keypress
  if (allowQuit == GLCD_FALSE)
    printf("<wait: press key to continue> ");
  else
    printf("<wait: q = quit, other key = continue> ");
  fflush(stdout);
  while (ch == '\0')
  {
    // Sleep 150 msec and scan keyboard
    waitSleep(150);
    ch = kbKeypressScan(GLCD_TRUE);
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
// Sleep amount of time (in msec) without keyboard interaction
//
void waitSleep(int sleep)
{
  struct timeval tvWait;

  tvWait.tv_sec = 0;
  tvWait.tv_usec = sleep * 1000;
  select(0, NULL, NULL, NULL, &tvWait);
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
  struct timeval tvWait;
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
    if (allowQuit == GLCD_TRUE)
    {
      if ((int)(timeDiff / 1000) == 0)
        ch = waitDelay(1);
      else
        ch = waitDelay((int)(timeDiff / 1000 + (float)0.5));
    }
    else
    {
      tvWait.tv_sec = timeDiff / 1E6;
      tvWait.tv_usec = timeDiff % (int)1E6;
      select(0, NULL, NULL, NULL, &tvWait);
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
