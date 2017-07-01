//*****************************************************************************
// Filename : 'monomain.c'
// Title    : The main clock engine for MONOCHRON
//*****************************************************************************

#ifndef EMULIN
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "i2c.h"
#include "util.h"
#else
#include "emulator/stub.h"
#include "emulator/stubrefs.h"
#endif
#include "buttons.h"
#include "ks0108.h"
#include "glcd.h"
#include "config.h"
#include "anim.h"
#include "alarm.h"
#include "monomain.h"

// When configured in alarm.h [firmware] load the mario melody data
#include "mariotune.h"

// Wait forever
#define halt(x)		while (1)

// Constants for calculating Timer2 interrupt return rates.
// Using a return divider we make the RTC readout at a certain time interval,
// x times per second. Using a secondary return divider we can make an event
// that fires about once every second.
// The original Monochron RTC readout rate is ~5.7Hz, which is sufficient to
// support the pong clock, since it does not support a seconds indicator.
// In Emuchron v1.0 this is increased to ~8.5Hz. This is done to detect
// changes in seconds faster, leading to smoother 'seconds tick' animation in
// clocks.
// In Emuchron v3.0 this is increased to ~13.6Hz, executing at least one time
// check per clock cycle of 75 msec (~13.3Hz). This is the best time event
// granularity we can get for a functional clock.
// Uncomment to implement RTC readout @ ~5.7Hz
//#define TIMER2_RETURN_1	80
//#define TIMER2_RETURN_2	6
// Uncomment to implement RTC readout @ ~8.5Hz
//#define TIMER2_RETURN_1	53
//#define TIMER2_RETURN_2	9
// Uncomment to implement RTC readout @ ~13.6Hz
#define TIMER2_RETURN_1	33
#define TIMER2_RETURN_2	14

// The following global variables are for use in any Monochron clock.
// In a Monochron clock its contents are defined and stable.
extern volatile uint8_t mcClockTimeEvent;
extern volatile uint8_t mcAlarmH, mcAlarmM;
extern volatile uint8_t mcMchronClock;
extern volatile uint8_t mcCycleCounter;
extern volatile uint8_t mcBgColor, mcFgColor;

// The following variables drive the configuration menu
extern volatile uint8_t cfgScreenMutex;
extern volatile uint8_t cfgTickerActivity;

// The following variables drive the button state and ADC conversion rate
extern volatile uint8_t btnPressed;
extern volatile uint8_t btnTickerHold;
extern volatile uint8_t btnTickerConv;

// The following variables drive the realtime clock
volatile rtcDateTime_t rtcDateTime;
volatile rtcDateTime_t rtcDateTimeNext;
volatile uint8_t rtcTimeEvent = GLCD_FALSE;

// The following variables drive the Monochron alarm
volatile uint8_t almAlarming = GLCD_FALSE;
volatile uint8_t almAlarmSelect = 0;
int16_t almTickerAlarm = 0;
uint16_t almTickerSnooze = 0;
static uint8_t almStopRequest = GLCD_FALSE;
volatile uint8_t almSwitchOn = GLCD_FALSE;

// The following variables drive the clock animation and display state
static volatile uint8_t animTickerCycle;
volatile uint8_t animDisplayMode = SHOW_TIME;

// Runtime sound data for Mario or two-tone alarm
static volatile uint16_t sndTickerTone = 0;
#ifdef MARIO
#ifndef EMULIN
static uint16_t sndMarioFreq = 0;
#endif
static uint8_t sndMarioIdx = 0;
static uint8_t sndMarioIdxEnd = 0;
static uint8_t sndMarioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
static uint8_t sndMarioPauze = GLCD_TRUE;
#else
static uint8_t sndAlarmTone = 0;
#endif

// Time dividers
static uint8_t t2divider1 = 0;
//static uint8_t t2divider2 = 0;

// Local function prototypes
static void almSnoozeSet(void);
static uint8_t bcdDecode(uint8_t element, uint8_t nibbleMask);
static void rtcFailure(uint8_t code, uint8_t id);

// The eeprom init default values upon eeprom reset/relocate.
// For eeprom definitions refer to monomain.h [firmware].
const uint8_t __attribute__ ((progmem)) eepDefault[] =
{
  EE_INITIALIZED,	// EE_OFFSET+0  - EE_INIT
  8,			// EE_OFFSET+1  - EE_ALARM_HOUR
  0,			// EE_OFFSET+2  - EE_ALARM_MIN
  OCR2A_VALUE,		// EE_OFFSET+3  - EE_BRIGHT
  1,			// EE_OFFSET+4  - EE_VOLUME (not used in Emuchron)
  DATELONG_DOW,		// EE_OFFSET+5  - EE_REGION (not used in Emuchron)
  TIME_24H,		// EE_OFFSET+6  - EE_TIME_FORMAT (not used in Emuchron)
  0,			// EE_OFFSET+7  - EE_SNOOZE (not used in Emuchron)
  0,			// EE_OFFSET+8  - EE_BGCOLOR
  9,			// EE_OFFSET+9  - EE_ALARM_HOUR2
  15,			// EE_OFFSET+10 - EE_ALARM_MIN2
  10,			// EE_OFFSET+11 - EE_ALARM_HOUR3
  30,			// EE_OFFSET+12 - EE_ALARM_MIN3
  11,			// EE_OFFSET+13 - EE_ALARM_HOUR4
  45,			// EE_OFFSET+14 - EE_ALARM_MIN4
  0			// EE_OFFSET+15 - EE_ALARM_SELECT
};

//
// Function: main
//
// The Monochron main() function. It initializes the Monochron environment
// and ends up in an infinite loop that processes button presses and
// switches between and updates Monochron clocks.
//
#ifdef EMULIN
int monoMain(void)
#else
int main(void)
#endif
{
  u08 actionDefault = GLCD_FALSE;

  // Check if we were reset
  MCUSR = 0;

  // Just in case we were reset inside of the glcd init function
  // which would happen if the lcd is not plugged in. The end result
  // of that is it will beep, pause, for as long as there is no lcd
  // plugged in.
  wdt_disable();

  // Init uart
  DEBUGP("*** UART");
  uart_init(BRRL_192);

  // Init piezo
  DEBUGP("*** Piezo");
  PIEZO_DDR |= _BV(PIEZO);

  // Init system real time clock
  DEBUGP("*** System clock");
  rtcDateTimeNext.timeSec = 60;
  rtcTimeInit();

  // Init data saved in eeprom
  DEBUGP("*** EEPROM");
  eepInit();
  mcBgColor = eeprom_read_byte((uint8_t *)EE_BGCOLOR) % 2;
  mcFgColor = (mcBgColor == GLCD_OFF ? GLCD_ON : GLCD_OFF);
  almAlarmSelect = eeprom_read_byte((uint8_t *)EE_ALARM_SELECT) % 4;
  almTimeGet(almAlarmSelect, &mcAlarmH, &mcAlarmM);

  // Init buttons
  DEBUGP("*** Buttons");
  btnInit();

  // Init based on alarm switch
  DEBUGP("*** Alarmstate");
  almStateSet();

  // Setup 1-ms timer on timer0
  DEBUGP("*** 1-ms Timer");
  TCCR0A = _BV(WGM01);
  TCCR0B = _BV(CS01) | _BV(CS00);
  OCR0A = 125;
  TIMSK0 |= _BV(OCIE0A);

  // Turn backlight on
  DEBUGP("*** Backlight");
  DDRD |= _BV(3);
#ifndef BACKLIGHT_ADJUST
  PORTD |= _BV(3);
#else
  TCCR2A = _BV(COM2B1); // PWM output on pin D3
  TCCR2A |= _BV(WGM21) | _BV(WGM20); // fast PWM
  TCCR2B |= _BV(WGM22);
  OCR2A = OCR2A_VALUE;
  OCR2B = eeprom_read_byte((uint8_t *)EE_BRIGHT);
#endif
  DDRB |= _BV(5);

  // Init lcd.
  // glcdInit locks and disables interrupts in one of its functions.
  // If the lcd is not plugged in, glcd will run forever. For good
  // reason, it would be desirable to know that the lcd is plugged in
  // and working correctly as a result. This is why we are using a
  // watch dog timer. The lcd should be initialized in way less than
  // 500 ms.
  DEBUGP("*** LCD");
  beep(4000, 100);
  wdt_enable(WDTO_2S);
  glcdInit(mcBgColor);

  // Be friendly and give a welcome message
  DEBUGP("*** Welcome");
  animWelcome();

  // Init to display the first defined Monochron clock
  DEBUGP("*** Start initial clock");
  rtcMchronTimeInit();
  animClockDraw(DRAW_INIT_FULL);
  DEBUGP("*** Init clock completed");

  // This is the main loop event handler that will run forever
  while (1)
  {
    // Set the duration of a single animation loop cycle
    animTickerCycle = ANIM_TICK_CYCLE_MS;

    // Check buttons to see if we have interaction stuff to deal with
    if (btnPressed && almAlarming == GLCD_TRUE)
    {
      // We're alarming while showing a clock. The M button will stop the
      // alarm while the +/S buttons will invoke/reset snoozing.
      if (btnPressed & BTN_MENU)
        almStopRequest = GLCD_TRUE;
      else
        almSnoozeSet();
    }
    else if (btnPressed & BTN_MENU)
    {
      // The Menu button is pressed so run the config menu.
      // When completed sync Monochron time and re-init the active clock.
      cfgMenuMain();
      rtcMchronTimeInit();
      animClockDraw(DRAW_INIT_FULL);
    }
    else // BTN_SET or BTN_PLUS
    {
      // Check the Set button
      if (btnPressed & BTN_SET)
      {
        if (animClockButton(btnPressed) == GLCD_FALSE)
        {
          // No button method has been defined for the active clock.
          // Default to the action set for the + button.
          actionDefault = GLCD_TRUE;
          DEBUGP("Set button dflt to +");
        }
      }

      // Check the + button and default Set button action
      if ((btnPressed & BTN_PLUS) || actionDefault == GLCD_TRUE)
      {
        u08 initType;
        u08 myClock = mcMchronClock;

        // Select the next clock
        DEBUGP("Clock -> Next clock");
        initType = animClockNext();

        // If one clock configured invoke buttonpress, else init clock
        if (mcMchronClock == myClock)
          animClockButton(btnPressed);
        else
          animClockDraw(initType);

        actionDefault = GLCD_FALSE;
      }
    }

    // Clear any button press to allow a new button event. Then have the
    // active clock update itself.
    btnPressed = BTN_NONE;
    animClockDraw(DRAW_CYCLE);

    // Get event(s) while waiting the remaining time of the loop cycle
#ifdef EMULIN
    if (stubEventGet() == 'q')
      return 0;
#else
    while (animTickerCycle);
    // Uncomment this to manually 'step' using a terminal keypress via FTDI
    //uart_getchar();
#endif

    // Admin on cycle counter
    mcCycleCounter++;
  }

  // We should never get here
  halt();
}

//
// 1 msec signal handler
//
// Used for handling msec countdown timers, audible alarm and switching between
// tones in audible alarm. As this is called every 1 msec try to keep its CPU
// footprint as small as possible.
//
#ifndef EMULIN
SIGNAL(TIMER0_COMPA_vect)
{
  // Countdown timer for main loop animation (every 75 msec)
  if (animTickerCycle > 0)
    animTickerCycle--;
  // Countdown timer for detecting press-hold of + button
  if (btnTickerHold > 0)
    btnTickerHold--;
  // Countdown timer for next ADC button conversion
  if (btnTickerConv > 0)
  {
    btnTickerConv--;
    if (btnTickerConv == 0)
      btnConvStart();
  }

  // In case of audible alarming update alarm melody, if needed
  if (almAlarming == GLCD_TRUE && almTickerSnooze == 0)
  {
    if (sndTickerTone == 0)
    {
      // Audio countdown timer expired: we need to change the piezo
#ifdef MARIO
      // Mario chiptune alarm
      if (sndMarioIdx == sndMarioIdxEnd && sndMarioPauze == GLCD_TRUE)
      {
        // End of current tune line. Move to next line or return to beginning.
        if (sndMarioMasterIdx == (uint8_t)(sizeof(MarioMaster) - 2))
          sndMarioMasterIdx = 0;
        else
          sndMarioMasterIdx = sndMarioMasterIdx + 2;

        sndMarioIdx = pgm_read_byte(&MarioMaster[sndMarioMasterIdx]);
        sndMarioIdxEnd = sndMarioIdx +
          pgm_read_byte(&MarioMaster[sndMarioMasterIdx + 1]);
      }

      // Should we play a tone or a post-tone half beat pauze
      if (sndMarioPauze == GLCD_TRUE)
      {
        // Last played a half beat pauze, so now we play a tone
        sndMarioPauze = GLCD_FALSE;
        sndMarioFreq = (uint16_t)(pgm_read_byte(&MarioTones[sndMarioIdx])) *
          MAR_TONE_FACTOR;
        sndTickerTone = (uint16_t)(pgm_read_byte(&MarioBeats[sndMarioIdx])) *
          MAR_TEMPO / MAR_BEAT_FACTOR;
      }
      else
      {
        // Last played a tone, so now we play a half beat pauze
        sndMarioPauze = GLCD_TRUE;
        sndMarioFreq = 0;
        sndTickerTone = MAR_TEMPO / 2;
        // When done we move to next tone
        sndMarioIdx++;
      }

      if (sndMarioFreq == 0)
      {
        // Be silent
        TCCR1B = 0;
        // Turn off piezo
        PIEZO_PORT &= ~_BV(PIEZO);
      }
      else
      {
        // Make noise
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS10);
        TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
        // Set the frequency to use
        OCR1A = (F_CPU / sndMarioFreq) / 2;
      }
#else
      // Two-tone alarm
      // Tone cycle period timeout: go to next one
      sndTickerTone = SND_TICK_TONE_MS;
      if (TCCR1B == 0)
      {
        // End of silent period: next one will do audio
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS10); // CTC with fastest timer
        TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
        // Select the frequency to use
        if (sndAlarmTone == 0)
          OCR1A = (F_CPU / ALARM_FREQ_1) / 2;
        else
          OCR1A = (F_CPU / ALARM_FREQ_2) / 2;
        // Toggle frequency for next audio cycle
        sndAlarmTone = ~sndAlarmTone;
      }
      else
      {
        // End of audio period: next one be silent
        TCCR1B = 0;
        // Turn off piezo
        PIEZO_PORT &= ~_BV(PIEZO);
      }
#endif
    }
    // Countdown timer for next change in audio
    sndTickerTone--;
  }
  else if (almTickerAlarm == -1)
  {
    // Respond to request to stop alarm due to alarm timeout
    TCCR1B = 0;
    // Turn off piezo
    PIEZO_PORT &= ~_BV(PIEZO);
    almTickerAlarm = 0;
#ifdef MARIO
    // On next audible alarm start at beginning of Mario tune
    sndMarioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
    sndMarioIdx = sndMarioIdxEnd;
    sndMarioPauze = GLCD_TRUE;
#else
    sndAlarmTone = 0;
#endif
  }
}
#endif

//
// Time signal handler
//
// Read and sync the RTC with internal system time. It can result in a
// a Monochron time event, alarm trip event or alarm-end event when
// appropriate.
// Runs at about every 2 msec, but will sync time considerably less due to
// time dividers.
//
#ifdef EMULIN
void monoTimer(void)
#else
SIGNAL(TIMER2_OVF_vect)
#endif
{
  wdt_reset();
#ifdef EMULIN
  if (t2divider1 == 0)
#else
#ifdef BACKLIGHT_ADJUST
  if (t2divider1 == TIMER2_RETURN_1)
#else
  if (t2divider1 == 5)
#endif
#endif
  {
    t2divider1 = 0;
  }
  else
  {
    t2divider1++;
    return;
  }

  // This occurs at approx 5.7Hz, 8.5Hz or 13.6Hz.
  // For this refer to defs of TIMER2_RETURN_x above.
  uint8_t lastSec = rtcDateTime.timeSec;

  //DEBUGP("* RTC");

  // Check alarm/snooze stop request from menu button
  if (almStopRequest == GLCD_TRUE)
  {
    almTickerAlarm = 0;
    almStopRequest = GLCD_FALSE;
  }

  // Check the alarm switch state
  almStateSet();

  // Get RTC time and compare with saved one
  rtcTimeRead();
  if (rtcDateTime.timeSec != lastSec)
  {
    // Time has changed. Do admin on countdown timers.
    if (cfgTickerActivity)
      cfgTickerActivity--;
    if (almAlarming == GLCD_TRUE)
    {
      if (almTickerSnooze == 1)
      {
        // Init alarm data at starting positions right before we
        // return from snooze
#ifdef MARIO
        sndMarioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
        sndMarioIdx = sndMarioIdxEnd;
        sndMarioPauze = GLCD_TRUE;
#else
        sndAlarmTone = 0;
#endif
        DEBUGP("Alarm -> Snooze timeout");
      }
      if (almTickerSnooze)
        almTickerSnooze--;
      if (almTickerAlarm > 0)
        almTickerAlarm--;
    }

    // Log new time
    DEBUG(putstring("**** "));
    DEBUG(uart_putw_dec(rtcDateTime.timeHour));
    DEBUG(uart_putchar(':'));
    DEBUG(uart_putw_dec(rtcDateTime.timeMin));
    DEBUG(uart_putchar(':'));
    DEBUG(uart_putw_dec(rtcDateTime.timeSec));
    DEBUG(putstring_nl(""));

    // If we're in the config main menu we have a continuous time update
    // except when editing time itself or alarms or when we're changing
    // menu (cfgScreenMutex)
    if (animDisplayMode != SHOW_TIME && cfgScreenMutex == GLCD_FALSE)
      cfgMenuTimeShow();
  }

  // Signal a clock time event only when the previous has not been processed
  // yet. This prevents a race condition on time data between the timer handler
  // and the functional clock handler. The functional clock handler will clear
  // the clock time event after which a new one can be raised.
  if (rtcTimeEvent == GLCD_FALSE && rtcDateTimeNext.timeSec != rtcDateTime.timeSec)
  {
    rtcDateTimeNext = rtcDateTime;
    DEBUGP("Raise time event");
    rtcTimeEvent = GLCD_TRUE;
  }

  // When the alarm switch is set to On we need to check a few things
  if (almSwitchOn == GLCD_TRUE)
  {
    if (almAlarming == GLCD_FALSE && rtcDateTime.timeSec == 0 &&
        rtcDateTime.timeMin == mcAlarmM && rtcDateTime.timeHour == mcAlarmH)
    {
      // The active alarm time is tripped
      DEBUGP("Alarm -> Tripped");
      almAlarming = GLCD_TRUE;
      almTickerAlarm = ALM_TICK_ALARM_SEC;
    }
    else if (almAlarming == GLCD_TRUE && almTickerAlarm == 0)
    {
      // Audible alarm has timed out (some may not wake up by an alarm)
      // or someone pressed the Menu button while alarming/snoozing
      DEBUGP("Alarm -> Timeout");
      almAlarming = GLCD_FALSE;
      almTickerSnooze = 0;
      almTickerAlarm = -1;
    }
  }

  // Control timeout counters. Note this is tricky stuff since entering
  // this code section is also influenced by the t2divider1 counter at
  // the top of this function. With the current settings this code section
  // is entered about once per second.
  // To use this, uncomment the t2divider2 declaration at the top of this
  // file, the code section below and then add timeout counter logic.
  /*if (t2divider2 == TIMER2_RETURN_2)
  {
    t2divider2 = 0;
  }
  else
  {
    t2divider2++;
    return;
  }
  // Add your timeout counter functionality here
  */
}

#ifndef EMULIN
SIGNAL(TIMER1_OVF_vect)
{
  PIEZO_PORT ^= _BV(PIEZO);
}
#endif

#ifndef EMULIN
SIGNAL(TIMER1_COMPA_vect)
{
  PIEZO_PORT ^= _BV(PIEZO);
}
#endif

//
// Function: almSnoozeSet
//
// Make the alarm go snoozing
//
static void almSnoozeSet(void)
{
  DEBUGP("Alarm -> Snooze");
  almTickerSnooze = ALM_TICK_SNOOZE_SEC;
  almTickerAlarm = ALM_TICK_ALARM_SEC + ALM_TICK_SNOOZE_SEC;
  TCCR1B = 0;
  // Turn off piezo
  PIEZO_PORT &= ~_BV(PIEZO);
}

//
// Function: almStateSet
//
// Turn on/off the alarm based on the alarm switch position
//
void almStateSet(void)
{
  if (ALARM_PIN & _BV(ALARM))
  {
    // Turn off alarm if needed
    if (almSwitchOn == GLCD_TRUE)
    {
      DEBUGP("Alarm -> Inactive");
      almSwitchOn = GLCD_FALSE;
      almTickerSnooze = 0;
      almTickerAlarm = 0;
      if (almAlarming == GLCD_TRUE)
      {
        // If there is audible alarm turn it off
        DEBUGP("Alarm -> Off");
        almAlarming = GLCD_FALSE;
        TCCR1B = 0;
#ifdef MARIO
        // On next audible alarm start at beginning of Mario tune
        sndMarioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
        sndMarioIdx = sndMarioIdxEnd;
        sndMarioPauze = GLCD_TRUE;
#else
        sndAlarmTone = 0;
#endif
        // Turn off piezo
        PIEZO_PORT &= ~_BV(PIEZO);
      }
    }
  }
  else
  {
    // Turn on functional alarm if needed
    if (almSwitchOn == GLCD_FALSE)
    {
      DEBUGP("Alarm -> Active");
      // Alarm on!
      almSwitchOn = GLCD_TRUE;
      // Reset snoozing and alarm
      almTickerSnooze = 0;
      almTickerAlarm = 0;
#ifdef MARIO
      // On next audible alarm start at beginning of Mario tune
      sndMarioMasterIdx = (uint8_t)(sizeof(MarioMaster) - 2);
      sndMarioIdx = sndMarioIdxEnd;
      sndMarioPauze = GLCD_TRUE;
#else
      sndAlarmTone = 0;
#endif
    }
  }
}

//
// Function: almTimeGet
//
// Get the requested alarm time from the eeprom
//
void almTimeGet(uint8_t alarmId, volatile uint8_t *hour,
  volatile uint8_t *min)
{
  uint8_t *eepHour, *eepMin;

  if (alarmId == 0)
  {
    eepHour = (uint8_t *)EE_ALARM_HOUR;
    eepMin = (uint8_t *)EE_ALARM_MIN;
  }
  else
  {
    // Alarm 2..4 are sequential in eeprom
    eepHour = (uint8_t *)(EE_ALARM_HOUR2) + (alarmId - 1) * 2;
    eepMin = eepHour + 1;
  }

  *hour = eeprom_read_byte(eepHour) % 24;
  *min = eeprom_read_byte(eepMin) % 60;
}

//
// Function: almTimeSet
//
// Save the requested alarm time in the eeprom
//
void almTimeSet(uint8_t alarmId, volatile uint8_t hour, volatile uint8_t min)
{
  uint8_t *aHour, *aMin;

  if (alarmId == 0)
  {
    aHour = (uint8_t *)EE_ALARM_HOUR;
    aMin = (uint8_t *)EE_ALARM_MIN;
  }
  else
  {
    // Alarm 2..4 are sequential in memory
    aHour = (uint8_t *)(EE_ALARM_HOUR2) + (alarmId - 1) * 2;
    aMin = aHour + 1;
  }

  eeprom_write_byte(aHour, hour);
  eeprom_write_byte(aMin, min);
}

//
// Function: bcdDecode
//
// Decode a BCD element into an integer type
//
static uint8_t bcdDecode(uint8_t element, uint8_t nibbleMask)
{
  return ((element >> 4) & nibbleMask) * 10 + (element & 0xF);
}

//
// Function: bcdEncode
//
// Encode an integer type into a BCD element
//
uint8_t bcdEncode(uint8_t x)
{
  return ((x / 10) << 4) | (x % 10);
}

//
// Function: beep
//
// Sound beep.
// Note: The beep duration granularity is 25 msec
//
void beep(uint16_t freq, uint8_t duration)
{
#ifdef EMULIN
  stubBeep(freq, duration);
#else
  uint8_t i = duration;

  // Start the beep
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10); // CTC with fastest timer
  TIMSK1 = _BV(TOIE1) | _BV(OCIE1A);
  OCR1A = (F_CPU / freq) / 2;

  // Wait time
  while (i >= 25)
  {
    _delay_ms(25);
    i = i - 25;
  }

  // Turn off piezo
  TCCR1B = 0;
  PIEZO_PORT &= ~_BV(PIEZO);
#endif
}

//
// Function: eepInit
//
// Initialize the eeprom. This should occur only in rare occasions as
// once it is set it should stay initialized forever.
//
void eepInit(void)
{
  uint8_t *i;

  // Check the integrity of the eeprom for Monochron defaults
  if (eeprom_read_byte((uint8_t *)EE_INIT) != EE_INITIALIZED)
  {
    // Set eeprom to a default state. The last address to update is EE_INIT
    // since that location identifies the integrity of the eeprom data.
    // So, start at address 1 up to the end of the init array.
    for (i = (uint8_t *)1; i < (uint8_t *)sizeof(eepDefault); i++)
      eeprom_write_byte(EE_OFFSET + i, pgm_read_byte(&eepDefault[(size_t)i]));
    eeprom_write_byte((uint8_t *)EE_INIT, EE_INITIALIZED);
  }
}

//
// Function: rtcDotw
//
// Return the day number of the week (0=Sun .. 6=Sat)
//
uint8_t rtcDotw(uint8_t mon, uint8_t day, uint8_t year)
{
  uint16_t myMon, myYear;

  // Calculate day of the week
  myMon = mon;
  myYear = 2000 + year;
  if (mon < 3)
  {
    myMon += 12;
    myYear -= 1;
  }
  return (day + (2 * myMon) + (6 * (myMon + 1) / 10) + myYear +
    (myYear / 4) - (myYear / 100) + (myYear / 400) + 1) % 7;
}

//
// Function: rtcFailure
//
// Report i2c RTC interface error
//
static void rtcFailure(uint8_t code, uint8_t id)
{
  // Not able to instruct/read/set RTC. Beep forever since we're screwed.
  DEBUG(putstring("i2c data: "));
  DEBUG(uart_putw_dec(id));
  DEBUG(putstring(", "));
  DEBUG(uart_putw_dec(code));
  DEBUG(putstring_nl(""));
  sei();
  while(1)
  {
    beep(4000, 100);
    _delay_ms(100);
    beep(4000, 100);
    _delay_ms(1000);
  }
}

//
// Function: rtcLeapYear
//
// Identify whether a year is a leap year
//
uint8_t rtcLeapYear(uint16_t year)
{
  return ((!(year % 4) && (year % 100)) || !(year % 400));
}

//
// Function: rtcMchronTimeInit
//
// (Re-)initialize the functional Monochron clock time. It will discard a
// pending time event (may be zero seconds old, a few seconds or even
// minutes) and will create a fresh time event that is based on *now*.
//
void rtcMchronTimeInit(void)
{
  mcClockTimeEvent = GLCD_FALSE;
#ifndef EMULIN
  // First wait for a registered time event (that may pass immediately)
  while (rtcTimeEvent == GLCD_FALSE);
  // Then restart the time scan mechanism while forcing a new time to
  // be generated
  DEBUGP("Clear time event");
  rtcDateTimeNext.timeSec = 60;
  rtcTimeEvent = GLCD_FALSE;
  // And finally wait for a new registered time event in next time scan cycle
  while (rtcTimeEvent == GLCD_FALSE);
#else
  // As the emulator event loop is not in a separate thread nor is
  // interrupt driven we have to do things a bit differently
  DEBUGP("Clear time event");
  rtcTimeEvent = GLCD_FALSE;
  rtcDateTimeNext.timeSec = 60;
  monoTimer();
#endif
  mcClockTimeEvent = GLCD_TRUE;
}

//
// Function: rtcTimeInit
//
// Initialize RTC time data for first time use
//
void rtcTimeInit(void)
{
  // Talk to clock
  i2cInit();

  if (rtcTimeRead())
  {
    DEBUGP("Require reset RTC");
    rtcTimeWrite(00, 00, 12, 1, 1, 17); // Noon Jan 1 2017
  }

  rtcTimeRead();

  DEBUG(putstring("\nread "));
  DEBUG(uart_putw_dec(rtcDateTime.timeHour));
  DEBUG(uart_putchar(':'));
  DEBUG(uart_putw_dec(rtcDateTime.timeMin));
  DEBUG(uart_putchar(':'));
  DEBUG(uart_putw_dec(rtcDateTime.timeSec));

  DEBUG(uart_putchar('\t'));
  DEBUG(uart_putw_dec(rtcDateTime.dateDay));
  DEBUG(uart_putchar('/'));
  DEBUG(uart_putw_dec(rtcDateTime.dateMon));
  DEBUG(uart_putchar('/'));
  DEBUG(uart_putw_dec(rtcDateTime.dateYear));
  DEBUG(putstring_nl(""));

  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20); // div by 1024
  // Overflow ~30Hz = 8MHz/(255 * 1024)

  // Enable interrupt
  TIMSK2 = _BV(TOIE2);

  sei();
}

//
// Function: rtcTimeRead
//
// Read the real-time clock (RTC)
//
uint8_t rtcTimeRead(void)
{
  uint8_t regaddr = 0, r;
  uint8_t clockdata[8];

  // Get the time from the RTC
  cli();
  r = i2cMasterSendNI(0xD0, 1, &regaddr);
  if (r != 0)
    rtcFailure(r, 0);
  r = i2cMasterReceiveNI(0xD0, 7, clockdata);
  sei();
  if (r != 0)
    rtcFailure(r, 1);

  // Process the time from the RTC
  rtcDateTime.timeSec = bcdDecode(clockdata[0], 0x7);
  rtcDateTime.timeMin = bcdDecode(clockdata[1], 0x7);
  if (clockdata[2] & _BV(6))
  {
    // "12 hr" mode
    rtcDateTime.timeHour = ((clockdata[2] >> 5) & 0x1) * 12 +
      bcdDecode(clockdata[2], 0x1);
  }
  else
  {
    // "24 hr" mode
    rtcDateTime.timeHour = bcdDecode(clockdata[2], 0x3);
  }
  rtcDateTime.dateDay = bcdDecode(clockdata[4], 0x3);
  rtcDateTime.dateMon = bcdDecode(clockdata[5], 0x1);
  rtcDateTime.dateYear = bcdDecode(clockdata[6], 0xF);

  return clockdata[0] & 0x80;
}

//
// Function: rtcTimeWrite
//
// Set the real-time clock (RTC)
//
void rtcTimeWrite(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day,
  uint8_t mon, uint8_t year)
{
  uint8_t clockdata[8];

  clockdata[0] = 0;           // address
  clockdata[1] = bcdEncode(sec);
  clockdata[2] = bcdEncode(min);
  clockdata[3] = bcdEncode(hour);
  clockdata[4] = 0;           // day of week
  clockdata[5] = bcdEncode(day);
  clockdata[6] = bcdEncode(mon);
  clockdata[7] = bcdEncode(year);

  cli();
  uint8_t r = i2cMasterSendNI(0xD0, 8, clockdata);
  sei();
  if (r != 0)
    rtcFailure(r, 2);
}

