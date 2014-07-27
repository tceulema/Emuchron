//*****************************************************************************
// Filename : 'stub.c'
// Title    : Stub functionality for MONOCHRON Emulator
//*****************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include "stub.h"
#include "lcd.h"
#include "../ks0108.h"
#include "../ratt.h"
#include "../glcd.h"
#include "../config.h"
#ifdef MARIO
#include "../mario.h"
#endif

// Monochron global data
extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t mcAlarming;
extern volatile uint8_t alarming;
extern uint16_t snoozeTimer;
extern int16_t alarmTimer;

// Stubbed button data
volatile uint8_t last_buttonstate, just_pressed, pressed;
volatile uint8_t buttonholdcounter = 0;

#ifdef MARIO
// Mario chiptune alarm data
extern const unsigned char __attribute__ ((progmem)) MarioTones[];
extern const unsigned char __attribute__ ((progmem)) MarioBeats[];
extern uint16_t marioLength;
#endif

// Stubbed hardware related stuff
uint16_t MCUSR = 0;
uint16_t DDRB = 0;
uint16_t DDRC = 0;
uint16_t DDRD = 0;
uint16_t TCCR0A = 0;
uint16_t TCCR0B = 0;
uint16_t OCR0A = 0;
uint16_t OCR2A = 0;
// Initial value 16 for OCR2B defines full LCD backlight brightness
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

// Stubbed eeprom data
uint8_t eeprom[EE_MAX+1];

// Stubbed alarm play pid
pid_t playPid = -1;

// Local brightness to detect changes
u08 stubBacklight = 16;

// Stuff for debugging
FILE *debugFile = NULL;

// Active help function when running an emulator
void (*stubHelp)(void) = NULL;

// Stuff for event handler stub
int eventCycleState = CYCLE_NOWAIT;
int eventInit = GLCD_TRUE;

// Local stuff for date/time and timer statistics
double timeDelta = 0L;
struct timeval timestampNext;
int inTimeCount = 0;
int outTimeCount = 0;
double waitTotal = 0L;
int minSleep = ANIMTICK_MS + 1;

// Local stuff for terminal settings for stdin keypress mode
static struct termios oldt, newt;
int kbMode = KB_MODE_LINE;

// Local function prototypes
int kbHit(void);

//
// Function: alarmClear
//
// Stub to reset the internal alarm settings. It is needed to prevent the
// alarm to restart when the clock feed or Monochron commands were quit with
// audible alarm and are resumed with the same or different settings.
//
void alarmClear(void)
{
  mcAlarming = GLCD_FALSE;
  alarming = GLCD_FALSE;
  snoozeTimer = 0;
  alarmTimer = -1;
}

//
// Function: alarmSoundKill
//
// Stub to stop playing the alarm and reset the alarm triggers.
// Audible alarm may resume later upon request.
//
void alarmSoundKill(void)
{
  alarmSoundStop();
  mcAlarming = GLCD_FALSE;
  snoozeTimer = 0;
}

//
// Function: alarmSoundStart
//
// Stub to start playing a continuous audio alarm
//
void alarmSoundStart(void)
{
  // Don't do anythng if we're already playing
  if (playPid >= 0)
    return;

  // Fork this process to play the alarm sound
  playPid = fork();

  // Were we successfull in forking
  if (playPid < 0)
  {
    // Failure to fork
    DEBUGP("*** Cannot fork alarm play process");
    playPid = -2;
    return;
  }
  else if (playPid == 0)
  {
    // We forked successfully!
#ifdef MARIO
    // Generate a command that plays the Mario chiptune

    char *mallocPtr = NULL;
    int i = 0;
    int paramsIdx = 0;
    char *params[marioLength * 2 + 5];

    // Begin of the execvp parameters
    params[0] = "/usr/bin/play";
    params[1] = "-q";

    // Generate the Mario chiptune tones for execvp
    paramsIdx = 2;
    for (i = 0; i < marioLength; i++)
    {
      // Add the tone to be played
      mallocPtr = malloc(80);
      sprintf(mallocPtr, "|/usr/bin/sox -n -p synth %f sin %d",
        (float)(MarioBeats[i] * MAR_TEMPO / MAR_BEATFACTOR) / 1000,
        MarioTones[i] * MAR_TONEFACTOR);
      params[paramsIdx] = mallocPtr;
      paramsIdx++;

      // Add the pauze between tones
      mallocPtr = malloc(80);
      sprintf(mallocPtr, "|/usr/bin/sox -n -p synth %f sin %d",
        (float)(MAR_TEMPO / 2) / 1000, 0);
      params[paramsIdx] = mallocPtr;
      paramsIdx++;
    }

    // Set play repeat and end of the execvp parameters
    params[paramsIdx] = "repeat";
    params[paramsIdx + 1] = "100";
    params[paramsIdx + 2] = NULL;

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
    //   "repeat", "3600", 0);
    //
    // Example of a similar (yet shorter) command in bash is as follows (copy/paste):
    // /usr/bin/play -q "|/usr/bin/sox -n -p synth 0.3 sin 4000" "|/usr/bin/sox -n -p synth 0.3 sin 0" repeat 7200

    char soxTone1[80], soxTone2[80], soxSilent[80];

    sprintf(soxTone1, "|/usr/bin/sox -n -p synth %f sin %d",
      (float)ALARMTICK_MS / 1000, ALARM_FREQ_1);
    sprintf(soxTone2, "|/usr/bin/sox -n -p synth %f sin %d",
      (float)ALARMTICK_MS / 1000, ALARM_FREQ_2);
    sprintf(soxSilent, "|/usr/bin/sox -n -p synth %f sin 0",
      (float)ALARMTICK_MS / 1000);
    execlp("/usr/bin/play", "/usr/bin/play", "-q",
      soxTone1, soxSilent, soxTone2, soxSilent,
      "repeat", "3600", NULL);
#endif
    exit(0);
  }
  else
  {
    // Log info on alarm audio process
    if (DEBUGGING)
    {
      char msg[35];
      sprintf(msg, "Playing alarm audio via PID %d", playPid);
      DEBUGP(msg);
    }
  }
}

//
// Function: alarmSoundStop
//
// Stub to stop playing the alarm. It may be restarted by functional
// code.
//
void alarmSoundStop(void)
{
  // Only stop when a process was started to play the alarm
  if (playPid >= 0)
  {
    // Kill it...
    char msg[45];
    sprintf(msg, "Stopping alarm audio via PID %d", playPid);
    DEBUGP(msg);
    sprintf(msg, "kill -s 9 %d", playPid);
    system(msg);
  }
  playPid = -1;
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
      printf("alarm : on\n");
    pinMask = ~(0x1 << ALARM);
    ALARM_PIN = ALARM_PIN & pinMask;
  }
  else
  {
    if (show == GLCD_TRUE)
      printf("alarm : off\n");
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
  {
    printf("alarm : off\n");
  }
  else
  {
    printf("alarm : on\n");
  }
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
    if (show == GLCD_TRUE)
      printf("alarm : on\n");
    pinMask = ~(0x1 << ALARM);
    ALARM_PIN = ALARM_PIN & pinMask;
  }
  else
  {
    if (show == GLCD_TRUE)
      printf("alarm : off\n");
    pinMask = 0x1 << ALARM;
    ALARM_PIN = ALARM_PIN | pinMask;
  }
}

//
// Function: coreDump
//
// There's something terribly wrong in the LCD interface. It is usually
// caused by bad functional clock code or a bad mchron command line request
// that tries to do stuff outside the boundaries of the LCD display.
// Provide some feedback and generate a coredump file (when enabled).
// Note: A graceful environment shutdown is taken care of by the SIGABRT
// signal handler, invoked by abort().
// Note: In order to get a coredump file it requires to run shell command 
// "ulimit -c unlimited" once in the mchron shell.
//
void coreDump(const char *location, u08 controller, u08 x, u08 y, u08 data)
{
  // Provide feedback
  // Note: y = vertical lcd byte location (0..7)
  printf("\n*** Invalid LCD api request in %s() ***\n", location);
  printf("Info = controller:x:y:data = %d:%d:%d:%d\n",
    (int)controller, (int)x, (int)y, (int)data);
  printf("Debug this by loading the coredump file (when created) in a debugger.\n");

  // Flush the LCD device so we get its contents as-is at the time of
  // the forced coredump: nice for analytic purposes. Works only for
  // ncurses. 
  lcdDeviceFlush(1);
  
  // Force coredump
  abort();
}

//
// Function: kbHit
//
// Get keypress (if any)
//
int kbHit(void)
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
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    kbMode = KB_MODE_SCAN;
  }
  else if (dir == KB_MODE_LINE && kbMode != KB_MODE_LINE)
  {
    // Setup line scan (signal cr/lf)
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    kbMode = KB_MODE_LINE;
  }
}

//
// Function: kbWaitDelay
//
// Wait amount of time (in msec) while allowing a 'q' keypress interrupt
//
char kbWaitDelay(int delay)
{
  char ch = '\0';
  struct timeval tvWait;
  struct timeval tvNow;
  struct timeval tvThen;
  suseconds_t timeDiff;
  int myKbMode = KB_MODE_LINE;

  // Set offset for wait period
  (void)gettimeofday(&tvNow, NULL);
  (void)gettimeofday(&tvThen, NULL);

  // Set end timestamp based current time plus delay
  tvThen.tv_usec = tvThen.tv_usec + delay * 1000;
  if (tvThen.tv_usec >= 1E6)
  {
    tvThen.tv_usec = tvThen.tv_usec - 1E6;
    tvThen.tv_sec++;
  }

  // Get the total time to wait
  timeDiff = (tvThen.tv_sec - tvNow.tv_sec) * 1E6 + 
    tvThen.tv_usec - tvNow.tv_usec;

  // Switch to keyboard scan mode if needed
  myKbMode = kbModeGet();
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_SCAN);

  // Wait till end of delay or a' 'q' keypress
  while (ch != 'q' && timeDiff > 1000)
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
        break;
      stubDelay(2);
    }

    // Based on last wait and keypress delays get time left to wait
    (void)gettimeofday(&tvNow, NULL);
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
// Function: kbWaitKeypress
//
// Wait for keyboard keypress
//
char kbWaitKeypress(int allowQuit)
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
    printf("<wait: q = quit, other key will continue> ");
  fflush(stdout);
  while (!kbHit())
  {
    // Wait 150 msec
    t.tv_sec = 0;
    t.tv_usec = 150 * 1000;
    select(0, NULL, NULL, NULL, &t);
  }
  ch = getchar();

  // Return to line mode if needed
  if (myKbMode == KB_MODE_LINE)
    kbModeSet(KB_MODE_LINE);

  printf("\n");

  return ch;
}
 
//
// Function: statsPrint
//
// Print stub and LCD device statistics
//
void statsPrint(void)
{
  printf("statistics:\n");

  // First print the stub statistics
  printf("stub   : cycle=%d msec, inTime=%d, outTime=%d\n         ",
    STUB_CYCLE/1000, inTimeCount, outTimeCount);
  if (inTimeCount == 0)
  {
    printf("avgSleep=- msec, ");
  }
  else
  {
    printf("avgSleep=%d msec, ",(int)(waitTotal / (double)inTimeCount / 1000));
  }
  if (minSleep == ANIMTICK_MS + 1)
  {
    printf("minSleep=- msec\n");
  }
  else
  {
    printf("minSleep=%d msec\n",minSleep);
  }
  
  // Print LCD device glut and/or ncurses statistics
  lcdStatsPrint();
}

//
// Function: statsReset
//
// Reset stub and LCD device statistics
//
void statsReset(void)
{
  // Reset stub statistics
  inTimeCount = 0;
  outTimeCount = 0;
  waitTotal = 0.0L;
  minSleep = ANIMTICK_MS + 1;
      
  // Reset LCD device statistics
  lcdStatsReset();
}

//
// Function: stubBeep
//
// Stub for system beep
//
void stubBeep(uint16_t hz, uint16_t msec)
{
  char shellCmd[100];

  sprintf(shellCmd, "/usr/bin/play -q -n synth %f sin %d 2>/dev/null",
    ((float)(msec)) / 1000L, hz);
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
// Function: stubEepromReset
//
// Stub for eeprom initialization
//
void stubEepromReset(void)
{
  stubEeprom_write_byte((uint8_t *)EE_INIT, 0);
}

//
// Function: stubEeprom_read_byte
//
// Stub for eeprom data read
//
uint8_t stubEeprom_read_byte(uint8_t *eprombyte)
{
  size_t idx = (size_t)eprombyte;
  return eeprom[idx];
}

//
// Function: stubEeprom_write_byte
//
// Stub for eeprom data write
//
void stubEeprom_write_byte(uint8_t *eprombyte, uint8_t value)
{
  size_t idx = (size_t)eprombyte;
  eeprom[idx] = value;
}

//
// Function: stubGetEvent
//
// Get an mchron event. It is a combination of a 75msec timer wait event since
// previous call, an optional keyboard event emulating the three buttons
// (m,s,+) and alarm switch (a), and misc emulator commands.
//
char stubGetEvent(void)
{
  struct timeval tv;
  struct timeval sleepThis;
  suseconds_t timeDiff;
  char c = '\0';

  // Flush the LCD device
  lcdDeviceFlush(0);

  // Get timediff with previous call
  (void) gettimeofday(&tv, NULL);
  if (eventInit == GLCD_TRUE)
  {
    // The first entry may not invoke a sleep
    timeDiff = 0;
  }
  else
  {
    // Get timediff between last and current cycle
    timeDiff = (timestampNext.tv_sec - tv.tv_sec) * 1E6 + 
    timestampNext.tv_usec - tv.tv_usec;
  }

  // Check if we need to sleep
  if (timeDiff > 0)
  {
    // Less than 75msec was passed: sleep the rest upto 75 msec
    inTimeCount++;
    waitTotal = waitTotal + timeDiff;
    if (minSleep > (int)(timeDiff / 1000))
      minSleep = (int)(timeDiff / 1000);
    sleepThis.tv_sec = 0;
    sleepThis.tv_usec = timeDiff; 
    select(0, NULL, NULL, NULL, &sleepThis);

    // Set next timestamp based on previous one
    timestampNext.tv_usec = timestampNext.tv_usec + STUB_CYCLE;
    if (timestampNext.tv_usec >= 1E6)
    {
      timestampNext.tv_usec = timestampNext.tv_usec - 1E6;
      timestampNext.tv_sec++;
    }
  }
  else
  {
    // More than 75msec was passed or first entry: do not sleep
    if (eventInit == GLCD_FALSE && eventCycleState == CYCLE_NOWAIT)
      outTimeCount++;

    // Set next timestamp based on current time.
    // So, don't bother to attempt to get back in line.
    timestampNext.tv_usec = tv.tv_usec + STUB_CYCLE;
    if (timestampNext.tv_usec >= 1E6)
    {
      timestampNext.tv_usec = timestampNext.tv_usec - 1E6;
      timestampNext.tv_sec = tv.tv_sec + 1;
    }
    else
    {
      timestampNext.tv_sec = tv.tv_sec;
    }
  }

  // Detect changes in LCD brightness
  if ((OCR2B >> OCR2B_BITSHIFT) != stubBacklight)
  {
    stubBacklight = (OCR2B >> OCR2B_BITSHIFT);
    lcdDeviceBacklightSet((u08)stubBacklight);
  }

  // Check if we run in single timer cycle
  if (eventCycleState == CYCLE_REQ_WAIT || eventCycleState == CYCLE_WAIT)
  {
    struct timeval t;
    char waitChar = '\0';

    // When going to cycle mode stop the alarm (if sounding) for non-nerve
    // wrecking emulator behavior, and also give a cycle mode helpmessage
    if (eventCycleState == CYCLE_REQ_WAIT)
    {
      alarmSoundStop();
      printf("<cycle: c = next cycle, other key will resume> ");
      fflush(stdout);
      // Continue in single cycle mode
      eventCycleState = CYCLE_WAIT;
    }

    // Wait for keypress every 75 msec interval
    while (!kbHit())
    {
      t.tv_sec = 0;
      t.tv_usec = 75 * 1000;
      select(0, NULL, NULL, NULL, &t);
    }

    // Clear buffer
    while (kbHit())
    {
      waitChar = getchar();
    }

    // Verify keypress
    if (waitChar != 'c' && waitChar != 'C')
    {
      // Not a 'c' character: resume normal mode
      eventCycleState = CYCLE_REQ_NOWAIT;
      printf("\n");

      // Resume alarm audio (when needed)
      playPid = -1;
    }
    waitChar = '\0';
  }
  else if (eventCycleState == CYCLE_REQ_NOWAIT)
  {
    // Resume from single cycle mode
    eventCycleState = CYCLE_NOWAIT;
  }

  // Get clock time and set alarm state
  stubTimer();

  // Do we need to do anything with the alarm sound
  if (playPid == -1 && alarming == GLCD_TRUE && snoozeTimer == 0 &&
      (eventCycleState == CYCLE_REQ_NOWAIT || eventCycleState == CYCLE_NOWAIT))
  {
    // Start playing the alarm sound
    alarmSoundStart();
  }
  else if (playPid >= 0 && (alarming == GLCD_FALSE ||
      snoozeTimer > 0 || eventCycleState == CYCLE_WAIT))
  {
    // Stop playing the alarm sound
    alarmSoundStop();
  }

  // Check if keyboard was hit
  pressed = 0;
  while (kbHit())
  {
    c = getchar();
    if (c == 'a' || c == 'A')
    {
      // Toggle the alarm switch
      alarmSwitchToggle(GLCD_TRUE);
    }
    else if (c == 'c' || c == 'C')
    {
      // Init single timer cycle
      if (eventCycleState == CYCLE_NOWAIT)
        eventCycleState = CYCLE_REQ_WAIT;
    }
    else if (c == 'h' || c == 'H')
    {
      // Provide help
      if (stubHelp != NULL)
        stubHelp();
      else
        printf("no help available\n");
    }
    else if (c == 'm' || c == 'M')
    {
      // Menu button
      just_pressed = just_pressed | BTTN_MENU;
      pressed = pressed | BTTN_MENU;
    }
    else if (c == 'p' || c == 'P')
    {
      // Print stub and LCD performance statistics
      statsPrint();
    }
    else if (c == 'r' || c == 'R')
    {
      // Reset stub and LCD performance statistics
      statsReset();
      printf("statistics reset\n");
    }
    else if (c == 's' || c == 'S')
    {
      // Set button
      just_pressed = just_pressed | BTTN_SET;
      pressed = pressed | BTTN_SET;
    }
    else if (c == '+')
    {
      // + button
      just_pressed = just_pressed | BTTN_PLUS;
      pressed = pressed | BTTN_PLUS;
    }
    else if (c == '\n')
    {
      // Maybe the user wants to see a blank line, so echo it
      printf("\n");
    }
  }

  // Signal first entry completion
  eventInit = GLCD_FALSE;

  return c;
}

//
// Function: stubHelpClockFeed
//
// Provide keypress help when running the clock emulator
//
void stubHelpClockFeed(void)
{
  printf("emuchron clock emulator:\n");
  printf("  c = execute single application cycle\n");
  printf("  h = provide emulator help\n");
  printf("  p = print performance statistics\n");
  printf("  q = quit\n");
  printf("  r = reset performance statistics\n");
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
  printf("  c = execute single application cycle\n");
  printf("  h = provide emulator help\n");
  printf("  p = print performance statistics\n");
  printf("  q = quit (valid only when clock is displayed)\n");
  printf("  r = reset performance statistics\n");
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
  
    (void) gettimeofday(&tv, NULL);
    timeClock = tv.tv_sec + timeDelta;
    tm = localtime(&timeClock);

    data[0] = i2bcd(tm->tm_sec);
    data[1] = i2bcd(tm->tm_min);
    data[2] = i2bcd(tm->tm_hour);
    data[4] = i2bcd(tm->tm_mday);
    data[5] = i2bcd(tm->tm_mon + 1);
    data[6] = i2bcd(tm->tm_year % 100);
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
// Function: stubPutstring
//
// Stub for debug string. Output is redirected to output file as specified
// on the mchron command line. If no output file is specified, debug info
// is discarded.
// Note: To enable debugging set DEBUGGING to 1 in ratt.h
//
void stubPutstring(char *x, char *format)
{
  if (debugFile != NULL)
    fprintf(debugFile, format, x);
  return;
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
  readi2ctime();
  (void) gettimeofday(&tvNow, NULL);
  tmNow = localtime(&tvNow.tv_sec);

  // Copy current time
  tmNowCopy = *tmNow;
  tmNewCopy = *tmNow;

  // Verify what to do with time
  if (sec == 70)
  {
    // Keep current time offset
    tmNewCopy.tm_sec = time_s;
    tmNewCopy.tm_min = time_m;
    tmNewCopy.tm_hour = time_h;
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
    tmNewCopy.tm_mday = date_d;
    tmNewCopy.tm_mon = date_m - 1;
    tmNewCopy.tm_year = date_y + 100;
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
  (void) gettimeofday(&tvNew, NULL);
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
  readi2ctime();

  return GLCD_TRUE;
}

//
// Function: stubUart_putchar
//
// Stub for debug char. Output is redirected to output file as specified on
// the mchron command line. If no output file is specified, debug info is
// discarded.
// Note: To enable debugging set DEBUGGING to 1 in ratt.h
//
void stubUart_putchar(char x)
{
  if (debugFile != NULL)
    fprintf(debugFile, "%c", x);
  return;
}

//
// Function: stubUart_putdec
//
// Stub for debug decimal. Output is redirected to output file as specified on
// the mchron command line. If no output file is specified, debug info is
// discarded.
// Note: To enable debugging set DEBUGGING to 1 in ratt.h
//
void stubUart_putdec(int x, char *format)
{
  if (debugFile != NULL)
    fprintf(debugFile, format, x);
  return;
}

//
// Several empty stubs for unlinked hardware related functions
//
void i2cInit(void) { return; }
void buttonsInit(void) { return; }
void uart_init(uint16_t x) { return; }
void wdt_disable(void) { return; }
void wdt_enable(uint16_t x) { return; }
void wdt_reset(void) { return; }

