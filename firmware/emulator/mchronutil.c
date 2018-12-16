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
int invokeExit = GLCD_FALSE;
static int closeWinMsg = GLCD_FALSE;

// Local function prototypes
static void emuSigCatch(int sig, siginfo_t *siginfo, void *context);

//
// Function: emuArgcArgvGet
//
// Process mchron startup command line arguments
//
int emuArgcArgvGet(int argc, char *argv[], emuArgcArgv_t *emuArgcArgv)
{
  FILE *fp;
  int argCount = 1;
  char *tty = emuArgcArgv->ctrlDeviceArgs.lcdNcurInitArgs.tty;
  int argHelp = GLCD_FALSE;
  int argError = GLCD_FALSE;

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
    system("/usr/bin/head -23 ../support/help.txt | /usr/bin/tail -20 2>&1");
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
      printf("%s: -l: unknown lcd stub device type: %s\n", __progname,
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

    // Scan the geometry argument using a regexp pattern
    regcomp(&regex, "^[0-9]+x[0-9]+$", REG_EXTENDED | REG_NOSUB);
    status = regexec(&regex, input, (size_t)0, NULL, 0);
    regfree(&regex);
    if (status != 0)
    {
      printf("%s: -g: invalid format glut geometry\n", __progname);
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

    // Scan the position argument using a regexp pattern
    regcomp(&regex, "^[0-9]+,[0-9]+$", REG_EXTENDED | REG_NOSUB);
    status = regexec(&regex, input, (size_t)0, NULL, 0);
    regfree(&regex);
    if (status != 0)
    {
      printf("%s: -p: invalid format glut position\n", __progname);
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
      printf("use switch '-t <tty>' to set lcd output device\n");
      return CMD_RET_ERROR;
    }
    fullPath = malloc(strlen(home) + strlen(NCURSES_TTYFILE) + 1);
    sprintf(fullPath,"%s%s", home, NCURSES_TTYFILE);

    // Open the file with the tty device
    fp = fopen(fullPath, "r");
    free(fullPath);
    if (fp == NULL)
    {
      printf("%s: cannot open file \"%s%s\".\n", __progname, "$HOME",
        NCURSES_TTYFILE);
      printf("start a new Monochron ncurses terminal or use switch '-t <tty>' to set mchron\n");
      printf("ncurses terminal tty\n");
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
      else
        break;
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
  // is killed by the application that is being aborted. Note that at this
  // point glut is still running in its own thread and will have its layout
  // constantly refreshed. This allows a glut screendump to be made if needed.
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
void emuSysTimerStart(timer_t *timer, int interval, void(*handler)(void))
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

  // Set end timestamp based current time plus delay
  tvEnd.tv_usec = tvEnd.tv_usec + delay * 1000;

  // Get the total time to wait
  timeDiff = (tvEnd.tv_sec - tvNow.tv_sec) * 1E6 +
    tvEnd.tv_usec - tvNow.tv_usec;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Wait till end of delay or a 'q' keypress and ignore a remaining wait time
  // that is less than 0.5 msec
  while (ch != 'q' && timeDiff > 500)
  {
    // Split time to delay up in parts of max 250msec
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
    timeDiff = (tvEnd.tv_sec - tvNow.tv_sec) * 1E6 +
      tvEnd.tv_usec - tvNow.tv_usec;
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
char waitKeypress(int allowQuit)
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
// Return parameter remaining will indicate the remaining timer time in usec
// upon entering this function, or -1 in case the timer had already expired.
//
char waitTimerExpiry(struct timeval *tvTimer, int expiry, int allowQuit,
  suseconds_t *remaining)
{
  char ch = '\0';
  struct timeval tvNow;
  struct timeval tvWait;
  suseconds_t timeDiff;

  // Get the total time to wait based on timer expiry
  gettimeofday(&tvNow, NULL);
  timeDiff = (tvTimer->tv_sec - tvNow.tv_sec) * 1E6 +
    tvTimer->tv_usec - tvNow.tv_usec + expiry * 1000;

  // See if timer has already expired
  if (timeDiff < 0)
  {
    // Get next timer offset using current time as reference, so do not even
    // attempt to compensate
    *remaining = -1;
    *tvTimer = tvNow;
  }
  else
  {
    // Wait the remaining time of the timer, defaulting to at least 1 msec
    *remaining = timeDiff;
    if (allowQuit == GLCD_TRUE)
    {
      if ((int)(*remaining / 1000) == 0)
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
