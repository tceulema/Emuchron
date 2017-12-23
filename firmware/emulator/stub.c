//*****************************************************************************
// Filename : 'stub.c'
// Title    : Stub functionality for emuchron emulator
//*****************************************************************************

// Everything we need for running this thing in Linux
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// Monochron and emuchron defines
#include "../monomain.h"
#include "../buttons.h"
#include "../ks0108.h"
#include "../glcd.h"
#include "../config.h"
#include "../alarm.h"
#include "stub.h"
#include "mchronutil.h"
#include "scanutil.h"

// When used in a VM, Linux ALSA performance gets progressively worse with
// every Debian release. As of Debian 8, short audio pulses are being clipped
// while also generating an alsa buffer under-run runtime error.
// In any case, it turns out that when prefixing a short audio pulse with a
// larger pulse with no audio (0 Hz), the buffer under-run is avoided, and the
// subsequent small audio pulse is no longer clipped. In case a series of
// pulses is to be played, such as an alarm that is repeated multiple times,
// the prefix pulse is required only once as the first tone in the sequence of
// tones.
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
// To re-iterate, only Debian 8 or later run as a VM seems to require a blank
// prefix pulse. Use mchron command 'b' (beep) to test this.
#define ALSA_PREFIX_PULSE_MS	95
//#define ALSA_PREFIX_PULSE_MS	0
#define ALSA_PREFIX_ADD_MS	(ALSA_PREFIX_PULSE_MS + 25)

// Monochron global data
extern volatile uint8_t almAlarming;
extern uint16_t almTickerSnooze;
extern int16_t almTickerAlarm;
extern volatile rtcDateTime_t rtcDateTime;
extern volatile uint8_t mcAlarming;

#ifdef MARIO
// Mario chiptune alarm data
extern const unsigned char __attribute__ ((progmem)) MarioTones[];
extern const unsigned char __attribute__ ((progmem)) MarioBeats[];
extern const unsigned char __attribute__ ((progmem)) MarioMaster[];
extern const int marioMasterLen;
#endif

// Stubbed button data
volatile uint8_t btnPressed = BTN_NONE;
volatile uint8_t btnHold = BTN_NONE;
volatile uint8_t btnTickerHold = 0;
volatile uint8_t btnHoldRelReq = GLCD_FALSE;
volatile uint8_t btnHoldRelCfm = GLCD_FALSE;

// Stubbed hardware related stuff
uint16_t MCUSR = 0;
uint16_t DDRB = 0;
uint16_t DDRC = 0;
uint16_t DDRD = 0;
uint16_t TCCR0A = 0;
uint16_t TCCR0B = 0;
uint16_t OCR0A = 0;
uint16_t OCR2A = 0;
// Initial value 16 for OCR2B defines full lcd backlight brightness
uint16_t OCR2B = 16;
uint16_t TIMSK0 = 0;
uint16_t TIMSK2 = 0;
uint16_t TCCR1B = 0;
uint16_t TCCR2A = 0;
uint16_t TCCR2B = 0;
uint16_t PORTB = 0;
uint16_t PORTC = 0;
uint16_t PORTD = 0;
uint16_t PINB = 0;
uint16_t PIND = 0;

// Debug output file
FILE *stubDebugStream = NULL;

// Keypress hold data
static uint8_t hold = GLCD_FALSE;
static uint8_t lastChar = '\0';

// Stubbed eeprom data. An atmega328p has 1KB of eeprom.
static uint8_t stubEeprom[EE_SIZE];

// Stubbed alarm play pid
static pid_t alarmPid = -1;

// Local brightness to detect changes
static u08 stubBacklight = 16;

// Active help function when running an emulator
static void (*stubHelp)(void) = NULL;

// Event handler stub data
static int eventCycleState = CYCLE_NOWAIT;
static int eventInit = GLCD_TRUE;

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
static int kbMode = KB_MODE_LINE;
static struct timeval tvKbThen;

// Local function prototypes
static int kbHit(void);
static void alarmPidStart(void);
static void alarmPidStop(void);

//
// Function: alarmSoundReset
//
// Stub to reset the internal alarm settings. It is needed to prevent the
// alarm to restart when the clock feed or Monochron command is quit while
// alarming and is later restarted with the same or different settings.
//
void alarmSoundReset(void)
{
  mcAlarming = GLCD_FALSE;
  almAlarming = GLCD_FALSE;
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
  mcAlarming = GLCD_FALSE;
  almTickerSnooze = 0;
}

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

  // Were we successfull in forking
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
#ifdef MARIO
    // Generate a command that plays the Mario chiptune

    char *mallocPtr = NULL;
    int i = 0;
    int j = 0;
    int lineStart = 0;
    int lineLength = 0;
    int paramsIdx = 0;
    int totalLength = 0;

    // Get the total number of Mario tones to play and based on that
    // declare an array that fits all required shell commands to play it
    for (i = 0; i <= marioMasterLen - 2; i = i + 2)
      totalLength = totalLength + MarioMaster[i + 1];
    if (ALSA_PREFIX_PULSE_MS > 0)
      totalLength++;
    char *params[totalLength * 2 + 7];

    // Begin of the execvp parameters
    params[0] = "/usr/bin/play";
    params[1] = "-q";

    // Generate the Mario chiptune tones for execvp
    paramsIdx = 2;

    // Prefix blank pulse when needed to mitigate alsa buffer under-run
    if (ALSA_PREFIX_PULSE_MS > 0)
    {
      mallocPtr = malloc(80);
      sprintf(mallocPtr, "|/usr/bin/sox -n -p synth %f sin 0",
        (float)(ALSA_PREFIX_ADD_MS) / 1000);
      params[paramsIdx] = mallocPtr;
      paramsIdx++;
    }

    // Mario alarm
    for (i = 0; i < marioMasterLen; i = i + 2)
    {
      lineStart = MarioMaster[i];
      lineLength = MarioMaster[i + 1];
      for (j = lineStart; j < lineStart + lineLength; j++)
      {
        // Add the tone to be played
        mallocPtr = malloc(80);

        // The tone using a beat byte
        sprintf(mallocPtr, "|/usr/bin/sox -n -p synth %f sin %d",
          (float)(MarioBeats[j] * MAR_TEMPO / MAR_BEAT_FACTOR) / 1000,
          MarioTones[j] * MAR_TONE_FACTOR);
        params[paramsIdx] = mallocPtr;
        paramsIdx++;

        // Add a pauze of half a beat between tones
        mallocPtr = malloc(80);
        sprintf(mallocPtr, "|/usr/bin/sox -n -p synth %f sin %d",
          (float)(MAR_TEMPO / 2) / 1000, 0);
        params[paramsIdx] = mallocPtr;
        paramsIdx++;
      }
    }

    // Set use of alsa and play repeat and end of the execvp parameters
    params[paramsIdx] = "-t";
    params[paramsIdx + 1] = "alsa";
    params[paramsIdx + 2] = "repeat";
    params[paramsIdx + 3] = "100";
    params[paramsIdx + 4] = NULL;

    // Play Mario!
    execvp("/usr/bin/play", params);
#else
    // Two-tone alarm
    // Go play the alarm for max (0.325 + 0.325 + 0.325 + 0.325) * 3600 = 4680 secs
    //
    // The code below will result in something like this:
    // execlp("/usr/bin/play", "/usr/bin/play", "-q",
    //   "|/usr/bin/sox -n -p synth 0.325000 sin 4000",
    //   "|/usr/bin/sox -n -p synth 0.325000 sin 0",
    //   "|/usr/bin/sox -n -p synth 0.325000 sin 3750",
    //   "|/usr/bin/sox -n -p synth 0.325000 sin 0",
    //   "-t", "alsa", "repeat", "3600", 0);
    //
    // Example of a similar (yet shorter) command in bash is as follows (copy/paste):
    // /usr/bin/play -q "|/usr/bin/sox -n -p synth 0.3 sin 4000" "|/usr/bin/sox -n -p synth 0.3 sin 0" -t alsa repeat 7200

    char soxTone1[80], soxTone2[80], soxSilent[80];

    sprintf(soxTone1, "|/usr/bin/sox -n -p synth %f sin %d",
      (float)SND_TICK_TONE_MS / 1000, ALARM_FREQ_1);
    sprintf(soxTone2, "|/usr/bin/sox -n -p synth %f sin %d",
      (float)SND_TICK_TONE_MS / 1000, ALARM_FREQ_2);
    sprintf(soxSilent, "|/usr/bin/sox -n -p synth %f sin 0",
      (float)SND_TICK_TONE_MS / 1000);
    execlp("/usr/bin/play", "/usr/bin/play", "-q",
      soxTone1, soxSilent, soxTone2, soxSilent,
      "-t", "alsa", "repeat", "3600", NULL);
#endif
    exit(0);
  }
  else
  {
    // Log info on alarm audio process
    if (DEBUGGING)
    {
      char msg[45];
      sprintf(msg, "Playing alarm audio via PID %d", alarmPid);
      DEBUGP(msg);
    }
  }
}

//
// Function: alarmPidStop
//
// Stub to stop playing the alarm. It may be restarted by functional
// code.
//
static void alarmPidStop(void)
{
  // Only stop when a process was started to play the alarm
  if (alarmPid >= 0)
  {
    // Kill it...
    char msg[45];
    sprintf(msg, "Stopping alarm audio via PID %d", alarmPid);
    DEBUGP(msg);
    sprintf(msg, "kill -s 9 %d", alarmPid);
    system(msg);
  }
  alarmPid = -1;
}

//
// Function: alarmSwitchSet
//
// Set the alarm switch position to on or off
//
void alarmSwitchSet(uint8_t on, uint8_t show)
{
  uint16_t pinMask = 0;

  // Switch alarm on or off
  if (on == GLCD_TRUE)
  {
    if (show == GLCD_TRUE)
      printf("alarm  : on\n");
    pinMask = ~(0x1 << ALARM);
    ALARM_PIN = ALARM_PIN & pinMask;
  }
  else
  {
    if (show == GLCD_TRUE)
      printf("alarm  : off\n");
    pinMask = 0x1 << ALARM;
    ALARM_PIN = ALARM_PIN | pinMask;
  }
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
void alarmSwitchToggle(uint8_t show)
{
  uint16_t pinMask = 0;

  // Toggle alarm switch
  if (ALARM_PIN & _BV(ALARM))
  {
    pinMask = ~(0x1 << ALARM);
    ALARM_PIN = ALARM_PIN & pinMask;
    if (show == GLCD_TRUE)
      alarmSwitchShow();
  }
  else
  {
    pinMask = 0x1 << ALARM;
    ALARM_PIN = ALARM_PIN | pinMask;
    if (show == GLCD_TRUE)
      alarmSwitchShow();
  }
}

//
// Function: kbHit
//
// Get keypress (if any)
//
static int kbHit(void)
{
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET(STDIN_FILENO, &rdfs);

  select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
  return FD_ISSET(STDIN_FILENO, &rdfs);
}

//
// Function: kbKeypressScan
//
// Scan keyboard for keypress. The last key in the keyboard buffer
// is returned. However, if a search for the quit key is requested
// and that key was pressed, the quit character is returned instead.
// No keypress returns the null character.
//
char kbKeypressScan(u08 quitFind)
{
  char ch = '\0';
  u08 quitFound = GLCD_FALSE;
  int myKbMode = KB_MODE_LINE;
  struct timeval tvKbNow;
  suseconds_t timeDiff;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
  {
    kbModeSet(KB_MODE_SCAN);
  }
  else
  {
    // For next keyboard scan wait at least 50 msec since last one
    gettimeofday(&tvKbNow, NULL);
    timeDiff = (tvKbNow.tv_sec - tvKbThen.tv_sec) * 1E6 +
      tvKbNow.tv_usec - tvKbThen.tv_usec;
    if (timeDiff >= 50000)
      tvKbThen = tvKbNow;
    else
      return '\0';
  }

  // Read pending input buffer
  while (kbHit())
  {
    ch = getchar();
    if (quitFind == GLCD_TRUE && (ch == 'q' || ch == 'Q'))
      quitFound = GLCD_TRUE;
  }

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  // If we found the quit key return that one
  if (quitFound == GLCD_TRUE)
    ch = 'q';

  return ch;
}

//
// Function: kbModeGet
//
// Get keyboard input mode (line or keypress)
//
int kbModeGet(void)
{
  return kbMode;
}

//
// Function: kbModeSet
//
// Set keyboard input mode
//
void kbModeSet(int dir)
{
  // Only change mode if needed to avoid weird keyboard behavior
  if (dir == KB_MODE_SCAN && kbMode != KB_MODE_SCAN)
  {
    // Setup keyboard scan (signal keypress)
    tcgetattr(STDIN_FILENO, &termOld);
    termNew = termOld;
    termNew.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &termNew);
    kbMode = KB_MODE_SCAN;
    gettimeofday(&tvKbThen, NULL);
  }
  else if (dir == KB_MODE_LINE && kbMode != KB_MODE_LINE)
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
void stubBeep(uint16_t hz, uint16_t msec)
{
  char shellCmd[150];

  if (ALSA_PREFIX_PULSE_MS > msec)
  {
    // This is a short beep that requires alsa buffer under-run mitigation.
    // Add the blank pulse in front of the actual pulse to be played.
    char temp[75];
    sprintf(shellCmd,
      "/usr/bin/play -q \"|/usr/bin/sox -n -p synth %f sin 0\" ",
      (float)(ALSA_PREFIX_ADD_MS) / 1000);
    sprintf(temp,
      "\"|/usr/bin/sox -n -p synth %f sin %d\" -t alsa > /dev/null 2>&1",
      (float)(msec) / 1000L, hz);
    strcat(shellCmd, temp);
  }
  else
  {
    // Just play the requested beep
    sprintf(shellCmd,
      "/usr/bin/play -q -n -t alsa synth %f sin %d > /dev/null 2>&1",
      (float)(msec) / 1000L, hz);
  }
  system(shellCmd);
}

//
// Function: stubDelay
//
// Stub to delay time in milliseconds
//
void stubDelay(int x)
{
  struct timeval sleepThis;

  // This is ugly....
  // When we're asked to sleep for KEYPRESS_DLY_1 msec assume we're called
  // from buttons.c.
  // Applying this sleep will make a poor UI experience with respect to
  // keyboard input handling. This is caused by undetected/unprocessed
  // trailing keyboard events caused by system sleep interaction. By ignoring
  // KEYPRESS_DLY_1 msec requests I will make you, dear user, very happy.
  if (x == KEYPRESS_DLY_1)
    return;
  sleepThis.tv_sec = 0;
  sleepThis.tv_usec = x * 1000;
  select(0, NULL, NULL, NULL, &sleepThis);
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
// Function: stubEepRead
//
// Stub for eeprom data read
//
uint8_t stubEepRead(uint8_t *eprombyte)
{
  if ((size_t)eprombyte >= EE_SIZE)
    emuCoreDump(CD_EEPROM, __func__, (size_t)eprombyte, 0, 0, 0);
  return stubEeprom[(size_t)eprombyte];
}

//
// Function: stubEepWrite
//
// Stub for eeprom data write
//
void stubEepWrite(uint8_t *eprombyte, uint8_t value)
{
  if ((size_t)eprombyte >= EE_SIZE)
    emuCoreDump(CD_EEPROM, __func__, (size_t)eprombyte, 0, 0, 0);
  stubEeprom[(size_t)eprombyte] = value;
}

//
// Function: stubEventGet
//
// Get an mchron event. It is a combination of a 75msec timer wait event since
// previous call, an optional keyboard event emulating the three buttons
// (m,s,+) and alarm switch (a), and misc emulator commands.
//
char stubEventGet(void)
{
  char c = '\0';
  uint8_t keypress = GLCD_FALSE;
  suseconds_t remaining;

  // Flush the lcd device
  ctrlLcdFlush();

  // Sleep the remainder of the current cycle
  if (eventInit == GLCD_TRUE)
  {
    // The first entry may not invoke a sleep but instead marks the
    // timer timestamp for the next cycle
    waitTimerStart(&tvCycleTimer);
    eventInit = GLCD_FALSE;
  }
  else
  {
    // Wait remaining time for cycle while detecting timer expiry
    waitTimerExpiry(&tvCycleTimer, ANIM_TICK_CYCLE_MS, GLCD_FALSE, &remaining);
    if (remaining >= 0)
    {
      inTimeCount++;
      waitTotal = waitTotal + remaining;
      if (minSleep > (int)(remaining / 1000))
        minSleep = (int)(remaining / 1000);
    }
    else if (eventCycleState == CYCLE_NOWAIT)
    {
      outTimeCount++;
    }
  }

  // Detect changes in lcd brightness
  if ((OCR2B >> OCR2B_BITSHIFT) != stubBacklight)
  {
    stubBacklight = (OCR2B >> OCR2B_BITSHIFT);
    ctrlLcdBacklightSet((u08)stubBacklight);
  }

  // Check if we run in single timer cycle
  if (eventCycleState == CYCLE_REQ_WAIT || eventCycleState == CYCLE_WAIT ||
      eventCycleState == CYCLE_WAIT_STATS)
  {
    struct timeval t;
    char waitChar = '\0';

    // Statistics
    singleCycleCount++;

    // Reset any keypress hold
    hold = GLCD_FALSE;

    // When going to cycle mode stop the alarm (if sounding) for non-nerve
    // wrecking emulator behavior, and also give a cycle mode helpmessage
    if (eventCycleState == CYCLE_REQ_WAIT)
    {
      alarmPidStop();
      printf("\n<cycle: c = next cycle, p = next cycle + stats, other key = resume> ");
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
      printf("\n<cycle: c = next cycle, p = next cycle + stats, other key = resume> ");
      fflush(stdout);
      ctrlStatsReset(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
    }

    // Wait for keypress every 75 msec interval
    while (!kbHit())
    {
      t.tv_sec = 0;
      t.tv_usec = 75 * 1000;
      select(0, NULL, NULL, NULL, &t);
    }

    // Clear buffer and make uppercase char a lowercase char
    while (kbHit())
      waitChar = getchar();
    if (waitChar >= 'A' && waitChar <= 'Z')
      waitChar = waitChar - 'A' + 'a';

    // Verify keypress and its impact on the wait state
    if (waitChar != 'c' && waitChar != 'p')
    {
      // Not a 'c' or 'p' character: resume to normal mode state
      eventCycleState = CYCLE_REQ_NOWAIT;
      printf("\n");

      // Resume alarm audio (when needed)
      alarmPid = -1;
    }
    else if (eventCycleState == CYCLE_WAIT && waitChar == 'p')
    {
      // Move from wait state to wait with statistics
      eventCycleState = CYCLE_WAIT_STATS;
      ctrlStatsReset(CTRL_STATS_GLCD | CTRL_STATS_CTRL);
    }
    else if (eventCycleState == CYCLE_WAIT_STATS && waitChar == 'c')
    {
      // Move from wait with statistics state back to wait
      eventCycleState = CYCLE_WAIT;
    }
    waitChar = '\0';
  }
  else if (eventCycleState == CYCLE_REQ_NOWAIT)
  {
    // Resume from single cycle mode
    eventCycleState = CYCLE_NOWAIT;
  }

  // Get clock time and set alarm state
  monoTimer();

  // Do we need to do anything with the alarm sound
  if (alarmPid == -1 && almAlarming == GLCD_TRUE && almTickerSnooze == 0 &&
      (eventCycleState == CYCLE_REQ_NOWAIT || eventCycleState == CYCLE_NOWAIT))
  {
    // Start playing the alarm sound
    alarmPidStart();
  }
  else if (alarmPid >= 0 && (almAlarming == GLCD_FALSE || almTickerSnooze > 0 ||
      eventCycleState == CYCLE_WAIT))
  {
    // Stop playing the alarm sound
    alarmPidStop();
  }

  // Check if keyboard was hit
  btnHold = BTN_NONE;
  while (kbHit())
  {
    // Get keyboard character and make uppercase char a lowercase char
    keypress = GLCD_TRUE;
    c = getchar();
    if (c >= 'A' && c <= 'Z')
      c = c - 'A' + 'a';

    // Detect button hold start/end
    if (c == lastChar)
    {
      hold = GLCD_TRUE;
    }
    else
    {
      lastChar = c;
      hold = GLCD_FALSE;
    }

    if (c == 'a')
    {
      // Toggle the alarm switch
      printf("\n");
      alarmSwitchToggle(GLCD_TRUE);
    }
    else if (c == 'c')
    {
      // Init single timer cycle
      if (eventCycleState == CYCLE_NOWAIT)
        eventCycleState = CYCLE_REQ_WAIT;
    }
    else if (c == 'h')
    {
      // Provide help
      printf("\n");
      if (stubHelp != NULL)
        stubHelp();
      else
        printf("no help available\n");
    }
    else if (c == 'm')
    {
      // Menu button
      btnPressed = BTN_MENU;
    }
    else if (c == 'p')
    {
      // Print stub and glcd/lcd performance statistics
      printf("\nstatistics:\n");
      stubStatsPrint();
      ctrlStatsPrint(CTRL_STATS_FULL);
    }
    else if (c == 'r')
    {
      // Reset stub and glcd/lcd performance statistics
      stubStatsReset();
      ctrlStatsReset(CTRL_STATS_FULL);
      printf("\nstatistics reset\n");
    }
    else if (c == 's')
    {
      // Set button
      btnPressed = BTN_SET;
    }
    else if (c == 't')
    {
      // Print time/date/alarm
      printf("\n");
      emuTimePrint(ALM_MONOCHRON);
    }
    else if (c == '+')
    {
      // + button
      if (hold == GLCD_FALSE)
        btnPressed = BTN_PLUS;
      else
        btnHold = BTN_PLUS;
    }
    else if (c == '\n')
    {
      // Maybe the user wants to see a blank line, so echo it
      printf("\n");
    }
  }

  // Detect button hold end and confirm when requested
  if (keypress == GLCD_FALSE || btnHold == BTN_NONE)
  {
    lastChar = '\0';
    hold = GLCD_FALSE;
    if (btnHoldRelReq == GLCD_TRUE)
    {
      btnHoldRelCfm = GLCD_TRUE;
      btnHoldRelReq = GLCD_FALSE;
    }
  }

  return c;
}

//
// Function: stubEventInit
//
// Prepare the event generator for initial use by a requesting process.
//
void stubEventInit(int startMode, void (*stubHelpHandler)(void))
{
  eventInit = GLCD_TRUE;
  eventCycleState = startMode;
  btnPressed = BTN_NONE;
  stubHelp = stubHelpHandler;
  stubHelp();
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
  printf("  h = provide emulator help\n");
  printf("  p = print performance statistics\n");
  printf("  q = quit (valid only when clock is displayed)\n");
  printf("  r = reset performance statistics\n");
  printf("  t = print time/date/alarm\n");
  printf("hardware stub keys:\n");
  printf("  a = toggle alarm switch\n");
  printf("  m = menu button\n");
  printf("  s = set button\n");
  printf("  + = + button\n");
}

//
// Function: stubI2cMasterReceiveNI
//
// Receive time data from RTC
//
u08 stubI2cMasterReceiveNI(u08 deviceAddr, u08 length, u08 *data)
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
// Function: stubI2cMasterSendNI
//
// Send command data to RTC
//
u08 stubI2cMasterSendNI(u08 deviceAddr, u08 length, u08* data)
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
    uint8_t sec, min, hr, day, date, mon, yr;

    sec = ((data[1] >> 4) & 0xF) * 10 + (data[1] & 0xF);
    min = ((data[2] >> 4) & 0xF) * 10 + (data[2] & 0xF);
    hr = ((data[3] >> 4) & 0xF) * 10 + (data[3] & 0xF);
    day = ((data[4] >> 4) & 0xF) * 10 + (data[4] & 0xF);
    date = ((data[5] >> 4) & 0xF) * 10 + (data[5] & 0xF);
    mon = ((data[6] >> 4) & 0xF) * 10 + (data[6] & 0xF);
    yr = ((data[7] >> 4) & 0xF) * 10 + (data[7] & 0xF);
    stubTimeSet(sec, min, hr, day, date, mon, yr);
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
// Function: stubPutString
//
// Stub for debug string. Output is redirected to output file as specified
// on the mchron command line. If no output file is specified, debug info
// is discarded.
// Note: To enable debugging set DEBUGGING to 1 in monomain.h
//
void stubPutString(char *x, char *format)
{
  if (stubDebugStream != NULL)
    fprintf(stubDebugStream, format, x);
  return;
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
    printf("         avgSleep=%d msec, ",
      (int)(waitTotal / (double)inTimeCount / 1000));

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
// Values for sec (for time) and date (for date):
// 70 = keep time or date as-is
// 80 = reset time or date to system value
// other = use parameters for new time or date
//
int stubTimeSet(uint8_t sec, uint8_t min, uint8_t hr, uint8_t day,
  uint8_t date, uint8_t mon, uint8_t yr)
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
  if (sec == 70)
  {
    // Keep current time offset
    tmNewCopy.tm_sec = rtcDateTime.timeSec;
    tmNewCopy.tm_min = rtcDateTime.timeMin;
    tmNewCopy.tm_hour = rtcDateTime.timeHour;
  }
  else if (sec != 80)
  {
    // Overide on hms
    tmNewCopy.tm_sec = sec;
    tmNewCopy.tm_min = min;
    tmNewCopy.tm_hour = hr;
  }
  // else 80 -> default back to system time as currently populated

  // Verify what to do with date
  if (date == 70)
  {
    // Keep current date offset
    tmNewCopy.tm_mday = rtcDateTime.dateDay;
    tmNewCopy.tm_mon = rtcDateTime.dateMon - 1;
    tmNewCopy.tm_year = rtcDateTime.dateYear + 100;
  }
  else if (date != 80)
  {
    // Overide on dmy
    tmNewCopy.tm_mday = date;
    tmNewCopy.tm_mon = mon - 1;
    tmNewCopy.tm_year = yr + 100;
  }
  // else 80 -> default back to system time as currently populated

  // Prepare for a validity check on the requested date
  tmNewDateValidate1 = tmNewCopy;
  tmNewDateValidate2 = tmNewCopy;

  // Get system timestamp for current and new time to obtain the time
  // delta between the two timestamps
  timeNow = mktime(&tmNowCopy);
  timeNew = mktime(&tmNewCopy);
  if (timeNew == -1)
  {
    // The requested date is too far in future or past
    printf("date? beyond system range\n");
    return GLCD_FALSE;
  }
  timeDeltaNew = difftime(timeNew, timeNow);

  // Verify the requested date by making use of the fact that mktime
  // 'corrects' an invalid input date such as sep 31 to oct 1.
  // As mktime also shifts a time due to DST settings, that may cause a
  // shift in a day as well, set the time in the date to verify to around
  // noon to make sure such a DST timeshift cannot affect the date.
  tmNewDateValidate1.tm_hour = 12;
  timeClock = mktime(&tmNewDateValidate1);
  if (tmNewDateValidate1.tm_mday != tmNewDateValidate2.tm_mday ||
      tmNewDateValidate1.tm_mon != tmNewDateValidate2.tm_mon ||
      tmNewDateValidate1.tm_year != tmNewDateValidate2.tm_year)
  {
    printf("date? invalid\n");
    return GLCD_FALSE;
  }

  // Get delta between earlier retrieved current and new time and apply it
  // on a fresh current timestamp
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

  return GLCD_TRUE;
}

//
// Function: stubUartPutChar
//
// Stub for debug char. Output is redirected to output file as specified on
// the mchron command line. If no output file is specified, debug info is
// discarded.
// Note: To enable debugging set DEBUGGING to 1 in monomain.h
//
void stubUartPutChar(char x)
{
  if (stubDebugStream != NULL)
    fprintf(stubDebugStream, "%c", x);
  return;
}

//
// Function: stubUartPutDec
//
// Stub for debug decimal. Output is redirected to output file as specified on
// the mchron command line. If no output file is specified, debug info is
// discarded.
// Note: To enable debugging set DEBUGGING to 1 in monomain.h
//
void stubUartPutDec(int x, char *format)
{
  if (stubDebugStream != NULL)
    fprintf(stubDebugStream, format, x);
  return;
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
  struct timeval tvThen;
  suseconds_t timeDiff;
  int myKbMode = KB_MODE_LINE;

  // Set offset for wait period
  gettimeofday(&tvNow, NULL);
  gettimeofday(&tvThen, NULL);

  // Set end timestamp based current time plus delay
  tvThen.tv_usec = tvThen.tv_usec + delay * 1000;

  // Get the total time to wait
  timeDiff = (tvThen.tv_sec - tvNow.tv_sec) * 1E6 +
    tvThen.tv_usec - tvNow.tv_usec;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Wait till end of delay or a 'q' keypress and ignore
  // a remaining wait time that is less than 0.5 msec
  while (ch != 'q' && timeDiff > 500)
  {
    // Split time to delay up in parts of max 250msec
    tvWait.tv_sec = 0;
    if (timeDiff >= 250000)
      tvWait.tv_usec = 250000;
    else
      tvWait.tv_usec = timeDiff;

    // Wait
    select(0, NULL, NULL, NULL, &tvWait);

    // Did anything happen on the keyboard
    while (kbHit())
    {
      ch = getchar();
      if (ch == 'q' || ch == 'Q')
      {
        ch = 'q';
        break;
      }
      stubDelay(2);
    }

    // Based on last wait and keypress delays get time left to wait
    gettimeofday(&tvNow, NULL);
    timeDiff = (tvThen.tv_sec - tvNow.tv_sec) * 1E6 +
      tvThen.tv_usec - tvNow.tv_usec;
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
// Wait for keyboard keypress
//
char waitKeypress(int allowQuit)
{
  char ch = '\0';
  int myKbMode = KB_MODE_LINE;
  struct timeval t;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Clear input buffer
  while (kbHit())
    ch = getchar();

  // Wait for single keypress
  if (allowQuit == GLCD_FALSE)
    printf("<wait: press key to continue> ");
  else
    printf("<wait: q = quit, other key = continue> ");
  fflush(stdout);
  while (!kbHit())
  {
    // Wait 150 msec
    t.tv_sec = 0;
    t.tv_usec = 150 * 1000;
    select(0, NULL, NULL, NULL, &t);
  }
  ch = getchar();
  if (ch >= 'A' && ch <= 'Z')
    ch = ch - 'A' + 'a';

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  printf("\n");

  return ch;
}

//
// Function: waitTimerExpiry
//
// Wait amount of time (in msec) after a timer (re)start while optionally
// allowing a 'q' keypress interrupt. Restart the timer when timer has already
// expired upon entering this function or has expired after the remaining timer
// period. When pressing the 'q' key the timer will not be restarted.
// Return parameter remaining will indicate the remaining timer time in
// usec upon entering this function, or -1 in case the timer had already
// expired.
//
char waitTimerExpiry(struct timeval *tvTimer, int expiry, int allowQuit,
  suseconds_t *remaining)
{
  char ch = '\0';
  struct timeval tvNow;
  struct timeval tvSleep;
  suseconds_t timeDiff;

  // Get the total time to wait based on timer expiry
  gettimeofday(&tvNow, NULL);
  timeDiff = (tvTimer->tv_sec - tvNow.tv_sec) * 1E6 +
    tvTimer->tv_usec - tvNow.tv_usec + expiry * 1000;

  // See if timer has already expired
  if (timeDiff < 0)
  {
    // Get next timer offset using current time as reference, so do
    // not even attempt to compensate
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
      tvSleep.tv_sec = 0;
      tvSleep.tv_usec = timeDiff;
      select(0, NULL, NULL, NULL, &tvSleep);
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


//
// Several empty stubs for unlinked hardware related functions
//
void i2cInit(void) { return; }
void btnInit(void) { return; }
void uart_init(uint16_t x) { return; }
void wdt_disable(void) { return; }
void wdt_enable(uint16_t x) { return; }
void wdt_reset(void) { return; }
