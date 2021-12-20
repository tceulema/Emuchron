//*****************************************************************************
// Filename : 'stub.c'
// Title    : Stub functionality for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

// Monochron and emuchron defines
#include "../global.h"
#include "../monomain.h"
#include "../buttons.h"
#include "../config.h"
#include "mchronutil.h"
#include "scanutil.h"
#include "stub.h"

// When used in a VM, Linux ALSA performance gets progressively worse with
// every Debian release. As of Debian 8, short audio pulses are being clipped
// while also generating an alsa buffer under-run runtime error. This behavior
// impacts the mchron 'b' (beep) command.
// In any case, it turns out that when prefixing a short audio pulse with a
// larger pulse with no audio (0 Hz), the buffer under-run is avoided, and the
// subsequent small audio pulse is no longer clipped.
// Hence the following (kludgy) method to mitigate the alsa buffer under-run
// runtime error using the #define below.
//
// ALSA_PREFIX_PULSE_MS: Set the blank audio pulse cutoff (msec).
// Value 0 means: Do not generate a prefix blank pulse.
// Only in case a pulse that is to be played is equal or smaller than this
// value, a blank pulse will be prefixed with this particular length plus a
// fixed 25 msec (ALSA_PREFIX_ADD_MS).
// Note that depending on the system on which emuchron runs this pulse length
// may be larger or smaller or not needed at all. Play (pun intended) with
// both values until you find something that makes you feel comfortable.
// On my machines prefix pulse value 95 (msec) seems to hit a sweet spot.
// To re-iterate, only Debian run as a VM seems to require a blank prefix
// pulse. Use mchron command 'b' (beep) to test this.
//
// Emuchron v4.1:
// By changing the parameters in the sox command to play audio, it looks like
// short audio pulses are no longer clipped. As such, the prefix pulse length
// is set to 0, meaning we're no longer using it. If needed switch it on again.
//
//#define ALSA_PREFIX_PULSE_MS	95
#define ALSA_PREFIX_PULSE_MS	0
#define ALSA_PREFIX_ADD_MS	(ALSA_PREFIX_PULSE_MS + 25)

// Emulator timer stub cycle state
#define CYCLE_NOWAIT		0
#define CYCLE_WAIT		1
#define CYCLE_WAIT_STATS	2
#define CYCLE_REQ_NOWAIT	3
#define CYCLE_REQ_WAIT		4

// Debug output buffer size
#define DEBUG_BUFSIZE		100

// Monochron global data
extern volatile uint8_t almAlarming;
extern uint16_t almTickerSnooze;
extern int16_t almTickerAlarm;
extern volatile rtcDateTime_t rtcDateTime;
extern volatile uint8_t mcAlarming;

// Monochron config event timeout counter
extern volatile uint8_t cfgTickerActivity;

// This is me
extern const char *__progname;

// Stubbed button data
volatile uint8_t btnPressed = BTN_NONE;
volatile uint8_t btnHold = BTN_NONE;
volatile uint8_t btnTickerHold = 0;
volatile uint8_t btnHoldRelReq = MC_FALSE;
volatile uint8_t btnHoldRelCfm = MC_FALSE;

// Stubbed hardware related stuff - Not sure if sizes are correct (8/16-bit)
uint8_t MCUSR = 0;
uint8_t DDRB = 0;
uint8_t DDRC = 0;
uint8_t DDRD = 0;
uint8_t UDR0 = 0;
uint16_t UBRR0 = 0;
uint8_t UCSR0A = 0;
uint8_t UCSR0B = 0;
uint8_t UCSR0C = 0;
uint8_t TCCR0A = 0;
uint8_t TCCR0B = 0;
uint8_t OCR0A = 0;
uint8_t OCR2A = 0;
uint8_t OCR2B = 16; // Init value 16 defines full lcd backlight brightness
uint8_t TIMSK0 = 0;
uint8_t TIMSK2 = 0;
uint8_t TCCR1B = 0;
uint8_t TCCR2A = 0;
uint8_t TCCR2B = 0;
uint8_t PORTB = 0;
uint8_t PORTC = 0;
uint8_t PORTD = 0;
uint8_t PINB = 0;
uint8_t PIND = 0;

// Debug output file and debug output buffer
FILE *stubDebugStream = NULL;
static unsigned char debugBuffer[DEBUG_BUFSIZE];
static u08 debugCount = 0;

// Keypress hold data
static u08 hold = MC_FALSE;
static char lastChar = '\0';

// Stubbed eeprom data. An atmega328p has 1 KB of eeprom.
static uint8_t stubEeprom[EE_SIZE];

// Stubbed alarm play pid
static pid_t alarmPid = -1;

// Local brightness to detect changes
static u08 stubBacklight = 16;

// Active help function when running an emulator
static void (*stubHelp)(void) = NULL;

// Event handler stub data
static u08 eventCycleState = CYCLE_NOWAIT;
static u08 eventCfgTimeout = MC_TRUE;
static u08 eventQuitReq = MC_FALSE;
static u08 eventInit = MC_TRUE;

// Date/time and timer statistics data
static double timeDelta = 0L;
static struct timeval tvCycleTimer;
static int inTimeCount = 0;
static int outTimeCount = 0;
static int singleCycleCount = 0;
static long long waitTotal = 0;
static int minSleep = ANIM_TICK_CYCLE_MS + 1;

// Terminal settings for stdin keypress mode data
static struct termios termOld, termNew;
static u08 kbMode = KB_MODE_LINE;

// Local function prototypes
static void alarmPidStart(void);
static void alarmPidStop(void);
static int kbHit(void);

//
// Function: alarmPidStart
//
// Stub to start playing a continuous audio alarm
//
static void alarmPidStart(void)
{
  // Don't do anything if we're already playing
  if (alarmPid >= 0)
    return;

  // Fork this process to play the alarm sound
  alarmPid = fork();

  // Were we successful in forking
  if (alarmPid < 0)
  {
    // Failure to fork
    DEBUGP("*** Cannot fork alarm play process");
    alarmPid = -2;
    return;
  }
  else if (alarmPid == 0)
  {
    // We forked successfully!
    // Emuchron tool genalarm generated an audio file for us to play.
    // The genalarm tool is built and run via MakefileEmu [firmware].
    execlp("/usr/bin/play", "/usr/bin/play", "-q", "emulator/alarm.au", "-t",
      "alsa", "repeat", "3600", NULL);
    exit(0);
  }
  else
  {
    // Log info on alarm audio process
    if (DEBUGGING)
    {
      char msg[45];
      sprintf(msg, "playing alarm audio via pid %d", alarmPid);
      DEBUGP(msg);
    }
  }
}

//
// Function: alarmPidStop
//
// Stub to stop playing the alarm. It may be restarted by functional code.
//
static void alarmPidStop(void)
{
  // Only stop when a process was started to play the alarm
  if (alarmPid >= 0)
  {
    // Kill it...
    char msg[45];
    sprintf(msg, "stopping alarm audio via pid %d", alarmPid);
    DEBUGP(msg);
    sprintf(msg, "kill -s 9 %d", alarmPid);
    system(msg);
  }
  alarmPid = -1;
}

//
// Function: alarmSoundReset
//
// Stub to reset the internal alarm settings. It is needed to prevent the alarm
// to restart when the clock feed or Monochron command is quit while alarming
// and is later restarted with the same or different settings.
//
void alarmSoundReset(void)
{
  mcAlarming = MC_FALSE;
  almAlarming = MC_FALSE;
  almTickerSnooze = 0;
  almTickerAlarm = -1;
}

//
// Function: alarmSoundStop
//
// Stub to stop playing the alarm and reset the alarm triggers.
// Audible alarm may resume later upon request.
//
void alarmSoundStop(void)
{
  alarmPidStop();
  mcAlarming = MC_FALSE;
  almTickerSnooze = 0;
}

//
// Function: alarmSwitchSet
//
// Set the alarm switch position to on or off
//
void alarmSwitchSet(u08 on, u08 show)
{
  // Switch alarm on or off
  if (on == MC_TRUE)
    ALARM_PIN = ALARM_PIN & ~_BV(ALARM);
  else
    ALARM_PIN = ALARM_PIN | _BV(ALARM);
  if (show == MC_TRUE)
    alarmSwitchShow();
}

//
// Function: alarmSwitchShow
//
// Report the alarm switch position
//
void alarmSwitchShow(void)
{
  // Show alarm switch state
  if (ALARM_PIN & _BV(ALARM))
    printf("alarm  : off\n");
  else
    printf("alarm  : on\n");
}

//
// Function: alarmSwitchToggle
//
// Toggle the alarm switch position
//
void alarmSwitchToggle(u08 show)
{
  // Toggle alarm switch
  if (ALARM_PIN & _BV(ALARM))
    ALARM_PIN = ALARM_PIN & ~_BV(ALARM);
  else
    ALARM_PIN = ALARM_PIN | _BV(ALARM);
  if (show == MC_TRUE)
    alarmSwitchShow();
}

//
// Function: _delay_ms
//
// Stub to delay time in milliseconds
//
void _delay_ms(int x)
{
  struct timeval tvWait;

  // This is ugly....
  // When we're asked to sleep for KEYPRESS_DLY_1 msec assume we're called
  // from buttons.c.
  // Applying this sleep will make a poor UI experience with respect to
  // keyboard input handling. This is caused by undetected/unprocessed
  // trailing keyboard events caused by system sleep interaction. By ignoring
  // KEYPRESS_DLY_1 msec requests I will make you, dear user, very happy.
  if (x == KEYPRESS_DLY_1)
    return;
  tvWait.tv_sec = 0;
  tvWait.tv_usec = x * 1000;
  select(0, NULL, NULL, NULL, &tvWait);
}

//
// Function: eeprom_read_byte
//
// Stub for eeprom data read
//
uint8_t eeprom_read_byte(uint8_t *eprombyte)
{
  if ((size_t)eprombyte >= EE_SIZE)
    emuCoreDump(CD_EEPROM, __func__, (int)(size_t)eprombyte, 0, 0, 0);
  return stubEeprom[(size_t)eprombyte];
}

//
// Function: eeprom_write_byte
//
// Stub for eeprom data write
//
void eeprom_write_byte(uint8_t *eprombyte, uint8_t value)
{
  if ((size_t)eprombyte >= EE_SIZE)
    emuCoreDump(CD_EEPROM, __func__, (int)(size_t)eprombyte, 0, 0, 0);
  stubEeprom[(size_t)eprombyte] = value;
}

//
// Function: i2cMasterReceiveNI
//
// Receive time data from RTC
//
u08 i2cMasterReceiveNI(u08 deviceAddr, u08 length, u08 *data)
{
  if (length == 7)
  {
    // Assume it is a request to get the RTC time
    struct timeval tv;
    struct tm *tm;
    time_t timeClock;

    gettimeofday(&tv, NULL);
    timeClock = tv.tv_sec + timeDelta;
    tm = localtime(&timeClock);

    data[0] = bcdEncode(tm->tm_sec);
    data[1] = bcdEncode(tm->tm_min);
    data[2] = bcdEncode(tm->tm_hour);
    data[4] = bcdEncode(tm->tm_mday);
    data[5] = bcdEncode(tm->tm_mon + 1);
    data[6] = bcdEncode(tm->tm_year % 100);
    return 0;
  }
  else
  {
    // Unsupported request
    return 1;
  }
}

//
// Function: i2cMasterSendNI
//
// Send command data to RTC
//
u08 i2cMasterSendNI(u08 deviceAddr, u08 length, u08* data)
{
  u08 retVal;

  if (length == 1)
  {
    // Assume it is a request to verify the presence of the RTC
    retVal = 0;
  }
  else if (length == 8)
  {
    // Assume it is a request to set the RTC time
    uint8_t sec, min, hr, date, mon, yr;

    sec = ((data[1] >> 4) & 0xf) * 10 + (data[1] & 0xf);
    min = ((data[2] >> 4) & 0xf) * 10 + (data[2] & 0xf);
    hr = ((data[3] >> 4) & 0xf) * 10 + (data[3] & 0xf);
    date = ((data[5] >> 4) & 0xf) * 10 + (data[5] & 0xf);
    mon = ((data[6] >> 4) & 0xf) * 10 + (data[6] & 0xf);
    yr = ((data[7] >> 4) & 0xf) * 10 + (data[7] & 0xf);
    stubTimeSet(sec, min, hr, date, mon, yr);
    retVal = 0;
  }
  else
  {
    // Unsupported request
    retVal = 1;
  }

  return retVal;
}

//
// Function: kbHit
//
// Get keypress (if any)
//
// WARNING: Prior to using this function make sure the keyboard is set to scan
// mode using kbModeGet()/kbModeSet().
//
static int kbHit(void)
{
  struct timeval tvWait;
  fd_set rdfs;

  tvWait.tv_sec = 0;
  tvWait.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET(STDIN_FILENO, &rdfs);

  // The keyboard scan result of an interrupted select is unreliable and will
  // cause a false positive keyboard hit. An interrupted select will happen
  // every now and then. Keep trying until select properly completes.
  while (select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tvWait) < 0);

  return FD_ISSET(STDIN_FILENO, &rdfs);
}

//
// Function: kbKeypressScan
//
// Scan keyboard for keypress. The lowercase value of the last key in the
// keyboard buffer is returned.
// However, if a search for the quit key is requested and that key was pressed,
// the quit character 'q' is returned instead.
// No keypress returns the null character.
//
char kbKeypressScan(u08 quitFind)
{
  char ch = '\0';
  u08 quitFound = MC_FALSE;
  u08 myKbMode = KB_MODE_LINE;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Read pending input buffer
  while (kbHit())
  {
    ch = getchar();
    if (quitFind == MC_TRUE && (ch == 'q' || ch == 'Q'))
      quitFound = MC_TRUE;
  }

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  // Return quit key or lowercase keypress
  if (quitFound == MC_TRUE)
    ch = 'q';
  else if (ch >= 'A' && ch <= 'Z')
    ch = ch - 'A' + 'a';

  return ch;
}

//
// Function: kbModeGet
//
// Get keyboard input mode (line or keypress)
//
u08 kbModeGet(void)
{
  return kbMode;
}

//
// Function: kbModeSet
//
// Set keyboard input mode
//
void kbModeSet(u08 mode)
{
  // Only change mode if needed to avoid weird keyboard behavior
  if (mode == KB_MODE_SCAN && kbMode != KB_MODE_SCAN)
  {
    // Setup keyboard scan (signal keypress)
    tcgetattr(STDIN_FILENO, &termOld);
    termNew = termOld;
    termNew.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &termNew);
    kbMode = KB_MODE_SCAN;
  }
  else if (mode == KB_MODE_LINE && kbMode != KB_MODE_LINE)
  {
    // Setup line scan (signal cr/lf)
    tcsetattr(STDIN_FILENO, TCSANOW, &termOld);
    kbMode = KB_MODE_LINE;
  }
}

//
// Function: stubBeep
//
// Stub for system beep
//
void stubBeep(uint16_t hz, uint8_t msec)
{
  char shellCmd[160];

  if (ALSA_PREFIX_PULSE_MS > msec)
  {
    // This is a short beep that requires alsa buffer under-run mitigation.
    // Add the blank pulse in front of the actual pulse to be played.
    int length;
    length = sprintf(shellCmd,
      "/usr/bin/play -q '|/usr/bin/sox -b16 -r21k -DGnp synth %f sin 0' ",
      (float)(ALSA_PREFIX_ADD_MS) / 1000);
    sprintf(&shellCmd[length],
      "'|/usr/bin/sox -b16 -r21k -DGnp synth %f sin %d' -t alsa 2>/dev/null",
      (float)(msec) / 1000L, hz);
  }
  else
  {
    // Just play the requested beep
    sprintf(shellCmd,
      "/usr/bin/play -q -b16 -r21k -DGn -t alsa synth %f sin %d 2>/dev/null",
      (float)(msec) / 1000L, hz);
  }
  system(shellCmd);
}

//
// Function: stubEepReset
//
// Stub for eeprom initialization
//
void stubEepReset(void)
{
  memset(stubEeprom, 0xff, EE_SIZE);
}

//
// Function: stubEventGet
//
// Get an mchron event. It is a combination of a 75 msec timer wait event since
// previous call, an optional keyboard event emulating the three buttons
// (m,s,+) and alarm switch (a), and misc emulator commands.
//
char stubEventGet(u08 stats)
{
  char ch = '\0';
  uint8_t keypress = MC_FALSE;
  suseconds_t remaining;

  // Flush the lcd device
  ctrlLcdFlush();

  // Prevent config menu event timeout
  if (eventCfgTimeout == MC_FALSE)
    cfgTickerActivity = CFG_TICK_ACTIVITY_SEC;

  // Sleep the remainder of the current cycle
  if (eventInit == MC_TRUE)
  {
    // The first entry may not invoke a sleep but instead marks the timer
    // timestamp for the next cycle
    waitTimerStart(&tvCycleTimer);
    eventInit = MC_FALSE;
  }
  else
  {
    // Wait remaining time for cycle while detecting timer expiry
    waitTimerExpiry(&tvCycleTimer, ANIM_TICK_CYCLE_MS, MC_FALSE, &remaining);
    if (stats == MC_TRUE)
    {
      if (remaining >= 0)
      {
        inTimeCount++;
        waitTotal = waitTotal + remaining;
        if (minSleep > (int)round(remaining / 1000))
          minSleep = (int)round(remaining / 1000);
      }
      else if (eventCycleState == CYCLE_NOWAIT)
      {
        outTimeCount++;
      }
    }
  }

  // Detect changes in lcd brightness
  if ((OCR2B >> OCR2B_BITSHIFT) != stubBacklight)
  {
    stubBacklight = (OCR2B >> OCR2B_BITSHIFT);
    ctrlLcdBacklightSet(stubBacklight);
  }

  // Check if we run in single timer cycle
  if (eventCycleState == CYCLE_REQ_WAIT || eventCycleState == CYCLE_WAIT ||
      eventCycleState == CYCLE_WAIT_STATS)
  {
    // Statistics
    if (stats == MC_TRUE)
      singleCycleCount++;

    // Reset any keypress hold
    hold = MC_FALSE;

    // When going to cycle mode stop the alarm (if sounding) for non-nerve
    // wrecking emulator behavior, and also give a cycle mode helpmessage
    if (eventCycleState == CYCLE_REQ_WAIT)
    {
      alarmPidStop();
      printf("\n<cycle: c = next cycle, p = next cycle + stats, q = quit, other key = resume> ");
      fflush(stdout);
      // Continue in single cycle mode
      eventCycleState = CYCLE_WAIT;
    }

    // If we're in cycle mode with stats print the stats that are current
    // after the last executed clock cycle, and repeat the cycle mode help
    // message
    if (eventCycleState == CYCLE_WAIT_STATS)
    {
      // Print and glcd/controller performance statistics
      printf("\n");
      ctrlStatsPrint(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
      printf("\n<cycle: c = next cycle, p = next cycle + stats, q = quit, other key = resume> ");
      fflush(stdout);
      ctrlStatsReset(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
    }

    // Wait for keypress every 75 msec interval
    ch = kbKeypressScan(MC_FALSE);
    while (ch == '\0')
    {
      waitSleep(75);
      ch = kbKeypressScan(MC_FALSE);
    }

    // Detect cascading quit that overides keypress
    if (eventQuitReq == MC_TRUE)
      ch = 'q';

    // Verify keypress and its impact on the wait state
    if (ch == 'q')
    {
      // Quit the clock emulator mode
      eventCycleState = CYCLE_REQ_NOWAIT;
      eventQuitReq = MC_TRUE;
      printf("\n");
      return ch;
    }
    else if (ch != 'c' && ch != 'p')
    {
      // Not a 'c' or 'p' character: resume to normal mode state
      eventCycleState = CYCLE_REQ_NOWAIT;
      printf("\n");

      // Resume alarm audio (when needed)
      alarmPid = -1;
    }
    else if (eventCycleState == CYCLE_WAIT && ch == 'p')
    {
      // Move from wait state to wait with statistics
      eventCycleState = CYCLE_WAIT_STATS;
      ctrlStatsReset(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
    }
    else if (eventCycleState == CYCLE_WAIT_STATS && ch == 'c')
    {
      // Move from wait with statistics state back to wait
      eventCycleState = CYCLE_WAIT;
    }
    ch = '\0';
  }
  else if (eventCycleState == CYCLE_REQ_NOWAIT)
  {
    // Resume from single cycle mode
    eventCycleState = CYCLE_NOWAIT;
  }

  // Get clock time and set alarm state
  monoTimer();

  // Do we need to do anything with the alarm sound
  if (alarmPid == -1 && almAlarming == MC_TRUE && almTickerSnooze == 0 &&
      (eventCycleState == CYCLE_REQ_NOWAIT || eventCycleState == CYCLE_NOWAIT))
  {
    // Start playing the alarm sound
    alarmPidStart();
  }
  else if (alarmPid >= 0 && (almAlarming == MC_FALSE ||
      almTickerSnooze > 0 || eventCycleState == CYCLE_WAIT))
  {
    // Stop playing the alarm sound
    alarmPidStop();
  }

  // Check if keyboard was hit
  btnHold = BTN_NONE;
  while (kbHit())
  {
    // Get keyboard character and make uppercase char a lowercase char
    keypress = MC_TRUE;
    ch = getchar();
    if (ch >= 'A' && ch <= 'Z')
      ch = ch - 'A' + 'a';

    // Detect button hold start/end
    if (ch == lastChar)
    {
      hold = MC_TRUE;
    }
    else
    {
      lastChar = ch;
      hold = MC_FALSE;
    }

    if (ch == 'a')
    {
      // Toggle the alarm switch
      printf("\n");
      alarmSwitchToggle(MC_TRUE);
    }
    else if (ch == 'c')
    {
      // Init single timer cycle
      if (eventCycleState == CYCLE_NOWAIT)
        eventCycleState = CYCLE_REQ_WAIT;
    }
    else if (ch == 'e')
    {
      // Print monochron eeprom settings
      printf("\n");
      emuEepromPrint();
    }
    else if (ch == 'h')
    {
      // Provide help
      printf("\n");
      if (stubHelp != NULL)
        stubHelp();
      else
        printf("no help available\n");
    }
    else if (ch == 'm')
    {
      // Menu button
      btnPressed = BTN_MENU;
    }
    else if (ch == 'p')
    {
      // Print stub and glcd/lcd performance statistics
      printf("\nstatistics:\n");
      stubStatsPrint();
      ctrlStatsPrint(CTRL_STATS_ALL);
    }
    else if (ch == 'q')
    {
      // Signal request to quit
      eventQuitReq = MC_TRUE;
    }
    else if (ch == 'r')
    {
      // Reset stub and glcd/lcd performance statistics
      stubStatsReset();
      ctrlStatsReset(CTRL_STATS_ALL);
      printf("\nstatistics reset\n");
    }
    else if (ch == 's')
    {
      // Set button
      btnPressed = BTN_SET;
    }
    else if (ch == 't')
    {
      // Print time/date/alarm
      printf("\n");
      emuTimePrint(ALM_MONOCHRON);
    }
    else if (ch == '+')
    {
      // + button
      if (hold == MC_FALSE)
        btnPressed = BTN_PLUS;
      else
        btnHold = BTN_PLUS;
    }
    else if (ch == '\n')
    {
      // Maybe the user wants to see a blank line, so echo it
      printf("\n");
    }
  }

  // Detect button hold end and confirm when requested
  if (keypress == MC_FALSE || btnHold == BTN_NONE)
  {
    lastChar = '\0';
    hold = MC_FALSE;
    if (btnHoldRelReq == MC_TRUE)
    {
      btnHoldRelCfm = MC_TRUE;
      btnHoldRelReq = MC_FALSE;
    }
  }

  // Detect cascading quit that overides any keypress
  if (eventQuitReq == MC_TRUE)
    ch = 'q';

  return ch;
}

//
// Function: stubEventInit
//
// Prepare the event generator for initial use by a requesting process
//
void stubEventInit(u08 startWait, u08 cfgTimeout, void (*stubHelpHandler)(void))
{
  eventInit = MC_TRUE;
  eventCfgTimeout = cfgTimeout;
  eventQuitReq = MC_FALSE;
  if (startWait == MC_TRUE)
    eventCycleState = CYCLE_REQ_WAIT;
  else
    eventCycleState = CYCLE_NOWAIT;
  btnPressed = BTN_NONE;
  stubHelp = stubHelpHandler;
  stubHelp();
}

//
// Function: stubEventQuitGet
//
// Has a quit keypress event occurred
//
u08 stubEventQuitGet(void)
{
  return eventQuitReq;
}

//
// Function: stubHelpClockFeed
//
// Provide keypress help when running the clock emulator
//
void stubHelpClockFeed(void)
{
  printf("emuchron clock emulator:\n");
  printf("  c = execute single application clock cycle\n");
  printf("  e = print monochron eeprom settings\n");
  printf("  h = provide emulator help\n");
  printf("  p = print performance statistics\n");
  printf("  q = quit\n");
  printf("  r = reset performance statistics\n");
  printf("  t = print time/date/alarm\n");
  printf("hardware stub keys:\n");
  printf("  a = toggle alarm switch\n");
  printf("  s = set button\n");
  printf("  + = + button\n");
}

//
// Function: stubHelpMonochron
//
// Provide keypress help when running the Monochron emulator
//
void stubHelpMonochron(void)
{
  printf("emuchron monochron emulator:\n");
  printf("  c = execute single application clock cycle\n");
  printf("  e = print monochron eeprom settings\n");
  printf("  h = provide emulator help\n");
  printf("  p = print performance statistics\n");
  printf("  q = quit\n");
  printf("  r = reset performance statistics\n");
  printf("  t = print time/date/alarm\n");
  printf("hardware stub keys:\n");
  printf("  a = toggle alarm switch\n");
  printf("  m = menu button\n");
  printf("  s = set button\n");
  printf("  + = + button\n");
}

//
// Function: stubLogfileClose
//
// Close the debug logfile
//
void stubLogfileClose(void)
{
  if (stubDebugStream != NULL)
    fclose(stubDebugStream);
}

//
// Function: stubLogfileOpen
//
// Open the debug logfile.
// Return: MC_TRUE (success) or MC_FALSE (failure).
//
u08 stubLogfileOpen(char fileName[])
{
  if (!DEBUGGING)
  {
    printf("%s: -d: master debugging is off\n", __progname);
    printf("assign value 1 to \"#define DEBUGGING\" in global.h [firmware] and rebuild\n");
    printf("mchron.\n");
    return MC_FALSE;
  }
  else
  {
    // Initialize the debug output buffer and try to open logfile
    memset(debugBuffer, '\0', sizeof(debugBuffer));
    stubDebugStream = fopen(fileName, "a");
    if (stubDebugStream == NULL)
    {
      // Something went wrong opening the logfile
      int error = errno;
      printf("%s: -d: error opening debug output file: %s\n", __progname,
        strerror(error));
      return MC_FALSE;
    }
    else
    {
      // Disable buffering so we can properly 'tail -f' the file
      setbuf(stubDebugStream, NULL);
      DEBUGP("**** logging started");
      return MC_TRUE;
    }
  }
}

//
// Function: stubStatsPrint
//
// Print stub statistics
//
void stubStatsPrint(void)
{
  printf("stub   : cycle=%d msec, inTime=%d, outTime=%d, singleCycle=%d\n",
    ANIM_TICK_CYCLE_MS, inTimeCount, outTimeCount, singleCycleCount);

  if (inTimeCount == 0)
    printf("         avgSleep=- msec, ");
  else
    printf("         avgSleep=%.0f msec, ",
      waitTotal / (double)inTimeCount / 1000);

  if (minSleep == ANIM_TICK_CYCLE_MS + 1)
    printf("minSleep=- msec\n");
  else
    printf("minSleep=%d msec\n",minSleep);
}

//
// Function: stubStatsReset
//
// Reset stub statistics
//
void stubStatsReset(void)
{
  // Reset stub statistics
  inTimeCount = 0;
  outTimeCount = 0;
  singleCycleCount = 0;
  waitTotal = 0;
  minSleep = ANIM_TICK_CYCLE_MS + 1;
}

//
// Function: stubTimeSet
//
// Set mchron time
//
// Values for respectively sec (for time) / date (for date):
// DT_TIME_KEEP / DT_DATE_KEEP = keep time or date as-is
// DT_TIME_RESET / DT_DATE_RESET = reset time or date to <now>
// other = use parameters for new time or date
//
u08 stubTimeSet(uint8_t sec, uint8_t min, uint8_t hr, uint8_t date,
  uint8_t mon, uint8_t yr)
{
  double timeDeltaNew = 0L;
  struct timeval tvNow;
  struct timeval tvNew;
  struct tm *tmNow;
  struct tm *tmNew;
  struct tm tmNowCopy;
  struct tm tmNewCopy;
  struct tm tmNewDateValidate1;
  struct tm tmNewDateValidate2;
  time_t timeNow;
  time_t timeNew;
  time_t timeClock;

  // Init system time and get monochron time
  rtcTimeRead();
  gettimeofday(&tvNow, NULL);
  tmNow = localtime(&tvNow.tv_sec);

  // Copy current time
  tmNowCopy = *tmNow;
  tmNewCopy = *tmNow;

  // Verify what to do with time
  if (sec == DT_TIME_KEEP)
  {
    // Keep current time offset
    tmNewCopy.tm_sec = rtcDateTime.timeSec;
    tmNewCopy.tm_min = rtcDateTime.timeMin;
    tmNewCopy.tm_hour = rtcDateTime.timeHour;
  }
  else if (sec != DT_TIME_RESET)
  {
    // Overide on hms
    tmNewCopy.tm_sec = sec;
    tmNewCopy.tm_min = min;
    tmNewCopy.tm_hour = hr;
  }
  // else DT_TIME_RESET -> default back to system time as currently populated

  // Verify what to do with date
  if (date == DT_DATE_KEEP)
  {
    // Keep current date offset
    tmNewCopy.tm_mday = rtcDateTime.dateDay;
    tmNewCopy.tm_mon = rtcDateTime.dateMon - 1;
    tmNewCopy.tm_year = rtcDateTime.dateYear + 100;
  }
  else if (date != DT_DATE_RESET)
  {
    // Overide on dmy
    tmNewCopy.tm_mday = date;
    tmNewCopy.tm_mon = mon - 1;
    tmNewCopy.tm_year = yr + 100;
  }
  // else DT_DATE_RESET -> default back to system time as currently populated

  // Prepare for a validity check on the requested date
  tmNewDateValidate1 = tmNewCopy;
  tmNewDateValidate2 = tmNewCopy;

  // Get system timestamp for current and new time to obtain the time delta
  // between the two timestamps
  timeNow = mktime(&tmNowCopy);
  timeNew = mktime(&tmNewCopy);
  if (timeNew == -1)
  {
    // The requested date is too far in future or past
    printf("date? beyond system range\n");
    return MC_FALSE;
  }
  timeDeltaNew = difftime(timeNew, timeNow);

  // Verify the requested date by making use of the fact that mktime 'corrects'
  // an invalid input date such as sep 31 to oct 1.
  // As mktime also shifts a time due to DST settings, that may cause a shift
  // in a day as well, set the time in the date to verify to around noon to
  // make sure such a DST timeshift cannot affect the date.
  tmNewDateValidate1.tm_hour = 12;
  timeClock = mktime(&tmNewDateValidate1);
  if (tmNewDateValidate1.tm_mday != tmNewDateValidate2.tm_mday ||
      tmNewDateValidate1.tm_mon != tmNewDateValidate2.tm_mon ||
      tmNewDateValidate1.tm_year != tmNewDateValidate2.tm_year)
  {
    printf("date? invalid\n");
    return MC_FALSE;
  }

  // Get delta between earlier retrieved current and new time and apply it on a
  // fresh current timestamp
  gettimeofday(&tvNew, NULL);
  timeClock = tvNew.tv_sec + timeDeltaNew;
  tmNew = localtime(&timeClock);

  // Apply compensation for DST shifts
  if (tmNowCopy.tm_isdst == 0 && tmNew->tm_isdst > 0)
  {
    // Moving from non-DST to DST: subtract 1 hour
    timeDeltaNew = timeDeltaNew - 3600;
  }
  else if (tmNowCopy.tm_isdst > 0 && tmNew->tm_isdst == 0)
  {
    // Moving from DST to non-DST: add 1 hour
    timeDeltaNew = timeDeltaNew + 3600;
  }

  // Accept new time delta
  timeDelta = timeDeltaNew;

  // Sync mchron clock time based on new delta
  rtcTimeRead();

  return MC_TRUE;
}

//
// Function: stubUartPutChar
//
// Stub for debug char. Output is redirected to output file as specified on the
// mchron command line. If no output file is specified, debug info is
// discarded.
// The debug char is buffered until a newline is pushed or the buffer is full.
// Note: To enable debugging set DEBUGGING to 1 in monomain.h
//
void stubUartPutChar(void)
{
  unsigned char debugChar = (unsigned char)UDR0;

  if (stubDebugStream != NULL)
  {
    // Buffer the debug character
    debugBuffer[debugCount] = debugChar;
    debugCount++;

    // Dump buffer when a newline is pushed or the buffer is full
    if (debugChar == '\n' || debugCount == sizeof(debugBuffer) - 1)
    {
      // Dump and reset buffer
      fprintf(stubDebugStream, "%s", debugBuffer);
      memset(debugBuffer, '\0', debugCount);
      debugCount = 0;
    }
  }
}

//
// Several empty stubs for unlinked hardware related functions
//
void i2cInit(void) { return; }
void btnInit(void) { return; }
